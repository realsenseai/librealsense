import axios, { AxiosInstance } from 'axios'
import { socketService } from './socket'
import { visibleOptions } from './types'
import type {
  AdvancedControls,
  AdvancedModeStatus,
  DeviceInfo,
  SensorFilters,
  SensorInfo,
  OptionInfo,
  WebRTCOffer,
  WebRTCSession,
  ICECandidate,
  SensorStreamConfig,
  SensorStreamStatus,
  ViewerSettings,
  ViewerSettingsPatch,
  RegionOfInterest,
  PlaybackActionName,
  PlaybackStatus,
  RecordStatus,
  RecordingFile,
  PresetFile,
  LogEntry,
  UpdatesReport,
  HdrPreset,
  HdrStatus,
  PointCloudGeometry,
  CalibrationStatus,
  CalibrationTable,
  CalibrationTablePatch,
  OccParams,
  TareParams,
  JobInfo,
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

  /** Download link for the newest frame of a stream: PNG + raw + metadata CSV, zipped. */
  snapshotUrl(deviceId: string, streamType: string): string {
    return `${API_BASE}/devices/${deviceId}/stream/snapshot?stream=${encodeURIComponent(streamType)}`
  }

  async getDepthAtPixel(
    deviceId: string,
    x: number,
    y: number
  ): Promise<{ depth: number | null; x: number; y: number; units: string }> {
    // Mouse-rate query: the open socket answers faster than an HTTP round trip.
    try {
      return await socketService.request('depth_at_pixel', { device_id: deviceId, x, y })
    } catch {
      // socket down: REST below
    }
    const response = await this.client.get<{
      depth: number | null
      x: number
      y: number
      units: string
    }>(`/devices/${deviceId}/stream/depth-at-pixel`, { params: { x, y } })
    return response.data
  }

  async getMaxUsableRange(deviceId: string): Promise<{ supported: boolean; enabled: boolean; range_m: number | null }> {
    const response = await this.client.get(`/devices/${deviceId}/stream/max-usable-range`)
    return response.data
  }

  async getDepthRange(
    deviceId: string
  ): Promise<{ min_depth: number; max_depth: number; units: string }> {
    const response = await this.client.get<{
      min_depth: number
      max_depth: number
      units: string
    }>(`/devices/${deviceId}/stream/depth-range`)
    return response.data
  }

  // ============ Per-Sensor Streaming (Sensor API) ============

  async getSensorStatus(deviceId: string, sensorId: string): Promise<SensorStreamStatus> {
    const response = await this.client.get<SensorStreamStatus>(`/devices/${deviceId}/sensors/${sensorId}/status`)
    return response.data
  }

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

  async getRoi(deviceId: string, sensorId: string): Promise<RegionOfInterest> {
    const response = await this.client.get<RegionOfInterest>(`/devices/${deviceId}/sensors/${sensorId}/roi`)
    return response.data
  }

  /** Auto-exposure region of interest in frame pixels; corners may come in any order. */
  async setRoi(deviceId: string, sensorId: string, roi: { min_x: number; min_y: number; max_x: number; max_y: number }): Promise<RegionOfInterest> {
    const response = await this.client.put<RegionOfInterest>(`/devices/${deviceId}/sensors/${sensorId}/roi`, roi)
    return response.data
  }

  /** Hold back (or release) a streaming sensor's frames; the tile keeps its last image. */
  async setSensorPaused(deviceId: string, sensorId: string, paused: boolean): Promise<SensorStreamStatus> {
    const response = await this.client.post<SensorStreamStatus>(
      `/devices/${deviceId}/sensors/${sensorId}/${paused ? 'pause' : 'resume'}`
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
    await this.client.post(`/devices/${deviceId}/point_cloud/activate`)
  }

  async disablePointCloud(deviceId: string): Promise<void> {
    await this.client.post(`/devices/${deviceId}/point_cloud/deactivate`)
  }

  // ============ WebRTC ============

  async createWebRTCOffer(offer: WebRTCOffer): Promise<WebRTCSession> {
    const response = await this.client.post<WebRTCSession>('/webrtc/offer', offer)
    return response.data
  }

  async sendWebRTCAnswer(sessionId: string, answer: RTCSessionDescriptionInit): Promise<void> {
    await this.client.post('/webrtc/answer', {
      session_id: sessionId,
      sdp: answer.sdp,
      type: answer.type,
    })
  }

  async addICECandidate(sessionId: string, candidate: ICECandidate): Promise<void> {
    await this.client.post('/webrtc/ice-candidates', {
      session_id: sessionId,
      candidate: candidate.candidate,
      sdpMid: candidate.sdpMid,
      sdpMLineIndex: candidate.sdpMLineIndex,
    })
  }

  async getICECandidates(sessionId: string): Promise<ICECandidate[]> {
    const response = await this.client.get<ICECandidate[]>(`/webrtc/sessions/${sessionId}/ice-candidates`)
    return response.data
  }

  async closeWebRTCSession(sessionId: string): Promise<void> {
    await this.client.delete(`/webrtc/sessions/${sessionId}`)
  }

  /** For the page-unload path, which must issue its own keepalive request. */
  webrtcSessionUrl(sessionId: string): string {
    return `${API_BASE}/webrtc/sessions/${sessionId}`
  }

  // ============ Calibration ============

  async getCalibration(deviceId: string): Promise<CalibrationStatus> {
    return (await this.client.get<CalibrationStatus>(`/devices/${deviceId}/calibration/`)).data
  }

  async startOnChipCalibration(deviceId: string, params: OccParams): Promise<JobInfo> {
    return (await this.client.post<JobInfo>(`/devices/${deviceId}/calibration/occ`, params)).data
  }

  async startTareCalibration(deviceId: string, params: TareParams): Promise<JobInfo> {
    return (await this.client.post<JobInfo>(`/devices/${deviceId}/calibration/tare`, params)).data
  }

  async applyCalibration(deviceId: string, useNew: boolean): Promise<CalibrationStatus> {
    return (await this.client.post<CalibrationStatus>(`/devices/${deviceId}/calibration/apply`, { use_new: useNew })).data
  }

  async keepCalibration(deviceId: string): Promise<CalibrationStatus> {
    return (await this.client.post<CalibrationStatus>(`/devices/${deviceId}/calibration/keep`)).data
  }

  async getCalibrationTable(deviceId: string): Promise<CalibrationTable> {
    return (await this.client.get<CalibrationTable>(`/devices/${deviceId}/calibration/table`)).data
  }

  async setCalibrationTable(deviceId: string, patch: CalibrationTablePatch): Promise<CalibrationTable> {
    return (await this.client.put<CalibrationTable>(`/devices/${deviceId}/calibration/table`, patch)).data
  }

  async resetFactoryCalibration(deviceId: string): Promise<CalibrationStatus> {
    return (await this.client.post<CalibrationStatus>(`/devices/${deviceId}/calibration/reset_factory`)).data
  }

  // ============ Point cloud geometry ============

  async exportPointCloud(deviceId: string, options: { mesh: boolean; normals: boolean; binary: boolean }): Promise<Blob> {
    return (await this.client.post<Blob>(`/devices/${deviceId}/point_cloud/export`, options, { responseType: 'blob' })).data
  }

  async getPointCloudGeometry(deviceId: string, texture: string | null): Promise<PointCloudGeometry> {
    return (await this.client.get<PointCloudGeometry>(`/devices/${deviceId}/point_cloud/geometry`, {
      params: { texture: texture ?? '' },
    })).data
  }

  // ============ HDR ============

  async getHdr(deviceId: string): Promise<HdrStatus> {
    return (await this.client.get<HdrStatus>(`/devices/${deviceId}/hdr/`)).data
  }

  async applyHdr(deviceId: string, preset: HdrPreset): Promise<HdrStatus> {
    return (await this.client.put<HdrStatus>(`/devices/${deviceId}/hdr/`, preset)).data
  }

  // ============ Updates ============

  async checkUpdates(deviceId: string): Promise<UpdatesReport> {
    return (await this.client.get<UpdatesReport>(`/updates/${deviceId}`)).data
  }

  // ============ Console ============

  async getLogs(after = 0, limit = 1000): Promise<LogEntry[]> {
    return (await this.client.get<LogEntry[]>('/logs/', { params: { after, limit } })).data
  }

  async clearLogs(): Promise<void> {
    await this.client.delete('/logs/')
  }

  async startFwLogs(deviceId: string): Promise<{ running: boolean; parsed: boolean }> {
    return (await this.client.post(`/devices/${deviceId}/fw_logs/start`)).data
  }

  async stopFwLogs(deviceId: string): Promise<{ running: boolean; parsed: boolean }> {
    return (await this.client.post(`/devices/${deviceId}/fw_logs/stop`)).data
  }

  async recoverFlashLogs(deviceId: string): Promise<{ messages: number }> {
    return (await this.client.post(`/devices/${deviceId}/fw_logs/flash`)).data
  }

  async runTerminal(deviceId: string, line: string): Promise<{ output: string }> {
    return (await this.client.post(`/devices/${deviceId}/terminal`, { line })).data
  }

  async getTerminalCommands(): Promise<string[]> {
    return (await this.client.get<string[]>('/terminal/commands')).data
  }

  // ============ Presets ============

  async listPresets(deviceId: string): Promise<PresetFile[]> {
    return (await this.client.get<PresetFile[]>(`/devices/${deviceId}/presets/`)).data
  }

  /** Download link for the device's current settings as a JSON preset. */
  presetDownloadUrl(deviceId: string): string {
    return `${API_BASE}/devices/${deviceId}/presets/current`
  }

  async loadPresetFile(deviceId: string, path: string): Promise<void> {
    await this.client.post(`/devices/${deviceId}/presets/load`, { path })
  }

  async uploadPreset(deviceId: string, file: File): Promise<void> {
    const form = new FormData()
    form.append('file', file)
    await this.client.post(`/devices/${deviceId}/presets/upload`, form, { headers: { 'Content-Type': undefined as unknown as string } })
  }

  async savePreset(deviceId: string, name: string): Promise<PresetFile[]> {
    return (await this.client.post<PresetFile[]>(`/devices/${deviceId}/presets/save`, { name })).data
  }

  // ============ Record / playback ============

  async getRecordStatus(deviceId: string): Promise<RecordStatus> {
    return (await this.client.get<RecordStatus>(`/devices/${deviceId}/record/`)).data
  }

  async startRecording(deviceId: string, path?: string): Promise<RecordStatus> {
    return (await this.client.post<RecordStatus>(`/devices/${deviceId}/record/start`, path ? { path } : {})).data
  }

  async setRecordingPaused(deviceId: string, paused: boolean): Promise<RecordStatus> {
    return (await this.client.post<RecordStatus>(`/devices/${deviceId}/record/${paused ? 'pause' : 'resume'}`)).data
  }

  async stopRecording(deviceId: string): Promise<RecordStatus> {
    return (await this.client.post<RecordStatus>(`/devices/${deviceId}/record/stop`)).data
  }

  /** Open a recording that already sits on the server. */
  async loadRecording(path: string): Promise<DeviceInfo> {
    return (await this.client.post<DeviceInfo>('/playback/load', { path })).data
  }

  /** Send a recording from the browser to the server's recordings folder and open it. */
  async uploadRecording(file: File): Promise<DeviceInfo> {
    const form = new FormData()
    form.append('file', file)
    return (await this.client.post<DeviceInfo>('/playback/upload', form, { headers: { 'Content-Type': undefined as unknown as string } })).data
  }

  async listRecordings(): Promise<RecordingFile[]> {
    return (await this.client.get<RecordingFile[]>('/playback/files')).data
  }

  async getPlaybackStatus(deviceId: string): Promise<PlaybackStatus> {
    return (await this.client.get<PlaybackStatus>(`/playback/${deviceId}`)).data
  }

  async playbackControl(deviceId: string, action: PlaybackActionName, value?: number): Promise<PlaybackStatus> {
    return (await this.client.post<PlaybackStatus>(`/playback/${deviceId}`, { action, value })).data
  }

  async unloadRecording(deviceId: string): Promise<void> {
    await this.client.delete(`/playback/${deviceId}`)
  }

  // ============ Jobs ============

  async getJobs(deviceId?: string): Promise<JobInfo[]> {
    const response = await this.client.get<JobInfo[]>('/jobs/', { params: { device_id: deviceId } })
    return response.data
  }

  async cancelJob(jobId: string): Promise<JobInfo> {
    const response = await this.client.post<JobInfo>(`/jobs/${jobId}/cancel`)
    return response.data
  }

  // ============ Settings ============

  async getSettings(): Promise<ViewerSettings> {
    const response = await this.client.get<ViewerSettings>('/settings/')
    return response.data
  }

  /** Merge a partial update server-side; the whole resulting settings come back. */
  async updateSettings(patch: ViewerSettingsPatch): Promise<ViewerSettings> {
    const response = await this.client.put<ViewerSettings>('/settings/', patch)
    return response.data
  }

  // ============ System ============

  async enableMetadata(): Promise<{ status: string; note?: string }> {
    const response = await this.client.post<{ status: string; note?: string }>('/system/enable-metadata')
    return response.data
  }
}

export const apiClient = new ApiClient()
