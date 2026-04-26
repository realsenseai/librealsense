import { useEffect, useRef, useState, useCallback, useMemo } from 'react'
import { useAppStore } from '../store'
import { WebRTCHandler } from '../api/webrtc'
import { apiClient } from '../api/client'
import { DepthLegend } from './DepthLegend'
import type { DeviceState, StreamConfig } from '../api/types'

// A stream with its device context
interface DeviceStream {
  deviceId: string
  deviceName: string
  serialNumber: string
  config: StreamConfig
  isStreaming: boolean
  metadata?: {
    timestamp: number
    frame_number: number
    width: number
    height: number
  }
}

export function StreamViewer() {
  const { deviceStates } = useAppStore()
  
  // Collect all enabled streams from all active devices
  const allEnabledStreams = useMemo(() => {
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
        
        streams.push({
          deviceId: ds.device.device_id,
          deviceName: ds.device.name,
          serialNumber: ds.device.serial_number,
          config,
          isStreaming: streamIsActive,
          metadata: ds.streamMetadata[config.stream_type],
        })
      })
    })
    
    return streams
  }, [deviceStates])

  const activeDeviceCount = Object.values(deviceStates).filter(ds => ds.isActive).length

  return (
    <div className="h-full">
      {allEnabledStreams.length === 0 ? (
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
            <p className="text-lg">No Streams Enabled</p>
            <p className="text-sm mt-1">
              {activeDeviceCount === 0 
                ? 'Activate a device and enable streams to start viewing'
                : 'Enable streams in the right panel to start viewing'}
            </p>
          </div>
        </div>
      ) : (
        <div
          className="h-full grid gap-2"
          style={{
            gridTemplateColumns: `repeat(${Math.min(allEnabledStreams.length, 2)}, 1fr)`,
            gridTemplateRows: `repeat(${Math.ceil(allEnabledStreams.length / 2)}, 1fr)`,
          }}
        >
          {allEnabledStreams.map((stream) => {
            const isMotionStream = ['gyro', 'accel'].includes(stream.config.stream_type.toLowerCase())
            
            if (isMotionStream) {
              return (
                <IMUStreamTile
                  key={`${stream.deviceId}-${stream.config.sensor_id}-${stream.config.stream_type}`}
                  streamType={stream.config.stream_type}
                  isStreaming={stream.isStreaming}
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
                isStreaming={stream.isStreaming}
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
  isStreaming: boolean
  showDeviceName?: boolean
  metadata?: {
    timestamp: number
    frame_number: number
    width: number
    height: number
  }
}

function StreamTile({ deviceId, deviceName, serialNumber, streamType, isStreaming, showDeviceName, metadata }: StreamTileProps) {
  const videoRef = useRef<HTMLVideoElement>(null)
  const containerRef = useRef<HTMLDivElement>(null)
  const webrtcHandlerRef = useRef<WebRTCHandler | null>(null)
  const hoverRequestId = useRef(0)
  const [connectionState, setConnectionState] = useState<RTCPeerConnectionState | null>(null)
  const [fps, setFps] = useState(0)
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
    if (!isDepthStream || !isStreaming) return
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
  }, [isDepthStream, isStreaming, deviceId])

  // Calculate FPS from metadata updates
  useEffect(() => {
    if (metadata?.frame_number) {
      frameCount.current++
      const now = Date.now()
      if (now - lastFrameTime.current >= 1000) {
        setFps(frameCount.current)
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
      if (!isDepthStream || !isStreaming || !containerRef.current || !metadata) return

      const rect = containerRef.current.getBoundingClientRect()
      const x = Math.floor(((e.clientX - rect.left) / rect.width) * metadata.width)
      const y = Math.floor(((e.clientY - rect.top) / rect.height) * metadata.height)
      const mouseX = e.clientX - rect.left
      const mouseY = e.clientY - rect.top

      // Bounds check
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
    [isDepthStream, isStreaming, deviceId, metadata]
  )

  const handleMouseLeave = useCallback(() => {
    hoverRequestId.current++
    setHoverDepth(null)
  }, [])

  useEffect(() => {
    let mounted = true
    
    const startWebRTC = async () => {
      if (!isStreaming || !deviceId) return
      
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

    if (isStreaming && deviceId) {
      startWebRTC()
    } else {
      stopWebRTC()
    }

    return () => {
      mounted = false
      stopWebRTC()
    }
  }, [isStreaming, deviceId, streamType, handleTrack, handleConnectionStateChange])

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
      {isStreaming && connectionState && connectionState !== 'connected' && (
        <div className={`absolute ${showDeviceName ? 'top-7' : 'top-2'} right-2 px-2 py-1 bg-yellow-600 rounded text-xs text-white`}>
          {connectionState}
        </div>
      )}

      {/* Metadata Overlay */}
      {isStreaming && metadata && (
        <div className="absolute bottom-2 left-2 right-2 flex justify-between text-xs text-white bg-black/50 px-2 py-1 rounded">
          <span>
            {metadata.width}×{metadata.height}
          </span>
          <span>Frame: {metadata.frame_number}</span>
          <span>{fps} FPS</span>
        </div>
      )}

      {/* Placeholder when not streaming */}
      {!isStreaming && (
        <div className="absolute inset-0 flex items-center justify-center text-gray-500">
          <div className="text-center">
            <svg
              className="w-12 h-12 mx-auto mb-2 opacity-50"
              fill="none"
              stroke="currentColor"
              viewBox="0 0 24 24"
            >
              <path
                strokeLinecap="round"
                strokeLinejoin="round"
                strokeWidth={1}
                d="M14.752 11.168l-3.197-2.132A1 1 0 0010 9.87v4.263a1 1 0 001.555.832l3.197-2.132a1 1 0 000-1.664z"
              />
              <path
                strokeLinecap="round"
                strokeLinejoin="round"
                strokeWidth={1}
                d="M21 12a9 9 0 11-18 0 9 9 0 0118 0z"
              />
            </svg>
            <p>Press Start to stream</p>
          </div>
        </div>
      )}

      {/* Depth Legend (for depth streams) */}
      {isDepthStream && isStreaming && (
        <div className="absolute top-12 right-2 bottom-12 w-16">
          <DepthLegend minDepth={depthRange.min} maxDepth={depthRange.max} colorScheme="jet" show={true} />
        </div>
      )}

      {/* Hover Depth Tooltip (for depth streams) */}
      {isDepthStream && hoverDepth && (
        <div
          className="absolute bg-black/90 text-white text-sm px-3 py-2 rounded shadow-lg pointer-events-none z-20"
          style={{
            left: `${hoverDepth.mouseX + 14}px`,
            top: `${hoverDepth.mouseY + 14}px`,
            willChange: 'transform',
          }}
        >
          <div className="font-mono">
            <span className="text-gray-400">Pixel:</span> ({hoverDepth.x}, {hoverDepth.y})
          </div>
          <div className="font-mono font-bold">
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
  isStreaming: boolean
  showDeviceName?: boolean
  deviceName: string
  serialNumber: string
}

function IMUStreamTile({ streamType, isStreaming, showDeviceName, deviceName, serialNumber }: IMUStreamTileProps) {
  const { imuHistory } = useAppStore()
  
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
          {isStreaming && (
            <span className="w-2 h-2 bg-green-500 rounded-full animate-pulse" />
          )}
        </div>
        <span className="text-xs text-gray-400">{unit}</span>
      </div>
      
      {showDeviceName && (
        <div className="px-3 py-1 text-xs text-gray-400 bg-black/20">
          {deviceName} ({serialNumber})
        </div>
      )}
      
      {/* Content */}
      <div className="flex-1 flex flex-col justify-center p-4">
        {!isStreaming ? (
          <div className="text-center text-gray-500">
            <p>Not streaming</p>
          </div>
        ) : !latest ? (
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
