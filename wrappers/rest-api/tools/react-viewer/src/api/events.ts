// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// The server's events (app/services/events.py): JSON messages {event, ...fields} on one WebSocket,
// reopened whenever it drops
import { useAppStore } from '../store'
import { WS_ORIGIN } from './backend'
import { apiClient } from './client'
import type { SupportedStreamProfile } from './types'

type Handler = (message: never) => void
const handlers = new Map<string, Set<Handler>>()
let socket: WebSocket | null = null

/** Call handler with each message of the event until the returned function is called */
export function onEvent<T>(event: string, handler: (message: T) => void): () => void {
  if (!handlers.has(event)) handlers.set(event, new Set())
  handlers.get(event)!.add(handler as Handler)
  return () => handlers.get(event)!.delete(handler as Handler)
}

export function connectEvents(): void {
  if (socket) return
  const ws = new WebSocket(`${WS_ORIGIN}/api/v1/events`)
  socket = ws
  let opened = false
  ws.onopen = () => {
    opened = true
    const store = useAppStore.getState()
    store.setConnected(true)
    // Events are not replayed: read what changed while disconnected
    store.fetchDevices()
    for (const { device } of store.getActiveDevices()) {
      apiClient.getSensors(device.device_id)
        .then(sensors => store.applyStreaming(device.device_id,
          Object.fromEntries(sensors.map(s => [s.sensor_id, s.active_profiles]))))
        .catch(() => {}) // the device left; fetchDevices drops it
    }
  }
  ws.onmessage = ({ data }) => {
    const message = JSON.parse(data)
    handlers.get(message.event)?.forEach(h => h(message as never))
  }
  ws.onclose = () => {
    socket = null
    if (opened) useAppStore.getState().setConnected(false)
    setTimeout(connectEvents, 1000) // the local backend may come up any moment
  }
}

// A device returning after a FW flash must not be served from the cached list
onEvent('devices_changed', () => useAppStore.getState().fetchDevices(true))

// Any client's start or stop, through either API: every page shows what streams
onEvent<{ device_id: string; sensors: Record<string, SupportedStreamProfile[]> }>('streaming_changed',
  m => useAppStore.getState().applyStreaming(m.device_id, m.sensors))
