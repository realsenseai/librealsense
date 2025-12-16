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
}

export interface StreamStatus {
  is_streaming: boolean
  active_streams: string[]
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

// Per-device state for multi-camera support
export interface DeviceState {
  device: DeviceInfo
  firmware?: FirmwareState
  sensors: SensorInfo[]
  options: Record<string, OptionInfo[]> // keyed by sensor_id
  streamConfigs: StreamConfig[]
  isStreaming: boolean
  isActive: boolean // whether this device is shown in viewer
  isLoading: boolean // loading sensors/options
  streamMetadata: Record<string, StreamMetadata> // keyed by stream_type
}
