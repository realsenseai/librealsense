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
  SensorStreamConfig,
  SensorStreamStatus,
  BatchSensorStartRequest,
  BatchSensorStopRequest,
  BatchSensorStatus,
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

  private subscribeFirmwareEvent<T>(
    eventName: string,
    callbackMap: Map<string, T>,
    deviceId: string,
    callback: T,
    handler: (data: unknown) => void
  ): () => void {
    callbackMap.set(deviceId, callback)
    if (this.socket) {
      this.socket.on(eventName, handler)
    }
    return () => {
      callbackMap.delete(deviceId)
      if (this.socket) this.socket.off(eventName)
    }
  }

  onFirmwareProgress(deviceId: string, callback: FirmwareProgressCallback): () => void {
    return this.subscribeFirmwareEvent(
      `firmware_progress_${deviceId}`,
      this.firmwareProgressCallbacks,
      deviceId,
      callback,
      (data: unknown) => {
        const cb = this.firmwareProgressCallbacks.get(deviceId)
        if (cb) cb((data as { progress: number }).progress)
      }
    )
  }

  onFirmwareError(deviceId: string, callback: FirmwareErrorCallback): () => void {
    return this.subscribeFirmwareEvent(
      `firmware_update_failed_${deviceId}`,
      this.firmwareErrorCallbacks,
      deviceId,
      callback,
      (data: unknown) => {
        const cb = this.firmwareErrorCallbacks.get(deviceId)
        if (cb) cb((data as { error: string }).error)
      }
    )
  }

  onFirmwareSuccess(deviceId: string, callback: FirmwareSuccessCallback): () => void {
    return this.subscribeFirmwareEvent(
      `firmware_update_success_${deviceId}`,
      this.firmwareSuccessCallbacks,
      deviceId,
      callback,
      (data: unknown) => {
        const cb = this.firmwareSuccessCallbacks.get(deviceId)
        if (cb) cb((data as { firmware_version: string | null }).firmware_version)
      }
    )
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

  // ============ Per-Sensor Streaming (Sensor API) ============

  async startSensor(
    deviceId: string,
    sensorId: string,
    configs: SensorStreamConfig[]  // Array of configs for multi-profile support
  ): Promise<SensorStreamStatus> {
    const response = await this.client.post<SensorStreamStatus>(
      `/devices/${deviceId}/sensors/${sensorId}/start`,
      { configs }  // Send as list
    )
    return response.data
  }

  async stopSensor(deviceId: string, sensorId: string): Promise<SensorStreamStatus> {
    const response = await this.client.post<SensorStreamStatus>(
      `/devices/${deviceId}/sensors/${sensorId}/stop`
    )
    return response.data
  }

  async getSensorStatus(deviceId: string, sensorId: string): Promise<SensorStreamStatus> {
    const response = await this.client.get<SensorStreamStatus>(
      `/devices/${deviceId}/sensors/${sensorId}/status`
    )
    return response.data
  }

  async batchStartSensors(
    deviceId: string,
    request: BatchSensorStartRequest
  ): Promise<BatchSensorStatus> {
    const response = await this.client.post<BatchSensorStatus>(
      `/devices/${deviceId}/sensors/batch/start`,
      request
    )
    return response.data
  }

  async batchStopSensors(
    deviceId: string,
    request?: BatchSensorStopRequest
  ): Promise<BatchSensorStatus> {
    const response = await this.client.post<BatchSensorStatus>(
      `/devices/${deviceId}/sensors/batch/stop`,
      request || {}
    )
    return response.data
  }

  async getBatchSensorStatus(deviceId: string): Promise<BatchSensorStatus> {
    const response = await this.client.get<BatchSensorStatus>(
      `/devices/${deviceId}/sensors/batch/status`
    )
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
