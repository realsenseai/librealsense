import { useEffect, useRef, useState, useCallback, useMemo, type ReactNode } from 'react'
import { useAppStore } from '../store'
import { WebRTCHandler } from '../api/webrtc'
import { apiClient } from '../api/client'
import { DepthLegend } from './DepthLegend'
import type { DeviceState, StreamConfig, StreamMetadata } from '../api/types'

// A stream with its device context
interface DeviceStream {
  deviceId: string
  deviceName: string
  serialNumber: string
  config: StreamConfig
  metadata?: StreamMetadata
}

export function StreamViewer() {
  const { deviceStates } = useAppStore()
  
  // Collect all enabled streams from all active devices; hide tiles until they actually stream.
  const activeStreams = useMemo(() => {
    const streams: DeviceStream[] = []
    
    Object.values(deviceStates).forEach((ds: DeviceState) => {
      if (!ds.isActive) return
      
      ds.streamConfigs.filter(c => c.enable).forEach(config => {
        // Determine if this specific stream is actually streaming
        let streamIsActive = false
        if (ds.streamingMode === 'pipeline') {
          // Pipeline mode: all enabled streams are active when isStreaming=true
          streamIsActive = ds.isStreaming
        } else if (ds.streamingMode === 'sensor') {
          // Sensor mode: check if this specific stream type is running on its sensor
          const sensorStatus = ds.sensorStreamingStatus?.[config.sensor_id]
          // Check if stream type is in the list of active streams (new) or matches single stream (backward compat)
          const activeTypes = sensorStatus?.stream_types || (sensorStatus?.stream_type ? [sensorStatus.stream_type] : [])
          streamIsActive = sensorStatus?.is_streaming === true && 
                          activeTypes.some(st => st.toLowerCase() === config.stream_type.toLowerCase())
        }
        
        if (!streamIsActive) return

        streams.push({
          deviceId: ds.device.device_id,
          deviceName: ds.device.name,
          serialNumber: ds.device.serial_number,
          config,
          metadata: ds.streamMetadata[config.stream_type],
        })
      })
    })
    
    return streams
  }, [deviceStates])

  const activeDeviceCount = Object.values(deviceStates).filter(ds => ds.isActive).length

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
            gridTemplateColumns: `repeat(${Math.min(activeStreams.length, 2)}, 1fr)`,
            gridTemplateRows: `repeat(${Math.ceil(activeStreams.length / 2)}, 1fr)`,
          }}
        >
          {activeStreams.map((stream) => {
            const isMotionStream = ['gyro', 'accel'].includes(stream.config.stream_type.toLowerCase())
            
            if (isMotionStream) {
              return (
                <IMUStreamTile
                  key={`${stream.deviceId}-${stream.config.sensor_id}-${stream.config.stream_type}`}
                  streamType={stream.config.stream_type}
                  showDeviceName={activeDeviceCount > 1}
                  deviceName={stream.deviceName}
                  serialNumber={stream.serialNumber}
                  metadata={stream.metadata}
                />
              )
            }
            
            return (
              <StreamTile
                key={`${stream.deviceId}-${stream.config.sensor_id}-${stream.config.stream_type}`}
                deviceId={stream.deviceId}
                deviceName={stream.deviceName}
                serialNumber={stream.serialNumber}
                streamType={stream.config.stream_type}
                metadata={stream.metadata}
                showDeviceName={activeDeviceCount > 1}
              />
            )
          })}
        </div>
      )}
    </div>
  )
}

interface StreamTileProps {
  deviceId: string
  deviceName: string
  serialNumber: string
  streamType: string
  showDeviceName?: boolean
  metadata?: StreamMetadata
}

