import { useEffect, useState } from 'react'
import { useAppStore } from '../store'
import type { DeviceInfo, SensorInfo, OptionInfo, StreamConfig, DeviceState, FirmwareState, SensorConfig } from '../api/types'
import { FirmwareProgressModal } from './FirmwareProgressModal'
import { ToastContainer, type ToastType } from './Toast'

interface Toast {
  id: string
  type: ToastType
  message: string
}

export function DevicePanel() {
  const {
    devices,
    deviceStates,
    isLoadingDevices,
    fetchDevices,
    toggleDeviceActive,
    resetDevice,
    error,
    clearError,
    isAnyDeviceStreaming,
    updateStreamConfig,
    updateSensorConfig,
    setOption,
    startSensorStreaming,
    stopSensorStreaming,
    checkFirmwareUpdates,
    updateFirmware,
  } = useAppStore()

  const [toasts, setToasts] = useState<Toast[]>([])
  const [firmwareProgressDeviceId, setFirmwareProgressDeviceId] = useState<string | null>(null)
  const [firmwareProgressStates, setFirmwareProgressStates] = useState<Record<string, FirmwareState>>({})

  const isStreaming = isAnyDeviceStreaming()
  const isFirmwareUpdating = firmwareProgressDeviceId !== null

  useEffect(() => {
    // Don't poll during firmware update - device may be in DFU mode
    if (isFirmwareUpdating) {
      return
    }
    
    fetchDevices()
    // Only poll for device changes when NOT streaming (polling causes frame hiccups)
    if (!isStreaming) {
      const interval = setInterval(fetchDevices, 5000)
      return () => clearInterval(interval)
    }
  }, [fetchDevices, isStreaming, isFirmwareUpdating])

  const addToast = (type: ToastType, message: string) => {
    const id = Date.now().toString()
    setToasts((prev) => [...prev, { id, type, message }])
  }

  const removeToast = (id: string) => {
    setToasts((prev) => prev.filter((t) => t.id !== id))
  }

  const handleFirmwareProgressUpdate = (deviceId: string, progress: number) => {
    setFirmwareProgressStates((prev) => {
      const current = prev[deviceId] || { status: 'unknown' as const, is_updating: true, progress: 0 }
      return {
        ...prev,
        [deviceId]: {
          ...current,
          progress,
        },
      }
    })
  }

  const handleFirmwareSuccess = (deviceId: string, fwVersion: string | null) => {
    // Set progress to 100% to show completion state in modal
    setFirmwareProgressStates((prev) => {
      const current = prev[deviceId] || { status: 'unknown' as const, is_updating: false, progress: 1.0 }
      return {
        ...prev,
        [deviceId]: {
          ...current,
          progress: 1.0,
          is_updating: false,
        },
      }
    })
    addToast('success', `Firmware update successful! Version: ${fwVersion || 'Unknown'}`)
    // Note: modal will auto-close after 2 seconds (handled in FirmwareProgressModal)
  }

  const handleFirmwareError = (deviceId: string, error: string) => {
    // Set error state in modal
    setFirmwareProgressStates((prev) => {
      const current = prev[deviceId] || { status: 'unknown' as const, is_updating: false, progress: 0 }
      return {
        ...prev,
        [deviceId]: {
          ...current,
          is_updating: false,
          last_error: error,
        },
      }
    })
    addToast('error', `Firmware update failed: ${error}`)
  }

  const handleCloseFirmwareModal = () => {
    setFirmwareProgressDeviceId(null)
  }

  return (
    <div className="p-4">
      <div className="flex items-center justify-between mb-4">
        <h2 className="panel-header mb-0">Devices</h2>
        <button
          onClick={() => fetchDevices()}
          className="p-2 hover:bg-gray-700 rounded-lg transition-colors"
          title="Refresh devices"
        >
          <svg
            className={`w-5 h-5 ${isLoadingDevices ? 'animate-spin' : ''}`}
            fill="none"
            stroke="currentColor"
            viewBox="0 0 24 24"
          >
            <path
              strokeLinecap="round"
              strokeLinejoin="round"
              strokeWidth={2}
              d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15"
            />
          </svg>
        </button>
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
                onToggle={() => toggleDeviceActive(device)}
                onReset={() => resetDevice(device.device_id)}
                onUpdateStreamConfig={(config) => updateStreamConfig(device.device_id, config)}
                onUpdateSensorConfig={(sensorId, config) => updateSensorConfig(device.device_id, sensorId, config)}
                onSetOption={(sensorId, optionId, value) => 
                  setOption(device.device_id, sensorId, optionId, value)
                }
                onStartSensorStreaming={(sensorId) => startSensorStreaming(device.device_id, sensorId)}
                onStopSensorStreaming={(sensorId) => stopSensorStreaming(device.device_id, sensorId)}
                onCheckFirmwareUpdates={() => checkFirmwareUpdates(device.device_id)}
                onShowToast={addToast}
                onUpdateFirmware={() => {
                  setFirmwareProgressDeviceId(device.device_id)
                  // Initialize progress state with progress at 0 and clear any previous errors
                  const baseFirmware = deviceState?.firmware || {
                    current: device.firmware_version,
                    recommended: device.recommended_firmware_version,
                    status: 'unknown' as const,
                    file_available: device.firmware_file_available,
                  }
                  setFirmwareProgressStates((prev) => ({
                    ...prev,
                    [device.device_id]: {
                      ...baseFirmware,
                      is_updating: true,
                      progress: 0,
                      last_error: null,
                    },
                  }))
                  updateFirmware(device.device_id)
                }}
              />
            )
          })}
        </div>
      )}

      {/* Firmware Progress Modal */}
      {firmwareProgressDeviceId && (
        <FirmwareProgressModal
          isOpen={true}
          device={devices.find((d) => d.device_id === firmwareProgressDeviceId) || { device_id: '', name: 'Unknown' }}
          firmware={
            firmwareProgressStates[firmwareProgressDeviceId] || {
              current: 'Unknown',
              recommended: 'Unknown',
              status: 'unknown' as const,
              file_available: false,
              is_updating: true,
              progress: 0,
              last_error: null,
            }
          }
          onClose={handleCloseFirmwareModal}
          onProgressUpdate={(progress) => handleFirmwareProgressUpdate(firmwareProgressDeviceId, progress)}
          onSuccess={(fwVersion) => handleFirmwareSuccess(firmwareProgressDeviceId, fwVersion)}
          onError={(error) => handleFirmwareError(firmwareProgressDeviceId, error)}
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
  onSetOption: (sensorId: string, optionId: string, value: number | boolean | string) => Promise<void>
  onStartSensorStreaming: (sensorId: string) => void
  onStopSensorStreaming: (sensorId: string) => void
  onCheckFirmwareUpdates: () => void
  onUpdateFirmware: () => void
  onShowToast: (type: ToastType, message: string) => void
}

function DeviceCard({
  device,
  deviceState,
  onToggle,
  onReset,
  onUpdateStreamConfig,
  onUpdateSensorConfig,
  onSetOption,
  onStartSensorStreaming,
  onStopSensorStreaming,
  onCheckFirmwareUpdates,
  onUpdateFirmware,
  onShowToast,
}: DeviceCardProps) {
  const [showMenu, setShowMenu] = useState(false)
  const [expandedSensor, setExpandedSensor] = useState<string | null>(null)

  const isActive = deviceState?.isActive || false
  const isLoading = deviceState?.isLoading || false
  const isStreaming = deviceState?.isStreaming || false
  const sensors = deviceState?.sensors || []
  const options = deviceState?.options || {}
  const streamConfigs = deviceState?.streamConfigs || []
  const sensorConfigs = deviceState?.sensorConfigs || {}
  const streamingMode = deviceState?.streamingMode || 'idle'
  const sensorStreamingStatus = deviceState?.sensorStreamingStatus || {}

  // Group stream configs by sensor
  const streamsBySensor: Record<string, StreamConfig[]> = {}
  for (const config of streamConfigs) {
    if (!streamsBySensor[config.sensor_id]) {
      streamsBySensor[config.sensor_id] = []
    }
    streamsBySensor[config.sensor_id].push(config)
  }

  const firmware = deviceState?.firmware || {
    current: device.firmware_version,
    recommended: device.recommended_firmware_version,
    status: (device.firmware_status as FirmwareState['status']) || 'unknown',
    file_available: device.firmware_file_available,
    is_updating: false,
    progress: undefined,
    last_error: null,
  }

  // Clear dismiss so the banner re-appears when user explicitly checks from the hamburger menu
  const dismissKey = `fw-dismiss-${device.device_id}-${firmware.recommended}`
  const handleCheckFirmwareUpdates = () => {
    localStorage.removeItem(dismissKey)
    onCheckFirmwareUpdates()
  }

  return (
    <div
      className={`device-card rounded-lg transition-all ${
        isActive
          ? 'bg-rs-blue/10 border border-rs-blue'
          : 'bg-gray-800 border border-gray-700 hover:border-gray-600 cursor-pointer'
      }`}
      data-testid="device-card"
      onClick={!isActive && !isLoading ? onToggle : undefined}
    >
      {/* Device Header */}
      <div className="p-3">
        <div className="flex items-start justify-between">
          <div className="flex-1 min-w-0">
            <h3 className="font-semibold text-white truncate">{device.name}</h3>
            <p className="text-sm text-gray-400 truncate">S/N: {device.serial_number}</p>
          </div>
          <div className="flex items-center gap-2 ml-2">
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
                      <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
                      </svg>
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
                isActive ? 'bg-rs-blue' : 'bg-gray-600'
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

        {/* Firmware status / update */}
        <FirmwareBanner
          device={device}
          firmware={firmware}
          isStreaming={isStreaming}
          onUpdateFirmware={onUpdateFirmware}
        />

        {/* Device Details */}
        <div className="mt-2 grid grid-cols-2 gap-1 text-xs text-gray-500">
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
                className="px-2 py-0.5 bg-gray-700 rounded text-xs text-gray-300"
              >
                {sensor}
              </span>
            ))}
          </div>
        )}
      </div>

      {/* Device Controls - shown when active */}
      {isActive && !isLoading && (
        <div className="border-t border-gray-700">
          {/* Stream Configuration */}
          <div className="p-3">
            <div className="flex items-center justify-between mb-2">
              <div className="flex items-center gap-2">
                <h4 className="text-sm font-medium text-gray-300">Streams</h4>
                {/* Streaming mode indicator */}
                {streamingMode !== 'idle' && (
                  <span className={`text-xs px-1.5 py-0.5 rounded ${
                    streamingMode === 'pipeline' 
                      ? 'bg-blue-900 text-blue-300' 
                      : 'bg-purple-900 text-purple-300'
                  }`}>
                    {streamingMode === 'pipeline' ? 'All' : 'Per-sensor'}
                  </span>
                )}
              </div>
            </div>
            
            {/* Stream configs grouped by sensor */}
            <SensorStreamControls
              sensors={sensors}
              streamsBySensor={streamsBySensor}
              streamingMode={streamingMode}
              sensorStreamingStatus={sensorStreamingStatus}
              sensorConfigs={sensorConfigs}
              onUpdateStreamConfig={onUpdateStreamConfig}
              onUpdateSensorConfig={onUpdateSensorConfig}
              onStartSensorStreaming={onStartSensorStreaming}
              onStopSensorStreaming={onStopSensorStreaming}
            />
          </div>

          {/* Camera Controls */}
          <div className="border-t border-gray-700 p-3">
            <h4 className="text-sm font-medium text-gray-300 mb-2">Controls</h4>
            {sensors.map((sensor) => (
              <SensorOptionsPanel
                key={sensor.sensor_id}
                sensor={sensor}
                options={options[sensor.sensor_id] || []}
                isExpanded={expandedSensor === sensor.sensor_id}
                onToggle={() =>
                  setExpandedSensor(expandedSensor === sensor.sensor_id ? null : sensor.sensor_id)
                }
                onSetOption={onSetOption}
              />
            ))}
          </div>
        </div>
      )}
    </div>
  )
}

