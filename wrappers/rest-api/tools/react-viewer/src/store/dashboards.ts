import { create } from 'zustand'
import type { MetadataUpdate } from '../api/types'

export const HISTORY = 100 // seconds kept per stream, as the legacy dashboard does

export interface StreamHistory {
  deviceId: string
  stream: string
  drops: number[]
  frames: number[]
  expectedFps: number
  lastFrameNumber: number
}

interface DashboardsState {
  streams: Record<string, StreamHistory> // "device:stream"
  open: boolean
  selected: string | null
  ingest: (update: MetadataUpdate) => void
  setOpen: (open: boolean) => void
  select: (key: string | null) => void
  clear: (deviceId?: string) => void
}

/** Ring of per-second frame/drop counts per stream, fed from the metadata stream. A second is
 * recorded when the server's per-second window advances (the frame_number keeps moving). */
export const useDashboardsStore = create<DashboardsState>()((set) => ({
  streams: {},
  open: false,
  selected: null,

  ingest: (update) => set((s) => {
    let streams = s.streams
    for (const [stream, md] of Object.entries(update.metadata_streams)) {
      if (!md?.stats) continue
      const key = `${update.device_id}:${stream}`
      const prev = streams[key]
      const last = prev ? prev.drops[prev.drops.length - 1] : undefined
      const lastFrames = prev ? prev.frames[prev.frames.length - 1] : undefined
      // The server only changes these once a second; record each new window once
      if (prev && last === md.stats.drops_per_second && lastFrames === md.stats.frames_per_second && prev.lastFrameNumber === md.frame_number) continue
      if (prev && last === md.stats.drops_per_second && lastFrames === md.stats.frames_per_second) {
        streams = { ...streams, [key]: { ...prev, lastFrameNumber: md.frame_number, expectedFps: md.stats.expected_fps } }
        continue
      }
      const next: StreamHistory = {
        deviceId: update.device_id, stream,
        drops: [...(prev?.drops ?? []), md.stats.drops_per_second].slice(-HISTORY),
        frames: [...(prev?.frames ?? []), md.stats.frames_per_second].slice(-HISTORY),
        expectedFps: md.stats.expected_fps,
        lastFrameNumber: md.frame_number,
      }
      streams = { ...streams, [key]: next }
    }
    return streams === s.streams ? {} : { streams }
  }),
  setOpen: (open) => set({ open }),
  select: (key) => set({ selected: key }),
  clear: (deviceId) => set((s) => ({
    streams: deviceId ? Object.fromEntries(Object.entries(s.streams).filter(([, v]) => v.deviceId !== deviceId)) : {},
  })),
}))
