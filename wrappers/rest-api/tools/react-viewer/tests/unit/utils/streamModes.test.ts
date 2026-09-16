import { describe, it, expect } from 'vitest'
import { unsupportedStreams } from '@/utils/streamModes'
import { createMockStreamConfig } from '../../utils/test-utils'

const depth = {
  stream_type: 'depth', resolutions: [[640, 480], [848, 480]] as [number, number][], fps: [30, 15], formats: ['Z16'],
  modes: [[640, 480, 30, 'z16'], [848, 480, 30, 'z16'], [640, 480, 15, 'z16']] as [number, number, number, string][],
}

describe('unsupportedStreams', () => {
  it('accepts a listed mode and flags one the SDK does not list', () => {
    const cfg = createMockStreamConfig({ stream_type: 'depth', format: 'Z16', enable: true })
    expect(unsupportedStreams([cfg], [depth], { resolution: { width: 848, height: 480 }, framerate: 30 })).toEqual([])
    expect(unsupportedStreams([cfg], [depth], { resolution: { width: 848, height: 480 }, framerate: 15 })).toEqual(['depth'])
  })

  it('uses the stream\'s own values when the sensor selection is per stream', () => {
    const cfg = createMockStreamConfig({ stream_type: 'depth', format: 'Z16', enable: true, resolution: { width: 640, height: 480 }, framerate: 15 })
    expect(unsupportedStreams([cfg], [depth], { resolution: { width: 848, height: 480 }, framerate: 30, perStreamResolution: true, perStreamFps: true })).toEqual([])
  })

  it('ignores disabled streams and profiles without a mode list', () => {
    const off = createMockStreamConfig({ stream_type: 'depth', enable: false, framerate: 99 })
    expect(unsupportedStreams([off], [depth], { resolution: { width: 1, height: 1 }, framerate: 99 })).toEqual([])
    const noModes = { ...depth, modes: undefined }
    expect(unsupportedStreams([createMockStreamConfig({ enable: true, framerate: 99 })], [noModes])).toEqual([])
  })
})
