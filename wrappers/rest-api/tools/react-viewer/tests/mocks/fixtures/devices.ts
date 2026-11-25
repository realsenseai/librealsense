import type { DeviceInfo, DeviceState, StreamConfig } from '@/api/types'
import { mockSensors } from './sensors'

export const mockDevice: DeviceInfo = {
  device_id: '123456789',
  info: {
    name: 'RealSense D435',
    serial_number: '123456789',
    firmware_update_id: '123456789',
    firmware_version: '5.16.0.1',
    physical_port: '2-3',
    usb_type_descriptor: '3.2',
    product_id: '0B07',
  },
}

export const mockDevice2: DeviceInfo = {
  device_id: '987654321',
  info: {
    name: 'RealSense D455',
    serial_number: '987654321',
    firmware_update_id: '987654321',
    firmware_version: '5.15.0.0',
    physical_port: '2-4',
    usb_type_descriptor: '3.2',
    product_id: '0B5C',
  },
}

export const mockStreamConfigs: StreamConfig[] = [
  {
    sensor_index: 0,
    stream_type: 'Depth',
    format: 'Z16',
    resolution: { width: 640, height: 480 },
    framerate: 30,
    enable: true,
  },
  {
    sensor_index: 0,
    stream_type: 'Infrared',
    format: 'Y8',
    resolution: { width: 640, height: 480 },
    framerate: 30,
    enable: false,
  },
  {
    sensor_index: 1,
    stream_type: 'Color',
    format: 'RGB8',
    resolution: { width: 640, height: 480 },
    framerate: 30,
    enable: false,
  },
  {
    sensor_index: 2,
    stream_type: 'Accel',
    format: 'MOTION_XYZ32F',
    resolution: { width: 1, height: 1 },
    framerate: 200,
    enable: false,
  },
  {
    sensor_index: 2,
    stream_type: 'Gyro',
    format: 'MOTION_XYZ32F',
    resolution: { width: 1, height: 1 },
    framerate: 200,
    enable: false,
  },
]

export const mockDeviceState: DeviceState = {
  device: mockDevice,
  firmware: {
    recommended: '5.16.0.1',
    is_updating: false,
    progress: undefined,
    last_error: null,
  },
  sensors: mockSensors,
  options: {},
  streamConfigs: mockStreamConfigs,
  isLoading: false,
  sensorStreamingStatus: {},
}

export const mockDeviceStates: Record<string, DeviceState> = {
  [mockDevice.device_id]: mockDeviceState,
}

export const mockDeviceList: DeviceInfo[] = [mockDevice, mockDevice2]

