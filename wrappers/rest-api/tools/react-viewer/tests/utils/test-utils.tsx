import { ReactElement, ReactNode } from 'react'
import { render, RenderOptions } from '@testing-library/react'
import { useAppStore } from '@/store'
import type { DeviceInfo, SensorInfo, OptionInfo, StreamConfig, DeviceState } from '@/api/types'

/**
 * Reset the store to initial state before each test
 */
export function resetStore() {
  useAppStore.setState({
    isConnected: false,
    devices: [],
    deviceStates: {},
    selectedDevice: null,
    isLoadingDevices: false,
    hasUserInteracted: false,
    error: null,
    viewMode: '2d',
    isChatOpen: false,
    isChatAvailable: false,
    isChatLoading: false,
    chatMessages: [],
    pendingSettings: null,
    imuHistory: { accel: [], gyro: [] },
    isIMUViewerExpanded: false,
  })
}

/**
 * Create a mock DeviceInfo object
 */
export function createMockDevice(overrides: Partial<DeviceInfo> = {}): DeviceInfo {
  return {
    device_id: 'test-device-1',
    name: 'Intel RealSense D435',
    serial_number: '123456789',
    firmware_version: '5.16.0.1',
    usb_type: '3.2',
    product_line: 'D400',
    recommended_firmware_version: '5.16.0.1',
    firmware_status: 'up_to_date',
    firmware_file_available: true,
    sensors: ['sensor-0', 'sensor-1', 'sensor-2'],
    ...overrides,
  } as DeviceInfo
}

/**
 * Create a mock DeviceState object
 */
export function createMockDeviceState(device: DeviceInfo, overrides: Partial<DeviceState> = {}): DeviceState {
  return {
    device,
    sensors: [],
    options: {},
    streamConfigs: [],
    sensorConfigs: {},
    isActive: false,
    isStreaming: false,
    streamingMode: 'idle',
    sensorStreamingStatus: {},
    isPointCloudEnabled: false,
    pointCloudVertices: null,
    streamMetadata: {},
    latestMetadata: null,
    ...overrides,
  }
}

/**
 * Create a mock SensorInfo object
 */
export function createMockSensor(overrides: Partial<SensorInfo> = {}): SensorInfo {
  return {
    sensor_id: 'test-device-1-sensor-0',
    name: 'Stereo Module',
    profiles: [
      {
        stream_type: 'depth',
        format: 'Z16',
        width: 640,
        height: 480,
        framerate: 30,
      },
    ],
    is_streaming: false,
    ...overrides,
  }
}

/**
 * Create a mock OptionInfo object
 */
export function createMockOption(overrides: Partial<OptionInfo> = {}): OptionInfo {
  return {
    option_id: 'exposure',
    name: 'Exposure',
    description: 'Depth Exposure (usec)',
    current_value: 8500,
    default_value: 8500,
    min_value: 1,
    max_value: 165000,
    step: 1,
    read_only: false,
    category: 'General',
    ...overrides,
  }
}

/**
 * Create a mock StreamConfig object
 */
export function createMockStreamConfig(overrides: Partial<StreamConfig> = {}): StreamConfig {
  return {
    sensor_id: 'test-device-1-sensor-0',
    stream_type: 'depth',
    format: 'Z16',
    enable: true,
    resolution: { width: 640, height: 480 },
    framerate: 30,
    ...overrides,
  }
}

/**
 * Custom render function that wraps components with necessary providers
 */
export function renderWithProviders(
  ui: ReactElement,
  options?: RenderOptions & { initialStoreState?: Partial<ReturnType<typeof useAppStore.getState>> }
) {
  const { initialStoreState, ...renderOptions } = options || {}

  // Reset store first to ensure clean state
  resetStore()

  // Set initial store state if provided
  if (initialStoreState) {
    useAppStore.setState(initialStoreState as any)
  }

  function Wrapper({ children }: { children: ReactNode }) {
    return <>{children}</>
  }

  return {
    ...render(ui, { wrapper: Wrapper, ...renderOptions }),
    // Return store for assertions
    store: useAppStore,
  }
}

// Re-export everything from React Testing Library except render (avoid duplicate export)
export * from '@testing-library/react'
export { renderWithProviders }
