import { describe, it, expect, vi } from 'vitest'
import { renderHook, act } from '@testing-library/react'
import { useFrameStream } from '@/api/useFrameStream'
import type { SupportedStreamProfile } from '@/api/types'

// The test hands frames to the stream's callback itself
const stream = vi.hoisted(() => ({ onFrame: (_: unknown) => {} }))
vi.stubGlobal('requestAnimationFrame', () => 1)
vi.mock('@/api/frames', () => ({
  openFrameStream: (_d: string, _p: unknown, onFrame: (frame: unknown) => void) => {
    stream.onFrame = onFrame
    return () => {}
  },
}))

const profile: SupportedStreamProfile = {
  stream_type: 'depth', stream_index: 0, format: 'z16', width: 640, height: 480, fps: 30, default: true,
}
const frame = (frame_number: number, timestamp: number) => ({
  header: { frame_number, timestamp, timestamp_domain: 'global_time', format: 'z16', metadata: {} },
  pixels: new ArrayBuffer(0),
})

describe('useFrameStream', () => {
  it('measures the device rate from frame numbers and timestamps, dropped frames included', () => {
    vi.useFakeTimers({ now: 0 })
    const { result } = renderHook(() => useFrameStream('dev', profile, false, () => {}))
    act(() => stream.onFrame(frame(100, 5000)))
    // A second later only 2 frames reached the page, but the device counted 30 in 1000 ms
    vi.setSystemTime(1000)
    act(() => stream.onFrame(frame(130, 6000)))
    expect(result.current.hardwareFps).toBe(30)
    expect(result.current.fps).toBe(2)
    vi.useRealTimers()
  })
})
