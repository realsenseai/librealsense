import { useState } from 'react'
import { useAppStore } from '../store'
import type { SensorInfo, OptionInfo, StreamConfig } from '../api/types'

export function ControlsPanel() {
  const { selectedDevice, sensors, options, streamConfigs, updateStreamConfig, isStreaming } =
    useAppStore()

  const [expandedSensor, setExpandedSensor] = useState<string | null>(null)

  if (!selectedDevice) return null

  return (
    <div className="p-4 space-y-4">
      <h2 className="panel-header">Stream Configuration</h2>

      {/* Stream Toggles */}
      <div className="space-y-2">
        {streamConfigs.map((config) => (
          <StreamConfigItem
            key={`${config.sensor_id}-${config.stream_type}`}
            config={config}
            sensors={sensors}
            onUpdate={updateStreamConfig}
            disabled={isStreaming}
          />
        ))}
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
          />
        ))}
      </div>
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
}

function SensorOptionsPanel({ sensor, options, isExpanded, onToggle }: SensorOptionsPanelProps) {
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
              <OptionControl key={option.option_id} option={option} sensorId={sensor.sensor_id} />
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
}

function OptionControl({ option, sensorId }: OptionControlProps) {
  const { selectedDevice, setOption } = useAppStore()
  const [localValue, setLocalValue] = useState(option.current_value)

  const handleChange = async (value: number | boolean | string) => {
    if (!selectedDevice) return
    setLocalValue(value)
    try {
      await setOption(selectedDevice.device_id, sensorId, option.option_id, value)
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
