import { create, type StoreApi } from 'zustand'
import type {
  DeviceInfo,
  SensorInfo,
  OptionInfo,
  StreamConfig,
  MetadataUpdate,
  IMUData,
  ViewMode,
  DeviceState,
  AdvancedControls,
  ControlGroup,
  SensorFilters,
  FirmwareState,
  SensorStreamConfig,
  SensorConfig,
} from '../api/types'

// Map to track pending stop operations by "deviceId:sensorId" key
// Used to await completion before allowing a new start
const pendingStopPromises = new Map<string, Promise<void>>()

// Enumerations are unordered across connections; only the newest response may be applied.
let _fetchSeq = 0

type States = { deviceStates: Record<string, DeviceState> }

/** Update one device's state, leaving the others alone; a device that is gone is a no-op. */
function patchDevice(state: States, deviceId: string, patch: (ds: DeviceState) => Partial<DeviceState>) {
  const ds = state.deviceStates[deviceId]
  return ds ? { deviceStates: { ...state.deviceStates, [deviceId]: { ...ds, ...patch(ds) } } } : state
}

/** Same, for one control group; a group that is not loaded is a no-op. */
function patchGroup(state: States, deviceId: string, key: string, patch: (g: ControlGroup) => ControlGroup) {
  return patchDevice(state, deviceId, (ds) =>
    ds.controls[key] ? { controls: { ...ds.controls, [key]: patch(ds.controls[key]) } } : {})
}

// Server sends point-cloud buffers as base64 strings over Socket.IO; the
// ArrayBuffer branch is here for a future binary-attachment transport.
function decodeUint8Payload(raw: ArrayBuffer | string): Uint8Array {
  if (typeof raw === 'string') {
    return Uint8Array.from(atob(raw), (c) => c.charCodeAt(0))
  }
  return new Uint8Array(raw)
}

function decodeFloat32Payload(raw: ArrayBuffer | string): Float32Array {
  const u8 = decodeUint8Payload(raw)
  return new Float32Array(u8.buffer, u8.byteOffset, u8.byteLength >> 2)
}

/**
 * Initial stream selection. Like the legacy viewer, a sensor starts with the profiles the
 * SDK marks default; a sensor that reports none falls back to depth/color/IMU at the
 * first listed mode.
 */
function buildStreamConfigs(sensors: SensorInfo[]): StreamConfig[] {
  const configs: StreamConfig[] = []
  for (const sensor of sensors) {
    const profiles = sensor.supported_stream_profiles.filter(
      p => p.resolutions.length > 0 && p.fps.length > 0
    )
    const hasDefaults = profiles.some(p => p.default)
    for (const profile of profiles) {
      const streamTypeLower = profile.stream_type.toLowerCase()
      const enableByDefault = hasDefaults
        ? profile.default !== undefined
        : streamTypeLower === 'depth' || streamTypeLower === 'color' ||
          streamTypeLower === 'gyro' || streamTypeLower === 'accel'
      const [width, height] = profile.default?.resolution ?? profile.resolutions[0]
      configs.push({
        sensor_id: sensor.sensor_id,
        stream_type: profile.stream_type,
        format: profile.default?.format ?? profile.formats[0] ?? 'rgb8',
        resolution: { width, height },
        framerate: profile.default?.fps ?? profile.fps[0],
        enable: enableByDefault,
      })
    }
  }
  return configs
}

