// API Types for RealSense REST API

export interface DeviceInfo {
  device_id: string
  name: string
  serial_number: string
  firmware_version?: string
  physical_port?: string
  usb_type?: string
  product_id?: string
  sensors: string[]
  is_streaming: boolean
  metadata_enabled?: boolean | null
  /** Every RS2_CAMERA_INFO field the device reports, by field name. */
  info?: Record<string, string>
  is_playback?: boolean
  file_name?: string | null
}

// Wire shape of /playback/{id} (app/models/playback.py)
export type PlaybackState = 'unknown' | 'playing' | 'paused' | 'stopped'
export interface PlaybackStatus {
  device_id: string
  file_name: string
  state: PlaybackState
  position_ns: number
  duration_ns: number
  speed: number
  repeat: boolean
}
export type PlaybackActionName = 'play' | 'pause' | 'stop' | 'seek' | 'speed' | 'step' | 'repeat'

// Wire shape of /devices/{id}/record
export interface RecordStatus {
  device_id: string
  recording: boolean
  paused: boolean
  file: string | null
}

export interface RecordingFile {
  path: string
  name: string
  size: number
  modified: number
}

/** Display label for an SDK name: "deepSeaMedianThreshold" -> "Deep Sea Median Threshold". */
export function optionLabel(id: string): string {
  return id
    .replace(/([a-z0-9])([A-Z])/g, '$1 $2')
    .replace(/_/g, ' ')
    .replace(/\b\w/g, (c) => c.toUpperCase())
}

/**
 * Options that are plumbing rather than controls - the set the legacy viewer hides in
 * viewer_model::hide_common_options(). What to draw is the viewer's call, so everything
 * the API reports passes through here.
 */
const HIDDEN_OPTIONS = [
  'frames_queue_size', 'stream_filter', 'stream_format_filter', 'stream_index_filter',
  'noise_estimation', 'region_of_interest', 'readout_shaping', 'sensors_config_mode',
]

export function visibleOptions(options: OptionInfo[]): OptionInfo[] {
  return options.filter((o) => !HIDDEN_OPTIONS.includes(o.option_id.toLowerCase()))
}

// One output-console line (GET /logs/, `log_batch` Socket.IO event)
export interface LogEntry {
  id: number
  ts: number  // server clock, seconds
  severity: string  // debug | info | warn | error | fatal (firmware: its own words, lower-cased)
  message: string
  source: string  // sdk | server | fw | fw-flash | terminal
  file?: string | null
  line?: number | null
  device_id?: string
  command?: string
  thread?: string
  module?: string | null
}

// Calibration session (GET /devices/{d}/calibration/), a port of common/on-chip-calib.cpp
export interface CalibrationStatus {
  kind: 'occ' | 'tare' | null
  state: 'idle' | 'running' | 'done' | 'failed'
  health: number[] | null
  verdict: 'good' | 'ok' | 'bad' | 'unknown' | null
  has_new_table: boolean
  active: 'old' | 'new'
  written: boolean
  error: string | null
  started_at: number | null
}
export interface OccParams {
  speed: number // 0 very fast .. 3 slow, 4 white wall
  average_step_count: number
  step_count: number
  accuracy: number // 0 very high .. 3 low
  apply_preset: boolean
  intrinsic_scan: boolean
  host_assistance: boolean
}
export interface TareParams {
  ground_truth_mm: number
  average_step_count: number
  step_count: number
  accuracy: number
  apply_preset: boolean
  host_assistance: boolean
}

// Camera geometry for the client-side point cloud (GET /devices/{d}/point_cloud/geometry)
export interface CameraIntrinsics {
  width: number
  height: number
  fx: number
  fy: number
  ppx: number
  ppy: number
  model: string // 'brown_conrady' | 'inverse_brown_conrady' | 'modified_brown_conrady' | ...
  coeffs: number[]
}
export interface Extrinsics {
  rotation: number[] // 3x3, column-major (rs2_extrinsics)
  translation: number[]
}
export interface PointCloudGeometry {
  depth: (CameraIntrinsics & { stream: string; units: number }) | null
  texture: (CameraIntrinsics & { stream: string; extrinsics: Extrinsics }) | null
}
// The binary `depth_frame` Socket.IO event
export interface DepthFrameEvent {
  device_id: string
  width: number
  height: number
  frame_number: number
  units: number
  format: string
  data: ArrayBuffer | Uint8Array
}

