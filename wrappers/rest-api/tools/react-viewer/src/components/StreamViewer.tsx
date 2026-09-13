import { useEffect, useRef, useState, useCallback, useMemo, type ReactNode } from 'react'
import { useAppStore } from '../store'
import { WebRTCHandler } from '../api/webrtc'
import { apiClient } from '../api/client'
import { DepthLegend } from './DepthLegend'
import { useMetric, useSettingsStore } from '../store/settings'
import { orderKeys, tileKey, useLayoutStore } from '../store/layout'
import { describeMetadata, formatMetadataValue, isDepthMappingDevice, lessScreamy, metadataLabel } from '../utils/metadataDecoders'
import { boxFromPixels, displayRect, pixelFromMouse } from '../utils/tileGeometry'
import { FULL_FRAME, WHEEL_STEP, isZoomed, pan, zoomAt, zoomTransform, type Zoom } from '../utils/zoom'
import type { RegionOfInterest } from '../api/types'
import { formatDistance } from '../utils/units'
import type { DeviceState, StreamConfig, StreamMetadata } from '../api/types'

/** Marks the one element a tile may be dragged by (the stream label). */
const DRAG_HANDLE = 'data-tile-drag-handle'

// A stream with its device context
interface DeviceStream {
  paused: boolean
  /** A recording that is paused or stopped legitimately delivers no frames. */
  playbackIdle?: boolean
  recording?: boolean
  metadataServerTime?: number
  deviceId: string
  deviceName: string
  serialNumber: string
  config: StreamConfig
  metadata?: StreamMetadata
}

export function StreamViewer() {
  const { deviceStates } = useAppStore()
  const { tileOrder, maximized, swapTiles, setMaximized } = useLayoutStore()
  const [dragging, setDragging] = useState<string | null>(null)
  const dragArmed = useRef(false)
  
  // Collect all enabled streams from all active devices; hide tiles until they actually stream.
  const activeStreams = useMemo(() => {
    const streams: DeviceStream[] = []
    
    Object.values(deviceStates).forEach((ds: DeviceState) => {
      ds.streamConfigs.filter(c => c.enable).forEach(config => {
        // Is this specific stream running on its sensor?
        const sensorStatus = ds.sensorStreamingStatus?.[config.sensor_id]
        // stream_types is the current shape; stream_type is the older single-stream one
        const activeTypes = sensorStatus?.stream_types || (sensorStatus?.stream_type ? [sensorStatus.stream_type] : [])
        const streamIsActive = sensorStatus?.is_streaming === true &&
                        activeTypes.some(st => st.toLowerCase() === config.stream_type.toLowerCase())

        if (!streamIsActive) return

        streams.push({
          deviceId: ds.device.device_id,
          deviceName: ds.device.name,
          serialNumber: ds.device.serial_number,
          config,
          metadata: ds.streamMetadata[config.stream_type],
          paused: !!sensorStatus?.paused,
          playbackIdle: !!ds.playback && ds.playback.state !== 'playing',
          metadataServerTime: ds.metadataServerTime,
          recording: !!ds.record?.recording && !ds.record.paused,
        })
      })
    })
    
    // Legacy order within a device (depth, color, IR, motion) unless the user rearranged.
    const byDevice = new Map<string, DeviceStream[]>()
    for (const s of streams) byDevice.set(s.deviceId, [...(byDevice.get(s.deviceId) ?? []), s])
    const ordered: DeviceStream[] = []
    for (const [deviceId, list] of byDevice) {
      const keyed = new Map(list.map((s) => [tileKey(deviceId, s.config.stream_type), s]))
      for (const key of orderKeys([...keyed.keys()], tileOrder[deviceId])) ordered.push(keyed.get(key)!)
    }
    return ordered
  }, [deviceStates, tileOrder])

  const activeDeviceCount = Object.keys(deviceStates).length
  const maximizedStream = maximized ? activeStreams.find((s) => tileKey(s.deviceId, s.config.stream_type) === maximized) : undefined
  const shown = maximizedStream ? [maximizedStream] : activeStreams

  const tileFrame = (stream: DeviceStream, child: ReactNode) => {
    const key = tileKey(stream.deviceId, stream.config.stream_type)
    const present = activeStreams.filter((s) => s.deviceId === stream.deviceId).map((s) => tileKey(s.deviceId, s.config.stream_type))
    return (
      <div
        key={key}
        data-testid="stream-tile"
        draggable={!maximizedStream}
        // Only the stream label starts a rearrange. Anywhere else the press belongs to the
        // tile itself (drawing an ROI, panning a zoomed frame, a header button): a native
        // drag there would steal the mouseup and the click with it.
        onMouseDown={(e) => { dragArmed.current = !!(e.target as HTMLElement).closest?.(`[${DRAG_HANDLE}]`) }}
        onDragStart={(e) => { if (!dragArmed.current) { e.preventDefault(); return } setDragging(key) }}
        onDragOver={(e) => { if (dragging && dragging !== key) e.preventDefault() }}
        onDrop={() => { if (dragging) swapTiles(stream.deviceId, dragging, key, present); setDragging(null) }}
        onDragEnd={() => { dragArmed.current = false; setDragging(null) }}
        className={`min-h-0 min-w-0 h-full ${dragging === key ? 'opacity-50' : ''}`}
      >
        {child}
      </div>
    )
  }

  return (
    <div className="h-full">
      {activeStreams.length === 0 ? (
        <div className="h-full flex items-center justify-center text-gray-500">
          <div className="text-center">
            <svg
              className="w-16 h-16 mx-auto mb-4 opacity-50"
              fill="none"
              stroke="currentColor"
              viewBox="0 0 24 24"
            >
              <path
                strokeLinecap="round"
                strokeLinejoin="round"
                strokeWidth={1}
                d="M15 10l4.553-2.276A1 1 0 0121 8.618v6.764a1 1 0 01-1.447.894L15 14M5 18h8a2 2 0 002-2V8a2 2 0 00-2-2H5a2 2 0 00-2 2v8a2 2 0 002 2z"
              />
            </svg>
            <p className="text-lg">Nothing is streaming!</p>
            <p className="text-sm mt-1">Connect a device and enable any stream to start</p>
          </div>
        </div>
      ) : (
        <div
          className="h-full grid gap-2"
          style={{
            // minmax(0, 1fr) and not 1fr: a tile whose content is taller than its share
            // (the IMU readout) would otherwise stretch its row and squash the video rows.
            gridTemplateColumns: `repeat(${Math.min(shown.length, 2)}, minmax(0, 1fr))`,
            gridTemplateRows: `repeat(${Math.ceil(shown.length / 2)}, minmax(0, 1fr))`,
          }}
        >
          {shown.map((stream) => {
            const isMotionStream = ['gyro', 'accel'].includes(stream.config.stream_type.toLowerCase())
            const key = tileKey(stream.deviceId, stream.config.stream_type)
            const maximize = { maximized: !!maximizedStream, toggle: () => setMaximized(maximizedStream ? null : key) }

            if (isMotionStream) {
              return tileFrame(stream,
                <IMUStreamTile
                  deviceId={stream.deviceId}
                  streamType={stream.config.stream_type}
                  showDeviceName={activeDeviceCount > 1}
                  deviceName={stream.deviceName}
                  serialNumber={stream.serialNumber}
                  metadata={stream.metadata}
                  pause={{ deviceId: stream.deviceId, sensorId: stream.config.sensor_id, paused: stream.paused }}
                  maximize={maximize}
                />
              )
            }

            return tileFrame(stream,
              <StreamTile
                deviceId={stream.deviceId}
                sensorId={stream.config.sensor_id}
                deviceName={stream.deviceName}
                serialNumber={stream.serialNumber}
                streamType={stream.config.stream_type}
                format={stream.config.format}
                metadata={stream.metadata}
                showDeviceName={activeDeviceCount > 1}
                pause={{ deviceId: stream.deviceId, sensorId: stream.config.sensor_id, paused: stream.paused }}
                metadataServerTime={stream.metadataServerTime}
                playbackIdle={stream.playbackIdle}
                maximize={maximize}
                recording={stream.recording}
              />
            )
          })}
        </div>
      )}
    </div>
  )
}

