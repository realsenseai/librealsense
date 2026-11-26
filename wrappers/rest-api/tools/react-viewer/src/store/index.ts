import { create } from 'zustand'
import type {
  DeviceInfo,
  SensorInfo,
  OptionInfo,
  StreamConfig,
  ViewMode,
  DeviceState,
  AdvancedControls,
  ControlGroup,
  SensorFilters,
SensorStreamStatus,
  SupportedStreamProfile,
} from '../api/types'

// Tracks the in-flight legacy-chatbot request so `stopChatMessage` can abort it.
let currentChatAbortController: AbortController | null = null

// Enumerations are unordered across connections; only the newest response may be applied.
let _fetchSeq = 0

type States = { deviceStates: Record<string, DeviceState> }

// The filters the legacy viewer starts with on (common/subdevice-model.cpp): all but these, and
// decimation stays off on a color sensor
const LEGACY_OFF_FILTERS = new Set(['Hole Filling Filter', 'Filter By Sequence id', 'Rotation Filter', 'Threshold Filter'])
function legacyFilterDefault(name: string, colorSensor: boolean): boolean {
  return !LEGACY_OFF_FILTERS.has(name) && !(colorSensor && name === 'Decimation Filter')
}

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

/** The group with these options' values replaced */
const withValues = (g: ControlGroup, values: Record<string, OptionInfo['current_value']>): ControlGroup =>
  ({ ...g, options: g.options.map(o => o.option_id in values ? { ...o, current_value: values[o.option_id] } : o) })

const resolutionOf = (p: SupportedStreamProfile) => ({ width: p.width, height: p.height })

/**
 * The profile each stream starts with: its SDK default, else (infrared has none) the one at
 * the resolution/fps of the sensor's default, else its first.
 */
function startupProfiles(sensor: SensorInfo): SupportedStreamProfile[] {
  const profiles = sensor.stream_profiles
  const anchor = profiles.find(p => p.default) ?? profiles[0]
  return [...new Set(profiles.map(streamKey))].map(key => {
    const ofType = profiles.filter(p => streamKey(p) === key)
    return ofType.find(p => p.default)
      ?? ofType.find(p => p.width === anchor.width && p.height === anchor.height && p.fps === anchor.fps)
      ?? ofType[0]
  })
}

function buildStreamConfigs(sensors: SensorInfo[]): StreamConfig[] {
  return sensors.flatMap(sensor => startupProfiles(sensor).map(profile => {
    const streamTypeLower = profile.stream_type.toLowerCase()
    const enableByDefault =
      streamTypeLower === 'depth' || streamTypeLower === 'color' ||
      streamTypeLower === 'gyro' || streamTypeLower === 'accel'
    return {
      sensor_index: sensor.sensor_index,
      stream_type: streamKey(profile),
      format: profile.format,
      resolution: resolutionOf(profile),
      framerate: profile.fps,
      enable: enableByDefault,
    }
  }))
}

// The viewer hides infrared index 0
function visibleProfiles(profiles: SupportedStreamProfile[]) {
  return profiles.filter(p => streamKey(p) !== 'infrared-0')
}

/**
 * The device with each sensor's streaming as the server reports it, by sensor_index, whichever client started
 * it: a streaming sensor shows its streams selected as they run. Kept from before: a stop this page has in
 * flight, a start error, whether this page started it, and the profiles while they stay the same (a tile
 * reopens its socket when its profile object changes).
 */