function buildSensorConfigs(sensors: SensorInfo[]): Record<string, SensorConfig> {
  const sensorConfigs: Record<string, SensorConfig> = {}
  for (const sensor of sensors) {
    const isMotionSensor = sensor.name.toLowerCase().includes('motion')
    const profiles = sensor.supported_stream_profiles.filter(
      p => p.resolutions.length > 0 && p.fps.length > 0
    )

    let commonResolutions = new Set<string>()
    let commonFps = new Set<number>()
    let isFirst = true

    for (const profile of profiles) {
      const profileRes = new Set<string>(profile.resolutions.map(([w, h]) => `${w}x${h}`))
      const profileFps = new Set<number>(profile.fps)
      if (isFirst) {
        commonResolutions = profileRes
        commonFps = profileFps
        isFirst = false
      } else {
        commonResolutions = new Set<string>([...commonResolutions].filter(r => profileRes.has(r)))
        commonFps = new Set<number>([...commonFps].filter(f => profileFps.has(f)))
      }
    }

    // The SDK default of a video stream on this sensor wins over the first common mode.
    const preferred = isMotionSensor ? undefined : profiles.find(p => p.default)?.default
    const preferredRes = preferred && `${preferred.resolution[0]}x${preferred.resolution[1]}`
    if (preferred && preferredRes && commonResolutions.has(preferredRes) && commonFps.has(preferred.fps)) {
      const [width, height] = preferred.resolution
      sensorConfigs[sensor.sensor_id] = { resolution: { width, height }, framerate: preferred.fps, isMotionSensor }
    } else if (commonResolutions.size > 0 && commonFps.size > 0) {
      const firstCommonRes = [...commonResolutions][0]
      const [width, height] = firstCommonRes.split('x').map(Number)
      const sortedFps = [...commonFps].sort((a, b) => b - a)
      let selectedFps = sortedFps[0]
      if (commonFps.has(30)) selectedFps = 30
      else if (commonFps.has(15)) selectedFps = 15
      sensorConfigs[sensor.sensor_id] = { resolution: { width, height }, framerate: selectedFps, isMotionSensor }
    } else if (sensor.supported_stream_profiles.length > 0) {
      const firstProfile = sensor.supported_stream_profiles[0]
      const width = firstProfile.resolutions[0]?.[0] || 320
      const height = firstProfile.resolutions[0]?.[1] || 120
      const selectedFps = firstProfile.fps[0] || 200
      sensorConfigs[sensor.sensor_id] = { resolution: { width, height }, framerate: selectedFps, isMotionSensor }
    }
  }
  return sensorConfigs
}
import { apiClient } from '../api/client'
import { optionLabel } from '../api/types'
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
  fetchDevices: (forceRefresh?: boolean) => Promise<void>
  enableMetadata: () => Promise<{ status: string; note?: string }>
  checkFirmwareUpdates: (deviceId: string) => Promise<string | undefined>
  updateFirmwareFromFile: (deviceId: string, file: File) => Promise<void>
  updateFirmwareFromRecommended: (deviceId: string) => Promise<void>
  toggleAdvancedMode: (deviceId: string, enable: boolean) => Promise<void>
  fetchDeviceControls: (deviceId: string) => Promise<void>
  setControl: (deviceId: string, key: string, optionId: string, value: number | boolean | string) => Promise<void>
  setControlEnabled: (deviceId: string, key: string, enabled: boolean) => Promise<void>
  setPostProcessing: (deviceId: string, sensorId: string, enabled: boolean) => Promise<void>

  /** Load a newly connected device's sensors and controls; every device is open. */
  openDevice: (device: DeviceInfo) => Promise<void>
  getDeviceStates: () => DeviceState[]
  isAnyDeviceStreaming: () => boolean
  
  resetDevice: (deviceId: string) => Promise<void>

  // Per-device sensors fetch
  fetchSensors: (deviceId: string) => Promise<void>

  // Per-device stream configuration
  updateStreamConfig: (deviceId: string, config: StreamConfig) => void
  updateSensorConfig: (deviceId: string, sensorId: string, config: Partial<SensorConfig>) => void

  // Per-sensor streaming (sensor API)
  startSensorStreaming: (deviceId: string, sensorId: string) => Promise<void>
  stopSensorStreaming: (deviceId: string, sensorId: string) => Promise<void>
  setSensorPaused: (deviceId: string, sensorId: string, paused: boolean) => Promise<void>
  /** Space in the legacy viewer: pause every streaming sensor, or resume them all. */
  togglePauseAll: () => Promise<void>

  // Metadata from Socket.IO
  updateMetadata: (metadata: MetadataUpdate) => void

  // IMU data history for graphs (global for now)
  imuHistory: IMUHistory
  maxIMUHistoryLength: number
  addIMUData: (type: 'accel' | 'gyro', data: IMUData) => void
  clearIMUHistory: () => void

  // UI state
  viewMode: ViewMode
  setViewMode: (mode: ViewMode) => Promise<void>

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

  pointCloudVertices: Float32Array | null
  // Per-vertex RGB sampled from the live color frame on the server (1 Uint8 per
  // channel, 3 channels per vertex; aligned 1:1 with pointCloudVertices). Null
  // when the server didn't texture the cloud (no color stream / unsupported
  // format) — the 3D viewer falls back to a depth colormap in that case.
  pointCloudColors: Uint8Array | null
}

