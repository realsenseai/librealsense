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
} from '../api/types'
import { apiClient } from '../api/client'

interface IMUHistory {
  accel: { timestamp: number; x: number; y: number; z: number }[]
  gyro: { timestamp: number; x: number; y: number; z: number }[]
}

interface AppState {
  // Connection state
  isConnected: boolean
  setConnected: (connected: boolean) => void

  // Devices
  devices: DeviceInfo[]
  selectedDevice: DeviceInfo | null
  isLoadingDevices: boolean
  fetchDevices: () => Promise<void>
  selectDevice: (device: DeviceInfo | null) => void
  resetDevice: (deviceId: string) => Promise<void>

  // Sensors
  sensors: SensorInfo[]
  isLoadingSensors: boolean
  fetchSensors: (deviceId: string) => Promise<void>

  // Options
  options: Record<string, OptionInfo[]> // keyed by sensor_id
  isLoadingOptions: boolean
  fetchOptions: (deviceId: string, sensorId: string) => Promise<void>
  setOption: (
    deviceId: string,
    sensorId: string,
    optionId: string,
    value: number | boolean | string
  ) => Promise<void>

  // Stream configuration
  streamConfigs: StreamConfig[]
  updateStreamConfig: (config: StreamConfig) => void
  setStreamConfigs: (configs: StreamConfig[]) => void

  // Streaming state
  isStreaming: boolean
  startStreaming: () => Promise<void>
  stopStreaming: () => Promise<void>

  // Metadata from Socket.IO
  latestMetadata: MetadataUpdate | null
  streamMetadata: Record<string, StreamMetadata>
  updateMetadata: (metadata: MetadataUpdate) => void

  // IMU data history for graphs
  imuHistory: IMUHistory
  maxIMUHistoryLength: number
  addIMUData: (type: 'accel' | 'gyro', data: IMUData) => void
  clearIMUHistory: () => void

  // Point cloud
  isPointCloudEnabled: boolean
  pointCloudVertices: Float32Array | null
  togglePointCloud: () => Promise<void>
  setPointCloudVertices: (vertices: Float32Array | null) => void

  // UI state
  viewMode: ViewMode
  setViewMode: (mode: ViewMode) => void
  isIMUViewerExpanded: boolean
  toggleIMUViewer: () => void

  // Error handling
  error: string | null
  setError: (error: string | null) => void
  clearError: () => void
}