interface PauseState {
  deviceId: string
  sensorId: string
  paused: boolean
}

interface MaximizeState {
  maximized: boolean
  toggle: () => void
}

/** The legacy tile-header maximize/restore button. */
function MaximizeButton({ maximize, className = '' }: { maximize: MaximizeState; className?: string }) {
  return (
    <button
      type="button"
      onClick={maximize.toggle}
      title={maximize.maximized ? 'Restore tile' : 'Maximize tile'}
      aria-label={maximize.maximized ? 'Restore tile' : 'Maximize tile'}
      className={`px-2 py-0.5 bg-black/60 hover:bg-black/80 rounded text-xs text-white border border-gray-600 z-20 ${className}`}
    >
      {maximize.maximized ? '⤡' : '⤢'}
    </button>
  )
}

/** The legacy stream-header pause/resume button; pausing holds the whole sensor. */
function PauseButton({ pause, className = '' }: { pause: PauseState; className?: string }) {
  const setSensorPaused = useAppStore((s) => s.setSensorPaused)
  return (
    <button
      type="button"
      onClick={() => void setSensorPaused(pause.deviceId, pause.sensorId, !pause.paused)}
      title={pause.paused ? 'Resume sensor' : 'Pause sensor'}
      aria-label={pause.paused ? 'Resume sensor' : 'Pause sensor'}
      className={`px-2 py-0.5 bg-black/60 hover:bg-black/80 rounded text-xs text-white border border-gray-600 z-20 ${className}`}
    >
      {pause.paused ? '▶' : '❚❚'}
    </button>
  )
}

/** The legacy stream-header save button: downloads PNG + raw + metadata CSV as one zip. */
function SnapshotButton({ deviceId, streamType, className = '' }: { deviceId: string; streamType: string; className?: string }) {
  return (
    <a
      href={apiClient.snapshotUrl(deviceId, streamType)}
      download
      title="Save snapshot (PNG, raw, metadata)"
      aria-label="Save snapshot"
      className={`px-2 py-0.5 bg-black/60 hover:bg-black/80 rounded text-xs text-white border border-gray-600 z-20 ${className}`}
    >
      📷
    </a>
  )
}

// The legacy viewer cannot draw these either ("Rendering not supported", viewer.cpp).
const UNRENDERABLE_FORMATS = new Set(['raw10', 'raw16', 'mjpeg'])
// A stream whose newest frame is older than this on the server clock has stalled.
const STALE_AFTER_S = 2

interface StreamTileProps {
  deviceId: string
  sensorId: string
  deviceName: string
  serialNumber: string
  streamType: string
  format?: string
  showDeviceName?: boolean
  metadata?: StreamMetadata
  pause?: PauseState
  metadataServerTime?: number
  playbackIdle?: boolean
  maximize?: MaximizeState
  recording?: boolean
}