interface FirmwareBannerProps {
  device: DeviceInfo
  firmware: FirmwareState
  isStreaming: boolean
  onUpdateFirmware: () => void
}

function FirmwareBanner({ device, firmware, isStreaming, onUpdateFirmware }: FirmwareBannerProps) {
  const [showDismissMenu, setShowDismissMenu] = useState(false)

  const dismissKey = `fw-dismiss-${device.device_id}-${firmware.recommended}`
  const dismissData = localStorage.getItem(dismissKey)
  const isDismissed = dismissData === 'forever'
  const isRemindLater = dismissData ? new Date(dismissData).getTime() > Date.now() : false

  const showFirmwareUpdate = firmware.status === 'outdated' && !isDismissed && !isRemindLater
  const showMissingFirmware = firmware.status === 'missing_file'

  const handleDismiss = (mode: 'later' | 'forever') => {
    if (mode === 'forever') {
      localStorage.setItem(dismissKey, 'forever')
    } else {
      const remindDate = new Date(Date.now() + 7 * 24 * 60 * 60 * 1000)
      localStorage.setItem(dismissKey, remindDate.toISOString())
    }
    setShowDismissMenu(false)
  }

  return (
    <>
      {showFirmwareUpdate && (
        <div className="mt-2 p-2 bg-amber-900/40 border border-amber-600 rounded text-amber-200 text-xs">
          <div className="flex items-start gap-2">
            <div className="flex-1">
              <div className="font-semibold text-amber-100">Firmware update available</div>
              <div className="text-amber-50/90">
                Current: {firmware.current || 'Unknown'} · Recommended: {firmware.recommended || 'Unknown'}
              </div>
              {!firmware.file_available && (
                <div className="text-amber-300 mt-1">FW update needed but bundled file not found (debug message)</div>
              )}
              {firmware.last_error && (
                <div className="text-red-200 mt-1">{firmware.last_error}</div>
              )}
            </div>
            <div className="flex items-center gap-1">
              <button
                onClick={onUpdateFirmware}
                disabled={isStreaming || firmware.is_updating || !firmware.file_available}
                className={`px-3 py-1 rounded text-xs font-semibold transition-colors ${
                  isStreaming || firmware.is_updating || !firmware.file_available
                    ? 'bg-gray-700 text-gray-400 cursor-not-allowed'
                    : 'bg-amber-500 hover:bg-amber-400 text-black'
                }`}
              >
                {firmware.is_updating ? 'Updating…' : 'Install update'}
              </button>
              <div className="relative">
                <button
                  onClick={() => setShowDismissMenu(!showDismissMenu)}
                  className="p-1 hover:bg-amber-800 rounded transition-colors"
                  title="Dismiss"
                >
                  <svg className="w-4 h-4" fill="currentColor" viewBox="0 0 20 20">
                    <path fillRule="evenodd" d="M4.293 4.293a1 1 0 011.414 0L10 8.586l4.293-4.293a1 1 0 111.414 1.414L11.414 10l4.293 4.293a1 1 0 01-1.414 1.414L10 11.414l-4.293 4.293a1 1 0 01-1.414-1.414L8.586 10 4.293 5.707a1 1 0 010-1.414z" clipRule="evenodd" />
                  </svg>
                </button>
                {showDismissMenu && (
                  <>
                    <div className="fixed inset-0 z-10" onClick={() => setShowDismissMenu(false)} />
                    <div className="absolute right-0 mt-1 w-40 bg-gray-800 border border-gray-600 rounded-lg shadow-xl z-20 py-1">
                      <button
                        onClick={() => handleDismiss('later')}
                        className="w-full px-3 py-2 text-left text-xs hover:bg-gray-700 transition-colors"
                      >
                        Remind me later
                      </button>
                      <button
                        onClick={() => handleDismiss('forever')}
                        className="w-full px-3 py-2 text-left text-xs hover:bg-gray-700 transition-colors"
                      >
                        Dismiss
                      </button>
                    </div>
                  </>
                )}
              </div>
            </div>
          </div>
        </div>
      )}

      {showMissingFirmware && !showFirmwareUpdate && (
        <div className="mt-2 p-2 bg-amber-900/40 border border-amber-600 rounded text-amber-200 text-xs">
          <div className="font-semibold text-amber-100">FW update needed but bundled file not found (debug message)</div>
          <div>Current: {firmware.current || 'Unknown'} · Recommended: {firmware.recommended || 'Unknown'}</div>
        </div>
      )}

      {firmware.is_updating && (
        <div className="mt-2 text-xs text-blue-200">
          Updating firmware... {Math.round((firmware.progress || 0) * 100)}%
        </div>
      )}
    </>
  )
}

