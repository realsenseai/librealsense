import { create } from 'zustand'
import type {
  DeviceInfo,
  SensorInfo,
  OptionInfo,
  StreamConfig,
  MetadataUpdate,
  StreamMetadata,
  IMUData,
  ViewMode,
  DeviceState,
  FirmwareState,
  SensorStreamConfig,
  SensorStreamStatus,
  SensorConfig,
} from '../api/types'

// Map to track pending stop operations by "deviceId:sensorId" key
// Used to await completion before allowing a new start
const pendingStopPromises = new Map<string, Promise<void>>()
import { apiClient } from '../api/client'
import {
  checkChatAvailability,
  sendChatMessage as sendChatMessageApi,
  generateMessageId,
  type ChatMessage,
  type ChatResponse,
} from '../api/chat'
import type { ProposedSettings } from '../utils/chatPrompt'

interface IMUHistory {
  accel: { timestamp: number; x: number; y: number; z: number }[]
  gyro: { timestamp: number; x: number; y: number; z: number }[]
}

interface AppState {
  // Connection state
  isConnected: boolean
  setConnected: (connected: boolean) => void

  // Devices - multi-camera support
  devices: DeviceInfo[]
  deviceStates: Record<string, DeviceState> // keyed by device_id
  isLoadingDevices: boolean
  fetchDevices: () => Promise<void>
  checkFirmwareUpdates: (deviceId: string) => Promise<void>
  updateFirmware: (deviceId: string) => Promise<void>
  
  // Device activation (multi-select support)
  toggleDeviceActive: (device: DeviceInfo) => Promise<void>
  getActiveDevices: () => DeviceState[]
  isAnyDeviceStreaming: () => boolean
  
  // Legacy single device selection (for compatibility)
  selectedDevice: DeviceInfo | null
  selectDevice: (device: DeviceInfo | null) => void
  resetDevice: (deviceId: string) => Promise<void>

  // Per-device sensors fetch
  fetchSensors: (deviceId: string) => Promise<void>

  // Per-device options
  fetchOptions: (deviceId: string, sensorId: string) => Promise<void>
  setOption: (
    deviceId: string,
    sensorId: string,
    optionId: string,
    value: number | boolean | string
  ) => Promise<void>

  // Per-device stream configuration  
  updateStreamConfig: (deviceIdOrConfig: string | StreamConfig, config?: StreamConfig) => void
  updateSensorConfig: (deviceId: string, sensorId: string, config: Partial<SensorConfig>) => void

  // Per-device streaming
  startDeviceStreaming: (deviceId: string) => Promise<void>
  stopDeviceStreaming: (deviceId: string) => Promise<void>
  startAllStreaming: () => Promise<void>
  stopAllStreaming: () => Promise<void>
  startStreaming: () => Promise<void>
  stopStreaming: () => Promise<void>

  // Per-sensor streaming (sensor API)
  startSensorStreaming: (deviceId: string, sensorId: string) => Promise<void>
  stopSensorStreaming: (deviceId: string, sensorId: string) => Promise<void>
  refreshSensorStatus: (deviceId: string) => Promise<void>

  // Metadata from Socket.IO
  updateMetadata: (metadata: MetadataUpdate) => void

  // IMU data history for graphs (global for now)
  imuHistory: IMUHistory
  maxIMUHistoryLength: number
  addIMUData: (type: 'accel' | 'gyro', data: IMUData) => void
  clearIMUHistory: () => void

  // Point cloud (per device)
  togglePointCloud: (deviceId?: string) => Promise<void>
  setPointCloudVertices: (deviceIdOrVertices: string | Float32Array | null, vertices?: Float32Array | null) => void

  // UI state
  viewMode: ViewMode
  setViewMode: (mode: ViewMode) => void
  isIMUViewerExpanded: boolean
  toggleIMUViewer: () => void

  // Chat/AI Assistant state
  isChatOpen: boolean
  isChatAvailable: boolean
  isChatLoading: boolean
  chatMessages: ChatMessage[]
  pendingSettings: ProposedSettings | null
  toggleChat: () => void
  checkChatAvailability: () => Promise<void>
  sendChatMessage: (content: string) => Promise<void>
  applyProposedSettings: () => Promise<void>
  dismissProposedSettings: () => void
  clearChat: () => void

  // Error handling
  error: string | null
  setError: (error: string | null) => void
  clearError: () => void

  // Legacy compatibility getters
  sensors: SensorInfo[]
  options: Record<string, OptionInfo[]>
  streamConfigs: StreamConfig[]
  isStreaming: boolean
  streamMetadata: Record<string, StreamMetadata>
  latestMetadata: MetadataUpdate | null
  isLoadingSensors: boolean
  isLoadingOptions: boolean
  isPointCloudEnabled: boolean
  pointCloudVertices: Float32Array | null
}