// HDR sequence editor (app/services/hdr.py, a port of common/hdr-model.*)
export interface HdrControls {
  depth_gain: number
  depth_exp: number
  delta_gain: number
  delta_exp: number
}
export interface HdrItem {
  iterations: number
  controls: HdrControls
}
export interface HdrPreset {
  id: string
  iterations: number
  control_type_auto: boolean
  items: HdrItem[]
}
export interface OptionRangeInfo {
  min: number
  max: number
  step: number
  default: number
}
export interface HdrStatus {
  supported: boolean
  preset: HdrPreset | null
  exposure_range: OptionRangeInfo | null
  gain_range: OptionRangeInfo | null
  hdr_enabled: boolean | null
}

// Wire shape of GET /updates/{d} (app/services/updates.py)
export interface UpdateCandidate {
  version: string
  link?: string | null
  release_notes?: string | null
  description?: string | null
}
export interface UpdateSection {
  current: string | null
  essential: UpdateCandidate | null
  recommended: UpdateCandidate | null
  verdict: 'unknown' | 'up_to_date' | 'recommended' | 'essential'
}
export interface UpdatesReport {
  source: string
  reachable: boolean
  firmware: UpdateSection
  software: UpdateSection
}

// The `notification` Socket.IO event: an SDK notification from a sensor
export interface SdkNotification {
  device_id: string
  sensor_id: string
  category: string
  severity: string
  description: string
  serialized_data: string
  timestamp: number
}

// A preset file in the server's presets folder (GET /devices/{d}/presets/)
export interface PresetFile {
  path: string
  name: string
}

// Wire shape of /devices/{d}/sensors/{s}/roi
export interface RegionOfInterest {
  supported: boolean
  min_x?: number
  min_y?: number
  max_x?: number
  max_y?: number
}

// Wire shape of /jobs/ and the `job` Socket.IO event (app/models/job.py)
export type JobState = 'running' | 'done' | 'failed' | 'cancelled'
export interface JobInfo {
  id: string
  kind: string
  device_id: string | null
  state: JobState
  progress: number  // 0..1
  message: string | null
  result: unknown
  error: string | null
  created_at: number
  updated_at: number
}

// Wire shape of GET/PUT /settings/ (app/models/settings.py)
export interface ViewerSettings {
  record: { file_save_mode: 'auto' | 'ask'; default_path: string; compression: 'auto' | 'always' | 'never' }
  update: { sw_update_official_server: boolean; sw_update_url: string; recommend_calibration: boolean }
  console: { max_entries: number; log_to_file: boolean; log_filename: string; log_severity: 'debug' | 'info' | 'warn' | 'error' }
  paths: { hwlogger_xml: string; commands_xml: string; presets_folder: string }
  context: { dds_enabled: boolean; dds_domain: number }
  calibration: { enable_writing: boolean }
  post_processing: { performance_mode: boolean }
  viewer: {
    metric_system: boolean
    grid_horizontal_lines: number
    grid_vertical_lines: number
    grid_line_width: number
    grid_line_color: string
  }
}

/** Any subset of the settings groups, each with any subset of its keys. */
export type ViewerSettingsPatch = { [G in keyof ViewerSettings]?: Partial<ViewerSettings[G]> }

export type FirmwareStatus = 'up_to_date' | 'outdated' | 'unknown'

/** Numeric compare of dotted firmware versions. */
export function firmwareStatus(current?: string, recommended?: string): FirmwareStatus {
  const parse = (v?: string) => v?.split('.').map(Number)
  const [cur, rec] = [parse(current), parse(recommended)]
  if (!cur || !rec || cur.some(isNaN) || rec.some(isNaN)) return 'unknown'
  for (let i = 0; i < Math.max(cur.length, rec.length); i++) {
    if ((cur[i] ?? 0) !== (rec[i] ?? 0)) return (cur[i] ?? 0) < (rec[i] ?? 0) ? 'outdated' : 'up_to_date'
  }
  return 'up_to_date'
}

// No verdict stored: it would go stale as soon as the camera reports a different version.
export interface FirmwareState {
  recommended?: string
  is_updating?: boolean
  phase?: 'downloading' | 'installing'  // one-click update: download then install
  progress?: number
  last_error?: string | null
}

// Wire shape of GET/POST /devices/{id}/advanced_mode/
export interface AdvancedModeStatus {
  supported: boolean
  enabled: boolean
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
  /** The profile the SDK marks default for this stream, when it has one. */
  default?: { resolution: [number, number]; fps: number; format: string }
  /** Every exact (width, height, fps, format) the SDK lists. */
  modes?: [number, number, number, string][]
}