interface SensorStreamControlsProps {
  sensors: SensorInfo[]
  streamsBySensor: Record<string, StreamConfig[]>
  streamingMode: string
  sensorStreamingStatus: Record<string, { is_streaming: boolean; pendingOp?: string | null; error?: string }>
  sensorConfigs: Record<string, SensorConfig>
  onUpdateStreamConfig: (config: StreamConfig) => void
  onUpdateSensorConfig: (sensorId: string, config: Partial<SensorConfig>) => void
  onStartSensorStreaming: (sensorId: string) => void
  onStopSensorStreaming: (sensorId: string) => void
}

function SensorStreamControls({
  sensors,
  streamsBySensor,
  streamingMode,
  sensorStreamingStatus,
  sensorConfigs,
  onUpdateStreamConfig,
  onUpdateSensorConfig,
  onStartSensorStreaming,
  onStopSensorStreaming,
}: SensorStreamControlsProps) {
  return (
    <div className="space-y-3">
      {sensors.map((sensor) => {
        const sensorStreamConfigs = streamsBySensor[sensor.sensor_id] || []
        if (sensorStreamConfigs.length === 0) return null

        const sensorStatus = sensorStreamingStatus[sensor.sensor_id]
        const isSensorStreaming = sensorStatus?.is_streaming || false
        const isSensorPending = sensorStatus?.pendingOp === 'stopping'
        const sensorError = sensorStatus?.error
        const hasEnabledSensorStreams = sensorStreamConfigs.some(c => c.enable)
        const sensorConfig = sensorConfigs[sensor.sensor_id]

        const canStartSensor = hasEnabledSensorStreams && streamingMode !== 'pipeline'

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
          <div key={sensor.sensor_id} className="bg-gray-800/50 rounded-lg p-2">
            {/* Sensor header with per-sensor start button */}
            <div className="flex items-center justify-between mb-2">
              <div className="flex items-center gap-2">
                <span className="text-sm font-medium text-gray-300">{sensor.name}</span>
                {isSensorStreaming && (
                  <span className="w-2 h-2 bg-green-500 rounded-full animate-pulse" />
                )}
              </div>
              <button
                onClick={() => isSensorStreaming
                  ? onStopSensorStreaming(sensor.sensor_id)
                  : onStartSensorStreaming(sensor.sensor_id)
                }
                disabled={isSensorPending || (!canStartSensor && !isSensorStreaming)}
                data-testid={isSensorStreaming ? "stop-streaming" : "start-streaming"}
                title={streamingMode === 'pipeline' ? 'Stop all streams first' : isSensorPending ? 'Stopping...' : isSensorStreaming ? 'Stop' : 'Start'}
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
            </div>

            {sensorError && (
              <div className="mb-2 text-xs text-red-400 bg-red-900/30 rounded px-2 py-1">
                {sensorError}
              </div>
            )}

            {sensorConfig && !sensorConfig.isMotionSensor && (
              <div className="mb-2 flex items-center gap-2 text-xs">
                <div className="flex items-center gap-1">
                  <label className="text-gray-500">Res:</label>
                  <select
                    value={`${sensorConfig.resolution.width}x${sensorConfig.resolution.height}`}
                    onChange={(e) => {
                      const [width, height] = e.target.value.split('x').map(Number)
                      onUpdateSensorConfig(sensor.sensor_id, { resolution: { width, height } })
                    }}
                    disabled={streamingMode === 'pipeline' || isSensorStreaming}
                    className="bg-gray-700 text-white rounded px-1 py-0.5 text-xs"
                  >
                    {availableResolutions.map(([w, h]) => (
                      <option key={`${w}x${h}`} value={`${w}x${h}`}>
                        {w}×{h}
                      </option>
                    ))}
                  </select>
                </div>
                <div className="flex items-center gap-1">
                  <label className="text-gray-500">FPS:</label>
                  <select
                    value={sensorConfig.framerate}
                    onChange={(e) => onUpdateSensorConfig(sensor.sensor_id, { framerate: Number(e.target.value) })}
                    disabled={streamingMode === 'pipeline' || isSensorStreaming}
                    className="bg-gray-700 text-white rounded px-1 py-0.5 text-xs"
                  >
                    {availableFps.map((fps) => (
                      <option key={fps} value={fps}>
                        {fps}
                      </option>
                    ))}
                  </select>
                </div>
              </div>
            )}

            <div className="space-y-1">
              {sensorStreamConfigs.map((config) => (
                <StreamConfigItem
                  key={`${config.sensor_id}-${config.stream_type}`}
                  config={config}
                  sensors={sensors}
                  onUpdate={onUpdateStreamConfig}
                  disabled={streamingMode === 'pipeline' || isSensorStreaming}
                  isMotionSensor={sensorConfig?.isMotionSensor ?? false}
                />
              ))}
            </div>
          </div>
        )
      })}
    </div>
  )
}

