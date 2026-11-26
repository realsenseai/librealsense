import axios, { AxiosInstance } from 'axios'
import { BACKEND_ORIGIN } from './backend'
import { visibleOptions } from './types'
import type {
  AdvancedControls,
  OptionMap,
  WireAdvancedControl,
  DeviceInfo,
  SensorFilters,
  SensorInfo,
  OptionInfo,
  Extrinsics,
  SupportedStreamProfile,
  FirmwarePhase,
} from './types'

const API_BASE = `${BACKEND_ORIGIN}/api/v1`

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

  // ============ Health ============

  async getHealth(): Promise<{ sdk_version: string; warnings: string[] }> {
    return (await this.client.get('/health')).data
  }

  // ============ Devices ============

  async getDevices(): Promise<DeviceInfo[]> {
    const response = await this.client.get<Record<string, string>[]>('/devices/')
    return response.data.map((info) => ({ device_id: info.firmware_update_id, info }))
  }

  /** False only on Windows until frame metadata is enabled there */
  async getMetadataEnabled(deviceId: string): Promise<boolean> {
    const response = await this.client.get<boolean>(`/devices/${deviceId}/is_metadata_enabled`)
    return response.data
  }

  async resetDevice(deviceId: string): Promise<void> {
    await this.client.post(`/devices/${deviceId}/hardware_reset`)
  }

  /** Resets the device: it leaves and comes back through devices_changed */
  async setAdvancedMode(deviceId: string, enable: boolean): Promise<void> {
    await this.client.post(`/devices/${deviceId}/advanced_mode/`, { enable })
  }

  async getAdvancedControls(deviceId: string): Promise<AdvancedControls> {
    const response = await this.client.get<Record<string, Record<string, WireAdvancedControl>>>(
      `/devices/${deviceId}/advanced_mode/controls/`)
    // Drawn as options: a whole-numbered step for an int field, nothing more the SDK doesn't say
    return Object.fromEntries(Object.entries(response.data).map(([group, fields]) => [group,
      Object.entries(fields).map(([field, { integer, ...c }]) =>
        ({ option_id: field, ...c, step: integer ? 1 : undefined, read_only: false }))]))
  }

  /**
   * Write one control. `key` is the group's path under the device (`colorizer`, `advanced_mode/controls/depth_table`,
   * `sensors/<id>/filters/Spatial Filter`), which is all that separates the four sources. Nothing comes back, as
   * from the SDK's set_option
   */
  async setControl(deviceId: string, key: string, field: string, value: number | boolean | string): Promise<void> {
    await this.client.put(`/devices/${deviceId}/${key}/${field}`, { value })
  }

  /** The value one control holds now; `key` as for setControl */
  async getControlValue(deviceId: string, key: string, field: string): Promise<OptionInfo['current_value'] | null> {
    return (await this.client.get<{ current_value: OptionInfo['current_value'] | null }>(`/devices/${deviceId}/${key}/${field}`))
      .data.current_value
  }

  /** Bypass or apply one post-processing filter; `key` is the filter's own path. */
  async setFilterEnabled(deviceId: string, key: string, enabled: boolean): Promise<void> {
    await this.client.put(`/devices/${deviceId}/${key}/enabled`, { value: enabled })
  }

  async getSensorFilters(deviceId: string, sensorIndex: number): Promise<SensorFilters> {
    const response = await this.client.get<Record<string, { enabled: boolean; options: OptionMap }>>(
      `/devices/${deviceId}/sensors/${sensorIndex}/filters/`
    )
    return Object.fromEntries(
      Object.entries(response.data).map(([name, f]) => [name, { ...f, options: visibleOptions(f.options) }])
    )
  }

  async getColorizerOptions(deviceId: string): Promise<OptionInfo[]> {
    const response = await this.client.get<OptionMap>(`/devices/${deviceId}/colorizer/`)
    return visibleOptions(response.data)
  }

  /**
   * Update a camera's firmware through the SDK's calls in turn, as rs-fw-update makes them: check the image,
   * switch to recovery mode, write it, wait for the camera back. One already in recovery mode is written at once.
   * The write's progress comes as firmware_progress events.
   */
  async updateFirmware(deviceId: string, image: Blob, onStep: (phase: FirmwarePhase) => void = () => {}): Promise<void> {
    if (!(await this.isUpdateDevice(deviceId))) {
      onStep('checking')
      if (!(await this.postImage<boolean>(deviceId, 'check_firmware_compatibility', image))) {
        throw new Error('Firmware is not compatible with this device')
      }
      onStep('recovery')
      await this.client.post(`/devices/${deviceId}/firmware/enter_update_state`)
      await this.waitForUpdateDevice(deviceId, true, 'Recovery device did not connect in time')
    }
    onStep('installing')
    await this.postImage(deviceId, 'update', image)
    onStep('restarting')
    await this.waitForUpdateDevice(deviceId, false, 'Original device did not reconnect in time')
  }

  /** Whether the camera is in recovery mode */
  async isUpdateDevice(deviceId: string): Promise<boolean> {
    return (await this.client.get<boolean>(`/devices/${deviceId}/is_update_device`)).data
  }

  /** Until the camera is in (or out of) recovery mode, 60 s as the legacy viewer; gone meanwhile is not yet */
  private async waitForUpdateDevice(deviceId: string, recovery: boolean, error: string): Promise<void> {
    for (const deadline = Date.now() + 60_000; Date.now() < deadline;) {
      await new Promise((resolve) => setTimeout(resolve, 500))
      if ((await this.isUpdateDevice(deviceId).catch(() => undefined)) === recovery) return
    }
    throw new Error(error)
  }

  private async postImage<T>(deviceId: string, step: string, image: Blob): Promise<T> {
    // The body is the image itself
    const headers = { 'Content-Type': 'application/octet-stream' }
    return (await this.client.post<T>(`/devices/${deviceId}/firmware/${step}`, image, { headers })).data
  }

  // ============ Sensors ============

  async getSensors(deviceId: string): Promise<SensorInfo[]> {
    const response = await this.client.get<SensorInfo[]>(`/devices/${deviceId}/sensors/`)
    return response.data
  }

  async getSensorOptions(deviceId: string, sensorIndex: number): Promise<OptionInfo[]> {
    const response = await this.client.get<OptionMap>(`/devices/${deviceId}/sensors/${sensorIndex}/options/`)
    return visibleOptions(response.data)
  }

  // ============ Per-Sensor Streaming (Sensor API) ============

  /** Open the sensor with the profiles, then start it; one that won't start is closed again, as in the legacy viewer */
  async startSensor(
    deviceId: string,
    sensorIndex: number,
    profiles: SupportedStreamProfile[]
  ): Promise<void> {
    const path = `/devices/${deviceId}/sensors/${sensorIndex}`
    await this.client.post(`${path}/open`, { profiles })
    try {
      await this.client.post(`${path}/start`)
    } catch (error) {
      await this.client.post(`${path}/close`)
      throw error
    }
  }

  /** The device's calibration from one stream to another, as the SDK's get_extrinsics_to */
  async getExtrinsics(deviceId: string, from: SupportedStreamProfile, to: SupportedStreamProfile): Promise<Extrinsics> {
    const params = { from_stream: from.stream_type, from_index: from.stream_index,
                     to_stream: to.stream_type, to_index: to.stream_index }
    return (await this.client.get<Extrinsics>(`/devices/${deviceId}/extrinsics`, { params })).data
  }

  /** Stop the sensor, then close it, as the SDK's sensor does */
  async stopSensor(deviceId: string, sensorIndex: number): Promise<void> {
    const path = `/devices/${deviceId}/sensors/${sensorIndex}`
    // Opened but not started (e.g. by another client): stop is refused, close still frees it
    await this.client.post(`${path}/stop`).catch(() => {})
    await this.client.post(`${path}/close`)
  }

  // ============ System ============

  /** Behind a UAC prompt on Windows; declining it rejects */
  async enableMetadata(): Promise<void> {
    await this.client.post('/system/enable_metadata')
  }
}

export const apiClient = new ApiClient()