export interface OptionInfo {
  option_id: string
  description?: string
  current_value: number | boolean | string
  default_value: number | boolean | string
  min_value: number
  max_value: number
  step?: number
  units?: string
  read_only: boolean
  value_descriptions?: Record<string, string>  // For enum-type options: {value: description}
}

// A sensor's post-processing filters, keyed by name. Filters share option names
// (holes_fill lives on three of them), so the key is what tells those controls apart.
export type SensorFilters = Record<string, {
  enabled: boolean
  default_enabled: boolean
  options: OptionInfo[]
}>

/** Advanced-mode controls, keyed by the control group that owns them. */
export type AdvancedControls = Record<string, OptionInfo[]>

/** The panels the viewer draws, in the order the C++ viewer uses (device-model.cpp). */
export const SECTIONS = ['Controls', 'Advanced Controls', 'Depth Visualization', 'Post-Processing'] as const

/**
 * One list of controls with one endpoint behind it, keyed by that endpoint's path under
 * the device: the four control sources differ only in their path, not in their shape.
 */
export interface ControlGroup {
  section: typeof SECTIONS[number]
  sensorId: string    // the sensor it is drawn under, which for a device-level group
                      // (the colorizer, advanced mode) is the depth sensor
  name: string        // '' when the section draws its controls without a subheader
  enabled?: boolean   // filters only: whether the frame passes through it
  default_enabled?: boolean
  options: OptionInfo[]
}

export interface StreamConfig {
  sensor_id: string
  stream_type: string
  format: string
  resolution: { width: number; height: number }
  framerate: number
  enable: boolean
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
  received_at?: number  // server clock, seconds; compare with MetadataUpdate.timestamp_server
  timestamp: number
  frame_number: number
  // frame dims after post processing
  width: number
  height: number
  motion_data?: IMUData
  point_cloud?: PointCloudData
  frame_metadata?: Record<string, number>
  clock_domain?: string
  hardware_fps?: number
  pixel_format?: string
  // frame dims as received from camera
  hardware_width?: number
  hardware_height?: number
}

export interface IMUData {
  x: number
  y: number
  z: number
}

export interface PointCloudData {
  // Raw float32 bytes (Socket.IO binary attachment) or base64-encoded string (legacy server).
  vertices: ArrayBuffer | string
  texture_coordinates: number[]
  // Per-vertex RGB triplets (uint8, 3 bytes per vertex), matching `vertices` 1:1
  // when the server textured the cloud from a live color frame. Same wire
  // encoding as vertices: ArrayBuffer over binary socket, base64 string otherwise.
  colors?: ArrayBuffer | string
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
  isMotionSensor?: boolean // Motion sensors use per-stream FPS instead of shared
  // Set when the sensor's streams share no resolution / no frame rate (legacy: depth and IR
  // at different sizes, or no common FPS), so each stream picks its own.
  perStreamResolution?: boolean
  perStreamFps?: boolean
}

// Per-device state for multi-camera support
export interface DeviceState {
  device: DeviceInfo
  firmware?: FirmwareState
  advancedMode?: AdvancedModeStatus
  // Every control the device shows, keyed by the endpoint that writes it.
  controls: Record<string, ControlGroup>
  /**
   * Whether each sensor runs its post-processing at all, keyed by sensor_id, on unless
   * set. Neither the SDK nor the API has such a switch - it is the viewer's, as in the
   * legacy one (subdevice-model.h), and it leaves the per-filter choices alone.
   */
  postProcessing?: Record<string, boolean>
  sensors: SensorInfo[]
  streamConfigs: StreamConfig[]
  sensorConfigs: Record<string, SensorConfig> // Per-sensor resolution/FPS, keyed by sensor_id
  isStreaming: boolean
  isLoading: boolean // loading sensors/options
  streamMetadata: Record<string, StreamMetadata> // keyed by stream_type
  metadataServerTime?: number // timestamp_server of the last metadata_update
  record?: RecordStatus
  playback?: PlaybackStatus // loaded recordings only
  presetFiles?: PresetFile[] // JSON presets in the server's folder for this model
  // Per-sensor streaming state (sensor API)
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

export interface SensorStreamStatus {
  sensor_id: string
  name: string
  is_streaming: boolean
  paused?: boolean
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
