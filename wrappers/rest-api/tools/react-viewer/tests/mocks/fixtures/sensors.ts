import type { SensorInfo, OptionInfo, SupportedStreamProfile } from '@/api/types'

type Spec = [stream_type: string, stream_index: number, formats: string[], resolutions: [number, number][], fps: number[]]

/** Rows for every combination of each spec, as the SDK lists them; `isDefault` marks the SDK defaults. */
function profiles(specs: Spec[], isDefault: (p: SupportedStreamProfile) => boolean): SupportedStreamProfile[] {
  const rows: SupportedStreamProfile[] = []
  for (const [stream_type, stream_index, formats, resolutions, fpsList] of specs)
    for (const format of formats)
      for (const [width, height] of resolutions)
        for (const fps of fpsList) {
          const row = { stream_type, stream_index, format, width, height, fps, default: false }
          rows.push({ ...row, default: isDefault(row) })
        }
  return rows
}

// Infrared has no SDK default, and only y8 exists at 848x480
export const mockDepthSensorProfiles: SupportedStreamProfile[] = profiles([
  ['depth', 0, ['z16'], [[640, 480], [1280, 720], [848, 480]], [30, 15, 6]],
  ['infrared', 1, ['y8'], [[640, 480], [1280, 720], [848, 480]], [30, 15]],
  ['infrared', 1, ['y16'], [[640, 480], [1280, 720]], [30, 15]],
], (p) => p.stream_type === 'depth' && p.width === 848 && p.fps === 30)

export const mockColorSensorProfiles: SupportedStreamProfile[] = profiles([
  ['color', 0, ['rgb8', 'yuyv', 'bgr8'], [[640, 480], [1280, 720], [1920, 1080]], [30, 15, 6]],
], (p) => p.format === 'rgb8' && p.width === 1280 && p.fps === 30)

// Motion profiles have no resolution; the SDK reports 0x0
export const mockMotionSensorProfiles: SupportedStreamProfile[] = profiles([
  ['accel', 0, ['motion_xyz32f'], [[0, 0]], [100, 200]],
  ['gyro', 0, ['motion_xyz32f'], [[0, 0]], [200, 400]],
], (p) => (p.stream_type === 'accel' && p.fps === 100) || (p.stream_type === 'gyro' && p.fps === 200))

export const mockDepthSensor: SensorInfo = {
  sensor_id: '123456789-sensor-0',
  name: 'Stereo Module',
  type: 'depth',
  supported_stream_profiles: mockDepthSensorProfiles,
  options: [],
}

export const mockColorSensor: SensorInfo = {
  sensor_id: '123456789-sensor-1',
  name: 'RGB Camera',
  type: 'color',
  supported_stream_profiles: mockColorSensorProfiles,
  options: [],
}

export const mockMotionSensor: SensorInfo = {
  sensor_id: '123456789-sensor-2',
  name: 'Motion Module',
  type: 'motion',
  supported_stream_profiles: mockMotionSensorProfiles,
  options: [],
}

export const mockSensors: SensorInfo[] = [
  mockDepthSensor,
  mockColorSensor,
  mockMotionSensor,
]

// Sensor options with categories and post-processing
export const mockDepthOptions: OptionInfo[] = [
  {
    option_id: 'exposure',
    description: 'Depth exposure in microseconds',
    current_value: 8500,
    default_value: 8500,
    min_value: 1,
    max_value: 165000,
    step: 1,
    units: 'μs',
    read_only: false,
  },
  {
    option_id: 'gain',
    description: 'UVC image gain',
    current_value: 16,
    default_value: 16,
    min_value: 16,
    max_value: 248,
    step: 1,
    read_only: false,
  },
  {
    option_id: 'laser_power',
    description: 'Manual laser power in mW',
    current_value: 150,
    default_value: 150,
    min_value: 0,
    max_value: 360,
    step: 30,
    units: 'mW',
    read_only: false,
  },
]

// The sensor's post-processing filters, keyed by filter name (GET .../filters/).
export const mockFilters = {
  'Decimation Filter': {
    enabled: false,
    default_enabled: false,
    options: [
      {
        option_id: 'filter_magnitude',
        description: 'Decimation filter magnitude',
        current_value: 2,
        default_value: 2,
        min_value: 2,
        max_value: 8,
        step: 1,
        read_only: false,
        value_descriptions: {
          '2': '2x2 binning',
          '3': '3x3 binning',
          '4': '4x4 binning',
        },
      },
    ],
  },
  'Spatial Filter': { enabled: false, default_enabled: false, options: [] },
  'Temporal Filter': { enabled: false, default_enabled: false, options: [] },
}

export const mockColorOptions: OptionInfo[] = [
  {
    option_id: 'exposure',
    description: 'Color exposure in microseconds',
    current_value: 166,
    default_value: 166,
    min_value: 1,
    max_value: 10000,
    step: 1,
    units: 'μs',
    read_only: false,
  },
  {
    option_id: 'enable_auto_exposure',
    description: 'Enable auto exposure',
    current_value: 1,
    default_value: 1,
    min_value: 0,
    max_value: 1,
    step: 1,
    read_only: false,
  },
]

export const mockMotionOptions: OptionInfo[] = []