interface StreamConfigItemProps {
  config: StreamConfig
  sensors: SensorInfo[]
  onUpdate: (config: StreamConfig) => void
  disabled: boolean
  isMotionSensor: boolean
}

function StreamConfigItem({ config, sensors, onUpdate, disabled, isMotionSensor }: StreamConfigItemProps) {
  const sensor = sensors.find((s) => s.sensor_id === config.sensor_id)
  const profile = sensor?.supported_stream_profiles.find((p) => 
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
      {/* Per-stream FPS selector for motion sensors */}
      {config.enable && isMotionSensor && (
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

interface SensorOptionsPanelProps {
  sensor: SensorInfo
  options: OptionInfo[]
  isExpanded: boolean
  onToggle: () => void
  onSetOption: (sensorId: string, optionId: string, value: number | boolean | string) => Promise<void>
}

function SensorOptionsPanel({ sensor, options, isExpanded, onToggle, onSetOption }: SensorOptionsPanelProps) {
  const [expandedCategories, setExpandedCategories] = useState<Set<string>>(new Set())

  // Group options by category
  const optionsByCategory = options.reduce((acc, option) => {
    const category = option.category || 'Basic Controls'
    if (!acc[category]) {
      acc[category] = []
    }
    acc[category].push(option)
    return acc
  }, {} as Record<string, OptionInfo[]>)

  // Ensure consistent category order: Basic Controls first, then Post-Processing
  const categoryOrder = ['Basic Controls', 'Post-Processing']
  const sortedCategories = Object.keys(optionsByCategory).sort((a, b) => {
    const indexA = categoryOrder.indexOf(a)
    const indexB = categoryOrder.indexOf(b)
    if (indexA === -1 && indexB === -1) return a.localeCompare(b)
    if (indexA === -1) return 1
    if (indexB === -1) return -1
    return indexA - indexB
  })

  const toggleCategory = (category: string) => {
    setExpandedCategories(prev => {
      const newSet = new Set(prev)
      if (newSet.has(category)) {
        newSet.delete(category)
      } else {
        newSet.add(category)
      }
      return newSet
    })
  }

  const handleRestoreCategoryDefaults = async (category: string) => {
    const categoryOptions = optionsByCategory[category] || []
    for (const option of categoryOptions) {
      if (!option.read_only && option.current_value !== option.default_value) {
        await onSetOption(sensor.sensor_id, option.option_id, option.default_value)
      }
    }
  }

  const handleRestoreAllDefaults = async () => {
    for (const option of options) {
      if (!option.read_only && option.current_value !== option.default_value) {
        await onSetOption(sensor.sensor_id, option.option_id, option.default_value)
      }
    }
  }

  // Count modified options across all categories
  const modifiedCount = options.filter(o => !o.read_only && o.current_value !== o.default_value).length

  return (
    <div className="mb-1">
      <button
        onClick={onToggle}
        className="w-full flex items-center justify-between p-1.5 bg-gray-800/50 rounded hover:bg-gray-700 transition-colors text-xs"
      >
        <span className="font-medium">{sensor.name}</span>
        <div className="flex items-center gap-1">
          {modifiedCount > 0 && (
            <span className="px-1.5 py-0.5 bg-rs-blue/20 text-rs-blue rounded text-[10px]">
              {modifiedCount} modified
            </span>
          )}
          <svg
            className={`w-4 h-4 transition-transform ${isExpanded ? 'rotate-180' : ''}`}
            fill="none"
            stroke="currentColor"
            viewBox="0 0 24 24"
          >
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 9l-7 7-7-7" />
          </svg>
        </div>
      </button>

      {isExpanded && (
        <div className="mt-1 space-y-1 pl-2">
          {options.length === 0 ? (
            <p className="text-gray-500 text-xs py-1">No options available</p>
          ) : (
            <>
              {/* Global restore defaults for all sensor options */}
              {modifiedCount > 0 && (
                <button
                  onClick={handleRestoreAllDefaults}
                  className="w-full flex items-center justify-center gap-1 p-1 bg-gray-700/50 hover:bg-gray-600 rounded text-xs text-gray-300 transition-colors mb-1"
                >
                  <svg className="w-3 h-3" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
                  </svg>
                  Restore All Defaults
                </button>
              )}

              {sortedCategories.map(category => (
                <CategorySection
                  key={category}
                  category={category}
                  options={optionsByCategory[category]}
                  isExpanded={expandedCategories.has(category)}
                  onToggle={() => toggleCategory(category)}
                  onRestoreDefaults={() => handleRestoreCategoryDefaults(category)}
                  sensorId={sensor.sensor_id}
                  onSetOption={onSetOption}
                />
              ))}
            </>
          )}
        </div>
      )}
    </div>
  )
}

interface CategorySectionProps {
  category: string
  options: OptionInfo[]
  isExpanded: boolean
  onToggle: () => void
  onRestoreDefaults: () => void
  sensorId: string
  onSetOption: (sensorId: string, optionId: string, value: number | boolean | string) => Promise<void>
}

function CategorySection({ category, options, isExpanded, onToggle, onRestoreDefaults, sensorId, onSetOption }: CategorySectionProps) {
  const hasModifiedOptions = options.some(opt => !opt.read_only && opt.current_value !== opt.default_value)

  // For Post-Processing category, render specialized filter sections
  if (category === 'Post-Processing') {
    return (
      <PostProcessingSection
        options={options}
        isExpanded={isExpanded}
        onToggle={onToggle}
        onRestoreDefaults={onRestoreDefaults}
        sensorId={sensorId}
        onSetOption={onSetOption}
        hasModifiedOptions={hasModifiedOptions}
      />
    )
  }

  return (
    <div className="border border-gray-700 rounded overflow-hidden">
      <div className="flex items-center bg-gray-750 hover:bg-gray-700 transition-colors">
        <button
          onClick={onToggle}
          className="flex-1 flex items-center justify-between p-1.5"
        >
          <span className="text-xs font-medium text-gray-300">{category}</span>
          <svg
            className={`w-3 h-3 transition-transform ${isExpanded ? 'rotate-180' : ''}`}
            fill="none"
            stroke="currentColor"
            viewBox="0 0 24 24"
          >
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 9l-7 7-7-7" />
          </svg>
        </button>
        {hasModifiedOptions && (
          <button
            onClick={(e) => { e.stopPropagation(); onRestoreDefaults(); }}
            className="px-1.5 py-0.5 mr-1 text-[10px] text-rs-blue hover:text-blue-400"
            title={`Restore ${category} to defaults`}
          >
            <svg className="w-3 h-3" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
            </svg>
          </button>
        )}
      </div>

      {isExpanded && (
        <div className="p-1.5 space-y-1 bg-gray-800/30">
          {options.map((option) => (
            <OptionControl 
              key={option.option_id} 
              option={option} 
              sensorId={sensorId}
              onSetOption={onSetOption}
            />
          ))}
        </div>
      )}
    </div>
  )
}

interface PostProcessingSectionProps {
  options: OptionInfo[]
  isExpanded: boolean
  onToggle: () => void
  onRestoreDefaults: () => void
  sensorId: string
  onSetOption: (sensorId: string, optionId: string, value: number | boolean | string) => Promise<void>
  hasModifiedOptions: boolean
}

function PostProcessingSection({ options, isExpanded, onToggle, onRestoreDefaults, sensorId, onSetOption, hasModifiedOptions }: PostProcessingSectionProps) {
  const [expandedFilters, setExpandedFilters] = useState<Set<string>>(new Set())

  // Group options by filter_name
  const filterGroups = options.reduce((acc, option) => {
    const filterName = option.filter_name || 'Other'
    if (!acc[filterName]) {
      acc[filterName] = { enableOption: null as OptionInfo | null, paramOptions: [] as OptionInfo[] }
    }
    if (option.option_id.endsWith('_Enabled')) {
      acc[filterName].enableOption = option
    } else {
      acc[filterName].paramOptions.push(option)
    }
    return acc
  }, {} as Record<string, { enableOption: OptionInfo | null; paramOptions: OptionInfo[] }>)

  const toggleFilter = (filterName: string) => {
    setExpandedFilters(prev => {
      const newSet = new Set(prev)
      if (newSet.has(filterName)) {
        newSet.delete(filterName)
      } else {
        newSet.add(filterName)
      }
      return newSet
    })
  }

  return (
    <div className="border border-gray-700 rounded overflow-hidden">
      <div className="flex items-center bg-gray-750 hover:bg-gray-700 transition-colors">
        <button
          onClick={onToggle}
          className="flex-1 flex items-center justify-between p-1.5"
        >
          <span className="text-xs font-medium text-gray-300">Post-Processing</span>
          <svg
            className={`w-3 h-3 transition-transform ${isExpanded ? 'rotate-180' : ''}`}
            fill="none"
            stroke="currentColor"
            viewBox="0 0 24 24"
          >
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 9l-7 7-7-7" />
          </svg>
        </button>
        {hasModifiedOptions && (
          <button
            onClick={(e) => { e.stopPropagation(); onRestoreDefaults(); }}
            className="px-1.5 py-0.5 mr-1 text-[10px] text-rs-blue hover:text-blue-400"
            title="Restore Post-Processing to defaults"
          >
            <svg className="w-3 h-3" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
            </svg>
          </button>
        )}
      </div>

      {isExpanded && (
        <div className="p-1.5 space-y-1 bg-gray-800/30">
          {Object.entries(filterGroups).map(([filterName, group]) => (
            <FilterDropdown
              key={filterName}
              filterName={filterName}
              enableOption={group.enableOption}
              paramOptions={group.paramOptions}
              isExpanded={expandedFilters.has(filterName)}
              onToggle={() => toggleFilter(filterName)}
              sensorId={sensorId}
              onSetOption={onSetOption}
            />
          ))}
        </div>
      )}
    </div>
  )
}

interface FilterDropdownProps {
  filterName: string
  enableOption: OptionInfo | null
  paramOptions: OptionInfo[]
  isExpanded: boolean
  onToggle: () => void
  sensorId: string
  onSetOption: (sensorId: string, optionId: string, value: number | boolean | string) => Promise<void>
}

function FilterDropdown({ filterName, enableOption, paramOptions, isExpanded, onToggle, sensorId, onSetOption }: FilterDropdownProps) {
  const isEnabled = enableOption ? Boolean(enableOption.current_value) : false

  const handleToggleEnable = async (e: React.MouseEvent) => {
    e.stopPropagation()
    if (enableOption) {
      await onSetOption(sensorId, enableOption.option_id, isEnabled ? 0 : 1)
    }
  }

  return (
    <div className="border border-gray-600 rounded overflow-hidden">
      <div 
        className="flex items-center justify-between p-1.5 bg-gray-700/50 hover:bg-gray-700 transition-colors cursor-pointer"
        onClick={onToggle}
      >
        <div className="flex items-center gap-1.5">
          <svg
            className={`w-2.5 h-2.5 transition-transform text-gray-400 ${isExpanded ? 'rotate-90' : ''}`}
            fill="none"
            stroke="currentColor"
            viewBox="0 0 24 24"
          >
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 5l7 7-7 7" />
          </svg>
          <span className="text-xs font-medium text-gray-200">{filterName}</span>
        </div>
        
        {/* Toggle Switch */}
        {enableOption && (
          <button
            onClick={handleToggleEnable}
            className={`relative w-8 h-4 rounded-full transition-colors ${
              isEnabled ? 'bg-rs-blue' : 'bg-gray-600'
            }`}
          >
            <span
              className={`absolute top-0.5 left-0.5 w-3 h-3 rounded-full bg-white transition-transform ${
                isEnabled ? 'translate-x-4' : ''
              }`}
            />
          </button>
        )}
      </div>

      {isExpanded && paramOptions.length > 0 && (
        <div className="p-1.5 space-y-1 bg-gray-800/50">
          {paramOptions.map((option) => (
            <OptionControl
              key={option.option_id}
              option={option}
              sensorId={sensorId}
              onSetOption={onSetOption}
            />
          ))}
        </div>
      )}
    </div>
  )
}

interface OptionControlProps {
  option: OptionInfo
  sensorId: string
  onSetOption: (sensorId: string, optionId: string, value: number | boolean | string) => Promise<void>
}

function OptionControl({ option, sensorId, onSetOption }: OptionControlProps) {
  const [localValue, setLocalValue] = useState(option.current_value)

  // Sync with external changes (e.g., from chatbot)
  useEffect(() => {
    setLocalValue(option.current_value)
  }, [option.current_value])

  const handleChange = async (value: number | boolean | string) => {
    setLocalValue(value)
    try {
      await onSetOption(sensorId, option.option_id, value)
    } catch (error) {
      setLocalValue(option.current_value)
    }
  }

  const handleRestoreDefault = async () => {
    await handleChange(option.default_value)
  }

  const isModified = localValue !== option.default_value
  const isBoolean = typeof option.current_value === 'boolean' || 
    (option.min_value === 0 && option.max_value === 1 && option.step === 1 && !option.value_descriptions)
  const isEnum = option.value_descriptions && Object.keys(option.value_descriptions).length > 0
  const isSlider = typeof option.min_value === 'number' && typeof option.max_value === 'number'

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
          {option.name}
        </label>
        <div className="flex items-center gap-1">
          {option.units && <span className="text-gray-500">{option.units}</span>}
          {!option.read_only && isModified && (
            <button
              onClick={handleRestoreDefault}
              className="text-gray-500 hover:text-rs-blue transition-colors"
              title={`Restore default (${getDefaultDisplay()})`}
            >
              <svg className="w-3 h-3" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
              </svg>
            </button>
          )}
        </div>
      </div>

      {option.read_only ? (
        <div className="text-gray-400">{String(localValue)}</div>
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
      ) : isSlider ? (
        <div className="flex items-center gap-1">
          <input
            type="range"
            min={option.min_value}
            max={option.max_value}
            step={option.step || 1}
            value={Number(localValue)}
            onChange={(e) => setLocalValue(Number(e.target.value))}
            onMouseUp={() => handleChange(Number(localValue))}
            onTouchEnd={() => handleChange(Number(localValue))}
            className="flex-1 h-1"
          />
          <span className="text-gray-400 w-10 text-right">
            {typeof localValue === 'number' ? localValue.toFixed(option.step && option.step < 1 ? 1 : 0) : localValue}
          </span>
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
