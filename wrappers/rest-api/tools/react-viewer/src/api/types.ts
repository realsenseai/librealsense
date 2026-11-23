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

/** The options to draw: as the legacy viewer, none it can't read now */
export function visibleOptions(options: OptionMap): OptionInfo[] {
  return Object.entries(options)
    .filter(([optionId, o]) => o.current_value !== null && !HIDDEN_OPTIONS.includes(optionId.toLowerCase()))
    .map(([optionId, o]) => ({ option_id: optionId, ...o, current_value: o.current_value! }))
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
  sensor_index: number
  name: string
  stream_profiles: SupportedStreamProfile[]
  active_streams: SupportedStreamProfile[] // what it is opened with, by any client, from open to close
}

// One SDK stream profile, as listed; /sensors/{id}/open takes these back unchanged
export interface SupportedStreamProfile {
  stream_type: string
  stream_index: number
  format: string
  width: number // 0 for motion, as the SDK reports it
  height: number
  fps: number
  default: boolean
}

/** The viewer's key for a profile's stream ("depth", "infrared-1"), as stream configs and frame lookups use it. */
export const streamKey = (p: SupportedStreamProfile) =>
  p.stream_type === 'infrared' ? `infrared-${p.stream_index}` : p.stream_type

/** A sensor's profiles of one stream at a resolution/fps. */
export function profilesAt(
  sensor: SensorInfo, stream: string, resolution: { width: number; height: number }, fps: number
): SupportedStreamProfile[] {
  return sensor.stream_profiles.filter(p => streamKey(p) === stream && p.fps === fps &&
    p.width === resolution.width && p.height === resolution.height)
}

// Wire shapes: the API keys each option by its ID
// current_value null: one the device supports but can't read now (a temperature while not streaming)
export type WireOption = Omit<Required<Omit<OptionInfo, 'option_id'>>, 'current_value'> & { current_value: OptionInfo['current_value'] | null }
export type OptionMap = Record<string, WireOption>

/** An advanced-mode field as the API sends it: the SDK gives these only a value, a minimum and a maximum */
export interface WireAdvancedControl {
  current_value: number
  min_value: number
  max_value: number
  integer: boolean // an int field, which JSON can't tell from a float
}

export interface OptionInfo {
  option_id: string  // the key it came under
  description?: string  // these four: none for advanced-mode controls
  default_value?: number | boolean | string
  step?: number
  value_descriptions?: Record<string, string>  // each value's label, as far as the SDK has them
  current_value: number | boolean | string
  min_value: number
  max_value: number
  read_only: boolean
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
  sensorIndex?: number   // the sensor it is drawn under, which for a device-level group
                      // (the colorizer, advanced mode) is the depth sensor
  name: string        // '' when the section draws its controls without a subheader
  enabled?: boolean   // filters only: whether the frame passes through it
  default_enabled?: boolean // filters only: the legacy viewer's starting state, as the viewer starts them
  options: OptionInfo[]
}

export interface StreamConfig {
  sensor_index: number
  stream_type: string
  format: string
  resolution: { width: number; height: number }
  framerate: number
  enable: boolean
}

// UI State types
export type ViewMode = '2d' | '3d'

// Per-device state for multi-camera support
export interface DeviceState {
  device: DeviceInfo
  firmware?: FirmwareState
  advancedMode?: AdvancedModeStatus
  // Every control the device shows, keyed by the endpoint that writes it.
  controls: Record<string, ControlGroup>
  /**
   * Whether each sensor runs its post-processing at all, keyed by sensor_index, on unless
   * set. Neither the SDK nor the API has such a switch - it is the viewer's, as in the
   * legacy one (subdevice-model.h), and it leaves the per-filter choices alone.
   */
  postProcessing?: Record<number, boolean>
  sensors: SensorInfo[]
  streamConfigs: StreamConfig[]
  isLoading: boolean // loading sensors/options
  // Per-sensor streaming state (sensor API)
  sensorStreamingStatus: Record<number, SensorStreamStatus> // keyed by sensor_index
  extrinsics?: StreamExtrinsics[] // the device's calibration between its streams
}

/** Whether any of its sensors streams */
export const isStreaming = (ds: DeviceState) =>
  Object.values(ds.sensorStreamingStatus).some(s => s.is_streaming)

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
  sensor_index: number
  is_streaming: boolean
  profiles?: SupportedStreamProfile[]  // what it streams now, started by any client
  error?: string | null
  // UI-only: pending operation state for optimistic updates
  pendingOp?: 'stopping' | null
  startedHere?: boolean // this page started it, so this page stops it when the device is hidden
}
