import { io, Socket } from 'socket.io-client'
import type { MetadataUpdate } from './types'
import { useAppStore } from '../store'

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
      transports: ['polling', 'websocket'], // Start with polling, upgrade to websocket
      reconnection: true,
      reconnectionDelay: 1000,
      reconnectionDelayMax: 5000,
      reconnectionAttempts: 10,
      timeout: 20000,
    })

    this.socket.on('connect', () => {
      console.log('Socket.IO connected:', this.socket?.id)
      this.isConnecting = false
      useAppStore.getState().setConnected(true)
    })

    this.socket.on('disconnect', (reason) => {
      console.log('Socket.IO disconnected:', reason)
      useAppStore.getState().setConnected(false)
    })

    this.socket.on('connect_error', (error) => {
      console.error('Socket.IO connection error:', error.message)
      this.isConnecting = false
    })

    this.socket.on('metadata_update', (data: MetadataUpdate) => {
      useAppStore.getState().updateMetadata(data)
    })

    this.socket.on('welcome', (data) => {
      console.log('Socket.IO welcome:', data)
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
