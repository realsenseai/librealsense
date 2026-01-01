// API Types for RealSense REST API

export interface DeviceInfo {
  device_id: string
  name: string
  serial_number: string
  firmware_version?: string
  recommended_firmware_version?: string
  firmware_status?: FirmwareStatus
  firmware_file_available?: boolean
  physical_port?: string
  usb_type?: string
  product_id?: string
  sensors: string[]
  is_streaming: boolean
}

export type FirmwareStatus = 'up_to_date' | 'outdated' | 'missing_file' | 'unknown'

export interface FirmwareState {
  current?: string
  recommended?: string
  status: FirmwareStatus
  file_available?: boolean
  is_updating?: boolean
  progress?: number
  last_error?: string | null
}

export interface SensorInfo {
  sensor_id: string
  name: string
  type: string
  supported_stream_profiles: SupportedStreamProfile[]
  options: OptionInfo[]
}

export interface SupportedStreamProfile {
  stream_type: string
  resolutions: [number, number][]
  fps: number[]
  formats: string[]
}

export interface OptionInfo {
  option_id: string
  name: string
  description?: string
  current_value: number | boolean | string
  default_value: number | boolean | string
  min_value?: number
  max_value?: number
  step?: number
  units?: string
  read_only: boolean
}

export interface StreamConfig {
  sensor_id: string
  stream_type: string
  format: string
  resolution: { width: number; height: number }
  framerate: number
  enable: boolean
}

export interface StreamStartRequest {
  configs: StreamConfig[]
  align_to?: string
  apply_filters: boolean
  reuse_cache?: boolean
}

export interface StreamStatus {
  device_id?: string
  is_streaming: boolean
  active_streams: string[]
  stopping?: boolean
}

export interface WebRTCOffer {
  device_id: string
  stream_types: string[]
}

export interface WebRTCSession {
  session_id: string
  sdp: string
  type: string
}

export interface ICECandidate {
  candidate: string
  sdpMid: string
  sdpMLineIndex: number
}

// Metadata from Socket.IO
export interface StreamMetadata {
  stream_type: string
  timestamp: number
  frame_number: number
  width: number
  height: number
  motion_data?: IMUData
  point_cloud?: PointCloudData
}

export interface IMUData {
  x: number
  y: number
  z: number
}

export interface PointCloudData {
  vertices: string // Base64-encoded Float32Array
  texture_coordinates: number[]
}

export interface MetadataUpdate {
  device_id: string
  is_streaming: boolean
  timestamp_server: number
  metadata_streams: Record<string, StreamMetadata>
}

// UI State types
export type ViewMode = '2d' | '3d'

export interface StreamLayout {
  id: string
  streamType: string
  position: { x: number; y: number }
  size: { width: number; height: number }
}

// Per-sensor configuration (resolution/FPS shared across all streams from same sensor)
export interface SensorConfig {
  resolution: { width: number; height: number }
  framerate: number
}

// Per-device state for multi-camera support
export interface DeviceState {
  device: DeviceInfo
  firmware?: FirmwareState
  sensors: SensorInfo[]
  options: Record<string, OptionInfo[]> // keyed by sensor_id
  streamConfigs: StreamConfig[]
  sensorConfigs: Record<string, SensorConfig> // Per-sensor resolution/FPS, keyed by sensor_id
  isStreaming: boolean
  isStopping?: boolean
  isActive: boolean // whether this device is shown in viewer
  isLoading: boolean // loading sensors/options
  streamMetadata: Record<string, StreamMetadata> // keyed by stream_type
  // Per-sensor streaming state (sensor API)
  streamingMode: 'idle' | 'pipeline' | 'sensor' // which API is being used
  sensorStreamingStatus: Record<string, SensorStreamStatus> // keyed by sensor_id
}

// Per-sensor streaming types (for sensor API)
export interface SensorStreamConfig {
  stream_type: string
  format: string
  resolution: { width: number; height: number }
  framerate: number
}

export interface SensorStartRequest {
  config: SensorStreamConfig
}

export interface SensorStartItem {
  sensor_id: string
  config: SensorStreamConfig
}

export interface BatchSensorStartRequest {
  sensors: SensorStartItem[]
}

export interface BatchSensorStopRequest {
  sensor_ids?: string[] | null
}

export interface SensorStreamStatus {
  sensor_id: string
  name: string
  is_streaming: boolean
  // Single stream_type for backward compatibility (first stream)
  stream_type?: string | null
  resolution?: { width: number; height: number } | null
  framerate?: number | null
  format?: string | null
  // New: multiple streams support
  stream_types?: string[]  // All active stream types
  streams?: SensorStreamConfig[]  // All active stream configs
  error?: string | null
  started_at?: string | null
  // UI-only: pending operation state for optimistic updates
  pendingOp?: 'stopping' | null
}

export interface BatchSensorStatus {
  device_id: string
  mode: 'idle' | 'pipeline' | 'sensor'
  sensors: SensorStreamStatus[]
  errors: string[]
}
