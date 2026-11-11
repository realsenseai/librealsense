import axios, { AxiosInstance } from 'axios'
import { onEvent } from './events'
import { BACKEND_ORIGIN } from './backend'
import { visibleOptions } from './types'
import type {
  AdvancedControls,
  AdvancedModeStatus,
  DeviceInfo,
  SensorFilters,
  SensorInfo,
  OptionInfo,
  StreamExtrinsics,
  SupportedStreamProfile,
} from './types'

const API_BASE = `${BACKEND_ORIGIN}/api/v1`

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

  // ============ Firmware events (api/events.ts) ============

  onFirmwareProgress(deviceId: string, callback: FirmwareProgressCallback): () => void {
    return onEvent<{ device_id: string; progress: number; phase?: 'downloading' | 'installing' }>('firmware_progress',
      m => m.device_id === deviceId && callback(m.progress, m.phase))
  }

  onFirmwareError(deviceId: string, callback: FirmwareErrorCallback): () => void {
    return onEvent<{ device_id: string; error: string }>('firmware_update_failed',
      m => m.device_id === deviceId && callback(m.error))
  }

  onFirmwareSuccess(deviceId: string, callback: FirmwareSuccessCallback): () => void {
    return onEvent<{ device_id: string; firmware_version: string | null }>('firmware_update_success',
      m => m.device_id === deviceId && callback(m.firmware_version))
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

  async getAdvancedMode(deviceId: string): Promise<AdvancedModeStatus> {
    const response = await this.client.get(`/devices/${deviceId}/advanced_mode/`)
    return response.data
  }

  async setAdvancedMode(deviceId: string, enable: boolean): Promise<AdvancedModeStatus> {
    const response = await this.client.post(`/devices/${deviceId}/advanced_mode/`, { enable })
    return response.data
  }

  async getAdvancedControls(deviceId: string): Promise<AdvancedControls> {
    const response = await this.client.get(`/devices/${deviceId}/advanced_mode/controls/`)
    return response.data
  }

  /**
   * Write one control and get it back as the device now holds it. `key` is the group's
   * path under the device (`colorizer`, `advanced_mode/controls/depth_table`,
   * `sensors/<id>/filters/Spatial Filter`), which is all that separates the four sources.
   */
  async setControl(
    deviceId: string,
    key: string,
    field: string,
    value: number | boolean | string
  ): Promise<OptionInfo> {
    const response = await this.client.put(`/devices/${deviceId}/${key}/${field}/`, { value })
    return response.data
  }

  /** Bypass or apply one post-processing filter; `key` is the filter's own path. */
  async setFilterEnabled(deviceId: string, key: string, enabled: boolean): Promise<void> {
    await this.client.put(`/devices/${deviceId}/${key}/enabled/`, { value: enabled })
  }

  async getSensorFilters(deviceId: string, sensorId: string): Promise<SensorFilters> {
    const response = await this.client.get<SensorFilters>(
      `/devices/${deviceId}/sensors/${sensorId}/filters/`
    )
    return Object.fromEntries(
      Object.entries(response.data).map(([name, f]) => [name, { ...f, options: visibleOptions(f.options) }])
    )
  }

  async getColorizerOptions(deviceId: string): Promise<OptionInfo[]> {
    const response = await this.client.get(`/devices/${deviceId}/colorizer/`)
    return visibleOptions(response.data)
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
    return response.data.map((s) => ({ ...s, options: visibleOptions(s.options) }))
  }

  // ============ Per-Sensor Streaming (Sensor API) ============

  async startSensor(
    deviceId: string,
    sensorId: string,
    profiles: SupportedStreamProfile[]
  ): Promise<void> {
    await this.client.post(`/devices/${deviceId}/sensors/${sensorId}/start`, { profiles })
  }

  /** The device's calibration between its streams, from each to every other */
  async getExtrinsics(deviceId: string): Promise<StreamExtrinsics[]> {
    const { data } = await this.client.get<StreamExtrinsics[]>(`/devices/${deviceId}/extrinsics`)
    return data
  }

  async stopSensor(deviceId: string, sensorId: string): Promise<void> {
    await this.client.post(`/devices/${deviceId}/sensors/${sensorId}/stop`)
  }

  // ============ System ============

  async enableMetadata(): Promise<{ status: string; note?: string }> {
    const response = await this.client.post<{ status: string; note?: string }>('/system/enable-metadata')
    return response.data
  }
}

export const apiClient = new ApiClient()
