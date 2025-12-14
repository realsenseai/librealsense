import { useEffect, useRef, useState, useCallback } from 'react'
import { useAppStore } from '../store'
import { WebRTCHandler } from '../api/webrtc'

export function StreamViewer() {
  const { selectedDevice, streamConfigs, isStreaming, streamMetadata } = useAppStore()
  const enabledStreams = streamConfigs.filter((c) => c.enable)

  return (
    <div className="h-full">
      {enabledStreams.length === 0 ? (
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
            <p className="text-sm mt-1">Enable streams in the right panel to start viewing</p>
          </div>
        </div>
      ) : (
        <div
          className="h-full grid gap-2"
          style={{
            gridTemplateColumns: `repeat(${Math.min(enabledStreams.length, 2)}, 1fr)`,
            gridTemplateRows: `repeat(${Math.ceil(enabledStreams.length / 2)}, 1fr)`,
          }}
        >
          {enabledStreams.map((config) => (
            <StreamTile
              key={`${config.sensor_id}-${config.stream_type}`}
              deviceId={selectedDevice?.device_id || ''}
              streamType={config.stream_type}
              isStreaming={isStreaming}
              metadata={streamMetadata[config.stream_type]}
            />
          ))}
        </div>
      )}
    </div>
  )
}

interface StreamTileProps {
  deviceId: string
  streamType: string
  isStreaming: boolean
  metadata?: {
    timestamp: number
    frame_number: number
    width: number
    height: number
  }
}

function StreamTile({ deviceId, streamType, isStreaming, metadata }: StreamTileProps) {
  const videoRef = useRef<HTMLVideoElement>(null)
  const webrtcHandlerRef = useRef<WebRTCHandler | null>(null)
  const [connectionState, setConnectionState] = useState<RTCPeerConnectionState | null>(null)
  const [fps, setFps] = useState(0)
  const lastFrameTime = useRef(0)
  const frameCount = useRef(0)

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
    <div className="relative bg-black rounded-lg overflow-hidden">
      {/* Video Element */}
      <video
        ref={videoRef}
        autoPlay
        playsInline
        muted
        className="w-full h-full object-contain stream-video"
      />

      {/* Stream Label */}
      <div
        className={`absolute top-2 left-2 px-2 py-1 rounded text-xs font-semibold text-white ${getStreamColor(
          streamType
        )}`}
      >
        {streamType.toUpperCase()}
      </div>

      {/* Connection Status */}
      {isStreaming && connectionState && connectionState !== 'connected' && (
        <div className="absolute top-2 right-2 px-2 py-1 bg-yellow-600 rounded text-xs text-white">
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
    </div>
  )
}