function StreamTile({
  deviceId, sensorId, deviceName, serialNumber, streamType, format, showDeviceName, metadata, pause, metadataServerTime, playbackIdle, maximize, recording,
}: StreamTileProps) {
  const videoRef = useRef<HTMLVideoElement>(null)
  const containerRef = useRef<HTMLDivElement>(null)
  const webrtcHandlerRef = useRef<WebRTCHandler | null>(null)
  const hoverRequestId = useRef(0)
  const [connectionState, setConnectionState] = useState<RTCPeerConnectionState | null>(null)
  const [fps, setFps] = useState(0)
  const [showMetadata, setShowMetadata] = useState(false)
  const lastFrameTime = useRef(0)
  const frameCount = useRef(0)
  const [hoverDepth, setHoverDepth] = useState<{
    x: number
    y: number
    depth: number | null
    mouseX: number
    mouseY: number
  } | null>(null)
  const [depthRange, setDepthRange] = useState<{ min: number; max: number }>({ min: 0, max: 6 })
  // Max usable range (stream-model.cpp): shown while the depth sensor has the option on.
  const [maxUsableRange, setMaxUsableRange] = useState<number | null>(null)
  // Auto-exposure ROI (stream-model.cpp update_ae_roi_rect): drawn by dragging on the tile.
  const [roi, setRoi] = useState<RegionOfInterest | null>(null)
  const [roiMode, setRoiMode] = useState(false)
  const [roiDrag, setRoiDrag] = useState<{ x0: number; y0: number; x1: number; y1: number } | null>(null)
  // Wheel zoom / drag pan (stream-model.cpp show_frame) and the crosshair grid overlay
  const [zoom, setZoom] = useState<Zoom>(FULL_FRAME)
  const panStart = useRef<{ x: number; y: number } | null>(null)
  const previewRef = useRef<HTMLVideoElement>(null)
  const [tileRect, setTileRect] = useState<{ w: number; h: number } | null>(null)
  const [showGrid, setShowGrid] = useState(false)
  const gridPrefs = useSettingsStore((s) => s.settings?.viewer)

  const isDepthStream = streamType.toLowerCase() === 'depth'
  const metric = useMetric()
  const unrenderable = !!format && UNRENDERABLE_FORMATS.has(format.toLowerCase())
  const stalled = !pause?.paused && !playbackIdle && metadata?.received_at !== undefined && metadataServerTime !== undefined
    && metadataServerTime - metadata.received_at > STALE_AFTER_S

  // The sensor is starting as this tile mounts, and the SDK refuses the region until it
  // runs. One failed read must not hide the ROI button for the rest of the session, so a
  // failure is retried a few times; a sensor that answers "not supported" is taken at its word.
  useEffect(() => {
    let cancelled = false
    let timer: ReturnType<typeof setTimeout> | undefined
    let attempt = 0
    const probe = () => {
      apiClient.getRoi(deviceId, sensorId)
        .then((r) => { if (!cancelled) setRoi(r) })
        .catch(() => {
          if (cancelled) return
          setRoi({ supported: false })
          if (attempt++ < 5) timer = setTimeout(probe, 1500)
        })
    }
    probe()
    return () => { cancelled = true; if (timer) clearTimeout(timer) }
  }, [deviceId, sensorId])

  const tileSize = () => {
    const rect = containerRef.current?.getBoundingClientRect()
    return rect ? { w: rect.width, h: rect.height, left: rect.left, top: rect.top } : null
  }

  // Track the tile size so the zoomed <video> and the overlays stay aligned with the frame.
  useEffect(() => {
    const el = containerRef.current
    if (!el) return
    const measure = () => { const r = el.getBoundingClientRect(); setTileRect({ w: r.width, h: r.height }) }
    measure()
    if (typeof ResizeObserver === 'undefined') return
    const ro = new ResizeObserver(measure)
    ro.observe(el)
    return () => ro.disconnect()
  }, [])

  // The displayed frame area (letterboxed) and where a mouse position falls in it, as fractions.
  const display = tileRect && metadata ? displayRect(tileRect.w, tileRect.h, metadata.width, metadata.height) : null
  const fractionAt = (clientX: number, clientY: number) => {
    const t = tileSize()
    if (!t || !display) return null
    return { fx: (clientX - t.left - display.offsetX) / display.width, fy: (clientY - t.top - display.offsetY) / display.height }
  }

  // Wheel zoom needs a non-passive listener to keep the page from scrolling.
  useEffect(() => {
    const el = containerRef.current
    if (!el) return
    const onWheel = (e: WheelEvent) => {
      if (showMetadata || roiMode) return
      const f = fractionAt(e.clientX, e.clientY)
      if (!f || f.fx < 0 || f.fx > 1 || f.fy < 0 || f.fy > 1) return
      e.preventDefault()
      setZoom((z) => zoomAt(z, f.fx, f.fy, e.deltaY < 0 ? 1 / (1 + WHEEL_STEP) : 1 + WHEEL_STEP))
    }
    el.addEventListener('wheel', onWheel, { passive: false })
    return () => el.removeEventListener('wheel', onWheel)
  })

  const zoomed = isZoomed(zoom)
  useEffect(() => {
    if (zoomed && previewRef.current && videoRef.current) previewRef.current.srcObject = videoRef.current.srcObject
  }, [zoomed])

  const panMouse = {
    onMouseDown: (e: React.MouseEvent<HTMLDivElement>) => {
      if (!zoomed || (e.button !== 0 && e.button !== 1)) return
      e.preventDefault() // no tile drag-to-swap, no text selection
      panStart.current = { x: e.clientX, y: e.clientY }
    },
    onMouseMove: (e: React.MouseEvent<HTMLDivElement>) => {
      if (!panStart.current || !display) return
      const dx = (e.clientX - panStart.current.x) / display.width
      const dy = (e.clientY - panStart.current.y) / display.height
      panStart.current = { x: e.clientX, y: e.clientY }
      setZoom((z) => pan(z, dx, dy))
    },
    onMouseUp: () => { panStart.current = null },
  }

  // The ROI lives in sensor pixels; a decimation filter shrinks the displayed frame, so map
  // through the sensor's size when the server reports it.
  const roiFrame = metadata ? { w: metadata.hardware_width ?? metadata.width, h: metadata.hardware_height ?? metadata.height } : null

  const commitRoi = (drag: { x0: number; y0: number; x1: number; y1: number }) => {
    const t = tileSize()
    if (!t || !metadata || !roiFrame) return
    const a = pixelFromMouse(drag.x0, drag.y0, t.w, t.h, roiFrame.w, roiFrame.h, zoom)
    const b = pixelFromMouse(drag.x1, drag.y1, t.w, t.h, roiFrame.w, roiFrame.h, zoom)
    if (!a || !b || a.x === b.x || a.y === b.y) return
    apiClient.setRoi(deviceId, sensorId, { min_x: Math.min(a.x, b.x), min_y: Math.min(a.y, b.y), max_x: Math.max(a.x, b.x), max_y: Math.max(a.y, b.y) })
      .then(setRoi)
      .catch((error) => console.error('Failed to set ROI:', error))
  }

  // Reset is the legacy default: the centre 3/4 of the frame (stream-model.cpp). The firmware
  // refuses a full-frame region, so "everything" is not an option.
  const resetRoi = () => {
    if (!roiFrame) return
    const xm = Math.floor(roiFrame.w / 8)
    const ym = Math.floor(roiFrame.h / 8)
    apiClient.setRoi(deviceId, sensorId, { min_x: xm, min_y: ym, max_x: roiFrame.w - xm - 1, max_y: roiFrame.h - ym - 1 })
      .then(setRoi)
      .catch((error) => console.error('Failed to reset ROI:', error))
  }

  const roiMouse = {
    onMouseDown: (e: React.MouseEvent<HTMLDivElement>) => {
      e.preventDefault() // the tile wrapper is draggable (tile swap); a native drag would eat the mouseup
      const t = tileSize()
      if (!t) return
      const x = e.clientX - t.left, y = e.clientY - t.top
      setRoiDrag({ x0: x, y0: y, x1: x, y1: y })
    },
    onMouseMove: (e: React.MouseEvent<HTMLDivElement>) => {
      const t = tileSize()
      if (!t || !roiDrag) return
      setRoiDrag({ ...roiDrag, x1: e.clientX - t.left, y1: e.clientY - t.top })
    },
    onMouseUp: () => {
      if (roiDrag) commitRoi(roiDrag)
      setRoiDrag(null)
      setRoiMode(false) // like the legacy viewer, one rectangle per activation
    },
  }

  const roiBox = (() => {
    const t = tileSize()
    if (!roiMode || !t || !metadata || !roi?.supported || roi.min_x === undefined) return null
    const frame = roiFrame ?? { w: metadata.width, h: metadata.height }
    return boxFromPixels({ x: roi.min_x, y: roi.min_y! }, { x: roi.max_x!, y: roi.max_y! }, t.w, t.h, frame.w, frame.h, zoom)
  })()

  // Fetch dynamic depth range periodically for depth streams
  useEffect(() => {
    if (!isDepthStream) return
    let cancelled = false
    const fetchRange = async () => {
      try {
        const result = await apiClient.getDepthRange(deviceId)
        if (!cancelled) {
          setDepthRange({ min: result.min_depth, max: result.max_depth })
        }
      } catch (error) {
        // Ignore errors, keep previous range
      }
    }
    fetchRange()
    const interval = setInterval(fetchRange, 2000) // Update every 2 seconds
    return () => {
      cancelled = true
      clearInterval(interval)
    }
  }, [isDepthStream, deviceId])

  useEffect(() => {
    if (!isDepthStream) return
    let cancelled = false
    const poll = async () => {
      try {
        const r = await apiClient.getMaxUsableRange(deviceId)
        if (!cancelled) setMaxUsableRange(r.enabled ? r.range_m : null)
      } catch {
        if (!cancelled) setMaxUsableRange(null)
      }
    }
    void poll()
    const interval = setInterval(poll, 1000)
    return () => { cancelled = true; clearInterval(interval) }
  }, [isDepthStream, deviceId])

  // Calculate FPS from metadata updates
  useEffect(() => {
    if (metadata?.frame_number) {
      frameCount.current++
      const now = Date.now()
      if (now - lastFrameTime.current >= 1000) {
        setFps(+(frameCount.current * 1000 / (now - lastFrameTime.current)).toFixed(2))
        frameCount.current = 0
        lastFrameTime.current = now
      }
    }
  }, [metadata?.frame_number])

  const handleTrack = useCallback((event: RTCTrackEvent) => {
    if (videoRef.current && event.streams[0]) {
      videoRef.current.srcObject = event.streams[0]
    }
  }, [])

  const handleConnectionStateChange = useCallback((state: RTCPeerConnectionState) => {
    setConnectionState(state)
  }, [])

  // Throttle depth queries to avoid overloading the backend
  const lastQueryTime = useRef(0)
  const pendingQuery = useRef<{ x: number; y: number; mouseX: number; mouseY: number } | null>(null)
  const queryThrottleMs = 50 // Query at most every 50ms

  const handleMouseMove = useCallback(
    (e: React.MouseEvent<HTMLDivElement>) => {
      if (!isDepthStream || showMetadata || !containerRef.current || !metadata) return

      const rect = containerRef.current.getBoundingClientRect()
      const mouseX = e.clientX - rect.left
      const mouseY = e.clientY - rect.top

      // The <video> is letterboxed (object-contain); the bars map to no pixel.
      const pixel = pixelFromMouse(mouseX, mouseY, rect.width, rect.height, metadata.width, metadata.height, zoom)
      if (!pixel) {
        setHoverDepth(null)
        return
      }
      const { x, y } = pixel

      // Store pending query coords
      pendingQuery.current = { x, y, mouseX, mouseY }

      const now = Date.now()
      if (now - lastQueryTime.current < queryThrottleMs) {
        // Skip this event, a recent query is still fresh
        return
      }
      lastQueryTime.current = now

      const requestId = ++hoverRequestId.current
      apiClient.getDepthAtPixel(deviceId, x, y).then((result) => {
        if (requestId === hoverRequestId.current && pendingQuery.current) {
          setHoverDepth({
            x: pendingQuery.current.x,
            y: pendingQuery.current.y,
            depth: result.depth,
            mouseX: pendingQuery.current.mouseX,
            mouseY: pendingQuery.current.mouseY,
          })
        }
      }).catch((error) => {
        console.error('Error getting depth at pixel:', error)
      })
    },
    [isDepthStream, showMetadata, deviceId, metadata, zoom]
  )

  const handleMouseLeave = useCallback(() => {
    hoverRequestId.current++
    setHoverDepth(null)
  }, [])

  useEffect(() => {
    let mounted = true
    
    const startWebRTC = async () => {
      if (!deviceId) return
      
      // Clean up existing handler
      if (webrtcHandlerRef.current) {
        webrtcHandlerRef.current.disconnect()
        webrtcHandlerRef.current = null
      }
      
      const handler = new WebRTCHandler(
        deviceId,
        [streamType],
        handleTrack,
        handleConnectionStateChange
      )
      
      webrtcHandlerRef.current = handler
      
      try {
        await handler.connect()
      } catch (error) {
        if (mounted) {
          console.error('WebRTC connection failed:', error)
        }
      }
    }
    
    const stopWebRTC = () => {
      if (webrtcHandlerRef.current) {
        webrtcHandlerRef.current.disconnect()
        webrtcHandlerRef.current = null
      }
      if (videoRef.current) {
        videoRef.current.srcObject = null
      }
      setConnectionState(null)
    }

    if (deviceId) {
      startWebRTC()
    } else {
      stopWebRTC()
    }

    return () => {
      mounted = false
      stopWebRTC()
    }
  }, [deviceId, streamType, handleTrack, handleConnectionStateChange])

  const getStreamColor = (type: string) => {
    const colors: Record<string, string> = {
      depth: 'bg-blue-600',
      color: 'bg-green-600',
      infrared: 'bg-purple-600',
      fisheye: 'bg-yellow-600',
      gyro: 'bg-red-600',
      accel: 'bg-orange-600',
    }
    return colors[type.toLowerCase()] || 'bg-gray-600'
  }

  return (
    <div 
      ref={containerRef}
      className="relative h-full bg-black rounded-lg overflow-hidden"
      onMouseMove={(e) => {
        if (roiMode) roiMouse.onMouseMove(e)
        else { panMouse.onMouseMove(e); if (isDepthStream) handleMouseMove(e) }
      }}
      onMouseLeave={() => { panMouse.onMouseUp(); if (isDepthStream) handleMouseLeave() }}
      onMouseDown={roiMode ? roiMouse.onMouseDown : panMouse.onMouseDown}
      onMouseUp={roiMode ? roiMouse.onMouseUp : panMouse.onMouseUp}
      // Belt and braces: in ROI mode the press is a rectangle, never a tile drag
      onDragStart={roiMode ? (e) => e.preventDefault() : undefined}
      draggable={roiMode ? false : undefined}
      style={roiMode ? { cursor: 'crosshair' } : zoomed ? { cursor: 'grab' } : undefined}
    >
      {/* Video Element. The wrapper stays in normal flow (it gives the tile its height); when
          zoomed the video is scaled in place and the wrapper clips to the letterboxed frame area. */}
      <div
        className="relative w-full h-full overflow-hidden"
        style={display && zoomed
          ? { clipPath: `inset(${display.offsetY}px ${display.offsetX}px ${display.offsetY}px ${display.offsetX}px)` }
          : undefined}
      >
        <video
          ref={videoRef}
          autoPlay
          playsInline
          muted
          disablePictureInPicture
          controlsList="nodownload nofullscreen noremoteplayback"
          className="w-full h-full object-contain stream-video"
          style={display && zoomed
            ? { transform: zoomTransform(zoom, display.width, display.height, display.offsetX, display.offsetY), transformOrigin: '0 0' }
            : undefined}
        />
      </div>

      {/* Crosshair / grid overlay (stream-model.cpp draw_crosshair) */}
      {showGrid && display && (
        <svg className="absolute pointer-events-none" data-testid="grid-overlay"
          style={{ left: display.offsetX, top: display.offsetY, width: display.width, height: display.height }}
          viewBox={`0 0 ${display.width} ${display.height}`}>
          {Array.from({ length: gridPrefs?.grid_vertical_lines ?? 1 }, (_, i) => {
            const x = display.width * (i + 1) / ((gridPrefs?.grid_vertical_lines ?? 1) + 1)
            return <line key={`v${i}`} x1={x} y1={0} x2={x} y2={display.height} stroke={gridPrefs?.grid_line_color ?? '#ffffff'} strokeOpacity={0.7} strokeWidth={gridPrefs?.grid_line_width ?? 1} />
          })}
          {Array.from({ length: gridPrefs?.grid_horizontal_lines ?? 1 }, (_, i) => {
            const y = display.height * (i + 1) / ((gridPrefs?.grid_horizontal_lines ?? 1) + 1)
            return <line key={`h${i}`} x1={0} y1={y} x2={display.width} y2={y} stroke={gridPrefs?.grid_line_color ?? '#ffffff'} strokeOpacity={0.7} strokeWidth={gridPrefs?.grid_line_width ?? 1} />
          })}
        </svg>
      )}

      {/* Zoom preview thumbnail with the visible region (rendering.h show_preview) */}
      {zoomed && metadata && (
        <div className="absolute bottom-8 right-2 border border-black bg-black pointer-events-none" data-testid="zoom-preview"
          style={{ width: 141, height: Math.round(141 * metadata.height / metadata.width) }}>
          <video ref={previewRef} autoPlay playsInline muted disablePictureInPicture className="w-full h-full object-fill" />
          <div className="absolute border border-yellow-400"
            style={{ left: `${zoom.x * 100}%`, top: `${zoom.y * 100}%`, width: `${zoom.w * 100}%`, height: `${zoom.h * 100}%` }} />
        </div>
      )}

      {/* Device Name Header (shown for multi-camera) */}
      {showDeviceName && (
        <div className="absolute top-0 left-0 right-0 bg-gradient-to-b from-black/80 to-transparent px-2 py-1">
          <div className="text-xs text-white font-medium truncate">
            {deviceName} <span className="text-gray-400">({serialNumber})</span>
          </div>
        </div>
      )}

      {/* Stream label, and the only grip that rearranges tiles - except while a rectangle
          is being drawn, when nothing in the tile may start a drag. */}
      <div
        {...(roiMode ? {} : { [DRAG_HANDLE]: true })}
        title={roiMode ? 'Drag a rectangle over the image to set the ROI' : 'Drag to rearrange the tiles'}
        className={`absolute ${showDeviceName ? 'top-7' : 'top-2'} left-2 px-2 py-1 rounded text-xs font-semibold text-white select-none ${roiMode ? 'cursor-crosshair' : 'cursor-move'} ${getStreamColor(
          streamType
        )}`}
      >
        {streamType.toUpperCase()}
      </div>

      {/* Connection Status */}
      {connectionState && connectionState !== 'connected' && (
        <div className={`absolute ${showDeviceName ? 'top-7' : 'top-2'} right-2 px-2 py-1 bg-yellow-600 rounded text-xs text-white`}>
          {connectionState}
        </div>
      )}

      <div className={`absolute ${showDeviceName ? 'top-7' : 'top-2'} right-2 flex items-center gap-1`}
        // Clicks on the header buttons must not reach the tile's ROI / pan drag handlers: a
        // mouseup there ends ROI mode and unmounts the button before its click is delivered.
        onMouseDown={(e) => e.stopPropagation()} onMouseUp={(e) => e.stopPropagation()}>
        {/* Zoom, which the legacy viewer only offers on the wheel: the readout says the
            current factor and resets on click, so the feature is visible without guessing. */}
        <div className="flex items-center rounded border border-gray-600 bg-black/60 overflow-hidden z-20">
          <button type="button" aria-label="Zoom out" title="Zoom out (or scroll down over the image)"
            className="px-1.5 py-1 text-xs text-white hover:bg-black/80 disabled:opacity-40"
            disabled={!zoomed}
            onClick={() => setZoom((z) => zoomAt(z, 0.5, 0.5, 1 + WHEEL_STEP))}>−</button>
          <button type="button" aria-label="Reset zoom" title="Reset zoom to the whole frame"
            data-testid="zoom-level" className="px-1 py-1 text-xs text-white hover:bg-black/80 tabular-nums"
            onClick={() => setZoom(FULL_FRAME)}>{Math.round(100 / zoom.w)}%</button>
          <button type="button" aria-label="Zoom in" title="Zoom in (or scroll up over the image)"
            className="px-1.5 py-1 text-xs text-white hover:bg-black/80"
            onClick={() => setZoom((z) => zoomAt(z, 0.5, 0.5, 1 / (1 + WHEEL_STEP)))}>+</button>
        </div>
        {roi?.supported && (
          <button
            type="button"
            onClick={() => setRoiMode((m) => !m)}
            title={roiMode ? 'Now drag a rectangle over the image' : 'Set auto-exposure ROI: click, then drag a rectangle over the image'}
            aria-label="Set auto-exposure ROI"
            aria-pressed={roiMode}
            className={`px-2 py-1 rounded text-xs border border-gray-600 z-20 ${roiMode ? 'bg-yellow-600 text-black' : 'bg-black/60 hover:bg-black/80 text-white'}`}
          >
            ROI
          </button>
        )}
        {roiMode && (
          <button type="button" onClick={resetRoi} title="Reset ROI to the full frame" aria-label="Reset ROI"
            className="px-2 py-1 bg-black/60 hover:bg-black/80 rounded text-xs text-white border border-gray-600 z-20">
            ↺
          </button>
        )}
        <button
          type="button"
          onClick={() => setShowGrid((g) => !g)}
          title="Show crosshair/grid overlay"
          aria-label="Show crosshair/grid overlay"
          aria-pressed={showGrid}
          className={`px-2 py-1 rounded text-xs border border-gray-600 z-20 ${showGrid ? 'bg-rs-blue text-white' : 'bg-black/60 hover:bg-black/80 text-white'}`}
        >
          #
        </button>
        <SnapshotButton deviceId={deviceId} streamType={streamType} className="py-1" />
        {pause && <PauseButton pause={pause} className="py-1" />}
        {maximize && <MaximizeButton maximize={maximize} className="py-1" />}
        <MetadataPanel
          metadata={metadata}
          streamType={streamType}
          deviceName={deviceName}
          fps={fps}
          show={showMetadata}
          onToggle={setShowMetadata}
          buttonClassName="py-1"
        />
      </div>

      {roiBox && (
        <div className="absolute border-2 border-yellow-400 pointer-events-none" data-testid="roi-rect"
          style={{ left: roiBox.left, top: roiBox.top, width: roiBox.width, height: roiBox.height }} />
      )}
      {roiDrag && (
        <div className="absolute border-2 border-dashed border-white pointer-events-none"
          style={{ left: Math.min(roiDrag.x0, roiDrag.x1), top: Math.min(roiDrag.y0, roiDrag.y1),
            width: Math.abs(roiDrag.x1 - roiDrag.x0), height: Math.abs(roiDrag.y1 - roiDrag.y0) }} />
      )}

      {recording && (
        <div className="absolute bottom-2 left-1/2 -translate-x-1/2 px-2 py-0.5 bg-black/70 rounded text-red-500 text-xs font-bold animate-pulse pointer-events-none">
          ● REC
        </div>
      )}

      {/* Stream state overlays, as the legacy viewer draws over a tile */}
      {pause?.paused && (
        <div className="absolute inset-0 flex items-center justify-center pointer-events-none">
          <span className="px-3 py-1 bg-black/70 rounded text-white text-sm font-semibold animate-pulse">❚❚ Paused</span>
        </div>
      )}
      {unrenderable && (
        <div className="absolute inset-0 flex items-center justify-center pointer-events-none">
          <span className="px-3 py-1 bg-black/70 rounded text-yellow-300 text-sm">Rendering not supported for {format?.toUpperCase()}</span>
        </div>
      )}
      {stalled && !unrenderable && (
        <div className="absolute inset-0 flex items-center justify-center pointer-events-none">
          <span className="px-3 py-1 bg-black/70 rounded text-red-300 text-sm font-semibold animate-pulse">No frames received!</span>
        </div>
      )}

      {/* Depth Legend (for depth streams) */}
      {isDepthStream && (
        <div className="absolute top-12 right-2 bottom-12 w-16">
          <DepthLegend minDepth={depthRange.min} maxDepth={depthRange.max} colorScheme="jet" show={true} />
        </div>
      )}

      {/* Depth pixel info (fixed bottom-left, hidden in metadata view) */}
      {isDepthStream && hoverDepth && !showMetadata && (
        <div className="absolute bottom-2 left-2 bg-black/80 text-white text-xs px-2 py-1 rounded shadow pointer-events-none font-mono">
          <div>
            <span className="text-gray-400">Pixel:</span> ({hoverDepth.x}, {hoverDepth.y})
          </div>
          <div className="font-bold">
            <span className="text-gray-400">Depth:</span>{' '}
            {hoverDepth.depth !== null ? formatDistance(hoverDepth.depth, metric) : 'N/A'}
          </div>
        </div>
      )}
      {isDepthStream && maxUsableRange !== null && !showMetadata && (
        <div className="absolute bottom-2 right-2 bg-black/80 text-white text-xs px-2 py-1 rounded shadow pointer-events-none font-mono">
          Max usable range: {formatDistance(maxUsableRange, metric)}
        </div>
      )}
    </div>
  )
}

