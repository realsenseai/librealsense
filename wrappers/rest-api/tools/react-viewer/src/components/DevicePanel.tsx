import { useCallback, useEffect, useRef, useState } from 'react'
import type { MutableRefObject, ReactElement } from 'react'
import { useAppStore } from '../store'
import type { ControlGroup, DeviceInfo, SensorInfo, OptionInfo, StreamConfig, DeviceState, FirmwareState, SensorConfig } from '../api/types'
import { SECTIONS, firmwareStatus, optionLabel } from '../api/types'
import {
  applyFormat, applyFps, applyResolution, applyToggle, withStream,
  formatOptions, fpsOptions, resolutionOptions, sharedFps, type Selection,
} from '../utils/streamSelection'
import { RefreshCcw, Search, X } from 'lucide-react'
import { FirmwareProgressModal } from './FirmwareProgressModal'
import { ToastContainer, type ToastType, type ToastAction } from './Toast'
import { searchGroup } from '../utils/optionSearch'
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
    toggleDeviceActive,
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
  } = useAppStore()

  const [toasts, setToasts] = useState<Toast[]>([])
  // Only one FW update can run at a time, so a single-value state is enough.
  // Snapshot, not a lookup: the camera leaves the device list while it is in DFU.
  const [firmwareProgressDevice, setFirmwareProgressDevice] = useState<DeviceInfo | null>(null)
  const [firmwareProgressState, setFirmwareProgressState] = useState<FirmwareState | null>(null)
  const [firmwareFileName, setFirmwareFileName] = useState<string | null>(null)

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
      <div className="flex items-center justify-between mb-4">
        <h2 className="panel-header mb-0">Devices</h2>
        <button
          onClick={() => fetchDevices(true)}
          disabled={isLoadingDevices}
          aria-label={isLoadingDevices ? 'Refreshing devices…' : 'Refresh devices'}
          aria-busy={isLoadingDevices}
          className="p-2 text-rs-muted hover:text-rs-text hover:bg-rs-inset rounded-lg transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
          title={isLoadingDevices ? 'Refreshing…' : 'Refresh devices'}
        >
          <RefreshCcw className={`w-5 h-5 ${isLoadingDevices ? 'animate-spin' : ''}`} />
        </button>
      </div>

      {/* Error Display */}
      {error && (
        <div className="mb-4 p-3 bg-rs-err/10 border border-rs-err/40 rounded-lg text-sm text-rs-err">
          <div className="flex justify-between items-start">
            <span>{error}</span>
            <button onClick={clearError} className="text-rs-err hover:text-rs-text">
              ×
            </button>
          </div>
        </div>
      )}

      {/* Device List */}
      {devices.length === 0 ? (
        <div className="text-rs-dim text-center py-8">
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
                onToggle={() => toggleDeviceActive(device)}
                onReset={() => resetDevice(device.device_id)}
                onUpdateStreamConfig={(config) => updateStreamConfig(device.device_id, config)}
                onUpdateSensorConfig={(sensorId, config) => updateSensorConfig(device.device_id, sensorId, config)}
                onStartSensorStreaming={(sensorId) => startSensorStreaming(device.device_id, sensorId)}
                onStopSensorStreaming={(sensorId) => stopSensorStreaming(device.device_id, sensorId)}
                onCheckFirmwareUpdates={() => handleCheckFirmwareUpdates(device.device_id)}
                onUpdateFirmwareFromFile={(file) => handleUpdateFirmwareFromFile(device, file)}
                onToggleAdvancedMode={(enable) => toggleAdvancedMode(device.device_id, enable)}
                onShowToast={addToast}
              />
            )
          })}
        </div>
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
  onToggle: () => void
  onReset: () => void
  onUpdateStreamConfig: (config: StreamConfig) => void
  onUpdateSensorConfig: (sensorId: string, config: Partial<SensorConfig>) => void
  onStartSensorStreaming: (sensorId: string) => void
  onStopSensorStreaming: (sensorId: string) => void
  onCheckFirmwareUpdates: () => void
  onUpdateFirmwareFromFile: (file: File) => void
  onToggleAdvancedMode: (enable: boolean) => void
  onShowToast: (type: ToastType, message: string) => void
}

