import type { DeviceInfo, DeviceState, StreamConfig, SensorConfig } from '@/api/types'
import { mockSensors } from './sensors'

export const mockDevice: DeviceInfo = {
  device_id: '123456789',
  name: 'Intel RealSense D435',
  serial_number: '123456789',
  firmware_version: '5.16.0.1',
  recommended_firmware_version: '5.16.0.1',
  firmware_status: 'up_to_date',
  firmware_file_available: true,
  physical_port: '2-3',
  usb_type: '3.2',
  product_id: '0B07',
  sensors: ['Stereo Module', 'RGB Camera', 'Motion Module'],
  is_streaming: false,
}

export const mockDevice2: DeviceInfo = {
  device_id: '987654321',
  name: 'Intel RealSense D455',
  serial_number: '987654321',
  firmware_version: '5.15.0.0',
  recommended_firmware_version: '5.16.0.1',
  firmware_status: 'outdated',
  firmware_file_available: true,
  physical_port: '2-4',
  usb_type: '3.2',
  product_id: '0B5C',
  sensors: ['Stereo Module', 'RGB Camera', 'Motion Module'],
  is_streaming: false,
}

export const mockStreamConfigs: StreamConfig[] = [
  {
    sensor_id: '123456789-sensor-0',
    stream_type: 'Depth',
    format: 'Z16',
    resolution: { width: 640, height: 480 },
    framerate: 30,
    enable: true,
  },
  {
    sensor_id: '123456789-sensor-0',
    stream_type: 'Infrared',
    format: 'Y8',
    resolution: { width: 640, height: 480 },
    framerate: 30,
    enable: false,
  },
  {
    sensor_id: '123456789-sensor-1',
    stream_type: 'Color',
    format: 'RGB8',
    resolution: { width: 640, height: 480 },
    framerate: 30,
    enable: false,
  },
  {
    sensor_id: '123456789-sensor-2',
    stream_type: 'Accel',
    format: 'MOTION_XYZ32F',
    resolution: { width: 1, height: 1 },
    framerate: 200,
    enable: false,
  },
  {
    sensor_id: '123456789-sensor-2',
    stream_type: 'Gyro',
    format: 'MOTION_XYZ32F',
    resolution: { width: 1, height: 1 },
    framerate: 200,
    enable: false,
  },
]

export const mockSensorConfigs: Record<string, SensorConfig> = {
  '123456789-sensor-0': {
    resolution: { width: 640, height: 480 },
    framerate: 30,
    isMotionSensor: false,
  },
  '123456789-sensor-1': {
    resolution: { width: 640, height: 480 },
    framerate: 30,
    isMotionSensor: false,
  },
  '123456789-sensor-2': {
    resolution: { width: 1, height: 1 },
    framerate: 200,
    isMotionSensor: true,
  },
}

export const mockDeviceState: DeviceState = {
  device: mockDevice,
  firmware: {
    current: '5.16.0.1',
    recommended: '5.16.0.1',
    status: 'up_to_date',
    file_available: true,
    is_updating: false,
    progress: undefined,
    last_error: null,
  },
  sensors: mockSensors,
  options: {},
  streamConfigs: mockStreamConfigs,
  sensorConfigs: mockSensorConfigs,
  isStreaming: false,
  isStopping: false,
  isActive: true,
  isLoading: false,
  streamMetadata: {},
  streamingMode: 'idle',
  sensorStreamingStatus: {},
}

export const mockDeviceStateInactive: DeviceState = {
  ...mockDeviceState,
  isActive: false,
}

export const mockDeviceStateStreaming: DeviceState = {
  ...mockDeviceState,
  isStreaming: true,
  streamingMode: 'pipeline',
  streamMetadata: {
    depth: {
      stream_type: 'depth',
      timestamp: Date.now(),
      frame_number: 100,
      width: 640,
      height: 480,
    },
  },
}

export const mockDeviceStates: Record<string, DeviceState> = {
  [mockDevice.device_id]: mockDeviceState,
}

export const mockDeviceList: DeviceInfo[] = [mockDevice, mockDevice2]

