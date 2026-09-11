import { useCallback, useEffect, useRef, useState } from 'react'
import type { MutableRefObject, ReactElement } from 'react'
import { useAppStore } from '../store'
import { apiClient } from '../api/client'
import type { ControlGroup, DeviceInfo, SensorInfo, OptionInfo, StreamConfig, DeviceState, FirmwareState, SensorConfig } from '../api/types'
import { SECTIONS, firmwareStatus, optionLabel } from '../api/types'
import { RefreshCcw, Search, X } from 'lucide-react'
import { FirmwareProgressModal } from './FirmwareProgressModal'
import { ToastContainer, type ToastType, type ToastAction } from './Toast'
import { searchGroup } from '../utils/optionSearch'
import { unsupportedStreams } from '../utils/streamModes'
import { Transport } from './playback/Transport'
import { RecordButton } from './record/RecordButton'
import { UpdatesDialog } from './updates/UpdatesDialog'
import { lessScreamy } from '../utils/metadataDecoders'
import { Collapsible, ToggleSwitch } from './Collapsible'

interface Toast {
  id: string
  type: ToastType
  message: string
  action?: ToastAction
}

// Toast once per disabled-device set; re-arm when the set becomes empty.
function showMetadataEnablePromptIfNeeded(
  devices: DeviceInfo[],
  promptedSetRef: MutableRefObject<string>,
  enableInFlightRef: MutableRefObject<boolean>,
  addToast: (type: ToastType, message: string, action?: ToastAction) => void,
  removeToast: (id: string) => void,
  enableMetadata: () => Promise<{ status: string; note?: string }>,
): void {
  const disabledIds = devices
    .filter(d => d.device_id && d.metadata_enabled === false)
    .map(d => d.device_id)
    .sort()
    .join(' ')
  if (!disabledIds) { promptedSetRef.current = ''; return }
  if (disabledIds === promptedSetRef.current) return
  promptedSetRef.current = disabledIds
  addToast(
    'info',
    'Frame metadata is disabled for connected RealSense devices on this Windows host. ' +
      'Enable it once (admin required) to surface per-frame metadata.',
    {
      label: 'Enable',
      onClick: async (toastId) => {
        if (enableInFlightRef.current) return
        enableInFlightRef.current = true
        try {
          const result = await enableMetadata()
          removeToast(toastId)
          const variant = result.status === 'ok' ? 'success' : 'info'
          addToast(variant, result.note ?? 'Done.')
        } catch (err: any) {
          removeToast(toastId)
          addToast('error', `Failed to enable metadata: ${err?.response?.data?.detail ?? err?.message ?? 'unknown error'}`)
        } finally {
          enableInFlightRef.current = false
        }
      },
    },
  )
}

// Prompt once per (device, recommended version): a newer recommendation later re-prompts,
// and dismissing the toast is enough — no persisted dismissal state. Same pattern as the
// metadata prompt above. The serial number is in the message because the toast is global
// while the proposal is per-camera.
function showFirmwareUpdatePromptsIfNeeded(
  deviceStates: Record<string, DeviceState>,
  promptedRef: MutableRefObject<Set<string>>,
  addToast: (type: ToastType, message: string, action?: ToastAction) => void,
  removeToast: (id: string) => void,
  onUpdate: (device: DeviceInfo) => void,
): void {
  const live = new Set<string>()
  for (const ds of Object.values(deviceStates)) {
    const fw = ds.firmware
    if (firmwareStatus(ds.device.firmware_version, fw?.recommended) !== 'outdated') continue
    const key = `${ds.device.device_id}:${fw?.recommended ?? ''}`
    live.add(key)
    if (promptedRef.current.has(key)) continue
    promptedRef.current.add(key)
    addToast(
      'info',
      `${ds.device.name} (S/N ${ds.device.serial_number}): firmware ${ds.device.firmware_version ?? '?'} → ${fw?.recommended ?? '?'} is available.`,
      {
        label: 'Update',
        onClick: (toastId) => {
          removeToast(toastId)
          onUpdate(ds.device)
        },
      },
    )
  }
  // Re-arm for proposals that are gone (device removed, or flashed and now up to date).
  promptedRef.current = live
}

// Reusable hidden-file-input hook. Returns the JSX to render once at a stable
// location in the tree (so the OS file picker callback fires even after the
// menu that triggered it unmounts) and an `open()` function to trigger it.
function useFilePicker(onPick: (file: File) => void, accept: string): {
  open: () => void
  input: ReactElement
} {
  const ref = useRef<HTMLInputElement>(null)
  const open = () => ref.current?.click()
  const input = (
    <input
      ref={ref}
      type="file"
      accept={accept}
      className="hidden"
      onClick={(e) => e.stopPropagation()}
      onChange={(e) => {
        const f = e.target.files?.[0]
        if (f) onPick(f)
        // Reset so selecting the same file again still triggers onChange.
        e.target.value = ''
      }}
    />
  )
  return { open, input }
}