function withStreaming(ds: DeviceState, active: Record<number, SupportedStreamProfile[]>): Partial<DeviceState> {
  const sensorStreamingStatus = { ...ds.sensorStreamingStatus }
  let { streamConfigs } = ds
for (const [key, listed] of Object.entries(active)) {
    const sensorIndex = Number(key)
    const was = sensorStreamingStatus[sensorIndex]
    const visible = visibleProfiles(listed)
    const profiles = JSON.stringify(visible) === JSON.stringify(was?.profiles) ? was!.profiles! : visible
    const streaming = profiles.length > 0
    sensorStreamingStatus[sensorIndex] = {
      ...was, sensor_index: sensorIndex, profiles,
      is_streaming: streaming && was?.pendingOp !== 'stopping', // shown stopped from the click on
      startedHere: streaming && !!was?.startedHere, pendingOp: streaming ? was?.pendingOp : null,
    }
    if (!streaming) continue
    streamConfigs = streamConfigs.map(c => {
      if (c.sensor_index !== sensorIndex) return c
      const p = profiles.find(p => streamKey(p) === c.stream_type)
      return p ? { ...c, format: p.format, resolution: resolutionOf(p), framerate: p.fps, enable: true } : { ...c, enable: false }
    })
  }
  return { sensorStreamingStatus, streamConfigs }
}
import { apiClient } from '../api/client'
import { recommendedFirmware } from '../api/firmware'
import { isStreaming, optionLabel, profilesAt, streamKey } from '../api/types'
import {
  checkChatAvailability,
  sendChatMessage as sendChatMessageApi,
  generateMessageId,
  type ChatMessage,
  type ChatResponse,
} from '../api/chat'
import type { ProposedSettings } from '../utils/chatPrompt'
import type { AssistantChatMessage, AssistantFileAttachment } from '../api/assistantChat'
import { createAssistantSlice } from './assistantSlice'

interface AppState {
  // Connection state
  // The socket retries forever, so a lost connection is always being re-established.
  connection: 'starting' | 'connected' | 'reconnecting'
  setConnected: (connected: boolean) => void

  // Devices - multi-camera support
  devices: DeviceInfo[]
  deviceStates: Record<string, DeviceState> // keyed by device_id
  isLoadingDevices: boolean
  fetchDevices: () => Promise<void>
  enableMetadata: () => Promise<void>
  checkFirmwareUpdates: (deviceId: string) => Promise<string | undefined>
  toggleAdvancedMode: (deviceId: string, enable: boolean) => Promise<void>
  fetchDeviceControls: (deviceId: string) => Promise<void>
  /** One group of controls as the device now holds it, after any client wrote it */
  refreshControlGroup: (deviceId: string, key: string) => Promise<void>
  /** An options_changed event: the new values the SDK reported */
  applyOptionsChanged: (deviceId: string, key: string, values: Record<string, OptionInfo['current_value'] | null>) => Promise<void>
  setControl: (deviceId: string, key: string, optionId: string, value: number | boolean | string) => Promise<void>
  setControlEnabled: (deviceId: string, key: string, enabled: boolean) => Promise<void>
  setPostProcessing: (deviceId: string, sensorIndex: number, enabled: boolean) => Promise<void>

  // Device activation (multi-select support)
  toggleDeviceActive: (device: DeviceInfo) => Promise<void>
  getActiveDevices: () => DeviceState[]
  isAnyDeviceStreaming: () => boolean
  
  resetDevice: (deviceId: string) => Promise<void>

  // Per-device sensors fetch
  fetchSensors: (deviceId: string) => Promise<void>
  /** What each sensor of the device streams now, by sensor_index, as the server pushes it on any client's start or stop */
  applyStreaming: (deviceId: string, active: Record<number, SupportedStreamProfile[]>) => void

  // Per-device stream configuration
  updateStreamConfig: (deviceId: string, config: StreamConfig) => void

  // Per-sensor streaming (sensor API)
  startSensorStreaming: (deviceId: string, sensorIndex: number) => Promise<void>
  stopSensorStreaming: (deviceId: string, sensorIndex: number) => Promise<void>

  // UI state
  viewMode: ViewMode
  setViewMode: (mode: ViewMode) => void