// IMU Stream Tile - specialized visualization for gyro/accel streams
interface IMUStreamTileProps {
  deviceId: string
  streamType: string
  showDeviceName?: boolean
  deviceName: string
  serialNumber: string
  metadata?: StreamMetadata
  pause?: PauseState
  maximize?: MaximizeState
}

function IMUStreamTile({ deviceId, streamType, showDeviceName, deviceName, serialNumber, metadata, pause, maximize }: IMUStreamTileProps) {
  const { imuHistory } = useAppStore()
  const [fps, setFps] = useState(0)
  const [showMetadata, setShowMetadata] = useState(false)
  const lastFrameTime = useRef(0)
  const frameCount = useRef(0)

  useEffect(() => {
    if (metadata?.frame_number !== undefined) {
      frameCount.current++
      const now = Date.now()
      if (now - lastFrameTime.current >= 1000) {
        setFps(+(frameCount.current * 1000 / (now - lastFrameTime.current)).toFixed(2))
        frameCount.current = 0
        lastFrameTime.current = now
      }
    }
  }, [metadata?.frame_number])
  
  const isGyro = streamType.toLowerCase() === 'gyro'
  const isAccel = streamType.toLowerCase() === 'accel'
  
  const data = isGyro ? imuHistory.gyro : isAccel ? imuHistory.accel : []
  // The history is filled from the metadata broadcast; the frame's own sample is the
  // fallback so a tile shows numbers from the first frame, not "waiting".
  const latest = data[data.length - 1] ?? metadata?.motion_data
  
  // Calculate magnitude
  const magnitude = latest 
    ? Math.sqrt(latest.x ** 2 + latest.y ** 2 + latest.z ** 2)
    : null
  
  const getStreamColor = () => {
    if (isGyro) return { bg: 'bg-red-900/50', border: 'border-red-500', text: 'text-red-400' }
    if (isAccel) return { bg: 'bg-orange-900/50', border: 'border-orange-500', text: 'text-orange-400' }
    return { bg: 'bg-gray-900/50', border: 'border-gray-500', text: 'text-gray-400' }
  }
  
  const colors = getStreamColor()
  const unit = isGyro ? 'rad/s' : 'm/s²'
  
  // Calculate bar widths based on value (normalized to max expected range)
  const maxRange = isGyro ? 10 : 20  // rad/s for gyro, m/s² for accel
  const getBarWidth = (value: number) => {
    const normalized = Math.min(Math.abs(value) / maxRange, 1) * 100
    return `${normalized}%`
  }
  
  return (
    <div className={`relative h-full rounded-lg overflow-hidden ${colors.bg} border ${colors.border} flex flex-col`}>
      {/* Header */}
      <div className="flex items-center justify-between px-3 py-2 bg-black/30 shrink-0">
        <div className="flex items-center gap-2">
          <span {...{ [DRAG_HANDLE]: true }} title="Drag to rearrange the tiles"
            className={`font-semibold cursor-move select-none ${colors.text}`}>
            {streamType.toUpperCase()}
          </span>
          <span className="w-2 h-2 bg-green-500 rounded-full animate-pulse" />
        </div>
        <div className="flex items-center gap-2">
          <span className="text-xs text-gray-400">{unit}</span>
          <SnapshotButton deviceId={deviceId} streamType={streamType} />
          {pause && <PauseButton pause={pause} />}
          {maximize && <MaximizeButton maximize={maximize} />}
          <MetadataPanel
            metadata={metadata}
            streamType={streamType}
            deviceName={deviceName}
            fps={fps}
            show={showMetadata}
            onToggle={setShowMetadata}
          />
        </div>
      </div>
      
      {showDeviceName && (
        <div className="px-3 py-1 text-xs text-gray-400 bg-black/20">
          {deviceName} ({serialNumber})
        </div>
      )}
      
      {/* Content */}
      <div className="flex-1 min-h-0 overflow-auto flex flex-col justify-center p-4">
        {!latest ? (
          <div className="text-center text-gray-500">
            <p>Waiting for data…</p>
            <p className="text-xs mt-1">The {unit} samples arrive with the frame metadata.</p>
          </div>
        ) : (
          <>
            {/* X/Y/Z Values with visual bars */}
            <div className="space-y-3">
              {/* X */}
              <div className="flex items-center gap-3">
                <span className="text-red-400 font-bold w-4">X</span>
                <div className="flex-1 h-4 bg-gray-800 rounded overflow-hidden relative">
                  <div 
                    className="absolute top-0 h-full bg-red-500/70 transition-all duration-75"
                    style={{ 
                      width: getBarWidth(latest.x),
                      left: latest.x >= 0 ? '50%' : `calc(50% - ${getBarWidth(latest.x)})`,
                    }}
                  />
                  <div className="absolute inset-0 flex items-center justify-center">
                    <span className="text-xs font-mono text-white drop-shadow">
                      {latest.x.toFixed(3)}
                    </span>
                  </div>
                </div>
              </div>
              
              {/* Y */}
              <div className="flex items-center gap-3">
                <span className="text-green-400 font-bold w-4">Y</span>
                <div className="flex-1 h-4 bg-gray-800 rounded overflow-hidden relative">
                  <div 
                    className="absolute top-0 h-full bg-green-500/70 transition-all duration-75"
                    style={{ 
                      width: getBarWidth(latest.y),
                      left: latest.y >= 0 ? '50%' : `calc(50% - ${getBarWidth(latest.y)})`,
                    }}
                  />
                  <div className="absolute inset-0 flex items-center justify-center">
                    <span className="text-xs font-mono text-white drop-shadow">
                      {latest.y.toFixed(3)}
                    </span>
                  </div>
                </div>
              </div>
              
              {/* Z */}
              <div className="flex items-center gap-3">
                <span className="text-blue-400 font-bold w-4">Z</span>
                <div className="flex-1 h-4 bg-gray-800 rounded overflow-hidden relative">
                  <div 
                    className="absolute top-0 h-full bg-blue-500/70 transition-all duration-75"
                    style={{ 
                      width: getBarWidth(latest.z),
                      left: latest.z >= 0 ? '50%' : `calc(50% - ${getBarWidth(latest.z)})`,
                    }}
                  />
                  <div className="absolute inset-0 flex items-center justify-center">
                    <span className="text-xs font-mono text-white drop-shadow">
                      {latest.z.toFixed(3)}
                    </span>
                  </div>
                </div>
              </div>
            </div>
            
            {/* Magnitude */}
            {magnitude !== null && (
              <div className="mt-4 pt-3 border-t border-gray-700 flex items-center justify-between">
                <span className="text-purple-400 font-semibold">‖{isGyro ? 'ω' : 'a'}‖</span>
                <span className="font-mono font-bold text-lg">
                  {magnitude.toFixed(3)}
                  <span className="text-xs text-gray-400 ml-1">{unit}</span>
                </span>
                {isAccel && Math.abs(magnitude - 9.81) < 0.5 && (
                  <span className="text-xs text-green-400">(≈1g)</span>
                )}
              </div>
            )}
            
            {/* Sample count */}
            <div className="mt-2 text-xs text-gray-500 text-center">
              {data.length} samples
            </div>
          </>
        )}
      </div>
    </div>
  )
}

