import { io, Socket } from 'socket.io-client'
import type { JobInfo, MetadataUpdate } from './types'
import { useAppStore } from '../store'
import { useJobsStore } from '../store/jobs'

class SocketService {
  private socket: Socket | null = null
  private isConnecting = false

  connect(): void {
    if (this.socket?.connected || this.isConnecting) {
      return
    }

    this.isConnecting = true

    // Connect directly to the backend server in development
    const serverUrl = import.meta.env.DEV 
      ? 'http://localhost:8000' 
      : window.location.origin

    this.socket = io(serverUrl, {
      path: '/socket',
      // Websocket first: metadata is broadcast at 30 Hz, and HTTP long-polling
      // at that rate competes with the WebRTC event loop. tryAllTransports
      // makes polling a real fallback — engine.io only advances to the next
      // transport on connect failure when it is set.
      transports: ['websocket', 'polling'],
      tryAllTransports: true,
      reconnection: true,
      reconnectionDelay: 1000,
      reconnectionDelayMax: 5000,
      reconnectionAttempts: 10,
      timeout: 20000,
    })

    this.socket.on('connect', () => {
      if (import.meta.env.DEV) console.log('Socket.IO connected:', this.socket?.id)
      this.isConnecting = false
      useAppStore.getState().setConnected(true)
      // Refresh devices in case any were plugged in/out while disconnected.
      useAppStore.getState().fetchDevices()
    })

    this.socket.on('disconnect', (reason) => {
      if (import.meta.env.DEV) console.log('Socket.IO disconnected:', reason)
      useAppStore.getState().setConnected(false)
    })

    this.socket.on('connect_error', (error) => {
      console.error('Socket.IO connection error:', error.message)
      this.isConnecting = false
    })

    this.socket.on('metadata_update', (data: MetadataUpdate) => {
      useAppStore.getState().updateMetadata(data)
    })

    this.socket.on('devices_changed', (data: { added?: string[]; removed?: string[] }) => {
      if (import.meta.env.DEV) console.log('Socket.IO devices_changed:', data)
      // Force a re-enumeration: a device returning after a FW flash must not be
      // served from the cached list. fetchDevices handles first-load auto-activate.
      useAppStore.getState().fetchDevices(true)
    })

    this.socket.on('options_changed', (data: { device_id: string; sensor_id: string; options: { option_id: string; current_value: number }[] }) => {
      useAppStore.getState().applyOptionChanges(data.device_id, data.sensor_id, data.options)
    })

    this.socket.on('job', (job: JobInfo) => {
      useJobsStore.getState().upsert(job)
    })

    this.socket.on('welcome', (data) => {
      if (import.meta.env.DEV) console.log('Socket.IO welcome:', data)
    })
  }

  disconnect(): void {
    if (this.socket) {
      this.socket.removeAllListeners()
      this.socket.disconnect()
      this.socket = null
      this.isConnecting = false
    }
  }

  emit(event: string, data: unknown): void {
    if (this.socket?.connected) {
      this.socket.emit(event, data)
    }
  }

  on(event: string, callback: (...args: unknown[]) => void): void {
    if (this.socket) {
      this.socket.on(event, callback)
    }
  }

  off(event: string, callback?: (...args: unknown[]) => void): void {
    if (this.socket) {
      this.socket.off(event, callback)
    }
  }

  get isConnected(): boolean {
    return this.socket?.connected ?? false
  }
}

export const socketService = new SocketService()