  // Chat/AI Assistant state
  isChatOpen: boolean
  isChatAvailable: boolean
  isChatLoading: boolean
  chatMessages: ChatMessage[]
  pendingSettings: ProposedSettings | null
  toggleChat: () => void
  checkChatAvailability: () => Promise<void>
  sendChatMessage: (content: string) => Promise<void>
  stopChatMessage: () => void
  applyProposedSettings: () => Promise<void>
  dismissProposedSettings: () => void
  clearChat: () => void

  // RealSense AI Assistant state (hosted, anonymous product Q&A — separate from the chat slice above)
  isAssistantOpen: boolean
  isAssistantOnline: boolean
  isAssistantLoading: boolean
  assistantMessages: AssistantChatMessage[]
  assistantConversationId: string | null
  assistantSize: 'compact' | 'wide'
  toggleAssistant: () => void
  pingAssistantHealth: () => Promise<void>
  sendAssistantMessage: (
    content: string,
    attachments?: { imageDataUris?: string[]; fileDataUris?: AssistantFileAttachment[] }
  ) => Promise<void>
  regenerateLastAssistantMessage: () => Promise<void>
  stopAssistantMessage: () => void
  sendAssistantReaction: (value: 1 | -1) => Promise<boolean>
  clearAssistantChat: () => void
  toggleAssistantSize: () => void

  // Error handling
  error: string | null
  setError: (error: string | null) => void
  clearError: () => void
}

export type { AppState }

