import { describe, it, expect, beforeEach } from 'vitest'
import { useDashboardsStore } from '@/store/dashboards'
import type { MetadataUpdate } from '@/api/types'

const update = (frame: number, fps: number, drops: number): MetadataUpdate => ({
  device_id: 'dev', is_streaming: true, timestamp_server: frame,
  metadata_streams: { depth: { stream_type: 'depth', timestamp: frame * 33, frame_number: frame, width: 1, height: 1, stats: { frames_per_second: fps, drops_per_second: drops, expected_fps: 30 } } },
} as unknown as MetadataUpdate)

describe('dashboards store', () => {
  beforeEach(() => useDashboardsStore.setState({ streams: {}, open: false, selected: null }))

  it('records one sample per server window and ignores repeats within it', () => {
    const s = useDashboardsStore.getState()
    s.ingest(update(1, 0, 0))
    s.ingest(update(2, 0, 0)) // same window: no new sample
    s.ingest(update(40, 30, 0))
    s.ingest(update(80, 28, 2))
    const h = useDashboardsStore.getState().streams['dev:depth']
    expect(h.drops).toEqual([0, 0, 2])
    expect(h.frames).toEqual([0, 30, 28])
    expect(h.expectedFps).toBe(30)
  })

  it('clears per device', () => {
    const s = useDashboardsStore.getState()
    s.ingest(update(1, 0, 0))
    s.clear('other')
    expect(Object.keys(useDashboardsStore.getState().streams)).toEqual(['dev:depth'])
    s.clear('dev')
    expect(useDashboardsStore.getState().streams).toEqual({})
  })
})
