import axios, { AxiosInstance } from 'axios'
import { socketService } from './socket'
import type {
  DeviceInfo,
  SensorInfo,
  OptionInfo,
  WebRTCOffer,
  WebRTCSession,
  ICECandidate,
  SensorStreamConfig,
  SensorStreamStatus,
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

type FirmwareProgressCallback = (progress: number, phase?: 'downloading' | 'installing') => void
type FirmwareErrorCallback = (error: string) => void
type FirmwareSuccessCallback = (firmwareVersion: string | null) => void

class ApiClient {
  private client: AxiosInstance

  constructor() {
    this.client = axios.create({
      baseURL: API_BASE,
      headers: {
        'Content-Type': 'application/json',
      },
    })
  }

  // ============ Firmware Socket.IO events ============
  // These piggyback on the shared socketService connection (see api/socket.ts).

  onFirmwareProgress(deviceId: string, callback: FirmwareProgressCallback): () => void {
    const eventName = `firmware_progress_${deviceId}`
    const handler = (data: unknown) => {
      const d = data as { progress: number; phase?: 'downloading' | 'installing' }
      callback(d.progress, d.phase)
    }
    socketService.on(eventName, handler as (...args: unknown[]) => void)
    return () => socketService.off(eventName, handler as (...args: unknown[]) => void)
  }

  onFirmwareError(deviceId: string, callback: FirmwareErrorCallback): () => void {
    const eventName = `firmware_update_failed_${deviceId}`
    const handler = (data: unknown) => callback((data as { error: string }).error)
    socketService.on(eventName, handler as (...args: unknown[]) => void)
    return () => socketService.off(eventName, handler as (...args: unknown[]) => void)
  }

  onFirmwareSuccess(deviceId: string, callback: FirmwareSuccessCallback): () => void {
    const eventName = `firmware_update_success_${deviceId}`
    const handler = (data: unknown) =>
      callback((data as { firmware_version: string | null }).firmware_version)
    socketService.on(eventName, handler as (...args: unknown[]) => void)
    return () => socketService.off(eventName, handler as (...args: unknown[]) => void)
  }

  // ============ Health ============

  async getHealth(): Promise<{ status: string; service: string; sdk_version: string; warnings?: string[] }> {
    const response = await this.client.get<{
      status: string
      service: string
      sdk_version: string
      warnings?: string[]
    }>('/health')
    return response.data
  }

  // ============ Devices ============

  async getDevices(forceRefresh: boolean = false): Promise<DeviceInfo[]> {
    const response = await this.client.get<DeviceInfo[]>('/devices/', {
      params: { force_refresh: forceRefresh || undefined },
    })
    return response.data
  }

  /** The firmware version the online DB recommends for this device, if any. */
  async getRecommendedFirmware(deviceId: string): Promise<{ recommended?: string }> {
    const response = await this.client.get(`/devices/${deviceId}/firmware/`)
    return response.data
  }

  async resetDevice(deviceId: string): Promise<void> {
    await this.client.post(`/devices/${deviceId}/hw_reset/`)
  }

  async updateFirmwareFromFile(
    deviceId: string,
    file: File
  ): Promise<{ status: string; firmware_version?: string | null; progress?: number }> {
    const form = new FormData()
    form.append('file', file)
    // Clear the per-client default Content-Type (`application/json`) so the
    // browser sets `multipart/form-data; boundary=...` itself when posting
    // FormData. Hard-coding `multipart/form-data` here would strip the boundary
    // and break FastAPI parsing (422).
    const response = await this.client.post(
      `/devices/${deviceId}/firmware/update_from_file`,
      form,
      { headers: { 'Content-Type': undefined as unknown as string } },
    )
    return response.data
  }

  async updateFirmwareFromRecommended(
    deviceId: string
  ): Promise<{ status: string; firmware_version?: string | null; progress?: number }> {
    const response = await this.client.post(`/devices/${deviceId}/firmware/update_from_recommended`)
    return response.data
  }

  // ============ Sensors ============

  async getSensors(deviceId: string): Promise<SensorInfo[]> {
    const response = await this.client.get<SensorInfo[]>(`/devices/${deviceId}/sensors/`)
    return response.data
  }

  // ============ Options ============

  async getOptions(deviceId: string, sensorId: string): Promise<OptionInfo[]> {
    const response = await this.client.get<OptionInfo[]>(
      `/devices/${deviceId}/sensors/${sensorId}/options/`
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

  // ============ Point Cloud ============

  async enablePointCloud(deviceId: string): Promise<void> {
    await this.client.post(`/devices/${deviceId}/point_cloud/activate/`)
  }

  async disablePointCloud(deviceId: string): Promise<void> {
    await this.client.post(`/devices/${deviceId}/point_cloud/deactivate/`)
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

  async closeWebRTCSession(sessionId: string): Promise<void> {
    await this.client.delete(`/webrtc/sessions/${sessionId}/`)
  }

  // ============ System ============

  async enableMetadata(): Promise<{ status: string; note?: string }> {
    const response = await this.client.post<{ status: string; note?: string }>('/system/enable-metadata')
    return response.data
  }
}

export const apiClient = new ApiClient()