export const useAppStore = create<AppState>()((set, get, api) => ({
  // Connection state
  connection: 'starting',
  setConnected: (connected) => set({ connection: connected ? 'connected' : 'reconnecting' }),

  // Devices
  devices: [],
  deviceStates: {},
  isLoadingDevices: false,
  
  // Unguarded on purpose: dropping a concurrent call loses the post-flash refresh.
  fetchDevices: async () => {
    set({ isLoadingDevices: true, error: null })
    const seq = ++_fetchSeq
    try {
      const devices = await apiClient.getDevices()
      // A newer enumeration already landed: this response is older than what we show.
      if (seq !== _fetchSeq) return
      const known = new Set(get().devices.map((d) => d.device_id))
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

      // A camera just showed up and it is the only one: open it. Covers first load and a
      // return from DFU alike, and leaves a camera the user closed closed.
      const appeared = devices.filter((d) => !known.has(d.device_id))
      if (devices.length === 1 && appeared.length === 1 && get().getActiveDevices().length === 0) {
        await get().toggleDeviceActive(devices[0])
      }
    } catch (error) {
      set({
        error: `Failed to fetch devices: ${error instanceof Error ? error.message : 'Unknown error'}`,
        isLoadingDevices: false,
      })
    }
  },

  enableMetadata: async () => {
    await apiClient.enableMetadata()
    await get().fetchDevices()
  },

  toggleAdvancedMode: async (deviceId: string, enable: boolean) => {
    try {
      // The device resets: devices_changed drops it and brings it back, which reloads it as a new one
      await apiClient.setAdvancedMode(deviceId, enable)
    } catch (error) {
      set({ error: `Failed to ${enable ? 'enable' : 'disable'} advanced mode: ${error instanceof Error ? error.message : 'unknown error'}` })
    }
  },

  // Rebuilt wholesale so groups that stopped existing drop out, while a single source
  // failing leaves its section empty.
  fetchDeviceControls: async (deviceId: string) => {
    const sensors = get().deviceStates[deviceId]?.sensors || []
    // The controls exist only while advanced mode is on - the device refuses to read them
    // otherwise - so its camera info decides whether to ask at all.
    const advancedMode = get().deviceStates[deviceId]?.device.info.advanced_mode === 'YES'
    const [colorizer, advanced, options, filters] = await Promise.all([
      apiClient.getColorizerOptions(deviceId).catch(() => [] as OptionInfo[]),
      advancedMode
        ? apiClient.getAdvancedControls(deviceId).catch(() => ({} as AdvancedControls))
        : ({} as AdvancedControls),
      Promise.all(sensors.map((s) =>
        apiClient.getSensorOptions(deviceId, s.sensor_index).catch(() => [] as OptionInfo[]))),
      Promise.all(sensors.map((s) =>
        apiClient.getSensorFilters(deviceId, s.sensor_index).catch(() => ({} as SensorFilters)))),
    ])
    // The colorizer and the advanced controls belong to the device rather than to a
    // sensor, but the C++ viewer draws them with the depth sensor's controls, so they are
    // stored against it.
    const depthSensorId = sensors.find(
      (s) => s.stream_profiles.some((p) => p.stream_type.toLowerCase() === 'depth')
    )?.sensor_index
    const groups: Record<string, ControlGroup> = {
      colorizer: { section: 'Depth Visualization', sensorIndex: depthSensorId, name: '', options: colorizer },
    }
    // Firmware blocks the AE setpoint on D457 and on the D500 family, so the group is not
    // worth drawing there; the legacy viewer leaves it out for the same reason.
    const info = get().deviceStates[deviceId]?.device.info
    const noAeSetpoint = info?.name.includes('D457') || info?.product_line === 'D500'
    for (const [name, options] of Object.entries(advanced)) {
      if (noAeSetpoint && name === 'ae_control') continue
      groups[`advanced_mode/controls/${name}`] =
        { section: 'Advanced Controls', sensorIndex: depthSensorId, name, options }
    }
    sensors.forEach((s, i) => {
      groups[`sensors/${s.sensor_index}/options`] =
        { section: 'Controls', sensorIndex: s.sensor_index, name: '', options: options[i] }
      const color = s.stream_profiles.some((p) => p.stream_type.toLowerCase() === 'color')
      const sequenceIds = options[i].some((o) => o.option_id === 'sequence_id')
      for (const [name, filter] of Object.entries(filters[i])) {
        // The legacy viewer leaves HDR Merge out where the sensor has no sequence ids
        if (name === 'HDR Merge' && !sequenceIds) continue
        groups[`sensors/${s.sensor_index}/filters/${name}`] =
          { section: 'Post-Processing', sensorIndex: s.sensor_index, name, ...filter, default_enabled: legacyFilterDefault(name, color) }
      }
    })
    set((state) => patchDevice(state, deviceId, () => ({ controls: groups })))
  },

  refreshControlGroup: async (deviceId, key) => {
    const read = async (): Promise<Partial<ControlGroup>> => {
      if (key === 'colorizer') return { options: await apiClient.getColorizerOptions(deviceId) }
      const [source, index, kind, name] = key.split('/') // sensors/0/options, sensors/0/filters/<name>
      // advanced_mode/controls/<group>
      if (source === 'advanced_mode') return { options: (await apiClient.getAdvancedControls(deviceId))[kind] }
      if (kind === 'options') return { options: await apiClient.getSensorOptions(deviceId, Number(index)) }
      const { enabled, options } = (await apiClient.getSensorFilters(deviceId, Number(index)))[name]
      // While the sensor's post-processing is off, the server has every filter off but the viewer keeps the choices
      return get().deviceStates[deviceId]?.postProcessing?.[Number(index)] === false ? { options } : { enabled, options }
    }
    const fresh = await read()
    set((state) => patchGroup(state, deviceId, key, (group) => ({ ...group, ...fresh })))
  },

  applyOptionsChanged: async (deviceId, key, values) => {
    const group = get().deviceStates[deviceId]?.controls[key]
    if (!group) return
    // One the page doesn't show, or that can't be read now, changes which options show: read the group again
    if (Object.entries(values).some(([id, v]) => v === null || !group.options.some(o => o.option_id === id))) {
      return get().refreshControlGroup(deviceId, key)
    }
    set((state) => patchGroup(state, deviceId, key, (g) => withValues(g, values as Record<string, OptionInfo['current_value']>)))
  },

  setControl: async (deviceId, key, optionId, value) => {
    try {
      await apiClient.setControl(deviceId, key, optionId, value)
    } catch (error) {
      set({ error: `Failed to set option: ${error instanceof Error ? error.message : 'Unknown error'}` })
      throw error
    }
    // What the device took, which may be clamped, as the legacy viewer reads back its own write. The write
    // stands even if it can't be read now: the group is read again instead
    try {
      const taken = await apiClient.getControlValue(deviceId, key, optionId)
      if (taken === null) throw new Error('unreadable now')
      set((state) => patchGroup(state, deviceId, key, (g) => withValues(g, { [optionId]: taken })))
    } catch {
      await get().refreshControlGroup(deviceId, key).catch(() => {}) // a device gone meanwhile
    }
  },

  // While the sensor's post-processing switch is off nothing is applied, so the click only
  // records the choice for when it goes back on.
  setControlEnabled: async (deviceId, key, enabled) => {
    const ds = get().deviceStates[deviceId]
    const group = ds?.controls[key]
    if (group?.sensorIndex === undefined) return
    if (ds?.postProcessing?.[group.sensorIndex] !== false) {
      await apiClient.setFilterEnabled(deviceId, key, enabled)
    }
    set((state) => patchGroup(state, deviceId, key, (g) => ({ ...g, enabled })))
  },

  setPostProcessing: async (deviceId, sensorIndex, enabled) => {
    // Choices stay in `controls` untouched, as in the legacy viewer: its master switch and
    // its per-filter flags are separate (processing-block-model.h).
    const filters = Object.entries(get().deviceStates[deviceId]?.controls || {})
      .filter(([, g]) => g.section === 'Post-Processing' && g.sensorIndex === sensorIndex)
    await Promise.all(
      filters.map(([key, g]) => apiClient.setFilterEnabled(deviceId, key, enabled && !!g.enabled))
    )
    set((state) => patchDevice(state, deviceId, (ds) =>
      ({ postProcessing: { ...ds.postProcessing, [sensorIndex]: enabled } })))
  },

  // Returns the recommendation too: a device that isn't activated has no state to store it
  // in. Rejects on failure — reporting is the caller's business.
  checkFirmwareUpdates: async (deviceId: string) => {
    const name = get().devices.find((d) => d.device_id === deviceId)?.info.name ?? ''
    const recommended = (await recommendedFirmware(name))?.version
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

  toggleDeviceActive: async (device: DeviceInfo) => {
    const state = get()
    const existing = state.deviceStates[device.device_id]
    
    if (existing) {
// Deactivate: stop what this page started, then remove; other clients' streams go on
      for (const [key, status] of Object.entries(existing.sensorStreamingStatus)) {
        if (status.is_streaming && status.startedHere) await get().stopSensorStreaming(device.device_id, Number(key))
      }
      set((s) => {
        const newStates = { ...s.deviceStates }
        delete newStates[device.device_id]
        return { deviceStates: newStates }
      })
    } else {
      // Activate: create device state and fetch sensors
      const deviceState: DeviceState = {
        device,
        sensors: [],
        controls: {},
        streamConfigs: [],
isLoading: true,
        sensorStreamingStatus: {},
      }
      set((s) => ({
        deviceStates: { ...s.deviceStates, [device.device_id]: deviceState },
      }))
      
      // Fetch sensors for this device
      await get().fetchSensors(device.device_id)
      // Best-effort: a versions-DB outage shouldn't make opening a camera look like it failed.
      get().checkFirmwareUpdates(device.device_id).catch(() => {})
      get().fetchDeviceControls(device.device_id).then(async () => {
        // The server starts every filter off; the viewer starts from the legacy viewer's. A filter
        // already on was chosen by a client (this one earlier, or another), so the server's are kept.
        const filters = Object.entries(get().deviceStates[device.device_id]?.controls ?? {})
          .filter(([, g]) => g.section === 'Post-Processing')
        if (filters.some(([, g]) => g.enabled)) return
        // Last first: Disparity to Depth is on before Depth to Disparity, so a running stream never
        // sends disparity frames in between
        for (const [key, g] of filters.reverse()) {
          if (g.default_enabled) await get().setControlEnabled(device.device_id, key, true)
        }
      }).catch((error) => set({ error: `Failed to enable the default filters: ${error instanceof Error ? error.message : 'unknown error'}` }))
    }
  },

  getActiveDevices: () => Object.values(get().deviceStates),

  isAnyDeviceStreaming: () => Object.values(get().deviceStates).some(isStreaming),

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
      const sensors = (await apiClient.getSensors(deviceId)).map(s => ({
        ...s,
        stream_profiles: visibleProfiles(s.stream_profiles),
      }))
      const fresh = { sensors, streamConfigs: buildStreamConfigs(sensors), isLoading: false }
      const active = Object.fromEntries(sensors.map(s => [s.sensor_index, s.active_streams]))

      set((state) => patchDevice(state, deviceId, (ds) => ({ ...fresh, ...withStreaming({ ...ds, ...fresh }, active) })))
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

  applyStreaming: (deviceId, active) => set((state) => patchDevice(state, deviceId, (ds) => withStreaming(ds, active))),

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
              c.sensor_index === config.sensor_index && c.stream_type === config.stream_type ? config : c
            ),
          },
        },
      }
    })
  },