function StreamTile({ deviceId, deviceName, serialNumber, streamType, showDeviceName, metadata }: StreamTileProps) {
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

  const isDepthStream = streamType.toLowerCase() === 'depth'

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

      // The <video> uses object-contain so it is letterboxed within the tile.
      // Compute the actual displayed video rect and ignore the bars.
      const scale = Math.min(rect.width / metadata.width, rect.height / metadata.height)
      const displayW = metadata.width * scale
      const displayH = metadata.height * scale
      const offsetX = (rect.width - displayW) / 2
      const offsetY = (rect.height - displayH) / 2

      if (mouseX < offsetX || mouseX >= offsetX + displayW ||
          mouseY < offsetY || mouseY >= offsetY + displayH) {
        setHoverDepth(null)
        return
      }

      const x = Math.floor((mouseX - offsetX) / displayW * metadata.width)
      const y = Math.floor((mouseY - offsetY) / displayH * metadata.height)

      if (x < 0 || x >= metadata.width || y < 0 || y >= metadata.height) {
        setHoverDepth(null)
        return
      }

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
    [isDepthStream, showMetadata, deviceId, metadata]
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
      className="relative bg-black rounded-lg overflow-hidden"
      onMouseMove={isDepthStream ? handleMouseMove : undefined}
      onMouseLeave={isDepthStream ? handleMouseLeave : undefined}
    >
      {/* Video Element */}
      <video
        ref={videoRef}
        autoPlay
        playsInline
        muted
        disablePictureInPicture
        controlsList="nodownload nofullscreen noremoteplayback"
        className="w-full h-full object-contain stream-video"
      />

      {/* Device Name Header (shown for multi-camera) */}
      {showDeviceName && (
        <div className="absolute top-0 left-0 right-0 bg-gradient-to-b from-black/80 to-transparent px-2 py-1">
          <div className="text-xs text-white font-medium truncate">
            {deviceName} <span className="text-gray-400">({serialNumber})</span>
          </div>
        </div>
      )}

      {/* Stream Label */}
      <div
        className={`absolute ${showDeviceName ? 'top-7' : 'top-2'} left-2 px-2 py-1 rounded text-xs font-semibold text-white ${getStreamColor(
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

      <MetadataPanel
        metadata={metadata}
        streamType={streamType}
        fps={fps}
        show={showMetadata}
        onToggle={setShowMetadata}
        buttonClassName={`absolute ${showDeviceName ? 'top-7' : 'top-2'} right-2 py-1`}
      />

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
            {hoverDepth.depth !== null ? `${hoverDepth.depth.toFixed(3)} m` : 'N/A'}
          </div>
        </div>
      )}
    </div>
  )
}

// IMU Stream Tile - specialized visualization for gyro/accel streams
interface IMUStreamTileProps {
  streamType: string
  showDeviceName?: boolean
  deviceName: string
  serialNumber: string
  metadata?: StreamMetadata
}

function IMUStreamTile({ streamType, showDeviceName, deviceName, serialNumber, metadata }: IMUStreamTileProps) {
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
  const latest = data[data.length - 1]
  
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
    <div className={`relative rounded-lg overflow-hidden ${colors.bg} border ${colors.border} flex flex-col`}>
      {/* Header */}
      <div className="flex items-center justify-between px-3 py-2 bg-black/30">
        <div className="flex items-center gap-2">
          <span className={`font-semibold ${colors.text}`}>
            {streamType.toUpperCase()}
          </span>
          <span className="w-2 h-2 bg-green-500 rounded-full animate-pulse" />
        </div>
        <div className="flex items-center gap-2">
          <span className="text-xs text-gray-400">{unit}</span>
          <MetadataPanel
            metadata={metadata}
            streamType={streamType}
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
      <div className="flex-1 flex flex-col justify-center p-4">
        {!latest ? (
          <div className="text-center text-gray-500">
            <p>Waiting for data...</p>
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
}

export function MetadataOverlay({ streamType, metadata, fps }: MetadataOverlayProps) {
  const frameMd = metadata.frame_metadata ?? {}
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
          <MetadataItem key={k} label={lessScreamy(k)} value={v} />
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
  fps: number
  show: boolean
  onToggle: (show: boolean) => void
  buttonClassName?: string
}

export function MetadataPanel({ metadata, streamType, fps, show, onToggle, buttonClassName = '' }: MetadataPanelProps) {
  const hasMetadata = !!metadata && (
    metadata.frame_number !== undefined ||
    metadata.timestamp !== undefined ||
    Object.keys(metadata.frame_metadata ?? {}).length > 0
  )
  if (!hasMetadata) return null
  return (
    <>
      <button
        type="button"
        onClick={() => onToggle(!show)}
        title={show ? 'Hide frame metadata' : 'Show frame metadata'}
        className={`px-2 py-0.5 bg-black/60 hover:bg-black/80 rounded text-xs text-white border border-gray-600 z-20 ${buttonClassName}`}
      >
        {show ? '✕' : 'Metadata'}
      </button>
      {show && <MetadataOverlay streamType={streamType} metadata={metadata!} fps={fps} />}
    </>
  )
}

// Mirrors the SDK's rsutils::string::make_less_screamy: "ACTUAL_FPS" -> "Actual Fps".
export function lessScreamy(key: string): string {
  return key.split('_').map(w => w.charAt(0).toUpperCase() + w.slice(1).toLowerCase()).join(' ')
}

export function MetadataItem({ label, value }: { label: string; value: ReactNode }) {
  if (value === undefined || value === null) return null
  return (
    <div className="flex justify-between border-b border-gray-800/50 py-0.5">
      <span className="text-gray-300 truncate pr-2">{label}</span>
      <span className="text-right shrink-0">{value}</span>
    </div>
  )
}