function DeviceCard({
  device,
  deviceState,
  onToggle,
  onReset,
  onUpdateStreamConfig,
  onUpdateSensorConfig,
  onStartSensorStreaming,
  onStopSensorStreaming,
  onCheckFirmwareUpdates,
  onUpdateFirmwareFromFile,
  onToggleAdvancedMode,
  onShowToast,
}: DeviceCardProps) {
  const [showMenu, setShowMenu] = useState(false)
  const fwPicker = useFilePicker(onUpdateFirmwareFromFile, '.bin')

  const isActive = deviceState?.isActive || false
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
    <div
      className={`device-card rounded-lg transition-all ${
        isActive
          ? 'bg-rs-blue/[0.07] border border-rs-blue/70 shadow-[0_0_0_1px_rgba(0,113,197,0.15)]'
          : 'bg-rs-inset border border-rs-border hover:border-rs-dim cursor-pointer'
      }`}
      data-testid="device-card"
      onClick={!isActive && !isLoading ? onToggle : undefined}
    >
      {/* Hidden file input — rendered at card root so it persists when the
          hamburger menu closes; otherwise the OS file picker resolves into
          an unmounted input and onChange never fires. */}
      {fwPicker.input}
      {/* Device Header */}
      <div className="p-3">
        <div className="flex items-start justify-between">
          <div className="flex-1 min-w-0">
            <h3 className="font-semibold text-rs-text truncate">{device.name}</h3>
            <p className="text-sm text-rs-muted truncate nums">S/N: {device.serial_number}</p>
          </div>
          <div className="flex items-center gap-2 ml-2">
            {isStreaming && (
              <span className="w-2 h-2 bg-rs-ok rounded-full animate-pulse" title="Streaming" />
            )}
            {isLoading && (
              <div className="w-4 h-4 border-2 border-rs-blue border-t-transparent rounded-full animate-spin" title="Loading..." />
            )}
            
            {/* Hamburger Menu */}
            <div className="relative">
              <button
                onClick={(e) => { e.stopPropagation(); setShowMenu(!showMenu); }}
                className="p-1 text-rs-muted hover:text-rs-text hover:bg-rs-border/60 rounded transition-colors"
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
                  <div className="absolute right-0 mt-1 w-48 bg-rs-inset border border-rs-border rounded-lg shadow-xl z-20 py-1">
                    <button
                      onClick={() => {
                        setShowMenu(false)
                        onShowToast('info', 'Calibration feature coming soon')
                      }}
                      className="w-full px-4 py-2 text-left text-sm text-rs-text hover:bg-rs-border/60 flex items-center gap-2"
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
                      className="w-full px-4 py-2 text-left text-sm text-rs-text hover:bg-rs-border/60 flex items-center gap-2"
                    >
                      <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M3 6l3 1m0 0l-3 9a5.002 5.002 0 006.001 0M6 7l3 9M6 7l6-2m6 2l3-1m-3 1l-3 9a5.002 5.002 0 006.001 0M18 7l3 9m-3-9l-6-2m0-2v2m0 16V5m0 16H9m3 0h3" />
                      </svg>
                      Tare Calibration
                    </button>
                    <div className="border-t border-rs-border my-1" />
                    <button
                      onClick={() => {
                        setShowMenu(false)
                        handleCheckFirmwareUpdates()
                      }}
                      className="w-full px-4 py-2 text-left text-sm text-rs-text hover:bg-rs-border/60 flex items-center gap-2"
                    >
                      <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M7 16a4 4 0 01-.88-7.903A5 5 0 1115.9 6L16 6a5 5 0 011 9.9M15 13l-3-3m0 0l-3 3m3-3v12" />
                      </svg>
                      Check for Firmware Updates
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
                            ? 'text-rs-dim cursor-not-allowed'
                            : 'text-rs-text hover:bg-rs-border/60'
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
                          ? 'text-rs-dim cursor-not-allowed'
                          : 'text-rs-text hover:bg-rs-border/60'
                      }`}
                    >
                      <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 16v2a2 2 0 002 2h12a2 2 0 002-2v-2M7 10l5-5m0 0l5 5m-5-5v12" />
                      </svg>
                      Update FW from File
                    </button>
                    <div className="border-t border-rs-border my-1" />
                    <button
                      onClick={() => {
                        setShowMenu(false)
                        onReset()
                      }}
                      disabled={isStreaming}
                      className={`w-full px-4 py-2 text-left text-sm flex items-center gap-2 ${
                        isStreaming ? 'text-rs-dim cursor-not-allowed' : 'text-rs-err hover:bg-rs-err/10'
                      }`}
                    >
                      <RefreshCcw className="w-4 h-4" />
                      Hardware Reset
                    </button>
                  </div>
                </>
              )}
            </div>
            
            {/* Toggle switch */}
            <button
              onClick={(e) => { e.stopPropagation(); onToggle(); }}
              disabled={isLoading || isStreaming}
              className={`relative w-10 h-5 rounded-full transition-colors ${
                isActive ? 'bg-rs-blue' : 'bg-rs-border'
              } ${isLoading || isStreaming ? 'opacity-50 cursor-not-allowed' : 'cursor-pointer hover:opacity-90'}`}
              title={isActive ? 'Deactivate device' : 'Activate device'}
            >
              <span
                className={`absolute top-0.5 left-0.5 w-4 h-4 bg-white rounded-full transition-transform ${
                  isActive ? 'translate-x-5' : 'translate-x-0'
                }`}
              />
            </button>
          </div>
        </div>

        {/* Device Details */}
        <div className="mt-2 grid grid-cols-2 gap-1 text-xs text-rs-muted nums">
          {device.firmware_version && (
            <span>FW: {device.firmware_version}</span>
          )}
          {device.usb_type && <span>USB: {device.usb_type}</span>}
        </div>

        {/* Sensors Tags */}
        {device.sensors.length > 0 && !isActive && (
          <div className="mt-2 flex flex-wrap gap-1">
            {device.sensors.map((sensor) => (
              <span
                key={sensor}
                className="px-2 py-0.5 bg-rs-border/50 border border-rs-border rounded text-xs text-rs-muted"
              >
                {sensor}
              </span>
            ))}
          </div>
        )}
      </div>

      {/* Device Controls - shown when active */}
      {isActive && !isLoading && (
        <div className="border-t border-rs-border p-3 space-y-1.5">
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
  const canStartSensor = streams.some(c => c.enable)

  const modifiedCount = groups.flatMap(([, g]) => g.options).filter(isModified).length

  // Every change goes through streamSelection, which keeps the pick and moves the rest onto real SDK profiles
  const profiles = sensor.supported_stream_profiles
  const sel: Selection | undefined = sensorConfig && { ...sensorConfig, streams }
  const shared = sharedFps(profiles)
  const commit = (next: Selection) => {
    next.streams.forEach(onUpdateStreamConfig)
    onUpdateSensorConfig(sensor.sensor_id, { resolution: next.resolution, framerate: next.framerate })
  }

  return (
    <Collapsible
      variant="sensor"
      testId="sensor-module"
      label={
        <>
          <span className="text-sm font-semibold text-rs-text truncate">{sensor.name}</span>
          {isSensorStreaming && (
            <span className="w-1.5 h-1.5 bg-rs-ok rounded-full animate-pulse shrink-0" />
          )}
          {modifiedCount > 0 && (
            <span className="px-1.5 py-0.5 bg-rs-blue/15 border border-rs-blue/30 text-rs-accent rounded text-[11px] shrink-0">
              {modifiedCount} modified
            </span>
          )}
        </>
      }
      aside={
        <ToggleSwitch
          enabled={isSensorStreaming}
          disabled={isSensorPending || (!canStartSensor && !isSensorStreaming)}
          onClick={() => (isSensorStreaming ? onStopStreaming() : onStartStreaming())}
          title={isSensorStreaming ? 'Stop' : 'Start'}
          data-testid={isSensorStreaming ? 'stop-streaming' : 'start-streaming'}
        />
      }
      belowHeader={sensorError && (
        <div className="mb-2 text-xs text-rs-err bg-rs-err/10 border border-rs-err/30 rounded px-2 py-1">
          {sensorError}
        </div>
      )}
    >
      <div className="mt-2 space-y-1">
            {sel && shared && (
              <div className="mb-2 flex items-center gap-2 text-xs">
                <div className="flex items-center gap-1">
                  <label className="text-rs-muted font-semibold uppercase tracking-wide text-xs">Res</label>
                  <select
                    value={`${sel.resolution.width}x${sel.resolution.height}`}
                    onChange={(e) => {
                      const [width, height] = e.target.value.split('x').map(Number)
                      commit(applyResolution(profiles, sel, { width, height }))
                    }}
                    disabled={isSensorStreaming}
                    data-testid="sensor-resolution"
                    className="select-rs text-xs py-0.5"
                  >
                    {resolutionOptions(profiles).map(({ width: w, height: h }) => (
                      <option key={`${w}x${h}`} value={`${w}x${h}`}>
                        {w}×{h}
                      </option>
                    ))}
                  </select>
                </div>
                <div className="flex items-center gap-1">
                  <label className="text-rs-muted font-semibold uppercase tracking-wide text-xs">FPS</label>
                  <select
                    value={sel.framerate}
                    onChange={(e) => commit(applyFps(profiles, sel, Number(e.target.value)))}
                    disabled={isSensorStreaming}
                    data-testid="sensor-fps"
                    className="select-rs text-xs py-0.5"
                  >
                    {fpsOptions(profiles).map((fps) => (
                      <option key={fps} value={fps}>
                        {fps}
                      </option>
                    ))}
                  </select>
                </div>
              </div>
            )}

            <div className="space-y-1">
              {sel && streams.map((config) => (
                <StreamConfigItem
                  key={`${config.sensor_id}-${config.stream_type}`}
                  config={config}
                  formats={formatOptions(profiles, config.stream_type)}
                  fpsChoices={fpsOptions(profiles, config.stream_type)}
                  onToggle={(enable) => commit(applyToggle(profiles, sel, config.stream_type, enable))}
                  onFormat={(format) => commit(applyFormat(profiles, sel, config.stream_type, format))}
                  onFps={(framerate) => commit(withStream(sel, config.stream_type, { framerate }))}
                  disabled={isSensorStreaming}
                  perStreamFps={!shared}
                />
              ))}
            </div>

        <ControlsSearchBox value={searchQuery} onChange={setSearchQuery} />

        {modifiedCount > 0 && (
          <button
            onClick={() => restore(groups)}
            className="w-full flex items-center justify-center gap-1 p-1 bg-rs-border/40 border border-rs-border hover:border-rs-dim hover:text-rs-text rounded text-xs text-rs-muted transition-colors"
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
          <p className="text-rs-dim text-xs py-1">
            No controls match “{searchQuery.trim()}”.
          </p>
        )}
      </div>
    </Collapsible>
  )
}

interface StreamConfigItemProps {
  config: StreamConfig
  formats: string[] // every format the stream has; picking one moves the rest to fit
  fpsChoices: number[] // motion streams set their own fps
  onToggle: (enable: boolean) => void
  onFormat: (format: string) => void
  onFps: (fps: number) => void
  disabled: boolean
  perStreamFps: boolean
}

function StreamConfigItem({ config, formats, fpsChoices, onToggle, onFormat, onFps, disabled, perStreamFps }: StreamConfigItemProps) {
  return (
    <div className="flex items-center gap-2 py-0.5 flex-wrap">
      <label className="flex items-center gap-2 cursor-pointer">
        <input
          type="checkbox"
          checked={config.enable}
          onChange={(e) => onToggle(e.target.checked)}
          disabled={disabled}
          className="control-checkbox w-3 h-3 flex-shrink-0"
          data-testid={`toggle-stream-${config.stream_type.toLowerCase()}`}
        />
        {/* Same color whether or not the stream is selected; the checkbox carries that state. */}
        <span className="text-xs font-semibold min-w-[50px] text-rs-muted">
          {config.stream_type.toUpperCase()}
        </span>
      </label>
      {/* Format selector - only shown when enabled */}
      {config.enable && (
        <select
          value={config.format}
          onChange={(e) => onFormat(e.target.value)}
          disabled={disabled}
          className="select-rs text-xs py-0.5 max-w-[100px]"
        >
          {formats.map((format) => (
            <option key={format} value={format}>
              {format}
            </option>
          ))}
        </select>
      )}
      {/* Per-stream FPS selector for motion sensors */}
      {config.enable && perStreamFps && (
        <select
          value={config.framerate}
          onChange={(e) => onFps(Number(e.target.value))}
          disabled={disabled}
          className="select-rs text-xs py-0.5 w-[70px]"
        >
          {fpsChoices.map((fps) => (
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
      <Search className="absolute left-2 top-1/2 -translate-y-1/2 w-3.5 h-3.5 text-rs-dim pointer-events-none" />
      <input
        type="text"
        value={value}
        onChange={(e) => onChange(e.target.value)}
        placeholder="Search controls…"
        className="w-full pl-7 pr-7 py-1.5 bg-rs-darker/70 border border-rs-border rounded-lg text-rs-text placeholder-rs-dim shadow-inner focus:outline-none focus:border-rs-accent focus:ring-1 focus:ring-rs-accent/40 transition-colors text-xs"
      />
      {value && (
        <button
          onClick={() => onChange('')}
          className="absolute right-2 top-1/2 -translate-y-1/2 text-rs-dim hover:text-rs-text transition-colors"
          title="Clear search"
        >
          <X className="w-3.5 h-3.5" />
        </button>
      )}
    </div>
  )
}

interface ControlSectionProps {
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
      label={<span className="text-xs font-semibold text-rs-text">{title}</span>}
      aside={
        <>
          {modified && (
            <button
              onClick={(e) => { e.stopPropagation(); onRestoreDefaults(); }}
              className="px-1.5 py-0.5 mr-1 text-[10px] text-rs-muted hover:text-rs-accent transition-colors"
              title={`Restore ${title} to defaults`}
            >
              <RefreshCcw className="w-3 h-3" />
            </button>
          )}
          {sectionSwitch && (
            <span className="pr-1">
              <ToggleSwitch enabled={sectionSwitch.enabled} onClick={sectionSwitch.onToggle} />
            </span>
          )}
        </>
      }
    >
      <div className="p-1.5 space-y-1 bg-rs-darker/40">
        {matching.map(({ key, group, options }) => {
          const controls = options.map(option => (
            <OptionControl
              key={option.option_id}
              option={option}
              onSet={(optionId, value) => onSet(key, optionId, value)}
            />
          ))
          return !group.name ? controls : (
            <Collapsible
              key={key}
              variant="group"
              label={<span className="text-xs font-medium text-rs-text">{optionLabel(group.name)}</span>}
              forcedOpen={searching}
              aside={group.enabled !== undefined && (
                <ToggleSwitch
                  enabled={group.enabled}
                  onClick={() => onToggleGroup(key, !group.enabled)}
                />
              )}
            >
              <div className="p-1.5 space-y-1 bg-rs-darker/50">{controls}</div>
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

function OptionControl({ option, onSet }: OptionControlProps) {
  const [localValue, setLocalValue] = useState(option.current_value)

  // Sync with external changes (e.g., from chatbot)
  useEffect(() => {
    setLocalValue(option.current_value)
  }, [option.current_value])

  const handleChange = async (value: number | boolean | string) => {
    setLocalValue(value)
    try {
      await onSet(option.option_id, value)
    } catch (error) {
      setLocalValue(option.current_value)
    }
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
    <div className="bg-rs-inset/50 border border-rs-border/50 rounded p-1.5 text-xs" data-testid={`option-${option.option_id}`}>
      <div className="flex items-center justify-between mb-0.5">
        <label className="font-medium truncate text-rs-text flex-1" title={option.description}>
          {optionLabel(option.option_id)}
        </label>
        <div className="flex items-center gap-1">
          {option.units && <span className="text-rs-muted">{option.units}</span>}
          {!option.read_only && isModified && (
            <button
              onClick={handleRestoreDefault}
              className="text-rs-dim hover:text-rs-accent transition-colors"
              title={`Restore default (${getDefaultDisplay()})`}
            >
              <RefreshCcw className="w-3 h-3" />
            </button>
          )}
        </div>
      </div>

      {option.read_only ? (
        <div className="text-rs-text nums">{String(localValue)}</div>
      ) : isEnum ? (
        <select
          value={String(Math.round(Number(localValue)))}
          onChange={(e) => handleChange(Number(e.target.value))}
          className="select-rs w-full py-0.5"
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
          <span className="text-rs-text">{localValue ? 'On' : 'Off'}</span>
        </label>
      ) : isSlider ? (
        <div className="flex items-center gap-1">
          <input
            type="range"
            min={option.min_value}
            max={option.max_value}
            step={option.step ?? 'any'}
            value={Number(localValue)}
            onChange={(e) => setLocalValue(Number(e.target.value))}
            onMouseUp={() => handleChange(Number(localValue))}
            onTouchEnd={() => handleChange(Number(localValue))}
            className="flex-1 h-1"
          />
          <span className="text-rs-text w-10 text-right nums">
            {typeof localValue === 'number'
              ? localValue.toFixed((option.step ?? 0) >= 1 ? 0 : 2)
              : localValue}
          </span>
        </div>
      ) : (
        <input
          type="text"
          value={String(localValue)}
          onChange={(e) => setLocalValue(e.target.value)}
          onBlur={() => handleChange(localValue)}
          className="w-full bg-rs-darker/70 text-rs-text border border-rs-border rounded px-1 py-0.5 shadow-inner focus:border-rs-accent focus:outline-none"
        />
      )}
    </div>
  )
}
