import { useEffect, useState } from 'react'
import { useAppStore } from '../store'
import type { DeviceInfo, SensorInfo, OptionInfo, StreamConfig, DeviceState } from '../api/types'

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
    setOption,
    startDeviceStreaming,
    stopDeviceStreaming,
  } = useAppStore()

  const isStreaming = isAnyDeviceStreaming()

  useEffect(() => {
    fetchDevices()
    // Only poll for device changes when NOT streaming (polling causes frame hiccups)
    if (!isStreaming) {
      const interval = setInterval(fetchDevices, 5000)
      return () => clearInterval(interval)
    }
  }, [fetchDevices, isStreaming])

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
                onSetOption={(sensorId, optionId, value) => 
                  setOption(device.device_id, sensorId, optionId, value)
                }
                onStartStreaming={() => startDeviceStreaming(device.device_id)}
                onStopStreaming={() => stopDeviceStreaming(device.device_id)}
              />
            )
          })}
        </div>
      )}
    </div>
  )
}

interface DeviceCardProps {
  device: DeviceInfo
  deviceState?: DeviceState
  onToggle: () => void
  onReset: () => void
  onUpdateStreamConfig: (config: StreamConfig) => void
  onSetOption: (sensorId: string, optionId: string, value: number | boolean | string) => Promise<void>
  onStartStreaming: () => void
  onStopStreaming: () => void
}

