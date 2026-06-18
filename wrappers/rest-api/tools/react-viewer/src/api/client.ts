import axios, { AxiosInstance } from 'axios'
import { socketService } from './socket'
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
    const handler = (data: unknown) => callback((data as { progress: number }).progress)
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

  async getHealth(): Promise<{ status: string; service: string; sdk_version: string }> {
    const response = await this.client.get<{ status: string; service: string; sdk_version: string }>(
      '/health'
    )
    return response.data
  }

  // ============ Devices ============

  async getDevices(forceRefresh: boolean = false): Promise<DeviceInfo[]> {
    const response = await this.client.get<DeviceInfo[]>('/devices/', {
      params: { force_refresh: forceRefresh || undefined },
    })
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
    const response = await this.client.get(`/devices/${deviceId}/status`)
    return response.data
  }

  async resetDevice(deviceId: string): Promise<void> {
    await this.client.post(`/devices/${deviceId}/reset/`)
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

  // ============ System ============

  async enableMetadata(): Promise<{ status: string; note?: string }> {
    const response = await this.client.post<{ status: string; note?: string }>('/system/enable-metadata')
    return response.data
  }
}

export const apiClient = new ApiClient()