export const useAppStore = create<AppState>()((set, get) => ({
  // Connection state
  isConnected: false,
  setConnected: (connected) => set({ isConnected: connected }),

  // Devices
  devices: [],
  deviceStates: {},
  isLoadingDevices: false,
  
  fetchDevices: async () => {
    set({ isLoadingDevices: true, error: null })
    try {
      const devices = await apiClient.getDevices()
      // Update devices list, preserve existing device states for known devices
      set((state) => {
        const newDeviceStates = { ...state.deviceStates }
        // Remove states for devices that no longer exist
        for (const deviceId of Object.keys(newDeviceStates)) {
          if (!devices.find(d => d.device_id === deviceId)) {
            delete newDeviceStates[deviceId]
          }
        }

        // Refresh device info + firmware metadata for existing device states
        for (const device of devices) {
          const existing = newDeviceStates[device.device_id]
          if (existing) {
            const baseFirmware: FirmwareState = existing.firmware || {
              current: device.firmware_version,
              recommended: device.recommended_firmware_version,
              status: device.firmware_status || 'unknown',
              file_available: device.firmware_file_available,
              is_updating: false,
              progress: undefined,
              last_error: null,
            }

            const updatedFirmware: FirmwareState = {
              ...baseFirmware,
              current: device.firmware_version,
              recommended: device.recommended_firmware_version,
              status: device.firmware_status || baseFirmware.status || 'unknown',
              file_available: device.firmware_file_available,
            }

            newDeviceStates[device.device_id] = {
              ...existing,
              device,
              firmware: updatedFirmware,
            }
          }
        }

        return { devices, deviceStates: newDeviceStates, isLoadingDevices: false }
      })
    } catch (error) {
      set({
        error: `Failed to fetch devices: ${error instanceof Error ? error.message : 'Unknown error'}`,
        isLoadingDevices: false,
      })
    }
  },

  checkFirmwareUpdates: async (deviceId: string) => {
    try {
      const firmwareStatus = await apiClient.getFirmwareStatus(deviceId)
      set((state) => {
        const ds = state.deviceStates[deviceId]
        if (!ds) return state

        const updatedFirmware: FirmwareState = {
          current: firmwareStatus.current || ds.device.firmware_version,
          recommended: firmwareStatus.recommended || ds.device.recommended_firmware_version,
          status: (firmwareStatus.status as FirmwareState['status']) || 'unknown',
          file_available: firmwareStatus.file_available,
          is_updating: false,
          progress: undefined,
          last_error: null,
        }

        return {
          deviceStates: {
            ...state.deviceStates,
            [deviceId]: {
              ...ds,
              firmware: updatedFirmware,
            },
          },
        }
      })
    } catch (error) {
      set({
        error: `Failed to check firmware updates: ${error instanceof Error ? error.message : 'Unknown error'}`,
      })
    }
  },

  updateFirmware: async (deviceId: string) => {
    // Mark updating state
    set((state) => {
      const ds = state.deviceStates[deviceId]
      if (!ds) return state
      const prev = ds.firmware || {
        status: 'unknown' as FirmwareState['status'],
        current: ds.device.firmware_version,
        recommended: ds.device.recommended_firmware_version,
        file_available: ds.device.firmware_file_available,
        is_updating: false,
        progress: undefined,
        last_error: null,
      }
      const nextFirmware: FirmwareState = {
        ...prev,
        status: prev.status ?? 'unknown',
        is_updating: true,
        progress: 0,
        last_error: null,
      }
      return {
        deviceStates: {
          ...state.deviceStates,
          [deviceId]: {
            ...ds,
            firmware: nextFirmware,
          },
        },
      }
    })

    try {
      await apiClient.updateFirmware(deviceId)
      // Refresh devices to pick up new firmware version/status
      await get().fetchDevices()
      set((state) => {
        const ds = state.deviceStates[deviceId]
        if (!ds) return state
        const prev = ds.firmware || { status: 'unknown' as FirmwareState['status'] }
        const next: FirmwareState = { ...prev, status: prev.status ?? 'unknown', is_updating: false, progress: 1 }
        return {
          deviceStates: {
            ...state.deviceStates,
            [deviceId]: { ...ds, firmware: next },
          },
        }
      })
    } catch (error) {
      set((state) => {
        const ds = state.deviceStates[deviceId]
        if (!ds) return state
        const prev = ds.firmware || { status: 'unknown' as FirmwareState['status'] }
        const next: FirmwareState = {
          ...prev,
          status: prev.status ?? 'unknown',
          is_updating: false,
          last_error: error instanceof Error ? error.message : 'Firmware update failed',
        }
        return {
          deviceStates: {
            ...state.deviceStates,
            [deviceId]: { ...ds, firmware: next },
          },
          error: `Firmware update failed: ${error instanceof Error ? error.message : 'Unknown error'}`,
        }
      })
    }
  },

  toggleDeviceActive: async (device: DeviceInfo) => {
    const state = get()
    const existing = state.deviceStates[device.device_id]
    
    if (existing?.isActive) {
      // Deactivate: stop streaming if active, then remove
      if (existing.isStreaming) {
        await get().stopDeviceStreaming(device.device_id)
      }
      set((s) => {
        const newStates = { ...s.deviceStates }
        delete newStates[device.device_id]
        return { 
          deviceStates: newStates,
          selectedDevice: s.selectedDevice?.device_id === device.device_id ? null : s.selectedDevice
        }
      })
    } else {
      // Activate: create device state and fetch sensors
      const deviceState: DeviceState = {
        device,
        firmware: {
          current: device.firmware_version,
          recommended: device.recommended_firmware_version,
          status: device.firmware_status || 'unknown',
          file_available: device.firmware_file_available,
          is_updating: false,
          progress: undefined,
          last_error: null,
        },
        sensors: [],
        options: {},
        streamConfigs: [],
        sensorConfigs: {},
        isStreaming: false,
        isStopping: false,
        isActive: true,
        isLoading: true,
        streamMetadata: {},
        streamingMode: 'idle',
        sensorStreamingStatus: {},
      }
      set((s) => ({
        deviceStates: { ...s.deviceStates, [device.device_id]: deviceState },
        selectedDevice: device, // Set as selected for compatibility
      }))
      
      // Fetch sensors for this device
      await get().fetchSensors(device.device_id)
    }
  },

  getActiveDevices: () => {
    const state = get()
    return Object.values(state.deviceStates).filter(ds => ds.isActive)
  },

  isAnyDeviceStreaming: () => {
    const state = get()
    return Object.values(state.deviceStates).some(ds => ds.isStreaming)
  },

  // Legacy single device selection
  selectedDevice: null,
  selectDevice: (device) => {
    if (device) {
      // If device is not active, activate it
      const state = get()
      if (!state.deviceStates[device.device_id]?.isActive) {
        get().toggleDeviceActive(device)
      } else {
        set({ selectedDevice: device })
      }
    } else {
      set({ selectedDevice: null })
    }
  },

  resetDevice: async (deviceId) => {
    try {
      await apiClient.resetDevice(deviceId)
      // Remove device state
      set((state) => {
        const newStates = { ...state.deviceStates }
        delete newStates[deviceId]
        return {
          deviceStates: newStates,
          selectedDevice: state.selectedDevice?.device_id === deviceId ? null : state.selectedDevice
        }
      })
      // Refresh device list after reset
      setTimeout(() => get().fetchDevices(), 2000)
    } catch (error) {
      set({
        error: `Failed to reset device: ${error instanceof Error ? error.message : 'Unknown error'}`,
      })
    }
  },

  // Per-device sensors fetch
  fetchSensors: async (deviceId) => {
    set((state) => ({
      deviceStates: {
        ...state.deviceStates,
        [deviceId]: {
          ...state.deviceStates[deviceId],
          isLoading: true,
        },
      },
    }))

    try {
      const sensors = await apiClient.getSensors(deviceId)
      
      // Build initial stream configs from sensor profiles
      const configs: StreamConfig[] = []
      const optionsMap: Record<string, OptionInfo[]> = {}
      const sensorConfigs: Record<string, SensorConfig> = {}
      
      for (const sensor of sensors) {
        // Store options from sensor response
        if (sensor.options && sensor.options.length > 0) {
          optionsMap[sensor.sensor_id] = sensor.options
        }
        
        // Compute intersection of resolutions/FPS across all stream profiles
        // All streams on same sensor must use same resolution/FPS when opened together
        const profiles = sensor.supported_stream_profiles.filter(
          p => p.resolutions.length > 0 && p.fps.length > 0
        )
        
        let commonResolutions: Set<string> = new Set<string>()
        let commonFps: Set<number> = new Set<number>()
        let isFirst = true
        
        for (const profile of profiles) {
          const profileRes = new Set<string>(
            profile.resolutions.map(([w, h]) => `${w}x${h}`)
          )
          const profileFps = new Set<number>(profile.fps)
          
          if (isFirst) {
            commonResolutions = profileRes
            commonFps = profileFps
            isFirst = false
          } else {
            // Intersect
            commonResolutions = new Set<string>(
              [...commonResolutions].filter(r => profileRes.has(r))
            )
            commonFps = new Set<number>(
              [...commonFps].filter(f => profileFps.has(f))
            )
          }
          
          configs.push({
            sensor_id: sensor.sensor_id,
            stream_type: profile.stream_type,
            format: profile.formats[0] || 'rgb8',
            resolution: {
              width: profile.resolutions[0][0],
              height: profile.resolutions[0][1],
            },
            framerate: profile.fps[0],
            enable: false,
          })
        }
        
        // Set sensor-level resolution/fps from common options
        if (commonResolutions && commonResolutions.size > 0 && commonFps && commonFps.size > 0) {
          const firstCommonRes = [...commonResolutions][0]
          const [width, height] = firstCommonRes.split('x').map(Number)
          
          // Pick a reasonable FPS: prefer 30, else 15, else highest available
          const sortedFps = [...commonFps].sort((a, b) => b - a) // descending
          let selectedFps = sortedFps[0] // default to highest
          if (commonFps.has(30)) {
            selectedFps = 30
          } else if (commonFps.has(15)) {
            selectedFps = 15
          }
          
          sensorConfigs[sensor.sensor_id] = {
            resolution: { width, height },
            framerate: selectedFps,
          }
        }
      }
      
      set((state) => ({
        deviceStates: {
          ...state.deviceStates,
          [deviceId]: {
            ...state.deviceStates[deviceId],
            sensors,
            options: optionsMap,
            streamConfigs: configs,
            sensorConfigs,
            isLoading: false,
          },
        },
      }))
    } catch (error) {
      set((state) => ({
        deviceStates: {
          ...state.deviceStates,
          [deviceId]: {
            ...state.deviceStates[deviceId],
            isLoading: false,
          },
        },
        error: `Failed to fetch sensors: ${error instanceof Error ? error.message : 'Unknown error'}`,
      }))
    }
  },

  // Per-device options
  fetchOptions: async (deviceId, sensorId) => {
    try {
      const options = await apiClient.getOptions(deviceId, sensorId)
      set((state) => ({
        deviceStates: {
          ...state.deviceStates,
          [deviceId]: {
            ...state.deviceStates[deviceId],
            options: {
              ...state.deviceStates[deviceId]?.options,
              [sensorId]: options,
            },
          },
        },
      }))
    } catch (error) {
      console.error(`Failed to fetch options for sensor ${sensorId}:`, error)
    }
  },

  setOption: async (deviceId, sensorId, optionId, value) => {
    try {
      await apiClient.setOption(deviceId, sensorId, optionId, value)
      set((state) => {
        const deviceState = state.deviceStates[deviceId]
        if (!deviceState) return state
        
        return {
          deviceStates: {
            ...state.deviceStates,
            [deviceId]: {
              ...deviceState,
              options: {
                ...deviceState.options,
                [sensorId]: deviceState.options[sensorId]?.map((opt) =>
                  // Match by option_id OR by name (case-insensitive) for chatbot compatibility
                  (opt.option_id === optionId || opt.name.toLowerCase() === optionId.toLowerCase())
                    ? { ...opt, current_value: value } 
                    : opt
                ),
              },
            },
          },
        }
      })
    } catch (error) {
      set({
        error: `Failed to set option: ${error instanceof Error ? error.message : 'Unknown error'}`,
      })
      throw error
    }
  },

  // Per-device stream configuration - supports both old and new signatures
  updateStreamConfig: (deviceIdOrConfig: string | StreamConfig, config?: StreamConfig) => {
    // Legacy support: if first arg is StreamConfig, use selectedDevice
    if (typeof deviceIdOrConfig === 'object') {
      const state = get()
      const deviceId = state.selectedDevice?.device_id
      if (!deviceId) return
      
      const legacyConfig = deviceIdOrConfig
      set((s) => {
        const deviceState = s.deviceStates[deviceId]
        if (!deviceState) return s
        
        return {
          deviceStates: {
            ...s.deviceStates,
            [deviceId]: {
              ...deviceState,
              streamConfigs: deviceState.streamConfigs.map((c) =>
                c.sensor_id === legacyConfig.sensor_id && c.stream_type === legacyConfig.stream_type ? legacyConfig : c
              ),
            },
          },
        }
      })
      return
    }
    
    // New signature: deviceId, config
    const deviceId = deviceIdOrConfig
    if (!config) return
    
    set((state) => {
      const deviceState = state.deviceStates[deviceId]
      if (!deviceState) return state
      
      return {
        deviceStates: {
          ...state.deviceStates,
          [deviceId]: {
            ...deviceState,
            streamConfigs: deviceState.streamConfigs.map((c) =>
              c.sensor_id === config.sensor_id && c.stream_type === config.stream_type ? config : c
            ),
          },
        },
      }
    })
  },

  // Per-sensor configuration (resolution/FPS at sensor level)
  updateSensorConfig: (deviceId, sensorId, config) => {
    set((state) => {
      const deviceState = state.deviceStates[deviceId]
      if (!deviceState) return state
      
      const currentConfig = deviceState.sensorConfigs[sensorId] || { resolution: { width: 0, height: 0 }, framerate: 0 }
      
      return {
        deviceStates: {
          ...state.deviceStates,
          [deviceId]: {
            ...deviceState,
            sensorConfigs: {
              ...deviceState.sensorConfigs,
              [sensorId]: {
                ...currentConfig,
                ...config,
              },
            },
          },
        },
      }
    })
  },

  // Per-device streaming
  startDeviceStreaming: async (deviceId) => {
    const state = get()
    const deviceState = state.deviceStates[deviceId]
    if (!deviceState) return

    // If a stop is still in progress, wait briefly until it finishes
    if (deviceState.isStopping) {
      for (let i = 0; i < 20; i++) {
        try {
          const status = await apiClient.getStreamStatus(deviceId)
          if (!status.stopping) break
        } catch (e) {
          // ignore and retry
        }
        await new Promise((resolve) => setTimeout(resolve, 150))
      }
    }

    const enabledStreamConfigs = deviceState.streamConfigs.filter((c) => c.enable)
    if (enabledStreamConfigs.length === 0) {
      set({ error: 'Please enable at least one stream' })
      return
    }

    // Apply sensor-level resolution/FPS to each enabled stream config
    const configsWithSensorSettings = enabledStreamConfigs.map(c => {
      const sensorConfig = deviceState.sensorConfigs[c.sensor_id]
      if (sensorConfig) {
        return {
          ...c,
          resolution: sensorConfig.resolution,
          framerate: sensorConfig.framerate,
        }
      }
      return c
    })

    try {
      await apiClient.startStreaming(deviceId, {
        configs: configsWithSensorSettings,
        apply_filters: false,
        reuse_cache: true,
      })
      set((s) => ({
        deviceStates: {
          ...s.deviceStates,
          [deviceId]: {
            ...s.deviceStates[deviceId],
            isStreaming: true,
            isStopping: false,
            streamingMode: 'pipeline',
          },
        },
        error: null,
      }))
    } catch (error) {
      // Extract error message - axios errors have response.data.detail
      let errorMessage = 'Unknown error'
      if (error && typeof error === 'object') {
        const axiosError = error as { response?: { data?: { detail?: string } }; message?: string }
        if (axiosError.response?.data?.detail) {
          errorMessage = axiosError.response.data.detail
        } else if (axiosError.message) {
          errorMessage = axiosError.message
        }
      }
      set({
        error: `Failed to start streaming: ${errorMessage}`,
      })
    }
  },

  stopDeviceStreaming: async (deviceId) => {
    // Optimistically mark stopping and hide stream immediately
    set((state) => ({
      deviceStates: {
        ...state.deviceStates,
        [deviceId]: {
          ...state.deviceStates[deviceId],
          isStopping: true,
          isStreaming: false,
          streamMetadata: {},
        },
      },
    }))

    try {
      const status = await apiClient.stopStreaming(deviceId)
      set((state) => ({
        deviceStates: {
          ...state.deviceStates,
          [deviceId]: {
            ...state.deviceStates[deviceId],
            isStopping: !!status?.stopping,
            isStreaming: status?.is_streaming ?? false,
            streamingMode: 'idle',
            sensorStreamingStatus: {},
            streamMetadata: status?.stopping ? state.deviceStates[deviceId].streamMetadata : {},
          },
        },
      }))
    } catch (error) {
      set({
        error: `Failed to stop streaming: ${error instanceof Error ? error.message : 'Unknown error'}`,
      })
      // Roll back stopping flag on error
      set((state) => ({
        deviceStates: {
          ...state.deviceStates,
          [deviceId]: {
            ...state.deviceStates[deviceId],
            isStopping: false,
          },
        },
      }))
    }
  },

  startAllStreaming: async () => {
    const state = get()
    const activeDevices = Object.values(state.deviceStates).filter(ds => ds.isActive)
    
    for (const deviceState of activeDevices) {
      const enabledConfigs = deviceState.streamConfigs.filter(c => c.enable)
      if (enabledConfigs.length > 0) {
        await get().startDeviceStreaming(deviceState.device.device_id)
      }
    }
  },

  stopAllStreaming: async () => {
    const state = get()
    const streamingDevices = Object.values(state.deviceStates).filter(ds => ds.isStreaming)
    
    for (const deviceState of streamingDevices) {
      await get().stopDeviceStreaming(deviceState.device.device_id)
    }
  },

  // Legacy streaming methods
  startStreaming: async () => {
    await get().startAllStreaming()
  },

  stopStreaming: async () => {
    await get().stopAllStreaming()
  },

  // Per-sensor streaming (sensor API)
  startSensorStreaming: async (deviceId, sensorId) => {
    // Wait for any pending stop operation to complete before starting
    const pendingKey = `${deviceId}:${sensorId}`
    const pendingStop = pendingStopPromises.get(pendingKey)
    if (pendingStop) {
      await pendingStop
    }

    const state = get()
    const deviceState = state.deviceStates[deviceId]
    if (!deviceState) return

    // Check mode - can't use sensor API if pipeline is active
    if (deviceState.streamingMode === 'pipeline') {
      set({ error: 'Stop all streams before using per-sensor control' })
      return
    }

    // Find ALL enabled stream configs for this sensor (not just first)
    const enabledStreamConfigs = deviceState.streamConfigs.filter(
      c => c.sensor_id === sensorId && c.enable
    )
    if (enabledStreamConfigs.length === 0) {
      set({ error: 'Enable at least one stream for this sensor' })
      return
    }

    // Get sensor-level resolution/FPS (shared across all streams from this sensor)
    const sensorConfig = deviceState.sensorConfigs[sensorId]
    if (!sensorConfig) {
      set({ error: 'Sensor configuration not found' })
      return
    }

    // Build configs array for all enabled streams using sensor-level resolution/FPS
    const configs: SensorStreamConfig[] = enabledStreamConfigs.map(c => ({
      stream_type: c.stream_type,
      format: c.format,
      resolution: sensorConfig.resolution,  // Use sensor-level resolution
      framerate: sensorConfig.framerate,    // Use sensor-level FPS
    }))

    try {
      const status = await apiClient.startSensor(deviceId, sensorId, configs)
      
      set((s) => ({
        deviceStates: {
          ...s.deviceStates,
          [deviceId]: {
            ...s.deviceStates[deviceId],
            streamingMode: 'sensor',
            isStreaming: true,
            sensorStreamingStatus: {
              ...s.deviceStates[deviceId].sensorStreamingStatus,
              [sensorId]: status,
            },
          },
        },
        error: null,
      }))
    } catch (error) {
      // Extract error message - axios errors have response.data.detail
      let errorMessage = 'Failed to start sensor'
      if (error && typeof error === 'object') {
        const axiosError = error as { response?: { data?: { detail?: string } }; message?: string }
        if (axiosError.response?.data?.detail) {
          errorMessage = axiosError.response.data.detail
        } else if (axiosError.message) {
          errorMessage = axiosError.message
        }
      }
      
      // Store error in sensor status
      set((s) => ({
        deviceStates: {
          ...s.deviceStates,
          [deviceId]: {
            ...s.deviceStates[deviceId],
            sensorStreamingStatus: {
              ...s.deviceStates[deviceId].sensorStreamingStatus,
              [sensorId]: {
                sensor_id: sensorId,
                name: '',
                is_streaming: false,
                error: errorMessage,
              },
            },
          },
        },
      }))
    }
  },

  stopSensorStreaming: async (deviceId, sensorId) => {
    const pendingKey = `${deviceId}:${sensorId}`

    // Optimistically update UI immediately
    set((s) => {
      const deviceState = s.deviceStates[deviceId]
      if (!deviceState) return s

      const currentSensorStatus = deviceState.sensorStreamingStatus[sensorId] || {
        sensor_id: sensorId,
        name: '',
        is_streaming: false,
      }

      const newSensorStatus = {
        ...deviceState.sensorStreamingStatus,
        [sensorId]: {
          ...currentSensorStatus,
          is_streaming: false,  // Optimistic: immediately show as stopped
          pendingOp: 'stopping' as const,  // Track that we're stopping
        },
      }

      // Check if any sensors are still streaming (excluding this one)
      const anyStreaming = Object.entries(newSensorStatus).some(
        ([id, ss]) => id !== sensorId && ss.is_streaming
      )

      return {
        deviceStates: {
          ...s.deviceStates,
          [deviceId]: {
            ...deviceState,
            streamingMode: anyStreaming ? 'sensor' : 'idle',
            isStreaming: anyStreaming,
            sensorStreamingStatus: newSensorStatus,
          },
        },
      }
    })

    // Create and store the stop promise so startSensorStreaming can await it
    const stopPromise = (async () => {
      try {
        const status = await apiClient.stopSensor(deviceId, sensorId)
        
        set((s) => {
          const deviceState = s.deviceStates[deviceId]
          if (!deviceState) return s

          const newSensorStatus = { ...deviceState.sensorStreamingStatus }
          newSensorStatus[sensorId] = {
            ...status,
            pendingOp: null,  // Clear pending state
          }

          // Recheck streaming state with actual server response
          const anyStreaming = Object.values(newSensorStatus).some(ss => ss.is_streaming)

          return {
            deviceStates: {
              ...s.deviceStates,
              [deviceId]: {
                ...deviceState,
                streamingMode: anyStreaming ? 'sensor' : 'idle',
                isStreaming: anyStreaming,
                sensorStreamingStatus: newSensorStatus,
              },
            },
          }
        })
      } catch (error) {
        // Rollback: restore streaming state on error
        set((s) => {
          const deviceState = s.deviceStates[deviceId]
          if (!deviceState) return s

          const currentSensorStatus = deviceState.sensorStreamingStatus[sensorId]

          return {
            error: `Failed to stop sensor: ${error instanceof Error ? error.message : 'Unknown error'}`,
            deviceStates: {
              ...s.deviceStates,
              [deviceId]: {
                ...deviceState,
                streamingMode: 'sensor',
                isStreaming: true,
                sensorStreamingStatus: {
                  ...deviceState.sensorStreamingStatus,
                  [sensorId]: {
                    ...currentSensorStatus,
                    is_streaming: true,  // Rollback: restore streaming state
                    pendingOp: null,     // Clear pending state
                  },
                },
              },
            },
          }
        })
      } finally {
        // Always remove from pending map when done
        pendingStopPromises.delete(pendingKey)
      }
    })()

    pendingStopPromises.set(pendingKey, stopPromise)
    await stopPromise
  },

  refreshSensorStatus: async (deviceId) => {
    try {
      const batchStatus = await apiClient.getBatchSensorStatus(deviceId)
      
      set((s) => {
        const deviceState = s.deviceStates[deviceId]
        if (!deviceState) return s

        const sensorStatus: Record<string, SensorStreamStatus> = {}
        for (const ss of batchStatus.sensors) {
          sensorStatus[ss.sensor_id] = ss
        }

        const anyStreaming = batchStatus.sensors.some(ss => ss.is_streaming)
        const mode = batchStatus.mode === 'sensor' ? 'sensor' : 
                     batchStatus.mode === 'pipeline' ? 'pipeline' : 'idle'

        return {
          deviceStates: {
            ...s.deviceStates,
            [deviceId]: {
              ...deviceState,
              streamingMode: mode,
              isStreaming: anyStreaming || deviceState.isStreaming,
              sensorStreamingStatus: sensorStatus,
            },
          },
        }
      })
    } catch (error) {
      console.error('Failed to refresh sensor status:', error)
    }
  },

  // Metadata
  updateMetadata: (metadata) => {
    const deviceId = metadata.device_id
    set((state) => {
      const deviceState = state.deviceStates[deviceId]
      if (!deviceState) return state
      
      return {
        deviceStates: {
          ...state.deviceStates,
          [deviceId]: {
            ...deviceState,
            streamMetadata: metadata.metadata_streams,
          },
        },
      }
    })

    // Extract IMU data if present
    for (const [streamType, streamData] of Object.entries(metadata.metadata_streams)) {
      if (streamData.motion_data) {
        if (streamType.toLowerCase().includes('accel')) {
          get().addIMUData('accel', streamData.motion_data)
        } else if (streamType.toLowerCase().includes('gyro')) {
          get().addIMUData('gyro', streamData.motion_data)
        }
      }

      // Extract point cloud data if present
      if (streamData.point_cloud?.vertices) {
        try {
          const base64Data = streamData.point_cloud.vertices
          const binaryString = atob(base64Data)
          const bytes = new Uint8Array(binaryString.length)
          for (let i = 0; i < binaryString.length; i++) {
            bytes[i] = binaryString.charCodeAt(i)
          }
          const vertices = new Float32Array(bytes.buffer)
          set({ pointCloudVertices: vertices })
        } catch (error) {
          console.error('Failed to decode point cloud data:', error)
        }
      }
    }
  },

  // IMU history (global)
  imuHistory: { accel: [], gyro: [] },
  maxIMUHistoryLength: 100,
  addIMUData: (type, data) => {
    set((state) => {
      const history = [...state.imuHistory[type]]
      history.push({ timestamp: Date.now(), ...data })
      if (history.length > state.maxIMUHistoryLength) {
        history.shift()
      }
      return {
        imuHistory: {
          ...state.imuHistory,
          [type]: history,
        },
      }
    })
  },
  clearIMUHistory: () => set({ imuHistory: { accel: [], gyro: [] } }),

  // Point cloud - supports both old and new signatures
  togglePointCloud: async (deviceId?: string) => {
    const state = get()
    const targetDeviceId = deviceId || state.selectedDevice?.device_id
    if (!targetDeviceId) return

    const deviceState = state.deviceStates[targetDeviceId]
    if (!deviceState) return

    // Check if point cloud is currently enabled by looking at vertices
    const hasPointCloud = deviceState.streamMetadata?.['depth']?.point_cloud !== undefined

    try {
      if (hasPointCloud) {
        await apiClient.disablePointCloud(targetDeviceId)
      } else {
        await apiClient.enablePointCloud(targetDeviceId)
      }
    } catch (error) {
      set({
        error: `Failed to toggle point cloud: ${error instanceof Error ? error.message : 'Unknown error'}`,
      })
    }
  },

  setPointCloudVertices: (deviceIdOrVertices: string | Float32Array | null, vertices?: Float32Array | null) => {
    // Legacy support: if first arg is Float32Array or null, use it directly
    if (typeof deviceIdOrVertices !== 'string') {
      set({ pointCloudVertices: deviceIdOrVertices })
      return
    }
    // New signature: deviceId, vertices - store globally for now
    set({ pointCloudVertices: vertices || null })
  },

  // UI state
  viewMode: '2d',
  setViewMode: (mode) => set({ viewMode: mode }),
  isIMUViewerExpanded: false,
  toggleIMUViewer: () => set((state) => ({ isIMUViewerExpanded: !state.isIMUViewerExpanded })),

  // Chat/AI Assistant state
  isChatOpen: false,
  isChatAvailable: false,
  isChatLoading: false,
  chatMessages: [],
  pendingSettings: null,
  
  toggleChat: () => set((state) => ({ isChatOpen: !state.isChatOpen })),
  
  checkChatAvailability: async () => {
    const available = await checkChatAvailability()
    set({ isChatAvailable: available })
  },
  
  sendChatMessage: async (content: string) => {
    const state = get()
    
    // Add user message
    const userMessage: ChatMessage = {
      id: generateMessageId(),
      role: 'user',
      content,
      timestamp: Date.now(),
    }
    
    set((s) => ({
      chatMessages: [...s.chatMessages, userMessage],
      isChatLoading: true,
    }))
    
    try {
      // Get all messages for context
      const allMessages = [...state.chatMessages, userMessage]
      
      // Send to API with device context
      const response: ChatResponse = await sendChatMessageApi(allMessages, state.deviceStates)
      
      // Add assistant message
      const assistantMessage: ChatMessage = {
        id: generateMessageId(),
        role: 'assistant',
        content: response.content,
        proposedSettings: response.proposedSettings,
        timestamp: Date.now(),
      }
      
      set((s) => ({
        chatMessages: [...s.chatMessages, assistantMessage],
        pendingSettings: response.proposedSettings || s.pendingSettings,
        isChatLoading: false,
      }))
    } catch (error) {
      const errorMessage: ChatMessage = {
        id: generateMessageId(),
        role: 'assistant',
        content: `Sorry, I encountered an error: ${error instanceof Error ? error.message : 'Unknown error'}. Please try again.`,
        timestamp: Date.now(),
      }
      
      set((s) => ({
        chatMessages: [...s.chatMessages, errorMessage],
        isChatLoading: false,
      }))
    }
  },
  
  applyProposedSettings: async () => {
    const state = get()
    const settings = state.pendingSettings
    if (!settings) return
    
    try {
      // Find the device
      const deviceState = Object.values(state.deviceStates).find(
        ds => ds.device.serial_number === settings.deviceSerial
      )
      
      if (!deviceState) {
        throw new Error(`Device ${settings.deviceSerial} not found`)
      }
      
      const deviceId = deviceState.device.device_id
      
      // Apply stream configurations to local state first
      // Apply stream configurations - match by stream_type (case-insensitive)
      if (settings.streamConfigs && settings.streamConfigs.length > 0) {
        set((state) => {
          const deviceState = state.deviceStates[deviceId]
          if (!deviceState) return state
          
          // Create updated stream configs
          const updatedConfigs = deviceState.streamConfigs.map(existingConfig => {
            // Find matching proposed config by stream_type (case-insensitive)
            const proposedConfig = settings.streamConfigs!.find(
              pc => pc.stream_type.toLowerCase() === existingConfig.stream_type.toLowerCase()
            )
            
            if (proposedConfig) {
              // Merge proposed config with existing, keeping sensor_id from existing
              return {
                ...existingConfig,
                format: proposedConfig.format || existingConfig.format,
                resolution: proposedConfig.resolution || existingConfig.resolution,
                framerate: proposedConfig.framerate || existingConfig.framerate,
                enable: proposedConfig.enable,
              }
            }
            return existingConfig
          })
          
          return {
            deviceStates: {
              ...state.deviceStates,
              [deviceId]: {
                ...deviceState,
                streamConfigs: updatedConfigs,
              },
            },
          }
        })
      }
      
      // Apply option changes
      if (settings.optionChanges) {
        for (const change of settings.optionChanges) {
          // Map sensor label (e.g., "RGB Camera") to unique sensor_id for this device
          let uniqueSensorId = change.sensorId
          if (deviceState.sensors && deviceState.sensors.length > 0) {
            const match = deviceState.sensors.find(
              s => s.name === change.sensorId || s.sensor_id === change.sensorId
            )
            if (match) {
              uniqueSensorId = match.sensor_id
            }
          }
          await get().setOption(deviceId, uniqueSensorId, change.optionId, change.value)
        }
      }
      
      // Handle stream start/stop actions
      if (settings.streamAction === 'start') {
        // Make sure we have stream configs before starting
        const currentState = get().deviceStates[deviceId]
        const enabledConfigs = currentState?.streamConfigs.filter(c => c.enable) || []
        if (enabledConfigs.length === 0) {
          throw new Error('No streams enabled. Please configure at least one stream before starting.')
        }
        await get().startDeviceStreaming(deviceId)
      } else if (settings.streamAction === 'stop') {
        await get().stopDeviceStreaming(deviceId)
      }
      
      // Clear pending settings
      set({ pendingSettings: null })
      
      // Build confirmation message
      let confirmContent = '✓ Settings applied successfully'
      if (settings.streamAction === 'start') {
        confirmContent = '✓ Streaming started'
      } else if (settings.streamAction === 'stop') {
        confirmContent = '✓ Streaming stopped'
      }
      if (settings.explanation) {
        confirmContent += `: ${settings.explanation}`
      }
      
      // Add confirmation message
      const confirmMessage: ChatMessage = {
        id: generateMessageId(),
        role: 'assistant',
        content: confirmContent,
        timestamp: Date.now(),
      }
      set((s) => ({
        chatMessages: [...s.chatMessages, confirmMessage],
      }))
    } catch (error) {
      get().setError(`Failed to apply settings: ${error instanceof Error ? error.message : 'Unknown error'}`)
    }
  },
  
  dismissProposedSettings: () => {
    set({ pendingSettings: null })
  },
  
  clearChat: () => {
    set({
      chatMessages: [],
      pendingSettings: null,
    })
  },

  // Error handling
  error: null,
  setError: (error) => set({ error }),
  clearError: () => set({ error: null }),

  // Legacy compatibility getters - return data from selected device
  get sensors() {
    const state = get()
    if (!state.selectedDevice) return []
    return state.deviceStates[state.selectedDevice.device_id]?.sensors || []
  },

  get options() {
    const state = get()
    if (!state.selectedDevice) return {}
    return state.deviceStates[state.selectedDevice.device_id]?.options || {}
  },

  get streamConfigs() {
    const state = get()
    if (!state.selectedDevice) return []
    return state.deviceStates[state.selectedDevice.device_id]?.streamConfigs || []
  },

  get isStreaming() {
    const state = get()
    // Return true if any device is streaming
    return Object.values(state.deviceStates).some(ds => ds.isStreaming)
  },

  get streamMetadata() {
    const state = get()
    if (!state.selectedDevice) return {}
    return state.deviceStates[state.selectedDevice.device_id]?.streamMetadata || {}
  },

  get latestMetadata() {
    return null // Deprecated, use deviceStates[deviceId].streamMetadata
  },

  get isLoadingSensors() {
    const state = get()
    if (!state.selectedDevice) return false
    return state.deviceStates[state.selectedDevice.device_id]?.isLoading || false
  },

  get isLoadingOptions() {
    return false // Now handled per-device
  },

  get isPointCloudEnabled() {
    const state = get()
    if (!state.selectedDevice) return false
    const deviceState = state.deviceStates[state.selectedDevice.device_id]
    return deviceState?.streamMetadata?.['depth']?.point_cloud !== undefined
  },

  pointCloudVertices: null,
}))
