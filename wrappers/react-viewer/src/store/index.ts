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
} from '../api/types'
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

  // Per-device streaming
  startDeviceStreaming: (deviceId: string) => Promise<void>
  stopDeviceStreaming: (deviceId: string) => Promise<void>
  startAllStreaming: () => Promise<void>
  stopAllStreaming: () => Promise<void>
  startStreaming: () => Promise<void>
  stopStreaming: () => Promise<void>

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
        return { devices, deviceStates: newDeviceStates, isLoadingDevices: false }
      })
    } catch (error) {
      set({
        error: `Failed to fetch devices: ${error instanceof Error ? error.message : 'Unknown error'}`,
        isLoadingDevices: false,
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
        sensors: [],
        options: {},
        streamConfigs: [],
        isStreaming: false,
        isActive: true,
        isLoading: true,
        streamMetadata: {},
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
      
      for (const sensor of sensors) {
        // Store options from sensor response
        if (sensor.options && sensor.options.length > 0) {
          optionsMap[sensor.sensor_id] = sensor.options
        }
        
        for (const profile of sensor.supported_stream_profiles) {
          if (profile.resolutions.length > 0 && profile.fps.length > 0) {
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

  // Per-device streaming
  startDeviceStreaming: async (deviceId) => {
    const state = get()
    const deviceState = state.deviceStates[deviceId]
    if (!deviceState) return

    const enabledConfigs = deviceState.streamConfigs.filter((c) => c.enable)
    if (enabledConfigs.length === 0) {
      set({ error: 'Please enable at least one stream' })
      return
    }

    try {
      await apiClient.startStreaming(deviceId, {
        configs: enabledConfigs,
        apply_filters: false,
      })
      set((s) => ({
        deviceStates: {
          ...s.deviceStates,
          [deviceId]: {
            ...s.deviceStates[deviceId],
            isStreaming: true,
          },
        },
        error: null,
      }))
    } catch (error) {
      set({
        error: `Failed to start streaming: ${error instanceof Error ? error.message : 'Unknown error'}`,
      })
    }
  },

  stopDeviceStreaming: async (deviceId) => {
    try {
      await apiClient.stopStreaming(deviceId)
      set((state) => ({
        deviceStates: {
          ...state.deviceStates,
          [deviceId]: {
            ...state.deviceStates[deviceId],
            isStreaming: false,
            streamMetadata: {},
          },
        },
      }))
    } catch (error) {
      set({
        error: `Failed to stop streaming: ${error instanceof Error ? error.message : 'Unknown error'}`,
      })
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
      
      // Apply stream configurations
      if (settings.streamConfigs) {
        for (const config of settings.streamConfigs) {
          get().updateStreamConfig(deviceId, config)
        }
      }
      
      // Apply option changes
      if (settings.optionChanges) {
        for (const change of settings.optionChanges) {
          await get().setOption(deviceId, change.sensorId, change.optionId, change.value)
        }
      }
      
      // Clear pending settings
      set({ pendingSettings: null })
      
      // Add confirmation message
      const confirmMessage: ChatMessage = {
        id: generateMessageId(),
        role: 'assistant',
        content: `✓ Settings applied successfully${settings.explanation ? `: ${settings.explanation}` : '.'}`,
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