interface MetadataOverlayProps {
  streamType: string
  metadata: StreamMetadata
  fps: number
  deviceName?: string
}

export function MetadataOverlay({ streamType, metadata, fps, deviceName }: MetadataOverlayProps) {
  const frameMd = metadata.frame_metadata ?? {}
  const depthMapping = isDepthMappingDevice(deviceName)
  const isMotion = ['gyro', 'accel', 'motion'].includes(streamType.toLowerCase())
  // Mirrors C++ viewer (common/stream-model.cpp): when SDK falls back to system_time,
  // per-frame metadata is unavailable from the kernel UVC driver.
  const metadataUnavailable = metadata.clock_domain === 'system_time'
  return (
    <div className="absolute inset-0 overflow-y-auto bg-black/60 text-white text-xs z-10">
      <div className="sticky top-0 px-3 py-2 bg-gray-800 font-semibold border-b border-gray-700">
        Frame Metadata — {streamType.toUpperCase()}
      </div>
      <div className="px-3 py-2 border-b border-gray-700 bg-gray-900/60">
        <div className="text-gray-400 uppercase tracking-wide text-[10px] mb-1">Viewer Info</div>
        <div className="grid grid-cols-2 gap-x-4 gap-y-0.5 font-mono">
          <MetadataItem label="Frame Timestamp" value={metadata.timestamp} />
          <MetadataItem label="Clock Domain" value={metadata.clock_domain} />
          <MetadataItem label="Frame Number" value={metadata.frame_number} />
          <MetadataItem label="Pixel Format" value={metadata.pixel_format} />
          <MetadataItem label="Hardware Size" value={!isMotion ? resolutionFrom(metadata.hardware_width, metadata.hardware_height) : undefined} />
          <MetadataItem label="Display Size" value={!isMotion ? resolutionFrom(metadata.width, metadata.height) : undefined} />
          <MetadataItem label="Hardware FPS" value={metadata.hardware_fps} />
          <MetadataItem label="Viewer FPS" value={fps} />
        </div>
      </div>
      {metadataUnavailable && (
        <div
          role="alert"
          className="px-3 py-2 border-b border-red-900/60 bg-red-950/40 text-red-300 text-xs leading-tight"
        >
          <div>Per-frame metadata is not enabled at the OS level!</div>
          <div>Please follow the installation guide for the details.</div>
        </div>
      )}
      <div className="grid grid-cols-2 gap-x-4 gap-y-0.5 p-3 font-mono">
        {Object.entries(frameMd).map(([k, v]) => (
          <MetadataItem key={k} label={metadataLabel(k, depthMapping)} value={formatMetadataValue(k, v)} title={describeMetadata(k, v)} />
        ))}
      </div>
    </div>
  )
}