// Shared driver for both firmware-update paths (user file + recommended download):
// flips is_updating, runs the API call (progress arrives via Socket.IO), refreshes
// device info, and records any failure on the device's firmware state.
async function performFirmwareUpdate(
  set: StoreApi<AppState>['setState'],
  get: StoreApi<AppState>['getState'],
  deviceId: string,
  apiCall: () => Promise<unknown>,
): Promise<void> {
  const setFirmwareState = (patch: Partial<FirmwareState>) =>
    set((state) => {
      const ds = state.deviceStates[deviceId]
      if (!ds) return state
      const prev = ds.firmware ?? {}
      return { deviceStates: { ...state.deviceStates, [deviceId]: { ...ds, firmware: { ...prev, ...patch } } } }
    })

  setFirmwareState({ is_updating: true, progress: 0, last_error: null })

  try {
    await apiCall()
    // The backend only responds once the flashed device has re-enumerated
    // (_refresh_until_device_returns), so one forced fetch is enough to pick it up.
    await get().fetchDevices(true)
    setFirmwareState({ is_updating: false, progress: 1 })
  } catch (error) {
    const detail = (error as { response?: { data?: { detail?: string } } })?.response?.data?.detail
    const message = detail || (error instanceof Error ? error.message : 'Firmware update failed')
    setFirmwareState({ is_updating: false, last_error: message })
    // A request rejected outright emits no Socket.IO event, so only the caller can react.
    throw new Error(message)
  }
}

