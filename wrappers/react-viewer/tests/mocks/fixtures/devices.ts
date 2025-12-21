import { DeviceInfo, DeviceState } from '@/api/types'

export const mockDevice: DeviceInfo = {
  device_id: '123456789',
  name: 'Intel RealSense D435',
  serial_number: '123456789',
  firmware_version: '5.12.0.0',
  product_line: 'D400',
  usb_type: '3.2',
  physical_port: '2-3',
}

export const mockDevice2: DeviceInfo = {
  device_id: '987654321',
  name: 'Intel RealSense D455',
  serial_number: '987654321',
  firmware_version: '5.13.0.0',
  product_line: 'D400',
  usb_type: '3.2',
  physical_port: '2-4',
}

export const mockDeviceState: DeviceState = {
  device: mockDevice,
  isActive: true,
  isStreaming: true,
  isLoading: false,
  isStopping: false,
  streamConfigs: [
    {
      sensor_id: 0,
      stream_type: 'depth',
      width: 640,
      height: 480,
      fps: 30,
      format: 'Z16',
      enable: true,
    },
    {
      sensor_id: 0,
      stream_type: 'color',
      width: 640,
      height: 480,
      fps: 30,
      format: 'RGB8',
      enable: false,
    },
  ],
  streamMetadata: {
    depth: {
      timestamp: Date.now(),
      frame_number: 100,
      width: 640,
      height: 480,
    },
  },
  align_to: null,
  availableStreams: [],
  sensors: [],
}

export const mockDeviceStates = {
  [mockDevice.device_id]: mockDeviceState,
}

export const mockDeviceList: DeviceInfo[] = [mockDevice, mockDevice2]
