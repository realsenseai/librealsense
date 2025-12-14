import { useState } from 'react'
import { useAppStore } from '../store'
import type { SensorInfo, OptionInfo, StreamConfig, DeviceState } from '../api/types'

export function ControlsPanel() {
  const { deviceStates, updateStreamConfig, setOption, isAnyDeviceStreaming } = useAppStore()
  
  const activeDevices = Object.values(deviceStates).filter(ds => ds.isActive)
  const isStreaming = isAnyDeviceStreaming()
  const [expandedDevices, setExpandedDevices] = useState<Set<string>>(new Set())

  const toggleDeviceExpanded = (deviceId: string) => {
    setExpandedDevices(prev => {
      const newSet = new Set(prev)
      if (newSet.has(deviceId)) {
        newSet.delete(deviceId)
      } else {
        newSet.add(deviceId)
      }
      return newSet
    })
  }

  if (activeDevices.length === 0) {
    return (
      <div className="p-4 text-gray-500 text-center">
        <p>No devices activated</p>
        <p className="text-sm mt-1">Toggle a device on to configure streams</p>
      </div>
    )
  }

  return (
    <div className="p-4 space-y-4">
      {activeDevices.map((deviceState) => (
        <DeviceControlSection
          key={deviceState.device.device_id}
          deviceState={deviceState}
          isExpanded={activeDevices.length === 1 || expandedDevices.has(deviceState.device.device_id)}
          onToggle={() => toggleDeviceExpanded(deviceState.device.device_id)}
          onUpdateStreamConfig={(config) => updateStreamConfig(deviceState.device.device_id, config)}
          onSetOption={(sensorId, optionId, value) => 
            setOption(deviceState.device.device_id, sensorId, optionId, value)
          }
          isStreaming={isStreaming}
          showHeader={activeDevices.length > 1}
        />
      ))}
    </div>
  )
}

interface DeviceControlSectionProps {
  deviceState: DeviceState
  isExpanded: boolean
  onToggle: () => void
  onUpdateStreamConfig: (config: StreamConfig) => void
  onSetOption: (sensorId: string, optionId: string, value: number | boolean | string) => Promise<void>
  isStreaming: boolean
  showHeader: boolean
}

