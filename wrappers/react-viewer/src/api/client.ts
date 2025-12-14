import axios, { AxiosInstance } from 'axios'
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

const API_BASE = '/api'

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

  // ============ Devices ============

  async getDevices(): Promise<DeviceInfo[]> {
    const response = await this.client.get<DeviceInfo[]>('/devices/')
    return response.data
  }

  async getDevice(deviceId: string): Promise<DeviceInfo> {
    const response = await this.client.get<DeviceInfo>(`/devices/${deviceId}/`)
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

  async stopStreaming(deviceId: string): Promise<void> {
    await this.client.post(`/devices/${deviceId}/stream/stop/`)
  }

  async getStreamStatus(deviceId: string): Promise<StreamStatus> {
    const response = await this.client.get<StreamStatus>(`/devices/${deviceId}/stream/status/`)
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