function resolutionFrom(w: number | undefined, h: number | undefined): string | undefined {
  return w !== undefined && h !== undefined ? `${w}×${h}` : undefined
}

interface MetadataPanelProps {
  metadata?: StreamMetadata
  streamType: string
  deviceName?: string
  fps: number
  show: boolean
  onToggle: (show: boolean) => void
  buttonClassName?: string
}

export function MetadataPanel({ metadata, streamType, deviceName, fps, show, onToggle, buttonClassName = '' }: MetadataPanelProps) {
  const hasMetadata = !!metadata && (
    metadata.frame_number !== undefined ||
    metadata.timestamp !== undefined ||
    Object.keys(metadata.frame_metadata ?? {}).length > 0
  )
  // The button stays on the tile even before a frame carries metadata - it disappearing is
  // indistinguishable from the viewer not having the feature at all.
  return (
    <>
      <button
        type="button"
        onClick={() => hasMetadata && onToggle(!show)}
        disabled={!hasMetadata}
        title={!hasMetadata ? 'No frame metadata yet' : show ? 'Hide frame metadata' : 'Show frame metadata'}
        className={`px-2 py-0.5 bg-black/60 rounded text-xs text-white border border-gray-600 z-20 ${hasMetadata ? 'hover:bg-black/80' : 'opacity-40 cursor-not-allowed'} ${buttonClassName}`}
      >
        {show && hasMetadata ? '✕' : 'Metadata'}
      </button>
      {show && hasMetadata && <MetadataOverlay streamType={streamType} metadata={metadata!} fps={fps} deviceName={deviceName} />}
    </>
  )
}

export { lessScreamy }

export function MetadataItem({ label, value, title }: { label: string; value: ReactNode; title?: string }) {
  if (value === undefined || value === null) return null
  return (
    <div className="flex justify-between border-b border-gray-800/50 py-0.5" title={title}>
      <span className={`text-gray-300 truncate pr-2 ${title ? 'underline decoration-dotted' : ''}`}>{label}</span>
      <span className="text-right shrink-0">{value}</span>
    </div>
  )
}
