import type { SensorInfo, OptionInfo, SupportedStreamProfile } from '@/api/types'

export const mockDepthSensorProfiles: SupportedStreamProfile[] = [
  {
    stream_type: 'Depth',
    resolutions: [[640, 480], [1280, 720], [848, 480]],
    fps: [30, 15, 6],
    formats: ['Z16'],
    default: { resolution: [848, 480], fps: 30, format: 'Z16' },
  },
  {
    stream_type: 'Infrared',
    resolutions: [[640, 480], [1280, 720], [848, 480]],
    fps: [30, 15],
    formats: ['Y8', 'Y16'],
  },
]

export const mockColorSensorProfiles: SupportedStreamProfile[] = [
  {
    stream_type: 'Color',
    resolutions: [[640, 480], [1280, 720], [1920, 1080]],
    fps: [30, 15, 6],
    formats: ['RGB8', 'YUYV', 'BGR8'],
    default: { resolution: [1280, 720], fps: 30, format: 'RGB8' },
  },
]

export const mockMotionSensorProfiles: SupportedStreamProfile[] = [
  {
    stream_type: 'Accel',
    resolutions: [[1, 1]],
    fps: [100, 200, 400],
    formats: ['MOTION_XYZ32F'],
    default: { resolution: [1, 1], fps: 100, format: 'MOTION_XYZ32F' },
  },
  {
    stream_type: 'Gyro',
    resolutions: [[1, 1]],
    fps: [200, 400],
    formats: ['MOTION_XYZ32F'],
    default: { resolution: [1, 1], fps: 200, format: 'MOTION_XYZ32F' },
  },
]

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