export function DevicePanel() {
  const {
    devices,
    deviceStates,
    isLoadingDevices,
    fetchDevices,
    enableMetadata,
    resetDevice,
    error,
    clearError,
    updateStreamConfig,
    updateSensorConfig,
    startSensorStreaming,
    stopSensorStreaming,
    checkFirmwareUpdates,
    updateFirmwareFromFile,
    updateFirmwareFromRecommended,
    toggleAdvancedMode,
    uploadRecording,
    uploadPreset,
    savePreset,
  } = useAppStore()
  const recordingPicker = useFilePicker((file) => void uploadRecording(file), '.bag,.db3')

  const [toasts, setToasts] = useState<Toast[]>([])
  // Only one FW update can run at a time, so a single-value state is enough.
  // Snapshot, not a lookup: the camera leaves the device list while it is in DFU.
  const [firmwareProgressDevice, setFirmwareProgressDevice] = useState<DeviceInfo | null>(null)
  const [firmwareProgressState, setFirmwareProgressState] = useState<FirmwareState | null>(null)
  const [firmwareFileName, setFirmwareFileName] = useState<string | null>(null)
  const [updatesFor, setUpdatesFor] = useState<DeviceInfo | null>(null)

  useEffect(() => {
    fetchDevices(true)
  }, [fetchDevices])

  // Counter, not just Date.now(): several cameras can raise a firmware proposal in the
  // same tick, and duplicate keys would make React drop all but one toast.
  const toastSeqRef = useRef(0)
  const addToast = (type: ToastType, message: string, action?: ToastAction) => {
    const id = `${Date.now()}-${toastSeqRef.current++}`
    setToasts((prev) => [...prev, { id, type, message, action }])
  }

  const removeToast = (id: string) => {
    setToasts((prev) => prev.filter((t) => t.id !== id))
  }

  const handleFirmwareProgressUpdate = (progress: number, phase?: 'downloading' | 'installing') => {
    setFirmwareProgressState((prev) =>
      prev
        ? { ...prev, progress, ...(phase ? { phase } : {}) }
        : { is_updating: true, progress, phase }
    )
  }

  const handleFirmwareSuccess = (fwVersion: string | null) => {
    setFirmwareProgressState((prev) =>
      prev
        ? { ...prev, progress: 1.0, is_updating: false }
        : { is_updating: false, progress: 1.0 }
    )
    addToast('success', `Firmware update successful! Version: ${fwVersion || 'Unknown'}`)
    // Backend has already refreshed the device list (in _refresh_until_device_returns)
    // by the time it emits firmware_update_success; pull the new device_infos so
    // the UI shows the updated firmware_version without a manual Refresh click.
    fetchDevices(true)
  }

  const handleFirmwareError = (error: string) => {
    setFirmwareProgressState((prev) =>
      prev
        ? { ...prev, is_updating: false, last_error: error }
        : { is_updating: false, progress: 0, last_error: error }
    )
    addToast('error', `Firmware update failed: ${error}`)
    // Device may or may not have returned; refresh so the UI reflects the
    // current connection state (e.g. stuck-in-DFU disappears from the list).
    fetchDevices(true)
  }

  const handleCloseFirmwareModal = () => {
    setFirmwareProgressDevice(null)
    setFirmwareProgressState(null)
    setFirmwareFileName(null)
  }

  const startFirmwareUpdate = useCallback(
    (device: DeviceInfo, label: string, phase: FirmwareState['phase'], run: (deviceId: string) => Promise<void>) => {
      setFirmwareFileName(label)
      setFirmwareProgressDevice(device)
      setFirmwareProgressState({ is_updating: true, phase, progress: 0, last_error: null })
      // Rejected outright: no Socket.IO event will arrive, so close the modal ourselves.
      run(device.device_id).catch((error: Error) => {
        handleCloseFirmwareModal()
        addToast('error', error.message)
      })
    },
    [],
  )

  const handleUpdateFirmwareFromFile = (device: DeviceInfo, file: File) =>
    startFirmwareUpdate(device, file.name, undefined, (deviceId) => updateFirmwareFromFile(deviceId, file))

  const handleUpdateFromRecommended = useCallback((device: DeviceInfo) => {
    const recommended = useAppStore.getState().deviceStates[device.device_id]?.firmware?.recommended
    const label = recommended ? `Firmware ${recommended}` : 'Recommended firmware'
    startFirmwareUpdate(device, label, 'downloading', updateFirmwareFromRecommended)
  }, [startFirmwareUpdate, updateFirmwareFromRecommended])

  const promptedSetRef = useRef<string>('')
  const enableInFlightRef = useRef<boolean>(false)
  useEffect(() => {
    showMetadataEnablePromptIfNeeded(devices, promptedSetRef, enableInFlightRef, addToast, removeToast, enableMetadata)
  }, [devices])

  // Explicit "Check for Firmware Updates": an outdated result raises the proposal toast
  // via the effect below, so only the no-proposal outcomes need reporting here.
  const handleCheckFirmwareUpdates = async (deviceId: string) => {
    let recommended: string | undefined
    try {
      recommended = await checkFirmwareUpdates(deviceId)
    } catch (error) {
      addToast('error', `Failed to check firmware updates: ${error instanceof Error ? error.message : 'Unknown error'}`)
      return
    }
    const device = deviceStates[deviceId]?.device ?? devices.find((d) => d.device_id === deviceId)
    const status = firmwareStatus(device?.firmware_version, recommended)
    if (status === 'up_to_date') addToast('success', 'Firmware is up to date.')
    else if (status === 'unknown') addToast('info', 'No firmware recommendation available for this device.')
  }

  const promptedFirmwareRef = useRef<Set<string>>(new Set())
  useEffect(() => {
    showFirmwareUpdatePromptsIfNeeded(deviceStates, promptedFirmwareRef, addToast, removeToast, handleUpdateFromRecommended)
  }, [deviceStates, handleUpdateFromRecommended])

  return (
    <div className="p-4">
      {recordingPicker.input}
      <div className="flex items-center justify-between mb-4">
        <h2 className="panel-header mb-0">Devices</h2>
        <div className="flex items-center gap-1">
        <button
          onClick={() => recordingPicker.open()}
          aria-label="Load recorded sequence"
          title="Load Recorded Sequence (.bag / .db3)"
          className="p-2 hover:bg-gray-700 rounded-lg transition-colors"
        >
          <svg className="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M3 7a2 2 0 012-2h4l2 2h8a2 2 0 012 2v8a2 2 0 01-2 2H5a2 2 0 01-2-2V7z" />
          </svg>
        </button>
        <button
          onClick={() => fetchDevices(true)}
          disabled={isLoadingDevices}
          aria-label={isLoadingDevices ? 'Refreshing devices…' : 'Refresh devices'}
          aria-busy={isLoadingDevices}
          className="p-2 hover:bg-gray-700 rounded-lg transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
          title={isLoadingDevices ? 'Refreshing…' : 'Refresh devices'}
        >
          <RefreshCcw className={`w-5 h-5 ${isLoadingDevices ? 'animate-spin' : ''}`} />
        </button>
        </div>
      </div>

      {/* Error Display */}
      {error && (
        <div className="mb-4 p-3 bg-red-900/50 border border-red-700 rounded-lg text-sm">
          <div className="flex justify-between items-start">
            <span>{error}</span>
            <button onClick={clearError} className="text-red-400 hover:text-red-300">
              ×
            </button>
          </div>
        </div>
      )}

      {/* Device List */}
      {devices.length === 0 ? (
        <div className="text-gray-500 text-center py-8">
          {isLoadingDevices ? (
            <div className="flex flex-col items-center">
              <div className="w-8 h-8 border-2 border-rs-blue border-t-transparent rounded-full animate-spin mb-2" />
              <span>Searching for devices...</span>
            </div>
          ) : (
            <div>
              <svg
                className="w-12 h-12 mx-auto mb-2 opacity-50"
                fill="none"
                stroke="currentColor"
                viewBox="0 0 24 24"
              >
                <path
                  strokeLinecap="round"
                  strokeLinejoin="round"
                  strokeWidth={1}
                  d="M9.75 17L9 20l-1 1h8l-1-1-.75-3M3 13h18M5 17h14a2 2 0 002-2V5a2 2 0 00-2-2H5a2 2 0 00-2 2v10a2 2 0 002 2z"
                />
              </svg>
              <p>No devices found</p>
              <p className="text-sm mt-1">Connect a RealSense device</p>
            </div>
          )}
        </div>
      ) : (
        <div className="space-y-3">
          {devices.map((device) => {
            const deviceState = deviceStates[device.device_id]
            return (
              <DeviceCard
                key={device.device_id}
                device={device}
                deviceState={deviceState}
                onReset={() => resetDevice(device.device_id)}
                onUpdateStreamConfig={(config) => updateStreamConfig(device.device_id, config)}
                onUpdateSensorConfig={(sensorId, config) => updateSensorConfig(device.device_id, sensorId, config)}
                onStartSensorStreaming={(sensorId) => startSensorStreaming(device.device_id, sensorId)}
                onStopSensorStreaming={(sensorId) => stopSensorStreaming(device.device_id, sensorId)}
                onCheckFirmwareUpdates={() => handleCheckFirmwareUpdates(device.device_id)}
                onShowUpdates={() => setUpdatesFor(device)}
                onUpdateFirmwareFromFile={(file) => handleUpdateFirmwareFromFile(device, file)}
                onToggleAdvancedMode={(enable) => toggleAdvancedMode(device.device_id, enable)}
                onUploadPreset={(file) => uploadPreset(device.device_id, file)}
                onSavePreset={(name) => savePreset(device.device_id, name)}
                onShowToast={addToast}
              />
            )
          })}
        </div>
      )}

      {updatesFor && (
        <UpdatesDialog
          deviceId={updatesFor.device_id}
          deviceName={updatesFor.name}
          onClose={() => setUpdatesFor(null)}
          onInstallFirmware={() => { const d = updatesFor; setUpdatesFor(null); handleUpdateFromRecommended(d) }}
        />
      )}

      {firmwareProgressDevice && (
        <FirmwareProgressModal
          isOpen={true}
          device={firmwareProgressDevice}
          firmware={firmwareProgressState || { is_updating: true, progress: 0, last_error: null }}
          fileName={firmwareFileName || undefined}
          onClose={handleCloseFirmwareModal}
          onProgressUpdate={handleFirmwareProgressUpdate}
          onSuccess={handleFirmwareSuccess}
          onError={handleFirmwareError}
        />
      )}

      {/* Toast Notifications */}
      <ToastContainer toasts={toasts} onClose={removeToast} />
    </div>
  )
}

