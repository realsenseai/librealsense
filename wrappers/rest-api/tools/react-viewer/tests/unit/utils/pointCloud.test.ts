import { describe, it, expect } from 'vitest'
import { buildCloud } from '@/utils/pointCloud'
import type { Frame } from '@/api/frames'

// One depth pixel at 1 m straight ahead, textured by a one-pixel color frame that sees it
const color = (format: string, bytes: number[]): Frame => ({
  header: { frame_number: 0, timestamp: 0, timestamp_domain: '', metadata: {}, format, width: 1, height: 1, stride: bytes.length, bpp: bytes.length },
  pixels: new Uint8Array(bytes).buffer,
})
const pointColor = (texture: Frame) => {
  const depth: Frame = {
    header: { frame_number: 0, timestamp: 0, timestamp_domain: '', metadata: {}, format: 'z16', width: 1, height: 1, stride: 2, bpp: 2, units: 0.001 },
    pixels: new Uint16Array([1000]).buffer,
  }
  const colors = new Float32Array(3)
  buildCloud(depth, new Float32Array(2), new Float32Array(3), colors, new Float32Array(1), {
    frame: texture,
    intrinsics: { width: 1, height: 1, ppx: 0, ppy: 0, fx: 1, fy: 1, model: 'none', coeffs: [0, 0, 0, 0, 0] },
    depthToColor: { rotation: [1, 0, 0, 0, 1, 0, 0, 0, 1], translation: [0, 0, 0] },
  })
  return Array.from(colors, c => Math.round(c * 255))
}

describe('buildCloud', () => {
  it('takes each point its color from the texture, BGR in true colors as the 2D view draws it', () => {
    expect(pointColor(color('rgb8', [10, 20, 30]))).toEqual([10, 20, 30])
    expect(pointColor(color('bgr8', [30, 20, 10]))).toEqual([10, 20, 30])
    expect(pointColor(color('bgra8', [30, 20, 10, 255]))).toEqual([10, 20, 30])
  })
})
