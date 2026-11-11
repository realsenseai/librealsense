import { useEffect, useRef, useState } from 'react'
import { openFrameStream, type Frame, type FrameHeader, type FrameStreamState } from './frames'
import type { SupportedStreamProfile } from './types'

/**
 * Hands a view its stream's newest frame once per animation frame while it shows it. Also gives
 * the rate frames arrive at, and the newest header for the metadata panel: once, then per animation
 * frame while `watchHeader`.
 */
export function useFrameStream(
  deviceId: string,
  profile: SupportedStreamProfile,
  watchHeader: boolean,
  onFrame: (frame: Frame) => void,
) {
  const [state, setState] = useState<FrameStreamState | null>(null)
  const [fps, setFps] = useState(0)
  const [header, setHeader] = useState<FrameHeader | null>(null)
  const handlers = useRef({ onFrame, watchHeader })
  handlers.current = { onFrame, watchHeader }
  useEffect(() => {
    let count = 0
    let since = Date.now()
    let newest: Frame | null = null
    let shown: FrameHeader | null = null
    let animation = 0
    const stop = openFrameStream(deviceId, profile, (frame) => {
      newest = frame
      animation ||= requestAnimationFrame(() => {
        animation = 0
        handlers.current.onFrame(newest!)
        if (!shown || handlers.current.watchHeader) setHeader(shown = newest!.header)
      })
      count++
      const now = Date.now()
      if (now - since >= 1000) {
        setFps(+(count * 1000 / (now - since)).toFixed(2))
        count = 0
        since = now
      }
    }, setState)
    return () => {
      stop()
      cancelAnimationFrame(animation)
      setState(null)
    }
  }, [deviceId, profile]) // the status's profile object lives as long as the sensor streams
  return { state, fps, header }
}
