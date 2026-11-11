import { WS_ORIGIN } from './backend'
import type { Intrinsics, SupportedStreamProfile } from './types'

/** The header fields the viewer reads; the backend's app/services/frame_sender.py sends more. */
export interface FrameHeader {
  frame_number: number
  timestamp: number
  timestamp_domain: string
  format: string
  metadata: Record<string, number> // the frame's metadata the SDK supports, by rs.frame_metadata_value name
  width?: number // video frames only, as are stride, bpp and intrinsics
  height?: number
  stride?: number // bytes per row
  bpp?: number // bytes per pixel
  intrinsics?: Intrinsics // absent for uncalibrated streams
  units?: number // meters per depth value
  color_table_size?: number
}

export interface Frame {
  header: FrameHeader
  pixels: ArrayBuffer // the frame's bytes, unchanged; a motion frame's start with float32 x, y, z
  colorTable?: Uint8Array // depth only: the RGB of every depth value
}

export type FrameStreamState = 'connecting' | 'open' | 'closed'

interface Listener {
  onFrame: (frame: Frame) => void
  onState: (state: FrameStreamState) => void
}

// One WebSocket per stream, whoever uses its frames (the 2D tile, the 3D view): each frame arrives once
const sockets = new Map<string, { worker: Worker; state: FrameStreamState; listeners: Set<Listener> }>()

/**
 * Receives one stream's frames, read off its WebSocket in a Worker; returns the function that stops
 * receiving them. Listeners of the same stream share its socket, which closes with the last one.
 */
export function openFrameStream(
  deviceId: string,
  profile: SupportedStreamProfile,
  onFrame: (frame: Frame) => void,
  onState: (state: FrameStreamState) => void,
): () => void {
  const url = `${WS_ORIGIN}/api/v1/devices/${encodeURIComponent(deviceId)}/frames/${profile.stream_type}/${profile.stream_index}`
  if (!sockets.has(url)) {
    const worker = new Worker(new URL('../workers/frameWorker.ts', import.meta.url), { type: 'module' })
    const created = { worker, state: 'connecting' as FrameStreamState, listeners: new Set<Listener>() }
    worker.onmessage = ({ data }) => {
      if ('state' in data) created.state = data.state
      created.listeners.forEach(l => ('state' in data ? l.onState(data.state) : l.onFrame(data)))
    }
    worker.postMessage({ url })
    sockets.set(url, created)
  }
  const socket = sockets.get(url)!
  const listener = { onFrame, onState }
  socket.listeners.add(listener)
  onState(socket.state)
  return () => {
    socket.listeners.delete(listener)
    if (socket.listeners.size) return
    socket.worker.terminate()
    sockets.delete(url)
  }
}