function DeviceControlSection({
  deviceState,
  isExpanded,
  onToggle,
  onUpdateStreamConfig,
  onSetOption,
  isStreaming,
  showHeader,
}: DeviceControlSectionProps) {
  const [expandedSensor, setExpandedSensor] = useState<string | null>(null)
  const { device, sensors, options, streamConfigs } = deviceState

  return (
    <div className={showHeader ? 'border border-gray-700 rounded-lg overflow-hidden' : ''}>
      {/* Device Header - collapsible when multiple devices */}
      {showHeader && (
        <button
          onClick={onToggle}
          className="w-full flex items-center justify-between p-3 bg-gray-800 hover:bg-gray-750 transition-colors"
        >
          <div className="flex items-center gap-2">
            <span className="font-semibold text-white">{device.name}</span>
            {deviceState.isStreaming && (
              <span className="w-2 h-2 bg-green-500 rounded-full animate-pulse" />
            )}
          </div>
          <svg
            className={`w-5 h-5 transition-transform ${isExpanded ? 'rotate-180' : ''}`}
            fill="none"
            stroke="currentColor"
            viewBox="0 0 24 24"
          >
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 9l-7 7-7-7" />
          </svg>
        </button>
      )}

      {/* Device Controls */}
      {(isExpanded || !showHeader) && (
        <div className={showHeader ? 'p-3 space-y-4 bg-gray-900/50' : 'space-y-4'}>
          {/* Stream Configuration */}
          <div>
            <h2 className="panel-header">Stream Configuration</h2>
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

          {/* Sensor Options */}
          <div className="border-t border-gray-700 pt-4">
            <h2 className="panel-header">Camera Controls</h2>
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
  const profile = sensor?.supported_stream_profiles.find((p) => p.stream_type === config.stream_type)

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
    <div className="bg-gray-800 rounded-lg p-3">
      <div className="flex items-center gap-3 mb-2">
        <input
          type="checkbox"
          checked={config.enable}
          onChange={(e) => onUpdate({ ...config, enable: e.target.checked })}
          disabled={disabled}
          className="control-checkbox"
        />
        <span className={`font-semibold ${getStreamColor(config.stream_type)}`}>
          {config.stream_type.toUpperCase()}
        </span>
      </div>

      {config.enable && (
        <div className="ml-7 space-y-2 text-sm">
          {/* Resolution */}
          <div className="flex items-center gap-2">
            <label className="w-20 text-gray-400">Resolution:</label>
            <select
              value={`${config.resolution.width}x${config.resolution.height}`}
              onChange={(e) => {
                const [width, height] = e.target.value.split('x').map(Number)
                onUpdate({ ...config, resolution: { width, height } })
              }}
              disabled={disabled}
              className="control-select flex-1 text-sm py-1"
            >
              {profile.resolutions.map(([w, h]) => (
                <option key={`${w}x${h}`} value={`${w}x${h}`}>
                  {w} × {h}
                </option>
              ))}
            </select>
          </div>

          {/* FPS */}
          <div className="flex items-center gap-2">
            <label className="w-20 text-gray-400">FPS:</label>
            <select
              value={config.framerate}
              onChange={(e) => onUpdate({ ...config, framerate: Number(e.target.value) })}
              disabled={disabled}
              className="control-select flex-1 text-sm py-1"
            >
              {profile.fps.map((fps) => (
                <option key={fps} value={fps}>
                  {fps} fps
                </option>
              ))}
            </select>
          </div>

          {/* Format */}
          <div className="flex items-center gap-2">
            <label className="w-20 text-gray-400">Format:</label>
            <select
              value={config.format}
              onChange={(e) => onUpdate({ ...config, format: e.target.value })}
              disabled={disabled}
              className="control-select flex-1 text-sm py-1"
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
    <div className="mb-2">
      <button
        onClick={onToggle}
        className="w-full flex items-center justify-between p-2 bg-gray-800 rounded-lg hover:bg-gray-700 transition-colors"
      >
        <span className="font-medium">{sensor.name}</span>
        <svg
          className={`w-5 h-5 transition-transform ${isExpanded ? 'rotate-180' : ''}`}
          fill="none"
          stroke="currentColor"
          viewBox="0 0 24 24"
        >
          <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 9l-7 7-7-7" />
        </svg>
      </button>

      {isExpanded && (
        <div className="mt-2 space-y-2 pl-2">
          {options.length === 0 ? (
            <p className="text-gray-500 text-sm">No options available</p>
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
      // Revert on error
      setLocalValue(option.current_value)
    }
  }

  // Determine control type
  const isBoolean = typeof option.current_value === 'boolean' || 
    (option.min_value === 0 && option.max_value === 1 && option.step === 1)
  const isSlider = typeof option.min_value === 'number' && typeof option.max_value === 'number'

  return (
    <div className="bg-gray-800/50 rounded-lg p-2">
      <div className="flex items-center justify-between mb-1">
        <label className="text-sm font-medium truncate" title={option.description}>
          {option.name}
        </label>
        {option.units && <span className="text-xs text-gray-500">{option.units}</span>}
      </div>

      {option.read_only ? (
        <div className="text-sm text-gray-400">{String(localValue)}</div>
      ) : isBoolean ? (
        <label className="flex items-center gap-2 cursor-pointer">
          <input
            type="checkbox"
            checked={Boolean(localValue)}
            onChange={(e) => handleChange(e.target.checked)}
            className="control-checkbox"
          />
          <span className="text-sm text-gray-400">{localValue ? 'On' : 'Off'}</span>
        </label>
      ) : isSlider ? (
        <div className="flex items-center gap-2">
          <input
            type="range"
            min={option.min_value}
            max={option.max_value}
            step={option.step || 1}
            value={Number(localValue)}
            onChange={(e) => setLocalValue(Number(e.target.value))}
            onMouseUp={() => handleChange(Number(localValue))}
            onTouchEnd={() => handleChange(Number(localValue))}
            className="control-slider flex-1"
          />
          <span className="text-sm text-gray-400 w-16 text-right">
            {typeof localValue === 'number' ? localValue.toFixed(option.step && option.step < 1 ? 2 : 0) : localValue}
          </span>
        </div>
      ) : (
        <input
          type="text"
          value={String(localValue)}
          onChange={(e) => setLocalValue(e.target.value)}
          onBlur={() => handleChange(localValue)}
          className="w-full bg-gray-700 text-white rounded px-2 py-1 text-sm"
        />
      )}

      {/* Reset to default */}
      {!option.read_only && localValue !== option.default_value && (
        <button
          onClick={() => handleChange(option.default_value)}
          className="mt-1 text-xs text-rs-blue hover:text-blue-400"
        >
          Reset to default ({String(option.default_value)})
        </button>
      )}
    </div>
  )
}
