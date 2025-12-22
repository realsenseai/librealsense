import axios, { AxiosInstance } from 'axios'
import { io, Socket } from 'socket.io-client'
import type {
  DeviceInfo,
  SensorInfo,
  OptionInfo,
  StreamStartRequest,
  StreamStatus,
  WebRTCOffer,
  WebRTCSession,
  ICECandidate,
} from './types'

// Detect if running in Tauri desktop app
const isDesktopApp = typeof window !== 'undefined' && (window as any).__TAURI__ !== undefined

// Determine API base URL based on environment
const getApiBase = () => {
  if (isDesktopApp) {
    // Desktop app: API server runs on localhost:8000
    return 'http://localhost:8000/api/v1'
  }
  // Browser: use relative path (proxied by Vite in dev, served by backend in prod)
  return '/api/v1'
}

const API_BASE = getApiBase()

type FirmwareProgressCallback = (progress: number) => void
type FirmwareErrorCallback = (error: string) => void
type FirmwareSuccessCallback = (firmwareVersion: string | null) => void

class ApiClient {
  private client: AxiosInstance
  private socket: Socket | null = null
  private firmwareProgressCallbacks: Map<string, FirmwareProgressCallback> = new Map()
  private firmwareErrorCallbacks: Map<string, FirmwareErrorCallback> = new Map()
  private firmwareSuccessCallbacks: Map<string, FirmwareSuccessCallback> = new Map()

  constructor() {
    this.client = axios.create({
      baseURL: API_BASE,
      headers: {
        'Content-Type': 'application/json',
      },
    })
    this.initializeSocket()
  }

  private initializeSocket() {
    // Determine Socket.IO origin based on environment
    let socketOrigin: string
    
    if (isDesktopApp) {
      // Desktop app: connect to localhost:8000
      socketOrigin = 'http://localhost:8000'
      console.log('🖥️ Desktop app detected. Connecting to FastAPI backend at:', socketOrigin)
    } else {
      // Browser: use environment variable or current origin
      socketOrigin = (import.meta as any).env?.VITE_SOCKET_ORIGIN || window.location.origin
      console.log('🌐 Browser mode. Connecting to:', socketOrigin)
    }

    this.socket = io(socketOrigin, {
      path: '/socket',
      reconnectionDelay: 1000,
      reconnection: true,
      reconnectionAttempts: 5,
    })

    this.socket.on('connect', () => {
      console.log('✅ Socket.IO connected successfully', { 
        origin: socketOrigin, 
        id: this.socket?.id, 
        desktop: isDesktopApp 
      })
    })

    this.socket.on('connect_error', (err: any) => {
      console.error('❌ Socket.IO connection error', { 
        origin: socketOrigin,
        message: err?.message || 'Unknown error',
        type: err?.type,
        err,
        desktop: isDesktopApp,
        hint: isDesktopApp ? 'Make sure FastAPI backend is running and accessible' : 'Check if API proxy is configured'
      })
    })

    this.socket.on('disconnect', (reason: string) => {
      console.warn('⚠️ Socket.IO disconnected. Reason:', reason)
    })

    this.socket.on('error', (error: any) => {
      console.error('❌ Socket.IO error:', error)
    })
  }

  /**
   * Register a callback to receive firmware progress updates for a device
   */
  onFirmwareProgress(deviceId: string, callback: FirmwareProgressCallback): () => void {
    this.firmwareProgressCallbacks.set(deviceId, callback)

    // Set up the socket listener if not already done
    if (this.socket) {
      const eventName = `firmware_progress_${deviceId}`
      console.log(`[FW Update] Subscribing to ${eventName}`)
      this.socket.on(eventName, (data: { device_id: string; progress: number }) => {
        console.log(`[FW Update] Progress event received:`, data)
        const cb = this.firmwareProgressCallbacks.get(deviceId)
        if (cb) {
          cb(data.progress)
        }
      })
    } else {
      console.warn('[FW Update] Socket not available for progress subscription')
    }

    // Return unsubscribe function
    return () => {
      this.firmwareProgressCallbacks.delete(deviceId)
      if (this.socket) {
        this.socket.off(`firmware_progress_${deviceId}`)
      }
    }
  }

  /**
   * Register a callback to receive firmware error updates for a device
   */
  onFirmwareError(deviceId: string, callback: FirmwareErrorCallback): () => void {
    this.firmwareErrorCallbacks.set(deviceId, callback)

    if (this.socket) {
      const eventName = `firmware_update_failed_${deviceId}`
      console.log(`[FW Update] Subscribing to ${eventName}`)
      this.socket.on(eventName, (data: { device_id: string; error: string }) => {
        console.log(`[FW Update] Error event received:`, data)
        const cb = this.firmwareErrorCallbacks.get(deviceId)
        if (cb) {
          cb(data.error)
        }
      })
    }

    return () => {
      this.firmwareErrorCallbacks.delete(deviceId)
      if (this.socket) {
        this.socket.off(`firmware_update_failed_${deviceId}`)
      }
    }
  }

  /**
   * Register a callback to receive firmware success updates for a device
   */
  onFirmwareSuccess(deviceId: string, callback: FirmwareSuccessCallback): () => void {
    this.firmwareSuccessCallbacks.set(deviceId, callback)

    if (this.socket) {
      const eventName = `firmware_update_success_${deviceId}`
      console.log(`[FW Update] Subscribing to ${eventName}`)
      this.socket.on(eventName, (data: { device_id: string; firmware_version: string | null }) => {
        console.log(`[FW Update] Success event received:`, data)
        const cb = this.firmwareSuccessCallbacks.get(deviceId)
        if (cb) {
          cb(data.firmware_version)
        }
      })
    }

    return () => {
      this.firmwareSuccessCallbacks.delete(deviceId)
      if (this.socket) {
        this.socket.off(`firmware_update_success_${deviceId}`)
      }
    }
  }