interface DeviceCardProps {
  device: DeviceInfo
  deviceState?: DeviceState
  onReset: () => void
  onUpdateStreamConfig: (config: StreamConfig) => void
  onUpdateSensorConfig: (sensorId: string, config: Partial<SensorConfig>) => void
  onStartSensorStreaming: (sensorId: string) => void
  onStopSensorStreaming: (sensorId: string) => void
  onCheckFirmwareUpdates: () => void
  onShowUpdates: () => void
  onUpdateFirmwareFromFile: (file: File) => void
  onToggleAdvancedMode: (enable: boolean) => void
  onUploadPreset: (file: File) => void
  onSavePreset: (name: string) => void
  onShowToast: (type: ToastType, message: string) => void
}

function DeviceCard({
  device,
  deviceState,
  onReset,
  onUpdateStreamConfig,
  onUpdateSensorConfig,
  onStartSensorStreaming,
  onStopSensorStreaming,
  onCheckFirmwareUpdates,
  onShowUpdates,
  onUpdateFirmwareFromFile,
  onToggleAdvancedMode,
  onUploadPreset,
  onSavePreset,
  onShowToast,
}: DeviceCardProps) {
  const [showMenu, setShowMenu] = useState(false)
  const [showDetails, setShowDetails] = useState(false)
  const fwPicker = useFilePicker(onUpdateFirmwareFromFile, '.bin')
  const presetPicker = useFilePicker(onUploadPreset, '.json,.preset')
  const advancedOn = deviceState?.advancedMode?.enabled === true

  const isLoading = deviceState?.isLoading || false
  const isStreaming = deviceState?.isStreaming || false
  const sensors = deviceState?.sensors || []
  const controls = deviceState?.controls || {}
  const streamConfigs = deviceState?.streamConfigs || []
  const sensorConfigs = deviceState?.sensorConfigs || {}
  const sensorStreamingStatus = deviceState?.sensorStreamingStatus || {}

  // Group stream configs by sensor
  const streamsBySensor: Record<string, StreamConfig[]> = {}
  for (const config of streamConfigs) {
    if (!streamsBySensor[config.sensor_id]) {
      streamsBySensor[config.sensor_id] = []
    }
    streamsBySensor[config.sensor_id].push(config)
  }

  const handleCheckFirmwareUpdates = () => {
    onCheckFirmwareUpdates()
  }

  return (
    <div className="device-card rounded-lg bg-rs-blue/10 border border-rs-blue" data-testid="device-card">
      {/* Hidden file input — rendered at card root so it persists when the
          hamburger menu closes; otherwise the OS file picker resolves into
          an unmounted input and onChange never fires. */}
      {fwPicker.input}
      {presetPicker.input}
      {/* Device Header */}
      <div className="p-3">
        <div className="flex items-start justify-between">
          <div className="flex-1 min-w-0">
            <h3 className="font-semibold text-white truncate">
              {device.is_playback && <span className="mr-1 px-1 rounded bg-purple-700 text-[10px] uppercase align-middle">Playback</span>}
              {device.name}
            </h3>
            <p className="text-sm text-gray-400 truncate">S/N: {device.serial_number}</p>
          </div>
          <div className="flex items-center gap-2 ml-2">
            {!device.is_playback && <RecordButton deviceId={device.device_id} streaming={isStreaming} />}
            {isStreaming && (
              <span className="w-2 h-2 bg-green-500 rounded-full animate-pulse" title="Streaming" />
            )}
            {isLoading && (
              <div className="w-4 h-4 border-2 border-rs-blue border-t-transparent rounded-full animate-spin" title="Loading..." />
            )}
            
            {/* Hamburger Menu */}
            <div className="relative">
              <button
                onClick={(e) => { e.stopPropagation(); setShowMenu(!showMenu); }}
                className="p-1 hover:bg-gray-700 rounded transition-colors"
                title="Device actions"
              >
                <svg className="w-5 h-5" fill="currentColor" viewBox="0 0 20 20">
                  <path d="M10 6a2 2 0 110-4 2 2 0 010 4zM10 12a2 2 0 110-4 2 2 0 010 4zM10 18a2 2 0 110-4 2 2 0 010 4z" />
                </svg>
              </button>
              
              {showMenu && (
                <>
                  <div 
                    className="fixed inset-0 z-10" 
                    onClick={() => setShowMenu(false)}
                  />
                  <div className="absolute right-0 mt-1 w-48 bg-gray-800 border border-gray-600 rounded-lg shadow-xl z-20 py-1">
                    <button
                      onClick={() => {
                        setShowMenu(false)
                        onShowToast('info', 'Calibration feature coming soon')
                      }}
                      className="w-full px-4 py-2 text-left text-sm hover:bg-gray-700 flex items-center gap-2"
                    >
                      <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 6V4m0 2a2 2 0 100 4m0-4a2 2 0 110 4m-6 8a2 2 0 100-4m0 4a2 2 0 110-4m0 4v2m0-6V4m6 6v10m6-2a2 2 0 100-4m0 4a2 2 0 110-4m0 4v2m0-6V4" />
                      </svg>
                      On-Chip Calibration
                    </button>
                    <button
                      onClick={() => {
                        setShowMenu(false)
                        onShowToast('info', 'Tare calibration feature coming soon')
                      }}
                      className="w-full px-4 py-2 text-left text-sm hover:bg-gray-700 flex items-center gap-2"
                    >
                      <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M3 6l3 1m0 0l-3 9a5.002 5.002 0 006.001 0M6 7l3 9M6 7l6-2m6 2l3-1m-3 1l-3 9a5.002 5.002 0 006.001 0M18 7l3 9m-3-9l-6-2m0-2v2m0 16V5m0 16H9m3 0h3" />
                      </svg>
                      Tare Calibration
                    </button>
                    <div className="border-t border-gray-600 my-1" />
                    <button
                      onClick={() => { setShowMenu(false); setTimeout(() => presetPicker.open(), 0) }}
                      disabled={!advancedOn}
                      title={advancedOn ? 'Apply a JSON preset from a file' : 'Requires Advanced Mode'}
                      className={`w-full px-4 py-2 text-left text-sm flex items-center gap-2 ${advancedOn ? 'hover:bg-gray-700' : 'text-gray-500 cursor-not-allowed'}`}
                    >
                      <span className="w-4 text-center">⤒</span>
                      Load Preset (JSON)…
                    </button>
                    <a
                      href={advancedOn ? apiClient.presetDownloadUrl(device.device_id) : undefined}
                      download
                      onClick={() => setShowMenu(false)}
                      aria-disabled={!advancedOn}
                      title={advancedOn ? 'Download the current settings as a JSON preset' : 'Requires Advanced Mode'}
                      className={`w-full px-4 py-2 text-left text-sm flex items-center gap-2 ${advancedOn ? 'hover:bg-gray-700' : 'text-gray-500 cursor-not-allowed pointer-events-none'}`}
                    >
                      <span className="w-4 text-center">⤓</span>
                      Save Preset (JSON)…
                    </a>
                    <button
                      onClick={() => {
                        setShowMenu(false)
                        const name = window.prompt('Preset name (stored in the presets folder as "<model> <name>.preset")')
                        if (name?.trim()) onSavePreset(name.trim())
                      }}
                      disabled={!advancedOn}
                      title={advancedOn ? 'Store the current settings in the presets folder' : 'Requires Advanced Mode'}
                      className={`w-full px-4 py-2 text-left text-sm flex items-center gap-2 ${advancedOn ? 'hover:bg-gray-700' : 'text-gray-500 cursor-not-allowed'}`}
                    >
                      <span className="w-4 text-center">★</span>
                      Save to Presets Folder…
                    </button>
                    <div className="border-t border-gray-600 my-1" />
                    <button
                      onClick={() => {
                        setShowMenu(false)
                        handleCheckFirmwareUpdates()
                      }}
                      className="w-full px-4 py-2 text-left text-sm hover:bg-gray-700 flex items-center gap-2"
                    >
                      <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M7 16a4 4 0 01-.88-7.903A5 5 0 1115.9 6L16 6a5 5 0 011 9.9M15 13l-3-3m0 0l-3 3m3-3v12" />
                      </svg>
                      Check for Firmware Updates
                    </button>
                    <button
                      onClick={() => { setShowMenu(false); onShowUpdates() }}
                      className="w-full px-4 py-2 text-left text-sm hover:bg-gray-700 flex items-center gap-2"
                    >
                      <span className="w-4 text-center">⇪</span>
                      Software &amp; Firmware Updates…
                    </button>
                    {deviceState?.advancedMode?.supported && (
                      <button
                        onClick={() => {
                          setShowMenu(false)
                          onToggleAdvancedMode(!deviceState.advancedMode?.enabled)
                        }}
                        // Toggling either way restarts the device, so the C++ viewer keeps this
                        // item disabled while streaming (device-model.cpp) - do the same here.
                        disabled={isStreaming}
                        title={isStreaming ? 'Disabled while streaming' : undefined}
                        className={`w-full px-4 py-2 text-left text-sm flex items-center gap-2 ${
                          isStreaming
                            ? 'text-gray-500 cursor-not-allowed'
                            : 'hover:bg-gray-700'
                        }`}
                      >
                        <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                          <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M10.325 4.317c.426-1.756 2.924-1.756 3.35 0a1.724 1.724 0 002.573 1.066c1.543-.94 3.31.826 2.37 2.37a1.724 1.724 0 001.065 2.572c1.756.426 1.756 2.924 0 3.35a1.724 1.724 0 00-1.066 2.573c.94 1.543-.826 3.31-2.37 2.37a1.724 1.724 0 00-2.572 1.065c-.426 1.756-2.924 1.756-3.35 0a1.724 1.724 0 00-2.573-1.066c-1.543.94-3.31-.826-2.37-2.37a1.724 1.724 0 00-1.065-2.572c-1.756-.426-1.756-2.924 0-3.35a1.724 1.724 0 001.066-2.573c-.94-1.543.826-3.31 2.37-2.37.996.608 2.296.07 2.572-1.065z" />
                          <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M15 12a3 3 0 11-6 0 3 3 0 016 0z" />
                        </svg>
                        {deviceState.advancedMode.enabled ? 'Disable Advanced Mode' : 'Enable Advanced Mode'}
                      </button>
                    )}
                    <button
                      onClick={() => {
                        setShowMenu(false)
                        // Defer the click so React unmounts the menu first; the
                        // file input lives outside the menu and persists.
                        setTimeout(() => fwPicker.open(), 0)
                      }}
                      disabled={isStreaming}
                      className={`w-full px-4 py-2 text-left text-sm flex items-center gap-2 ${
                        isStreaming
                          ? 'text-gray-500 cursor-not-allowed'
                          : 'hover:bg-gray-700'
                      }`}
                    >
                      <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 16v2a2 2 0 002 2h12a2 2 0 002-2v-2M7 10l5-5m0 0l5 5m-5-5v12" />
                      </svg>
                      Update FW from File
                    </button>
                    <div className="border-t border-gray-600 my-1" />
                    <button
                      onClick={() => {
                        setShowMenu(false)
                        onReset()
                      }}
                      disabled={isStreaming}
                      className={`w-full px-4 py-2 text-left text-sm flex items-center gap-2 ${
                        isStreaming ? 'text-gray-500 cursor-not-allowed' : 'text-red-400 hover:bg-gray-700'
                      }`}
                    >
                      <RefreshCcw className="w-4 h-4" />
                      Hardware Reset
                    </button>
                  </div>
                </>
              )}
            </div>
          </div>
        </div>

        {/* Device Details */}
        <div className="mt-2 grid grid-cols-2 gap-1 text-xs text-gray-500">
          {device.firmware_version && (
            <span>FW: {device.firmware_version}</span>
          )}
          {device.usb_type && <span>USB: {device.usb_type}</span>}
        </div>
        {device.info && Object.keys(device.info).length > 0 && (
          <div className="mt-1 text-xs">
            <button
              type="button"
              onClick={() => setShowDetails((v) => !v)}
              className="text-gray-400 hover:text-white"
            >
              {showDetails ? 'Hide Device Details' : 'Show Device Details'}
            </button>
            {showDetails && (
              <table className="mt-1 w-full text-[11px] text-gray-300" data-testid="device-details">
                <tbody>
                  {Object.entries(device.info).map(([key, value]) => (
                    <tr key={key} className="border-t border-gray-700/60">
                      <td className="py-0.5 pr-2 text-gray-500 whitespace-nowrap">{lessScreamy(key)}</td>
                      <td className="py-0.5 break-all">{value}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            )}
          </div>
        )}

        {/* Sensor names, until the sensors themselves are loaded */}
        {device.sensors.length > 0 && isLoading && (
          <div className="mt-2 flex flex-wrap gap-1">
            {device.sensors.map((sensor) => (
              <span
                key={sensor}
                className="px-2 py-0.5 bg-gray-700 rounded text-xs text-gray-300"
              >
                {sensor}
              </span>
            ))}
          </div>
        )}
      </div>

      {device.is_playback && <Transport deviceId={device.device_id} />}

      {!isLoading && (
        <div className="border-t border-gray-700 p-3 space-y-1.5">
          {sensors.map((sensor) => (
            <SensorPanel
              key={sensor.sensor_id}
              deviceId={device.device_id}
              sensor={sensor}
              groups={Object.entries(controls).filter(([, g]) => g.sensorId === sensor.sensor_id)}
              streams={streamsBySensor[sensor.sensor_id] || []}
              sensorConfig={sensorConfigs[sensor.sensor_id]}
              status={sensorStreamingStatus[sensor.sensor_id]}
              onUpdateStreamConfig={onUpdateStreamConfig}
              onUpdateSensorConfig={onUpdateSensorConfig}
              onStartStreaming={() => onStartSensorStreaming(sensor.sensor_id)}
              onStopStreaming={() => onStopSensorStreaming(sensor.sensor_id)}
            />
          ))}
        </div>
      )}
    </div>
  )
}

interface SensorPanelProps {
  deviceId: string
  sensor: SensorInfo
  /** The sensor's control groups, each with the key its writes go to. */
  groups: [string, ControlGroup][]
  streams: StreamConfig[]
  sensorConfig?: SensorConfig
  status?: { is_streaming: boolean; pendingOp?: string | null; error?: string | null }
  onUpdateStreamConfig: (config: StreamConfig) => void
  onUpdateSensorConfig: (sensorId: string, config: Partial<SensorConfig>) => void
  onStartStreaming: () => void
  onStopStreaming: () => void
}

/**
 * One sensor: what it can stream and every control it has, in one expander, the way the
 * C++ viewer draws a sensor (device-model.cpp) rather than listing every sensor twice.
 */
function SensorPanel({
  deviceId, sensor, groups, streams, sensorConfig, status,
  onUpdateStreamConfig, onUpdateSensorConfig, onStartStreaming, onStopStreaming,
}: SensorPanelProps) {
  const { setControl, setControlEnabled, setPostProcessing, deviceStates } = useAppStore()
  const postProcessing = deviceStates[deviceId]?.postProcessing?.[sensor.sensor_id] !== false
  // The search covers this sensor's controls, so it lives with them, as in the C++ viewer.
  const [searchQuery, setSearchQuery] = useState('')
  const searching = searchQuery.trim().length > 0
  const nothingMatches = searching && !groups.some(([, g]) => searchGroup(g, searchQuery))

  /** Put every control of these groups back to the value the device reports as default. */
  const restore = async (gs: [string, ControlGroup][]) => {
    for (const [key, group] of gs) {
      if (group.enabled !== undefined && group.enabled !== group.default_enabled) {
        await setControlEnabled(deviceId, key, group.default_enabled!)
      }
      for (const option of group.options.filter(isModified)) {
        await setControl(deviceId, key, option.option_id, option.default_value)
      }
    }
  }

  const isSensorStreaming = status?.is_streaming || false
  const isSensorPending = status?.pendingOp === 'stopping'
  const sensorError = status?.error
  // The legacy viewer refuses to start a mode the SDK does not list ("Selected value is not supported").
  const unsupported = unsupportedStreams(streams, sensor.supported_stream_profiles, sensorConfig)
  const canStartSensor = streams.some(c => c.enable) && unsupported.length === 0

  const modifiedCount = groups.flatMap(([, g]) => g.options).filter(isModified).length

  const computeCommonOptions = () => {
          const profiles = sensor.supported_stream_profiles
          if (profiles.length === 0) return { resolutions: [], fps: [] }

          let commonResolutions = new Set(profiles[0].resolutions.map(([w, h]) => `${w}x${h}`))
          let commonFps = new Set(profiles[0].fps)

          for (let i = 1; i < profiles.length; i++) {
            const profileRes = new Set(profiles[i].resolutions.map(([w, h]) => `${w}x${h}`))
            const profileFps = new Set(profiles[i].fps)
            commonResolutions = new Set([...commonResolutions].filter(r => profileRes.has(r)))
            commonFps = new Set([...commonFps].filter(f => profileFps.has(f)))
          }

          const resolutions: [number, number][] = [...commonResolutions].map(r => {
            const [w, h] = r.split('x').map(Number)
            return [w, h] as [number, number]
          })
          const fps = [...commonFps].sort((a, b) => a - b)
          return { resolutions, fps }
        }

  const { resolutions: availableResolutions, fps: availableFps } = computeCommonOptions()

  return (
    <Collapsible
      variant="sensor"
      label={
        <>
          <span className="text-sm font-medium text-gray-300 truncate">{sensor.name}</span>
          {isSensorStreaming && (
            <span className="w-2 h-2 bg-green-500 rounded-full animate-pulse shrink-0" />
          )}
          {modifiedCount > 0 && (
            <span className="px-1.5 py-0.5 bg-rs-blue/20 text-rs-blue rounded text-[10px] shrink-0">
              {modifiedCount} modified
            </span>
          )}
        </>
      }
      aside={
        <button
          onClick={() => isSensorStreaming ? onStopStreaming() : onStartStreaming()}
          disabled={isSensorPending || (!canStartSensor && !isSensorStreaming)}
          data-testid={isSensorStreaming ? "stop-streaming" : "start-streaming"}
          title={isSensorPending ? 'Stopping...' : isSensorStreaming ? 'Stop'
            : unsupported.length ? `Selected mode is not supported: ${unsupported.join(', ')}` : 'Start'}
          className={`px-2 py-0.5 rounded text-xs font-medium transition-colors ${
            isSensorPending
              ? 'bg-yellow-600 text-white cursor-wait'
              : isSensorStreaming
                ? 'bg-red-600 hover:bg-red-700 text-white'
                : canStartSensor
                  ? 'bg-green-600/80 hover:bg-green-600 text-white'
                  : 'bg-gray-700 text-gray-500 cursor-not-allowed'
          }`}
        >
          <span className="sr-only">{isSensorStreaming ? 'Stop' : 'Start'}</span>
          {isSensorPending ? '⏳' : isSensorStreaming ? '■' : '▶'}
        </button>
      }
      belowHeader={(sensorError || (unsupported.length > 0 && !isSensorStreaming)) && (
        <div className="mb-2 text-xs text-red-400 bg-red-900/30 rounded px-2 py-1">
          {sensorError ?? `Selected mode is not supported: ${unsupported.join(', ')}`}
        </div>
      )}
    >
      <div className="mt-2 space-y-1">
            {sensorConfig && !sensorConfig.isMotionSensor && !(sensorConfig.perStreamResolution && sensorConfig.perStreamFps) && (
              <div className="mb-2 flex items-center gap-2 text-xs">
                {!sensorConfig.perStreamResolution && (
                <div className="flex items-center gap-1">
                  <label className="text-gray-500">Res:</label>
                  <select
                    value={`${sensorConfig.resolution.width}x${sensorConfig.resolution.height}`}
                    onChange={(e) => {
                      const [width, height] = e.target.value.split('x').map(Number)
                      onUpdateSensorConfig(sensor.sensor_id, { resolution: { width, height } })
                    }}
                    disabled={isSensorStreaming}
                    className="bg-gray-700 text-white rounded px-1 py-0.5 text-xs"
                  >
                    {availableResolutions.map(([w, h]) => (
                      <option key={`${w}x${h}`} value={`${w}x${h}`}>
                        {w}×{h}
                      </option>
                    ))}
                  </select>
                </div>
                )}
                {!sensorConfig.perStreamFps && (
                <div className="flex items-center gap-1">
                  <label className="text-gray-500">FPS:</label>
                  <select
                    value={sensorConfig.framerate}
                    onChange={(e) => onUpdateSensorConfig(sensor.sensor_id, { framerate: Number(e.target.value) })}
                    disabled={isSensorStreaming}
                    className="bg-gray-700 text-white rounded px-1 py-0.5 text-xs"
                  >
                    {availableFps.map((fps) => (
                      <option key={fps} value={fps}>
                        {fps}
                      </option>
                    ))}
                  </select>
                </div>
                )}
              </div>
            )}

            <div className="space-y-1">
              {streams.map((config) => (
                <StreamConfigItem
                  key={`${config.sensor_id}-${config.stream_type}`}
                  config={config}
                  sensor={sensor}
                  onUpdate={onUpdateStreamConfig}
                  disabled={isSensorStreaming}
                  isMotionSensor={sensorConfig?.isMotionSensor ?? false}
                  perStreamResolution={sensorConfig?.perStreamResolution ?? false}
                  perStreamFps={sensorConfig?.perStreamFps ?? false}
                />
              ))}
            </div>

        <ControlsSearchBox value={searchQuery} onChange={setSearchQuery} />

        {modifiedCount > 0 && (
          <button
            onClick={() => restore(groups)}
            className="w-full flex items-center justify-center gap-1 p-1 bg-gray-700/50 hover:bg-gray-600 rounded text-xs text-gray-300 transition-colors"
          >
            <RefreshCcw className="w-3 h-3" />
            Restore All Defaults
          </button>
        )}

        {/* SECTIONS is in the order the C++ viewer draws them (device-model.cpp). */}
        {SECTIONS.map((title) => {
          const mine = groups.filter(([, g]) => g.section === title)
          return (
            <ControlSection
              key={title}
              deviceId={deviceId}
              title={title}
              groups={mine}
              // Post-processing is the one section the viewer can bypass wholesale.
              sectionSwitch={title === 'Post-Processing' ? {
                enabled: postProcessing,
                onToggle: () => setPostProcessing(deviceId, sensor.sensor_id, !postProcessing),
              } : undefined}
              searchQuery={searchQuery}
              onSet={(key, optionId, value) => setControl(deviceId, key, optionId, value)}
              onToggleGroup={(key, enabled) => setControlEnabled(deviceId, key, enabled)}
              onRestoreDefaults={() => restore(mine)}
            />
          )
        })}

        {nothingMatches && (
          <p className="text-gray-500 text-xs py-1">
            No controls match “{searchQuery.trim()}”.
          </p>
        )}
      </div>
    </Collapsible>
  )
}

interface StreamConfigItemProps {
  config: StreamConfig
  sensor: SensorInfo
  onUpdate: (config: StreamConfig) => void
  disabled: boolean
  isMotionSensor: boolean
  perStreamResolution?: boolean
  perStreamFps?: boolean
}

function StreamConfigItem({ config, sensor, onUpdate, disabled, isMotionSensor, perStreamResolution, perStreamFps }: StreamConfigItemProps) {
  const profile = sensor.supported_stream_profiles.find((p) =>
    p.stream_type.toLowerCase() === config.stream_type.toLowerCase()
  )

  // Don't render if no matching profile found (sensor doesn't support this stream)
  if (!profile) return null

  const getStreamColor = (type: string) => {
    const colors: Record<string, string> = {
      depth: 'text-blue-400',
      color: 'text-green-400',
      infrared: 'text-purple-400',
      fisheye: 'text-yellow-400',
      gyro: 'text-red-400',
      accel: 'text-orange-400',
    }
    return colors[type.toLowerCase()] || 'text-gray-400'
  }

  // Available FPS options for this stream profile
  const availableFps = [...profile.fps].sort((a, b) => a - b)

  return (
    <div className="flex items-center gap-2 py-0.5 flex-wrap">
      <label className="flex items-center gap-2 cursor-pointer">
        <input
          type="checkbox"
          checked={config.enable}
          onChange={(e) => onUpdate({ ...config, enable: e.target.checked })}
          disabled={disabled}
          className="control-checkbox w-3 h-3 flex-shrink-0"
          data-testid={`toggle-stream-${config.stream_type.toLowerCase()}`}
        />
        <span className={`text-xs font-semibold min-w-[50px] ${getStreamColor(config.stream_type)}`}>
          {config.stream_type.toUpperCase()}
        </span>
      </label>
      {/* Format selector - only shown when enabled */}
      {config.enable && (
        <select
          value={config.format}
          onChange={(e) => onUpdate({ ...config, format: e.target.value })}
          disabled={disabled}
          className="bg-gray-700 text-white rounded px-1 py-0.5 text-xs max-w-[100px]"
        >
          {profile.formats.map((format) => (
            <option key={format} value={format}>
              {format}
            </option>
          ))}
        </select>
      )}
      {config.enable && perStreamResolution && !isMotionSensor && (
        <select
          value={`${config.resolution.width}x${config.resolution.height}`}
          onChange={(e) => {
            const [width, height] = e.target.value.split('x').map(Number)
            onUpdate({ ...config, resolution: { width, height } })
          }}
          disabled={disabled}
          className="bg-gray-700 text-white rounded px-1 py-0.5 text-xs"
          aria-label={`${config.stream_type} resolution`}
        >
          {profile.resolutions.map(([w, h]) => (
            <option key={`${w}x${h}`} value={`${w}x${h}`}>{w}×{h}</option>
          ))}
        </select>
      )}
      {/* Per-stream FPS: motion sensors always, video sensors when no frame rate is shared */}
      {config.enable && (isMotionSensor || perStreamFps) && (
        <select
          value={config.framerate}
          onChange={(e) => onUpdate({ ...config, framerate: Number(e.target.value) })}
          disabled={disabled}
          className="bg-gray-700 text-white rounded px-1 py-0.5 text-xs w-[70px]"
        >
          {availableFps.map((fps) => (
            <option key={fps} value={fps}>
              {fps}Hz
            </option>
          ))}
        </select>
      )}
    </div>
  )
}

interface ControlsSearchBoxProps {
  value: string
  onChange: (value: string) => void
}

function ControlsSearchBox({ value, onChange }: ControlsSearchBoxProps) {
  return (
    <div className="relative mb-2">
      <Search className="absolute left-2 top-1/2 -translate-y-1/2 w-3.5 h-3.5 text-gray-500 pointer-events-none" />
      <input
        type="text"
        value={value}
        onChange={(e) => onChange(e.target.value)}
        placeholder="Search controls…"
        className="w-full pl-7 pr-7 py-1.5 bg-gray-800 border border-gray-600 rounded-lg text-white placeholder-gray-500 focus:outline-none focus:border-rs-blue text-xs"
      />
      {value && (
        <button
          onClick={() => onChange('')}
          className="absolute right-2 top-1/2 -translate-y-1/2 text-gray-500 hover:text-gray-300"
          title="Clear search"
        >
          <X className="w-3.5 h-3.5" />
        </button>
      )}
    </div>
  )
}

interface ControlSectionProps {
  deviceId: string
  title: string
  /** The section's groups, each with the key its writes go to. */
  groups: [string, ControlGroup][]
  /** The section's own on/off switch, when it has one. */
  sectionSwitch?: { enabled: boolean; onToggle: () => void }
  searchQuery: string
  onSet: (key: string, optionId: string, value: number | boolean | string) => Promise<void>
  onToggleGroup: (key: string, enabled: boolean) => Promise<void>
  onRestoreDefaults: () => void
}

/**
 * A section of the controls tree: the sensor's own options, the colorizer, the advanced-mode
 * groups or the post-processing filters. They differ only in whether their groups are named
 * and whether a group can be switched off, so they are all this component.
 */
function ControlSection({
  deviceId,
  title, groups, sectionSwitch, searchQuery, onSet, onToggleGroup, onRestoreDefaults,
}: ControlSectionProps) {
  const searching = searchQuery.trim().length > 0

  // A group with nothing to interact with is dropped, so a section the device has no
  // controls for - the colorizer on a sensor that is not the depth one - shows no header.
  // A switch counts as something to interact with: the disparity transforms carry no
  // options at all.
  const matching = groups.flatMap(([key, group]) => {
    const options = searchGroup(group, searchQuery)
    if (!options || (options.length === 0 && group.enabled === undefined)) return []
    return [{ key, group, options }]
  })
  if (matching.length === 0) return null

  const modified = groups.some(([, g]) => g.options.some(isModified))

  return (
    <Collapsible
      variant="section"
      forcedOpen={searching}
      label={<span className="text-xs font-medium text-gray-300">{title}</span>}
      aside={
        <>
          {modified && (
            <button
              onClick={(e) => { e.stopPropagation(); onRestoreDefaults(); }}
              className="px-1.5 py-0.5 mr-1 text-[10px] text-rs-blue hover:text-blue-400"
              title={`Restore ${title} to defaults`}
            >
              <RefreshCcw className="w-3 h-3" />
            </button>
          )}
          {sectionSwitch && (
            <ToggleSwitch enabled={sectionSwitch.enabled} onToggle={sectionSwitch.onToggle} />
          )}
        </>
      }
    >
      <div className="p-1.5 space-y-1 bg-gray-800/30">
        {matching.map(({ key, group, options }) => {
          const controls = options.map(option => (
            <div key={option.option_id}>
              <OptionControl
                option={option}
                onSet={(optionId, value) => onSet(key, optionId, value)}
              />
              {option.option_id.toLowerCase() === 'visual_preset' && <VisualPresetFiles deviceId={deviceId} />}
            </div>
          ))
          return !group.name ? controls : (
            <Collapsible
              key={key}
              variant="group"
              label={<span className="text-xs font-medium text-gray-200">{optionLabel(group.name)}</span>}
              forcedOpen={searching}
              aside={group.enabled !== undefined && (
                <ToggleSwitch
                  enabled={group.enabled}
                  onToggle={() => onToggleGroup(key, !group.enabled)}
                />
              )}
            >
              <div className="p-1.5 space-y-1 bg-gray-800/50">{controls}</div>
            </Collapsible>
          )
        })}
      </div>
    </Collapsible>
  )
}

/** Whether a writable control sits away from the default the device reports for it. */
function isModified(option: OptionInfo): boolean {
  return !option.read_only && option.current_value !== option.default_value
}

interface OptionControlProps {
  option: OptionInfo
  onSet: (optionId: string, value: number | boolean | string) => Promise<void>
}

// Firmware writes take time; while a slider is dragged, send at most one value per interval
// and always the newest one, as the legacy viewer's option dispatcher does (option-model.h).
const SLIDER_WRITE_INTERVAL_MS = 200

/** The Visual Preset dropdown with the folder presets the legacy viewer appends to it. */
function VisualPresetFiles({ deviceId }: { deviceId: string }) {
  const files = useAppStore((s) => s.deviceStates[deviceId]?.presetFiles ?? [])
  const { fetchPresets, loadPresetFile } = useAppStore()
  useEffect(() => { void fetchPresets(deviceId) }, [deviceId, fetchPresets])
  if (files.length === 0) return null
  return (
    <select
      className="mt-1 w-full bg-gray-700 text-white rounded px-1 py-0.5 border border-gray-600 text-xs"
      value=""
      onChange={(e) => { if (e.target.value) void loadPresetFile(deviceId, e.target.value) }}
      aria-label="Presets from folder"
    >
      <option value="">Load from presets folder…</option>
      {files.map((f) => <option key={f.path} value={f.path}>{f.name}</option>)}
    </select>
  )
}

function OptionControl({ option, onSet }: OptionControlProps) {
  const [localValue, setLocalValue] = useState(option.current_value)
  // Text-edit mode: the legacy pencil button that turns a slider into a typed value.
  const [editing, setEditing] = useState(false)
  const [editText, setEditText] = useState('')
  const lastSentAt = useRef(0)
  const pendingWrite = useRef<ReturnType<typeof setTimeout> | null>(null)

  // Sync with external changes (e.g., from chatbot)
  useEffect(() => {
    setLocalValue(option.current_value)
  }, [option.current_value])

  useEffect(() => () => { if (pendingWrite.current) clearTimeout(pendingWrite.current) }, [])

  const handleChange = async (value: number | boolean | string) => {
    setLocalValue(value)
    try {
      await onSet(option.option_id, value)
    } catch (error) {
      setLocalValue(option.current_value)
    }
  }

  const handleDrag = (value: number) => {
    setLocalValue(value)
    if (pendingWrite.current) clearTimeout(pendingWrite.current)
    const wait = Math.max(0, SLIDER_WRITE_INTERVAL_MS - (Date.now() - lastSentAt.current))
    pendingWrite.current = setTimeout(() => {
      pendingWrite.current = null
      lastSentAt.current = Date.now()
      void handleChange(value)
    }, wait)
  }

  const commitEdit = () => {
    setEditing(false)
    const parsed = Number(editText)
    if (editText.trim() === '' || Number.isNaN(parsed)) return
    const clamped = Math.min(option.max_value, Math.max(option.min_value, parsed))
    void handleChange(clamped)
  }

  const handleRestoreDefault = async () => {
    await handleChange(option.default_value)
  }

  const isModified = localValue !== option.default_value
  // A dropdown only makes sense when every value the option accepts has a label; the
  // backend reports the labels the SDK has, which for a slider is a leading few or none.
  const labelCount = Object.keys(option.value_descriptions || {}).length
  const isEnum =
    labelCount > 0 &&
    option.step === 1 &&
    labelCount === option.max_value - option.min_value + 1
  // A whole-numbered control that cannot leave 0..1 is a flag, whether the device reports
  // the range as 0..1 or - having no range for the group - as the value itself.
  const isBoolean = option.step === 1 && option.min_value >= 0 && option.max_value <= 1
  const isSlider = option.min_value !== option.max_value

  // Get default value display for enum types
  const getDefaultDisplay = () => {
    if (isEnum && option.value_descriptions) {
      return option.value_descriptions[String(Math.round(Number(option.default_value)))] || String(option.default_value)
    }
    return String(option.default_value)
  }

  return (
    <div className="bg-gray-800/30 rounded p-1.5 text-xs">
      <div className="flex items-center justify-between mb-0.5">
        <label className="font-medium truncate text-gray-300 flex-1" title={option.description}>
          {optionLabel(option.option_id)}
        </label>
        <div className="flex items-center gap-1">
          {option.units && <span className="text-gray-500">{option.units}</span>}
          {!option.read_only && isModified && (
            <button
              onClick={handleRestoreDefault}
              className="text-gray-500 hover:text-rs-blue transition-colors"
              title={`Restore default (${getDefaultDisplay()})`}
            >
              <RefreshCcw className="w-3 h-3" />
            </button>
          )}
        </div>
      </div>

      {option.read_only ? (
        <div className="text-gray-400">{String(localValue)}</div>
      ) : isEnum ? (
        <select
          value={String(Math.round(Number(localValue)))}
          onChange={(e) => handleChange(Number(e.target.value))}
          className="w-full bg-gray-700 text-white rounded px-1 py-0.5 border border-gray-600 focus:border-rs-blue focus:outline-none"
        >
          {Object.entries(option.value_descriptions!).map(([val, desc]) => (
            <option key={val} value={val}>
              {desc}
            </option>
          ))}
        </select>
      ) : isBoolean ? (
        <label className="flex items-center gap-1 cursor-pointer">
          <input
            type="checkbox"
            checked={Boolean(localValue)}
            onChange={(e) => handleChange(e.target.checked)}
            className="w-3 h-3"
          />
          <span className="text-gray-400">{localValue ? 'On' : 'Off'}</span>
        </label>
      ) : isSlider ? (
        <div className="flex items-center gap-1">
          {editing ? (
            <input
              type="number"
              autoFocus
              aria-label={`${optionLabel(option.option_id)} value`}
              min={option.min_value}
              max={option.max_value}
              step={option.step ?? 'any'}
              value={editText}
              onChange={(e) => setEditText(e.target.value)}
              onBlur={commitEdit}
              onKeyDown={(e) => {
                if (e.key === 'Enter') commitEdit()
                if (e.key === 'Escape') setEditing(false)
              }}
              className="flex-1 bg-gray-700 text-white rounded px-1 py-0.5 border border-rs-blue focus:outline-none"
            />
          ) : (
            <>
              <input
                type="range"
                min={option.min_value}
                max={option.max_value}
                step={option.step ?? 'any'}
                value={Number(localValue)}
                onChange={(e) => handleDrag(Number(e.target.value))}
                className="flex-1 h-1"
              />
              <span className="text-gray-400 w-10 text-right">
                {typeof localValue === 'number'
                  ? localValue.toFixed((option.step ?? 0) >= 1 ? 0 : 2)
                  : localValue}
              </span>
            </>
          )}
          <button
            type="button"
            onClick={() => { setEditText(String(localValue)); setEditing((e) => !e) }}
            title={editing ? 'Exit text-edit mode' : 'Enter text-edit mode'}
            className={`px-1 ${editing ? 'text-rs-blue' : 'text-gray-500 hover:text-gray-300'}`}
          >
            ✎
          </button>
        </div>
      ) : (
        <input
          type="text"
          value={String(localValue)}
          onChange={(e) => setLocalValue(e.target.value)}
          onBlur={() => handleChange(localValue)}
          className="w-full bg-gray-700 text-white rounded px-1 py-0.5"
        />
      )}
    </div>
  )
}