export const useAppStore = create<AppState>()((set, get) => ({
    // Connection state
    isConnected: false,
    setConnected: (connected) => set({ isConnected: connected }),

    // Devices
    devices: [],
    selectedDevice: null,
    isLoadingDevices: false,
    fetchDevices: async () => {
      set({ isLoadingDevices: true, error: null })
      try {
        const devices = await apiClient.getDevices()
        set({ devices, isLoadingDevices: false })
      } catch (error) {
        set({
          error: `Failed to fetch devices: ${error instanceof Error ? error.message : 'Unknown error'}`,
          isLoadingDevices: false,
        })
      }
    },
    selectDevice: (device) => {
      set({
        selectedDevice: device,
        sensors: [],
        options: {},
        streamConfigs: [],
        isStreaming: false,
        streamMetadata: {},
        imuHistory: { accel: [], gyro: [] },
        pointCloudVertices: null,
      })
      if (device) {
        get().fetchSensors(device.device_id)
      }
    },
    resetDevice: async (deviceId) => {
      try {
        await apiClient.resetDevice(deviceId)
        // Refresh device list after reset
        setTimeout(() => get().fetchDevices(), 2000)
      } catch (error) {
        set({
          error: `Failed to reset device: ${error instanceof Error ? error.message : 'Unknown error'}`,
        })
      }
    },

    // Sensors
    sensors: [],
    isLoadingSensors: false,
    fetchSensors: async (deviceId) => {
      set({ isLoadingSensors: true, error: null })
      try {
        const sensors = await apiClient.getSensors(deviceId)
        set({ sensors, isLoadingSensors: false })

        // Build initial stream configs from sensor profiles
        const configs: StreamConfig[] = []
        const optionsMap: Record<string, typeof sensors[0]['options']> = {}
        
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
        set({ streamConfigs: configs, options: optionsMap })
      } catch (error) {
        set({
          error: `Failed to fetch sensors: ${error instanceof Error ? error.message : 'Unknown error'}`,
          isLoadingSensors: false,
        })
      }
    },

    // Options
    options: {},
    isLoadingOptions: false,
    fetchOptions: async (deviceId, sensorId) => {
      set({ isLoadingOptions: true })
      try {
        const options = await apiClient.getOptions(deviceId, sensorId)
        set((state) => ({
          options: { ...state.options, [sensorId]: options },
          isLoadingOptions: false,
        }))
      } catch (error) {
        console.error(`Failed to fetch options for sensor ${sensorId}:`, error)
        set({ isLoadingOptions: false })
      }
    },
    setOption: async (deviceId, sensorId, optionId, value) => {
      try {
        await apiClient.setOption(deviceId, sensorId, optionId, value)
        // Update the local option value (API returns {success: true}, not the option)
        set((state) => ({
          options: {
            ...state.options,
            [sensorId]: state.options[sensorId]?.map((opt) =>
              opt.option_id === optionId ? { ...opt, current_value: value } : opt
            ),
          },
        }))
      } catch (error) {
        set({
          error: `Failed to set option: ${error instanceof Error ? error.message : 'Unknown error'}`,
        })
        throw error  // Re-throw so component can revert
      }
    },

    // Stream configuration
    streamConfigs: [],
    updateStreamConfig: (config) => {
      set((state) => ({
        streamConfigs: state.streamConfigs.map((c) =>
          c.sensor_id === config.sensor_id && c.stream_type === config.stream_type ? config : c
        ),
      }))
    },
    setStreamConfigs: (configs) => set({ streamConfigs: configs }),

    // Streaming state
    isStreaming: false,
    startStreaming: async () => {
      const { selectedDevice, streamConfigs } = get()
      if (!selectedDevice) return

      const enabledConfigs = streamConfigs.filter((c) => c.enable)
      if (enabledConfigs.length === 0) {
        set({ error: 'Please enable at least one stream' })
        return
      }

      try {
        await apiClient.startStreaming(selectedDevice.device_id, {
          configs: enabledConfigs,
          apply_filters: false,
        })
        set({ isStreaming: true, error: null })
      } catch (error) {
        set({
          error: `Failed to start streaming: ${error instanceof Error ? error.message : 'Unknown error'}`,
        })
      }
    },
    stopStreaming: async () => {
      const { selectedDevice } = get()
      if (!selectedDevice) return

      try {
        await apiClient.stopStreaming(selectedDevice.device_id)
        set({ isStreaming: false })
      } catch (error) {
        set({
          error: `Failed to stop streaming: ${error instanceof Error ? error.message : 'Unknown error'}`,
        })
      }
    },

    // Metadata
    latestMetadata: null,
    streamMetadata: {},
    updateMetadata: (metadata) => {
      set({
        latestMetadata: metadata,
        streamMetadata: metadata.metadata_streams,
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

    // IMU history
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

    // Point cloud
    isPointCloudEnabled: false,
    pointCloudVertices: null,
    togglePointCloud: async () => {
      const { selectedDevice, isPointCloudEnabled } = get()
      if (!selectedDevice) return

      try {
        if (isPointCloudEnabled) {
          await apiClient.disablePointCloud(selectedDevice.device_id)
          set({ isPointCloudEnabled: false, pointCloudVertices: null })
        } else {
          await apiClient.enablePointCloud(selectedDevice.device_id)
          set({ isPointCloudEnabled: true })
        }
      } catch (error) {
        set({
          error: `Failed to toggle point cloud: ${error instanceof Error ? error.message : 'Unknown error'}`,
        })
      }
    },
    setPointCloudVertices: (vertices) => set({ pointCloudVertices: vertices }),

    // UI state
    viewMode: '2d',
    setViewMode: (mode) => set({ viewMode: mode }),
    isIMUViewerExpanded: false,
    toggleIMUViewer: () => set((state) => ({ isIMUViewerExpanded: !state.isIMUViewerExpanded })),

    // Error handling
    error: null,
    setError: (error) => set({ error }),
    clearError: () => set({ error: null }),
  }))