  // ============ Devices ============

  async getDevices(): Promise<DeviceInfo[]> {
    const response = await this.client.get<DeviceInfo[]>('/devices/')
    return response.data
  }

  async getDevice(deviceId: string): Promise<DeviceInfo> {
    const response = await this.client.get<DeviceInfo>(`/devices/${deviceId}/`)
    return response.data
  }

  async getFirmwareStatus(deviceId: string): Promise<{
    device_id: string
    current?: string
    recommended?: string
    status: string
    file_available?: boolean
  }> {
    const response = await this.client.get(`/devices/${deviceId}/status/`)
    return response.data
  }

  async updateFirmware(deviceId: string): Promise<{ status: string; firmware_version?: string; progress?: number }> {
    const response = await this.client.post(`/devices/${deviceId}/update`)
    return response.data
  }

  async resetDevice(deviceId: string): Promise<void> {
    await this.client.post(`/devices/${deviceId}/reset/`)
  }

  // ============ Sensors ============

  async getSensors(deviceId: string): Promise<SensorInfo[]> {
    const response = await this.client.get<SensorInfo[]>(`/devices/${deviceId}/sensors/`)
    return response.data
  }

  async getSensor(deviceId: string, sensorId: string): Promise<SensorInfo> {
    const response = await this.client.get<SensorInfo>(`/devices/${deviceId}/sensors/${sensorId}/`)
    return response.data
  }

  // ============ Options ============

  async getOptions(deviceId: string, sensorId: string): Promise<OptionInfo[]> {
    const response = await this.client.get<OptionInfo[]>(
      `/devices/${deviceId}/sensors/${sensorId}/options/`
    )
    return response.data
  }

  async getOption(deviceId: string, sensorId: string, optionId: string): Promise<OptionInfo> {
    const response = await this.client.get<OptionInfo>(
      `/devices/${deviceId}/sensors/${sensorId}/options/${optionId}/`
    )
    return response.data
  }

  async setOption(
    deviceId: string,
    sensorId: string,
    optionId: string,
    value: number | boolean | string
  ): Promise<{ success: boolean }> {
    const response = await this.client.put<{ success: boolean }>(
      `/devices/${deviceId}/sensors/${sensorId}/options/${optionId}/`,
      { value }
    )
    return response.data
  }

  // ============ Streams ============

  async startStreaming(deviceId: string, request: StreamStartRequest): Promise<void> {
    await this.client.post(`/devices/${deviceId}/stream/start/`, request)
  }

  async stopStreaming(deviceId: string): Promise<StreamStatus> {
    const response = await this.client.post<StreamStatus>(`/devices/${deviceId}/stream/stop/`)
    return response.data
  }

  async getStreamStatus(deviceId: string): Promise<StreamStatus> {
    const response = await this.client.get<StreamStatus>(`/devices/${deviceId}/stream/status/`)
    return response.data
  }

  async getDepthAtPixel(
    deviceId: string,
    x: number,
    y: number
  ): Promise<{ depth: number | null; x: number; y: number; units: string }> {
    const response = await this.client.get<{
      depth: number | null
      x: number
      y: number
      units: string
    }>(`/devices/${deviceId}/stream/depth-at-pixel/`, { params: { x, y } })
    return response.data
  }

  async getDepthRange(
    deviceId: string
  ): Promise<{ min_depth: number; max_depth: number; units: string }> {
    const response = await this.client.get<{
      min_depth: number
      max_depth: number
      units: string
    }>(`/devices/${deviceId}/stream/depth-range/`)
    return response.data
  }

  // ============ Point Cloud ============

  async enablePointCloud(deviceId: string): Promise<void> {
    await this.client.post(`/devices/${deviceId}/point_cloud/activate/`)
  }

  async disablePointCloud(deviceId: string): Promise<void> {
    await this.client.post(`/devices/${deviceId}/point_cloud/deactivate/`)
  }

  async getPointCloudStatus(deviceId: string): Promise<{ enabled: boolean }> {
    const response = await this.client.get<{ enabled: boolean }>(
      `/devices/${deviceId}/point_cloud/status/`
    )
    return response.data
  }

  // ============ WebRTC ============

  async createWebRTCOffer(offer: WebRTCOffer): Promise<WebRTCSession> {
    const response = await this.client.post<WebRTCSession>('/webrtc/offer/', offer)
    return response.data
  }

  async sendWebRTCAnswer(sessionId: string, answer: RTCSessionDescriptionInit): Promise<void> {
    await this.client.post('/webrtc/answer/', {
      session_id: sessionId,
      sdp: answer.sdp,
      type: answer.type,
    })
  }

  async addICECandidate(sessionId: string, candidate: ICECandidate): Promise<void> {
    await this.client.post('/webrtc/ice-candidates/', {
      session_id: sessionId,
      candidate: candidate.candidate,
      sdpMid: candidate.sdpMid,
      sdpMLineIndex: candidate.sdpMLineIndex,
    })
  }

  async getICECandidates(sessionId: string): Promise<ICECandidate[]> {
    const response = await this.client.get<ICECandidate[]>(`/webrtc/sessions/${sessionId}/ice-candidates/`)
    return response.data
  }

  async getWebRTCStatus(sessionId: string): Promise<{ status: string }> {
    const response = await this.client.get<{ status: string }>(`/webrtc/sessions/${sessionId}/`)
    return response.data
  }

  async closeWebRTCSession(sessionId: string): Promise<void> {
    await this.client.delete(`/webrtc/sessions/${sessionId}/`)
  }
}

export const apiClient = new ApiClient()
