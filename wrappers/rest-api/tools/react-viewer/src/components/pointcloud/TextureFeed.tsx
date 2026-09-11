import { useEffect, useRef } from 'react'
import { WebRTCHandler } from '../../api/webrtc'

interface TextureFeedProps {
  deviceId: string
  stream: string
  onVideo: (video: HTMLVideoElement | null) => void
}

/** A hidden <video> fed by its own WebRTC session, so the 3D view can texture the cloud
 * from a stream whose 2D tile is not mounted. */
export function TextureFeed({ deviceId, stream, onVideo }: TextureFeedProps) {
  const videoRef = useRef<HTMLVideoElement>(null)

  useEffect(() => {
    const video = videoRef.current
    if (!video) return
    let alive = true
    const handler = new WebRTCHandler(deviceId, [stream], (event) => {
      if (!alive) return
      video.srcObject = event.streams[0]
      void video.play().catch(() => undefined)
      onVideo(video)
    }, () => undefined)
    handler.connect().catch((error) => console.error('Texture feed failed:', error))
    return () => {
      alive = false
      handler.disconnect()
      video.srcObject = null
      onVideo(null)
    }
  }, [deviceId, stream, onVideo])

  return <video ref={videoRef} autoPlay playsInline muted className="hidden" data-testid={`texture-feed-${stream}`} />
}