function DeviceCard({ 
  device, 
  deviceState, 
  onToggle, 
  onReset, 
  onUpdateStreamConfig,
  onSetOption,
  onStartStreaming,
  onStopStreaming,
}: DeviceCardProps) {
  const [showMenu, setShowMenu] = useState(false)
  const [expandedSensor, setExpandedSensor] = useState<string | null>(null)
  
  const isActive = deviceState?.isActive || false
  const isLoading = deviceState?.isLoading || false
  const isStreaming = deviceState?.isStreaming || false
  const sensors = deviceState?.sensors || []
  const options = deviceState?.options || {}
  const streamConfigs = deviceState?.streamConfigs || []
  
  const hasEnabledStreams = streamConfigs.some(c => c.enable)

  return (
    <div
      className={`rounded-lg transition-all ${
        isActive
          ? 'bg-rs-blue/10 border border-rs-blue'
          : 'bg-gray-800 border border-gray-700 hover:border-gray-600'
      }`}
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
                onClick={() => setShowMenu(!showMenu)}
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
                        // TODO: Implement calibration
                        alert('Calibration feature coming soon')
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
                        // TODO: Implement tare calibration
                        alert('Tare calibration feature coming soon')
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
              onClick={onToggle}
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
              <h4 className="text-sm font-medium text-gray-300">Streams</h4>
              {/* Start/Stop Button */}
              <button
                onClick={isStreaming ? onStopStreaming : onStartStreaming}
                disabled={!hasEnabledStreams && !isStreaming}
                className={`px-3 py-1 rounded text-xs font-semibold transition-colors ${
                  isStreaming
                    ? 'bg-red-600 hover:bg-red-700 text-white'
                    : hasEnabledStreams
                      ? 'bg-green-600 hover:bg-green-700 text-white'
                      : 'bg-gray-600 text-gray-400 cursor-not-allowed'
                }`}
              >
                {isStreaming ? '■ Stop' : '▶ Start'}
              </button>
            </div>
            <div className="space-y-2">
              {streamConfigs.map((config) => (
                <StreamConfigItem
                  key={`${config.sensor_id}-${config.stream_type}`}
                  config={config}
                  sensors={sensors}
                  onUpdate={onUpdateStreamConfig}
                  disabled={isStreaming}
                />
              ))}
            </div>
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

interface StreamConfigItemProps {
  config: StreamConfig
  sensors: SensorInfo[]
  onUpdate: (config: StreamConfig) => void
  disabled: boolean
}

function StreamConfigItem({ config, sensors, onUpdate, disabled }: StreamConfigItemProps) {
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

  return (
    <div className="bg-gray-800/50 rounded p-2">
      <div className="flex items-center gap-2">
        <input
          type="checkbox"
          checked={config.enable}
          onChange={(e) => onUpdate({ ...config, enable: e.target.checked })}
          disabled={disabled}
          className="control-checkbox w-3 h-3"
        />
        <span className={`text-xs font-semibold ${getStreamColor(config.stream_type)}`}>
          {config.stream_type.toUpperCase()}
        </span>
        {config.enable && (
          <span className="text-xs text-gray-500 ml-auto">
            {config.resolution.width}×{config.resolution.height} @ {config.framerate}fps
          </span>
        )}
      </div>

      {config.enable && (
        <div className="mt-2 ml-5 space-y-1 text-xs">
          {/* Resolution */}
          <div className="flex items-center gap-2">
            <label className="w-16 text-gray-500">Res:</label>
            <select
              value={`${config.resolution.width}x${config.resolution.height}`}
              onChange={(e) => {
                const [width, height] = e.target.value.split('x').map(Number)
                onUpdate({ ...config, resolution: { width, height } })
              }}
              disabled={disabled}
              className="flex-1 bg-gray-700 text-white rounded px-1 py-0.5 text-xs"
            >
              {profile.resolutions.map(([w, h]) => (
                <option key={`${w}x${h}`} value={`${w}x${h}`}>
                  {w}×{h}
                </option>
              ))}
            </select>
          </div>

          {/* FPS */}
          <div className="flex items-center gap-2">
            <label className="w-16 text-gray-500">FPS:</label>
            <select
              value={config.framerate}
              onChange={(e) => onUpdate({ ...config, framerate: Number(e.target.value) })}
              disabled={disabled}
              className="flex-1 bg-gray-700 text-white rounded px-1 py-0.5 text-xs"
            >
              {profile.fps.map((fps) => (
                <option key={fps} value={fps}>
                  {fps}
                </option>
              ))}
            </select>
          </div>

          {/* Format */}
          <div className="flex items-center gap-2">
            <label className="w-16 text-gray-500">Format:</label>
            <select
              value={config.format}
              onChange={(e) => onUpdate({ ...config, format: e.target.value })}
              disabled={disabled}
              className="flex-1 bg-gray-700 text-white rounded px-1 py-0.5 text-xs"
            >
              {profile.formats.map((format) => (
                <option key={format} value={format}>
                  {format}
                </option>
              ))}
            </select>
          </div>
        </div>
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
  return (
    <div className="mb-1">
      <button
        onClick={onToggle}
        className="w-full flex items-center justify-between p-1.5 bg-gray-800/50 rounded hover:bg-gray-700 transition-colors text-xs"
      >
        <span className="font-medium">{sensor.name}</span>
        <svg
          className={`w-4 h-4 transition-transform ${isExpanded ? 'rotate-180' : ''}`}
          fill="none"
          stroke="currentColor"
          viewBox="0 0 24 24"
        >
          <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 9l-7 7-7-7" />
        </svg>
      </button>

      {isExpanded && (
        <div className="mt-1 space-y-1 pl-2">
          {options.length === 0 ? (
            <p className="text-gray-500 text-xs py-1">No options available</p>
          ) : (
            options.map((option) => (
              <OptionControl 
                key={option.option_id} 
                option={option} 
                sensorId={sensor.sensor_id}
                onSetOption={onSetOption}
              />
            ))
          )}
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

  const handleChange = async (value: number | boolean | string) => {
    setLocalValue(value)
    try {
      await onSetOption(sensorId, option.option_id, value)
    } catch (error) {
      setLocalValue(option.current_value)
    }
  }

  const isBoolean = typeof option.current_value === 'boolean' || 
    (option.min_value === 0 && option.max_value === 1 && option.step === 1)
  const isSlider = typeof option.min_value === 'number' && typeof option.max_value === 'number'

  return (
    <div className="bg-gray-800/30 rounded p-1.5 text-xs">
      <div className="flex items-center justify-between mb-0.5">
        <label className="font-medium truncate text-gray-300" title={option.description}>
          {option.name}
        </label>
        {option.units && <span className="text-gray-500 ml-1">{option.units}</span>}
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