export const useAppStore = create<AppState>()((set, get) => ({
  // Connection state
  isConnected: false,
  setConnected: (connected) => set({ isConnected: connected }),

  // Devices
  devices: [],
  deviceStates: {},
  isLoadingDevices: false,
  
  // Unguarded on purpose: dropping a concurrent call loses the post-flash refresh.
  fetchDevices: async (forceRefresh = false) => {
    set({ isLoadingDevices: true, error: null })
    const seq = ++_fetchSeq
    try {
      const devices = await apiClient.getDevices(forceRefresh)
      // A newer enumeration already landed: this response is older than what we show.
      if (seq !== _fetchSeq) return
      // Carry over the UI state of devices that are still here; the rest drop out.
      set((state) => ({
        devices,
        deviceStates: Object.fromEntries(
          devices
            .filter((d) => state.deviceStates[d.device_id])
            .map((d) => [d.device_id, { ...state.deviceStates[d.device_id], device: d }]),
        ),
        isLoadingDevices: false,
      }))

      // Like the legacy viewer, a camera is ready to use the moment it shows up: open
      // every one we have not seen (first load and a return from DFU alike).
      const appeared = devices.filter((d) => !get().deviceStates[d.device_id])
      await Promise.all(appeared.map((d) => get().openDevice(d)))
    } catch (error) {
      set({
        error: `Failed to fetch devices: ${error instanceof Error ? error.message : 'Unknown error'}`,
        isLoadingDevices: false,
      })
    }
  },

  updateFirmwareFromFile: (deviceId: string, file: File) =>
    performFirmwareUpdate(set, get, deviceId, () => apiClient.updateFirmwareFromFile(deviceId, file)),

  updateFirmwareFromRecommended: (deviceId: string) =>
    performFirmwareUpdate(set, get, deviceId, () => apiClient.updateFirmwareFromRecommended(deviceId)),

  enableMetadata: async () => {
    const result = await apiClient.enableMetadata()
    if (result.status === 'ok') await get().fetchDevices(true)
    return result
  },

  toggleAdvancedMode: async (deviceId: string, enable: boolean) => {
    try {
      // Restarts the device on the backend; wait, then refresh device + sensors + status.
      await apiClient.setAdvancedMode(deviceId, enable)
      await get().fetchDevices(true)
      await get().fetchSensors(deviceId)
      await get().fetchDeviceControls(deviceId)
    } catch (error) {
      set({ error: `Failed to ${enable ? 'enable' : 'disable'} advanced mode: ${error instanceof Error ? error.message : 'unknown error'}` })
    }
  },

  // The colorizer, the advanced controls and each sensor's filters have their own
  // endpoints rather than riding a sensor's option list. Rebuilt wholesale so groups that
  // stopped existing drop out, while a single source failing leaves its section empty.
  fetchDeviceControls: async (deviceId: string) => {
    const sensors = get().deviceStates[deviceId]?.sensors || []
    // The controls exist only while advanced mode is on - the device refuses to read them
    // otherwise - so its state decides whether to ask at all.
    const advancedMode = await apiClient.getAdvancedMode(deviceId).catch(() => undefined)
    const [colorizer, advanced, ...perSensor] = await Promise.all([
      apiClient.getColorizerOptions(deviceId).catch(() => [] as OptionInfo[]),
      advancedMode?.enabled
        ? apiClient.getAdvancedControls(deviceId).catch(() => ({} as AdvancedControls))
        : ({} as AdvancedControls),
      ...sensors.map((s) => apiClient.getSensorFilters(deviceId, s.sensor_id).catch(() => ({} as SensorFilters))),
    ])
    // The colorizer and the advanced controls belong to the device rather than to a
    // sensor, but the C++ viewer draws them with the depth sensor's controls, so they are
    // stored against it.
    const depthSensorId = sensors.find(
      (s) => s.supported_stream_profiles.some((p) => p.stream_type.toLowerCase() === 'depth')
    )?.sensor_id ?? ''
    const groups: Record<string, ControlGroup> = {
      colorizer: { section: 'Depth Visualization', sensorId: depthSensorId, name: '', options: colorizer },
    }
    // Firmware blocks the AE setpoint on D457 and on the D500 family, so the group is not
    // worth drawing there; the legacy viewer leaves it out for the same reason.
    const deviceName = get().deviceStates[deviceId]?.device.name ?? ''
    const noAeSetpoint = deviceName.includes('D457') || deviceName.includes('D5')
    for (const [name, options] of Object.entries(advanced)) {
      if (noAeSetpoint && name === 'ae_control') continue
      groups[`advanced_mode/controls/${name}`] =
        { section: 'Advanced Controls', sensorId: depthSensorId, name, options }
    }
    sensors.forEach((s, i) => {
      groups[`sensors/${s.sensor_id}/options`] =
        { section: 'Controls', sensorId: s.sensor_id, name: '', options: s.options }
      for (const [name, filter] of Object.entries(perSensor[i])) {
        groups[`sensors/${s.sensor_id}/filters/${name}`] =
          { section: 'Post-Processing', sensorId: s.sensor_id, name, ...filter }
      }
    })
    set((state) => patchDevice(state, deviceId, () => ({ controls: groups, advancedMode })))
  },

  setControl: async (deviceId, key, optionId, value) => {
    try {
      const applied = await apiClient.setControl(deviceId, key, optionId, value)
      set((state) => patchGroup(state, deviceId, key, (group) => ({
        ...group,
        options: group.options.map((o) =>
          // Match by option_id OR by label (case-insensitive) for chatbot compatibility
          (o.option_id === optionId || optionLabel(o.option_id).toLowerCase() === optionId.toLowerCase())
            ? applied
            : o
        ),
      })))
    } catch (error) {
      set({ error: `Failed to set option: ${error instanceof Error ? error.message : 'Unknown error'}` })
      throw error
    }
  },

  // While the sensor's post-processing switch is off nothing is applied, so the click only
  // records the choice for when it goes back on.
  setControlEnabled: async (deviceId, key, enabled) => {
    const ds = get().deviceStates[deviceId]
    const group = ds?.controls[key]
    if (!group?.sensorId) return
    if (ds?.postProcessing?.[group.sensorId] !== false) {
      await apiClient.setFilterEnabled(deviceId, key, enabled)
    }
    set((state) => patchGroup(state, deviceId, key, (g) => ({ ...g, enabled })))
  },

  setPostProcessing: async (deviceId, sensorId, enabled) => {
    // Choices stay in `controls` untouched, as in the legacy viewer: its master switch and
    // its per-filter flags are separate (processing-block-model.h).
    const filters = Object.entries(get().deviceStates[deviceId]?.controls || {})
      .filter(([, g]) => g.section === 'Post-Processing' && g.sensorId === sensorId)
    await Promise.all(
      filters.map(([key, g]) => apiClient.setFilterEnabled(deviceId, key, enabled && !!g.enabled))
    )
    set((state) => patchDevice(state, deviceId, (ds) =>
      ({ postProcessing: { ...ds.postProcessing, [sensorId]: enabled } })))
  },

  // Returns the recommendation too: a device that isn't activated has no state to store it
  // in. Rejects on failure — reporting is the caller's business.
  checkFirmwareUpdates: async (deviceId: string) => {
    const { recommended } = await apiClient.getRecommendedFirmware(deviceId)
    set((state) => {
      const ds = state.deviceStates[deviceId]
      if (!ds) return state
      return {
        deviceStates: {
          ...state.deviceStates,
          [deviceId]: { ...ds, firmware: { ...ds.firmware, recommended } },
        },
      }
    })
    return recommended
  },

  openDevice: async (device: DeviceInfo) => {
    const deviceState: DeviceState = {
      device,
      firmware: { is_updating: false, progress: undefined, last_error: null },
      sensors: [],
      controls: {},
      streamConfigs: [],
      sensorConfigs: {},
      isStreaming: false,
      isLoading: true,
      streamMetadata: {},
      sensorStreamingStatus: {},
    }
    set((s) => ({
      deviceStates: { ...s.deviceStates, [device.device_id]: deviceState },
    }))

    await get().fetchSensors(device.device_id)
    // Best-effort: a versions-DB outage shouldn't make opening a camera look like it failed.
    get().checkFirmwareUpdates(device.device_id).catch(() => {})
    get().fetchDeviceControls(device.device_id)
  },

  getDeviceStates: () => Object.values(get().deviceStates),

  isAnyDeviceStreaming: () => {
    const state = get()
    return Object.values(state.deviceStates).some(ds => ds.isStreaming)
  },

  resetDevice: async (deviceId) => {
    try {
      await apiClient.resetDevice(deviceId)
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

      const configs = buildStreamConfigs(sensors)
      const sensorConfigs = buildSensorConfigs(sensors)

      set((state) => patchDevice(state, deviceId, () =>
        ({ sensors, streamConfigs: configs, sensorConfigs, isLoading: false })))
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

  updateStreamConfig: (deviceId: string, config: StreamConfig) => {
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

    // Build configs array for all enabled streams
    // Motion sensors use per-stream FPS, others use sensor-level FPS
    const configs: SensorStreamConfig[] = enabledStreamConfigs.map(c => ({
      stream_type: c.stream_type,
      format: c.format,
      resolution: sensorConfig.isMotionSensor ? c.resolution : sensorConfig.resolution,
      framerate: sensorConfig.isMotionSensor ? c.framerate : sensorConfig.framerate,
    }))

    try {
      const status = await apiClient.startSensor(deviceId, sensorId, configs)
      
      set((s) => ({
        deviceStates: {
          ...s.deviceStates,
          [deviceId]: {
            ...s.deviceStates[deviceId],
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

  setSensorPaused: async (deviceId, sensorId, paused) => {
    try {
      const status = await apiClient.setSensorPaused(deviceId, sensorId, paused)
      set((s) => patchDevice(s, deviceId, (ds) =>
        ({ sensorStreamingStatus: { ...ds.sensorStreamingStatus, [sensorId]: status } })))
    } catch (error) {
      set({ error: `Failed to ${paused ? 'pause' : 'resume'} sensor: ${error instanceof Error ? error.message : 'unknown error'}` })
    }
  },

  togglePauseAll: async () => {
    const streaming: [string, string, boolean][] = []
    for (const ds of Object.values(get().deviceStates)) {
      for (const [sensorId, status] of Object.entries(ds.sensorStreamingStatus)) {
        if (status.is_streaming) streaming.push([ds.device.device_id, sensorId, !!status.paused])
      }
    }
    if (streaming.length === 0) return
    // Any sensor still running means "pause everything"; only when all are paused, resume.
    const pause = streaming.some(([, , paused]) => !paused)
    await Promise.all(streaming.map(([d, s]) => get().setSensorPaused(d, s, pause)))
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
        // Drop last point cloud so the 3D canvas doesn't freeze on the last
        // frame; a still-streaming sensor will repopulate within one frame.
        pointCloudVertices: null,
        pointCloudColors: null,
        deviceStates: {
          ...s.deviceStates,
          [deviceId]: {
            ...deviceState,
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
            metadataServerTime: metadata.timestamp_server,
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

      // Extract point cloud data if present.
      // Server sends raw float32 bytes as a Socket.IO binary attachment (ArrayBuffer);
      // fall back to base64 string for older servers.
      if (streamData.point_cloud?.vertices) {
        // Drop frames that arrive after the device was stopped — the server's
        // stop_broadcast can race with frames already in flight, and accepting
        // them would repopulate the cleared cloud and freeze the 3D canvas.
        if (!get().deviceStates[deviceId]?.isStreaming) continue
        try {
          const vertices = decodeFloat32Payload(streamData.point_cloud.vertices)
          // Colors are sampled server-side from the color frame (cpp-viewer
          // parity). Present only when depth+color are both active and the
          // color format is RGB8/BGR8.
          const colors = streamData.point_cloud.colors
            ? decodeUint8Payload(streamData.point_cloud.colors)
            : null
          set({ pointCloudVertices: vertices, pointCloudColors: colors })
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

  // UI state
  viewMode: '2d',
  setViewMode: async (mode) => {
    const prev = get().viewMode
    if (prev === mode) return
    set({ viewMode: mode })

    const activeDevices = Object.values(get().deviceStates)
    try {
      if (mode === '3d') {
        await Promise.all(
          activeDevices.map(ds => apiClient.enablePointCloud(ds.device.device_id))
        )
      } else {
        await Promise.all(
          activeDevices.map(ds => apiClient.disablePointCloud(ds.device.device_id))
        )
        set({ pointCloudVertices: null, pointCloudColors: null })
      }
    } catch (err) {
      // Roll back so the user can retry — otherwise viewMode is wedged at the
      // new mode and the `prev === mode` short-circuit at the top blocks the
      // retry click. Server may be partly-applied; the user is informed via
      // the error message and a re-click will reissue enable/disable on all
      // active devices.
      set({
        viewMode: prev,
        error: `Failed to ${mode === '3d' ? 'enable' : 'disable'} point cloud: ${
          err instanceof Error ? err.message : 'Unknown error'
        }`,
      })
    }
  },

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
            return { ...existingConfig, enable: false }
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
          await get().setControl(
            deviceId, `sensors/${uniqueSensorId}/options`, change.optionId, change.value
          )
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
        for (const sensorId of new Set(enabledConfigs.map(c => c.sensor_id))) {
          await get().startSensorStreaming(deviceId, sensorId)
        }
      } else if (settings.streamAction === 'stop') {
        const status = get().deviceStates[deviceId]?.sensorStreamingStatus || {}
        for (const [sensorId, ss] of Object.entries(status)) {
          if (ss.is_streaming) await get().stopSensorStreaming(deviceId, sensorId)
        }
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

  pointCloudVertices: null,
  pointCloudColors: null,
}))
