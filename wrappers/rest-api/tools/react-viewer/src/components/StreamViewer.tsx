import { useEffect, useRef, useState, useCallback, useMemo, type ReactNode } from 'react'
import { useAppStore } from '../store'
import type { Frame, FrameHeader } from '../api/frames'
import { useFrameStream } from '../api/useFrameStream'
import { DepthLegend } from './DepthLegend'
import { rulerGradient, sampleDistances, updateRuler } from '../utils/depthRuler'
import { appendIMUPoint, type IMUChartPoint } from '../utils/imuChart'
import { FrameRenderer } from '../utils/frameRenderer'
import { streamKey, type DeviceState, type StreamConfig, type SupportedStreamProfile } from '../api/types'

import IMUOrientation from './IMUOrientation'
import IMUChart from './IMUChart'

// A stream with its device context
interface DeviceStream {
  deviceId: string
  deviceName: string
  serialNumber: string
  config: StreamConfig
  profile: SupportedStreamProfile // the SDK profile the stream runs with
}

export function StreamViewer() {
  const { deviceStates } = useAppStore()
  
  // Collect all enabled streams from all active devices; hide tiles until they actually stream.
  const activeStreams = useMemo(() => {
    const streams: DeviceStream[] = []
    
    Object.values(deviceStates).forEach((ds: DeviceState) => {
      if (!ds.isActive) return
      
      ds.streamConfigs.filter(c => c.enable).forEach(config => {
        // The profile this stream runs with on its sensor, if it runs
        const sensorStatus = ds.sensorStreamingStatus?.[config.sensor_id]
        const profile = sensorStatus?.is_streaming ? sensorStatus.profiles?.find(p => streamKey(p) === config.stream_type) : undefined
        if (!profile) return

        streams.push({
          deviceId: ds.device.device_id,
          deviceName: ds.device.name,
          serialNumber: ds.device.serial_number,
          config,
          profile,
        })
      })
    })
    
    return streams
  }, [deviceStates])

  const activeDeviceCount = Object.values(deviceStates).filter(ds => ds.isActive).length

  return (
    <div className="h-full">
      {activeStreams.length === 0 ? (
        <div className="h-full flex items-center justify-center text-rs-dim">
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
                  deviceId={stream.deviceId}
                  streamType={stream.config.stream_type}
                  profile={stream.profile}
                  showDeviceName={activeDeviceCount > 1}
                  deviceName={stream.deviceName}
                  serialNumber={stream.serialNumber}
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
                profile={stream.profile}
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
  profile: SupportedStreamProfile
  showDeviceName?: boolean
}

function StreamTile({ deviceId, deviceName, serialNumber, streamType, profile, showDeviceName }: StreamTileProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const containerRef = useRef<HTMLDivElement>(null)
  const renderer = useRef<FrameRenderer | null>(null)
  const latestFrame = useRef<Frame | null>(null) // the frame on screen, for hover depth
  const [unsupportedFormat, setUnsupportedFormat] = useState<string | null>(null)
  const [showMetadata, setShowMetadata] = useState(false)
  useEffect(() => () => renderer.current?.dispose(), [])
  const [hoverDepth, setHoverDepth] = useState<{
    x: number
    y: number
    depth: number | null
    mouseX: number
    mouseY: number
  } | null>(null)
  // The depth ruler, from the frames: a Z16 frame comes with the color of every depth value
  const rulerTaken = useRef(0)
  const [legend, setLegend] = useState<{ min: number; max: number; gradient: string } | null>(null)

  const isDepthStream = streamType.toLowerCase() === 'depth'

  const { state: connectionState, fps, header } = useFrameStream(deviceId, profile, showMetadata, (frame) => {
    renderer.current ??= new FrameRenderer(canvasRef.current!)
    setUnsupportedFormat(renderer.current.draw(frame) ? null : frame.header.format)
    latestFrame.current = frame
    const { width = 0, height = 0, stride = 0, bpp = 1, units } = frame.header
    const now = performance.now()
    if (frame.colorTable && units && now - rulerTaken.current >= 100) { // like legacy: a scale needs no more
      rulerTaken.current = now
      const { colorTable } = frame
      const samples = sampleDistances(new Uint16Array(frame.pixels), width, height, stride / bpp, units)
      setLegend(last => { // the bounds may hold, the colors follow each frame
        const r = updateRuler(last, samples)
        return r && { ...r, gradient: rulerGradient(colorTable, units, r.min, r.max) }
      })
    }
  })

  // Read the hovered pixel's depth from the frame on screen
  const handleMouseMove = useCallback(
    (e: React.MouseEvent<HTMLDivElement>) => {
      const frame = latestFrame.current
      if (!isDepthStream || showMetadata || !containerRef.current || !frame) return
      const { width = 0, height = 0, stride = 0, bpp = 1, units } = frame.header

      const rect = containerRef.current.getBoundingClientRect()
      const mouseX = e.clientX - rect.left
      const mouseY = e.clientY - rect.top

      // The canvas uses object-contain so it is letterboxed within the tile.
      // Compute the actual displayed frame rect and ignore the bars.
      const scale = Math.min(rect.width / width, rect.height / height)
      const displayW = width * scale
      const displayH = height * scale
      const offsetX = (rect.width - displayW) / 2
      const offsetY = (rect.height - displayH) / 2
      const x = Math.floor((mouseX - offsetX) / displayW * width)
      const y = Math.floor((mouseY - offsetY) / displayH * height)
      if (x < 0 || x >= width || y < 0 || y >= height) {
        setHoverDepth(null)
        return
      }
      const value = new Uint16Array(frame.pixels)[y * (stride / bpp) + x]
      setHoverDepth({ x, y, depth: value ? value * units! : null, mouseX, mouseY })
    },
    [isDepthStream, showMetadata]
  )

  const handleMouseLeave = useCallback(() => setHoverDepth(null), [])

  return (
    <div 
      ref={containerRef}
      className="relative bg-black rounded-lg overflow-hidden"
      onMouseMove={isDepthStream ? handleMouseMove : undefined}
      onMouseLeave={isDepthStream ? handleMouseLeave : undefined}
    >
      <canvas ref={canvasRef} className="w-full h-full object-contain stream-video" />

      {unsupportedFormat && (
        <div className="absolute inset-0 flex items-center justify-center text-rs-dim text-sm">
          {unsupportedFormat.toUpperCase()} frames can't be drawn yet
        </div>
      )}

      {/* Device Name Header (shown for multi-camera) */}
      {showDeviceName && (
        <div className="absolute top-0 left-0 right-0 bg-gradient-to-b from-black/80 to-transparent px-2 py-1">
          <div className="text-xs text-white font-medium truncate">
            {deviceName} <span className="text-white/60 nums">({serialNumber})</span>
          </div>
        </div>
      )}

      {/* Stream Label */}
      <div
        className={`absolute ${showDeviceName ? 'top-7' : 'top-2'} left-2 px-2 py-0.5 rounded-md
                    bg-black/55 backdrop-blur-sm text-[11px] font-semibold text-white/90`}
      >
        {streamType.toUpperCase()}
      </div>

      {/* Connection Status */}
      {connectionState && connectionState !== 'open' && (
        <div className={`absolute ${showDeviceName ? 'top-7' : 'top-2'} right-2 px-2 py-0.5 rounded-md border border-rs-warn/40 bg-rs-warn/15 backdrop-blur-sm text-[11px] text-rs-warn`}>
          {connectionState}
        </div>
      )}

      <MetadataPanel
        header={header}
        profile={profile}
        streamType={streamType}
        fps={fps}
        show={showMetadata}
        onToggle={setShowMetadata}
        buttonClassName={`absolute ${showDeviceName ? 'top-7' : 'top-2'} right-2 py-1`}
      />

      {/* Depth Legend (for depth streams) */}
      {legend && (
        <div className="absolute top-12 right-2 bottom-12 w-16">
          <DepthLegend minDepth={legend.min} maxDepth={legend.max} gradient={legend.gradient} />
        </div>
      )}

      {/* Depth pixel info (fixed bottom-left, hidden in metadata view) */}
      {isDepthStream && hoverDepth && !showMetadata && (
        <div className="absolute bottom-2 left-2 bg-black/80 text-white text-xs px-2 py-1 rounded shadow pointer-events-none font-mono">
          <div>
            <span className="text-rs-muted">Pixel:</span> ({hoverDepth.x}, {hoverDepth.y})
          </div>
          <div className="font-bold">
            <span className="text-rs-muted">Depth:</span>{' '}
            {hoverDepth.depth !== null ? `${hoverDepth.depth.toFixed(3)} m` : 'N/A'}
          </div>
        </div>
      )}
    </div>
  )
}

// IMU Stream Tile - specialized visualization for gyro/accel streams
interface IMUStreamTileProps {
  deviceId: string
  streamType: string
  profile: SupportedStreamProfile
  showDeviceName?: boolean
  deviceName: string
  serialNumber: string
}

function IMUStreamTile({ deviceId, streamType, profile, showDeviceName, deviceName, serialNumber }: IMUStreamTileProps) {
  // The tile unmounts when its stream stops, so a restart starts a fresh window.
  const [points, setPoints] = useState<IMUChartPoint[]>([])
  const [showGraph, setShowGraph] = useState(false)
  const [showMetadata, setShowMetadata] = useState(false)
  const { fps, header } = useFrameStream(deviceId, profile, showMetadata, (frame) => {
    const [x, y, z] = new Float32Array(frame.pixels) // a motion frame's data starts with float32 x, y, z
    setPoints((prev) => appendIMUPoint(prev, { x, y, z }, Date.now()))
  })
  
  const isGyro = streamType.toLowerCase() === 'gyro'
  const latest = points[points.length - 1]

  const unit = isGyro ? 'rad/s' : 'm/s²'
  // Resting noise floor per stream: gyro drift is milli-rad/s, accel carries 1g.
  const axisFloor = isGyro ? 0.5 : 10

  return (
    <div className="relative rounded-lg overflow-hidden bg-rs-dark border border-rs-border flex flex-col">
      {/* Header */}
      <div className="flex items-center justify-between px-3 py-2 bg-rs-inset/70 border-b border-rs-border">
        <div className="flex items-center gap-2">
          <span className="font-semibold text-[11px] text-rs-text">
            {streamType.toUpperCase()}
          </span>
          <span className="w-1.5 h-1.5 bg-rs-ok rounded-full animate-pulse" />
        </div>
        <div className="flex items-center gap-2">
          {/* Numeric readout / graph switch, as in the C++ viewer's motion tiles. */}
          <button
            onClick={() => setShowGraph((v) => !v)}
            aria-pressed={showGraph}
            title={showGraph ? 'Close graph view' : 'Open graph view'}
            className={`transition-colors ${showGraph ? 'text-rs-accent' : 'text-rs-dim hover:text-rs-text'}`}
          >
            <span className="sr-only">{showGraph ? 'Close graph view' : 'Open graph view'}</span>
            <svg className="w-3.5 h-3.5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M3 3v18h18M7 15v3m5-9v9m5-13v13" />
            </svg>
          </button>
          <MetadataPanel
            header={header}
            profile={profile}
            streamType={streamType}
            fps={fps}
            show={showMetadata}
            onToggle={setShowMetadata}
          />
        </div>
      </div>
      
      {showDeviceName && (
        <div className="px-3 py-1 text-xs text-rs-muted bg-rs-darker/40 nums">
          {deviceName} ({serialNumber})
        </div>
      )}
      
      {/* Content, centred: the wireframe and the graph are both a fixed height, so
          in a sparse stream grid the tile is taller than they are. */}
      <div className="flex-1 min-h-0 flex flex-col justify-center p-4">
        {showGraph ? (
          <IMUChart data={points} axisFloor={axisFloor} />
        ) : (
          <IMUOrientation sample={latest ?? null} unit={unit} />
        )}
      </div>
    </div>
  )
}

interface MetadataOverlayProps {
  streamType: string
  header: FrameHeader
  profile: SupportedStreamProfile
  fps: number
}

export function MetadataOverlay({ streamType, header, profile, fps }: MetadataOverlayProps) {
  const isVideo = header.width !== undefined
  // Mirrors C++ viewer (common/stream-model.cpp): when SDK falls back to system_time,
  // per-frame metadata is unavailable from the kernel UVC driver.
  const metadataUnavailable = header.timestamp_domain === 'system_time'
  return (
    <div className="absolute inset-0 overflow-y-auto bg-black/60 text-white text-xs z-10">
      <div className="sticky top-0 px-3 py-2 bg-rs-inset font-semibold border-b border-rs-border">
        Frame Metadata — {streamType.toUpperCase()}
      </div>
      <div className="px-3 py-2 border-b border-rs-border bg-rs-darker/60">
        <div className="text-rs-muted uppercase tracking-wide text-[10px] mb-1">Viewer Info</div>
        <div className="grid grid-cols-2 gap-x-4 gap-y-0.5 font-mono">
          <MetadataItem label="Frame Timestamp" value={header.timestamp} />
          <MetadataItem label="Clock Domain" value={header.timestamp_domain} />
          <MetadataItem label="Frame Number" value={header.frame_number} />
          <MetadataItem label="Pixel Format" value={header.format.toUpperCase()} />
          <MetadataItem label="Hardware Size" value={isVideo ? `${profile.width}×${profile.height}` : undefined} />
          <MetadataItem label="Display Size" value={isVideo ? `${header.width}×${header.height}` : undefined} />
          <MetadataItem label="Hardware FPS" value={profile.fps} />
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
        {Object.entries(header.metadata).map(([k, v]) => (
          <MetadataItem key={k} label={lessScreamy(k)} value={v} />
        ))}
      </div>
    </div>
  )
}

interface MetadataPanelProps {
  header: FrameHeader | null // the newest frame's; null until one arrives
  profile: SupportedStreamProfile
  streamType: string
  fps: number
  show: boolean
  onToggle: (show: boolean) => void
  buttonClassName?: string
}

export function MetadataPanel({ header, profile, streamType, fps, show, onToggle, buttonClassName = '' }: MetadataPanelProps) {
  if (!header) return null
  return (
    <>
      <button
        type="button"
        onClick={() => onToggle(!show)}
        data-testid="toggle-metadata"
        title={show ? 'Hide frame metadata' : 'Show frame metadata'}
        className={`px-2 py-0.5 bg-black/60 hover:bg-black/80 rounded text-xs text-white border border-rs-border z-20 ${buttonClassName}`}
      >
        {show ? '✕' : 'Metadata'}
      </button>
      {show && <MetadataOverlay streamType={streamType} header={header} profile={profile} fps={fps} />}
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
    <div className="flex justify-between border-b border-rs-border/50 py-0.5">
      <span className="text-rs-muted truncate pr-2">{label}</span>
      <span className="text-right shrink-0" data-testid={`metadata-${label.toLowerCase().replace(/\s+/g, '-')}`}>{value}</span>
    </div>
  )
}