// Per-sensor streaming (sensor API)
  startSensorStreaming: async (deviceId, sensorIndex) => {
const state = get()
    const deviceState = state.deviceStates[deviceId]
    if (!deviceState) return

    // Find ALL enabled stream configs for this sensor (not just first)
    const enabledStreamConfigs = deviceState.streamConfigs.filter(
      c => c.sensor_index === sensorIndex && c.enable
    )
    if (enabledStreamConfigs.length === 0) {
      set({ error: 'Enable at least one stream for this sensor' })
      return
    }

    // A sensor with one resolution/fps has it on each of its streams (DevicePanel writes it to all)
    const sensor = deviceState.sensors.find(s => s.sensor_index === sensorIndex)!
    const profiles: SupportedStreamProfile[] = []
    for (const c of enabledStreamConfigs) {
      const { resolution, framerate } = c
const profile = profilesAt(sensor, c.stream_type, resolution, framerate).find(p => p.format === c.format)
      if (!profile) {
        set({ error: `${c.stream_type} has no ${c.format} profile at ${resolution.width}x${resolution.height} ${framerate}fps` })
        return
      }
      profiles.push(profile)
    }

    try {
      await apiClient.startSensor(deviceId, sensorIndex, profiles)
      // As the server's push, which may have come first: the same merge keeps the tiles' profile objects
      set((s) => ({ ...patchDevice(s, deviceId, (ds) => {
        const started = withStreaming(ds, { [sensorIndex]: profiles })
        started.sensorStreamingStatus![sensorIndex].startedHere = true
        return started
      }), error: null }))
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
              [sensorIndex]: {
                sensor_index: sensorIndex,
                is_streaming: false,
                error: errorMessage,
              },
            },
          },
        },
      }))
    }
  },

  stopSensorStreaming: async (deviceId, sensorIndex) => {
    // Shown stopped from the click on; the switch stays off until the stop is done
    set((s) => patchDevice(s, deviceId, (ds) => ({ sensorStreamingStatus: { ...ds.sensorStreamingStatus,
      [sensorIndex]: { ...ds.sensorStreamingStatus[sensorIndex], sensor_index: sensorIndex,
        is_streaming: false, pendingOp: 'stopping' } } })))
    try {
      await apiClient.stopSensor(deviceId, sensorIndex)
      get().applyStreaming(deviceId, { [sensorIndex]: [] })
    } catch (error) {
      set({ error: `Failed to stop sensor: ${error instanceof Error ? error.message : 'Unknown error'}` })
      // Still held, or the device is gone: show what the server has
      const sensors = await apiClient.getSensors(deviceId).catch(() => [])
      set((s) => patchDevice(s, deviceId, (ds) => ({ sensorStreamingStatus: { ...ds.sensorStreamingStatus,
        [sensorIndex]: { ...ds.sensorStreamingStatus[sensorIndex], pendingOp: null } } })))
      get().applyStreaming(deviceId, { [sensorIndex]: sensors.find(s => s.sensor_index === sensorIndex)?.active_streams ?? [] })
    }
  },

  // UI state
  viewMode: '2d',
  setViewMode: (mode) => set({ viewMode: mode }),

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

    // A prior turn must be cancelled first, otherwise it keeps running un-stoppably
    // in the background since only the latest controller stays reachable.
    currentChatAbortController?.abort()
    const controller = new AbortController()
    currentChatAbortController = controller

    try {
      // Get all messages for context
      const allMessages = [...state.chatMessages, userMessage]
      
      // Send to API with device context
      const response: ChatResponse = await sendChatMessageApi(allMessages, state.deviceStates, controller.signal)

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
      }))
    } catch (error) {
      if (error instanceof DOMException && error.name === 'AbortError') {
        // User-initiated stop — no error message, just stop loading.
      } else {
        const errorMessage: ChatMessage = {
          id: generateMessageId(),
          role: 'assistant',
          content: `Sorry, I encountered an error: ${error instanceof Error ? error.message : 'Unknown error'}. Please try again.`,
          timestamp: Date.now(),
        }
        set((s) => ({ chatMessages: [...s.chatMessages, errorMessage] }))
      }
    } finally {
      if (currentChatAbortController === controller) currentChatAbortController = null
      set({ isChatLoading: false })
    }
  },

  stopChatMessage: () => {
    currentChatAbortController?.abort()
  },

  applyProposedSettings: async () => {
    const state = get()
    const settings = state.pendingSettings
    if (!settings) return
    
    try {
      const deviceId = settings.deviceId
      const deviceState = state.deviceStates[deviceId]
      if (!deviceState) {
        throw new Error(`Device ${deviceId} not found`)
      }
      
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
              // Merge proposed config with existing, keeping sensor_index from existing
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
          // The API takes a sensor's index; its name ("RGB Camera") is only a fallback, as names need not be unique
          const match = deviceState.sensors?.find(s => s.sensor_index === change.sensorIndex)
            ?? deviceState.sensors?.find(s => s.name === change.sensorId)
          if (!match) throw new Error(`No sensor ${change.sensorIndex ?? `named "${change.sensorId}"`}`)
          const key = `sensors/${match.sensor_index}/options`
          // It names the option as the user sees it ("Laser Power"); the API takes its ID
          const named = change.optionId.toLowerCase()
          const option = deviceState.controls[key]?.options.find(o =>
            [o.option_id, optionLabel(o.option_id)].some(n => n.toLowerCase() === named))
          await get().setControl(deviceId, key, option?.option_id ?? change.optionId, change.value)
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
        for (const sensorIndex of new Set(enabledConfigs.map(c => c.sensor_index))) {
          await get().startSensorStreaming(deviceId, sensorIndex)
        }
      } else if (settings.streamAction === 'stop') {
        const status = get().deviceStates[deviceId]?.sensorStreamingStatus || {}
        for (const [key, ss] of Object.entries(status)) {
          if (ss.is_streaming) await get().stopSensorStreaming(deviceId, Number(key))
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

  // RealSense AI Assistant state — extracted to assistantSlice.ts
  ...createAssistantSlice(set, get, api),

  // Error handling
  error: null,
  setError: (error) => set({ error }),
  clearError: () => set({ error: null }),

}))
