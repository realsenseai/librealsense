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
  metadata_enabled?: boolean | null
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
  active_profiles: SupportedStreamProfile[] // what it streams now, started by any client; empty when stopped
  options: OptionInfo[]
}

// One SDK stream profile, as listed; /sensors/{id}/start takes these back unchanged
export interface SupportedStreamProfile {
  stream_type: string
  stream_index: number
  format: string
  width: number // 0 for motion, as the SDK reports it
  height: number
  fps: number
  default: boolean
}

/** The backend's name for a profile's stream ("depth", "infrared-1"), as stream configs and frame lookups use it. */
export const streamKey = (p: SupportedStreamProfile) =>
  p.stream_type === 'infrared' ? `infrared-${p.stream_index}` : p.stream_type

/** A sensor's profiles of one stream at a resolution/fps. */
export function profilesAt(
  sensor: SensorInfo, stream: string, resolution: { width: number; height: number }, fps: number
): SupportedStreamProfile[] {
  return sensor.supported_stream_profiles.filter(p => streamKey(p) === stream && p.fps === fps &&
    p.width === resolution.width && p.height === resolution.height)
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
  default_enabled?: boolean // filters only: the legacy viewer's starting state, as the viewer starts them
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
  isActive: boolean // whether this device is shown in viewer
  isLoading: boolean // loading sensors/options
  // Per-sensor streaming state (sensor API)
  sensorStreamingStatus: Record<string, SensorStreamStatus> // keyed by sensor_id
  extrinsics?: StreamExtrinsics[] // the device's calibration between its streams
}

/** As rs2_intrinsics; model is an rs2_distortion name such as "brown_conrady" */
export interface Intrinsics {
  width: number
  height: number
  ppx: number
  ppy: number
  fx: number
  fy: number
  model: string
  coeffs: number[]
}

/** As rs2_extrinsics: rotation is 3x3 column-major, translation in meters */
export interface Extrinsics {
  rotation: number[]
  translation: number[]
}

/** From a stream to each stream of its device it is calibrated with */
export interface StreamExtrinsics {
  stream_type: string
  stream_index: number
  to: (Extrinsics & { stream_type: string; stream_index: number })[]
}

// Per-sensor streaming state, kept by the viewer
export interface SensorStreamStatus {
  sensor_id: string
  is_streaming: boolean
  profiles?: SupportedStreamProfile[]  // what it streams now, started by any client
  error?: string | null
  // UI-only: pending operation state for optimistic updates
  pendingOp?: 'stopping' | null
  startedHere?: boolean // this page started it, so this page stops it when the device is hidden
}
