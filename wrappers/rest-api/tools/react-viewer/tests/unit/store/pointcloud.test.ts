import { describe, it, expect, beforeEach } from 'vitest'
import { pickTextureSource, usePointCloudStore } from '@/store/pointcloud'

describe('point cloud store', () => {
  beforeEach(() => usePointCloudStore.setState({ frames: {}, geometry: {}, textureSource: {}, depthSource: null }))

  it('keeps the latest frame per device and clears per device', () => {
    const f = (n: number) => ({ width: 1, height: 1, units: 0.001, frameNumber: n, data: new Uint16Array([n]) })
    const s = usePointCloudStore.getState()
    s.pushFrame('a', f(1))
    s.pushFrame('a', f(2))
    s.pushFrame('b', f(3))
    expect(usePointCloudStore.getState().frames.a.frameNumber).toBe(2)
    s.clear('a')
    expect(Object.keys(usePointCloudStore.getState().frames)).toEqual(['b'])
    s.clear()
    expect(usePointCloudStore.getState().frames).toEqual({})
  })

  it('prefers color as texture, never IR by default, honours an explicit choice', () => {
    expect(pickTextureSource(undefined, ['depth', 'color'])).toBe('color')
    expect(pickTextureSource(undefined, ['depth', 'infrared-1'])).toBeNull()
    expect(pickTextureSource('infrared-1', ['depth', 'infrared-1'])).toBe('infrared-1')
    expect(pickTextureSource('color', ['depth'])).toBeNull() // chosen but not streaming
    expect(pickTextureSource(null, ['depth', 'color'])).toBeNull()
  })

  it('keeps a measurement chain with Shift, restarts without, and undoes', () => {
    const s = usePointCloudStore.getState()
    s.clearMeasurement()
    s.addMeasurementPoint([0, 0, 0], false)
    s.addMeasurementPoint([1, 0, 0], false)
    s.addMeasurementPoint([2, 0, 0], false) // third click without Shift starts over
    expect(usePointCloudStore.getState().measurement).toEqual([[2, 0, 0]])
    s.addMeasurementPoint([3, 0, 0], true)
    s.addMeasurementPoint([4, 0, 0], true)
    expect(usePointCloudStore.getState().measurement).toHaveLength(3)
    s.undoMeasurement()
    expect(usePointCloudStore.getState().measurement).toEqual([[2, 0, 0], [3, 0, 0]])
    s.clearMeasurement()
    expect(usePointCloudStore.getState().measurement).toEqual([])
  })

  it('persists only the rendering preferences', () => {
    usePointCloudStore.getState().setShading('points')
    const saved = JSON.parse(localStorage.getItem('rs-viewer-3d') ?? '{}')
    expect(saved.state).toEqual({ shading: 'points', occlusionInvalidation: true })
  })
})
