import { useEffect } from 'react'
import { useAppStore } from '../store'
import type { DeviceInfo } from '../api/types'

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
        <div className="space-y-2">
          {devices.map((device) => {
            const deviceState = deviceStates[device.device_id]
            return (
              <DeviceCard
                key={device.device_id}
                device={device}
                isActive={deviceState?.isActive || false}
                isLoading={deviceState?.isLoading || false}
                isStreaming={deviceState?.isStreaming || false}
                onToggle={() => toggleDeviceActive(device)}
                onReset={() => resetDevice(device.device_id)}
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
  isActive: boolean
  isLoading: boolean
  isStreaming: boolean
  onToggle: () => void
  onReset: () => void
}

function DeviceCard({ device, isActive, isLoading, isStreaming, onToggle, onReset }: DeviceCardProps) {
  return (
    <div
      className={`p-3 rounded-lg transition-all ${
        isActive
          ? 'bg-rs-blue/20 border border-rs-blue'
          : 'bg-gray-800 border border-gray-700 hover:border-gray-600'
      }`}
    >
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

      {/* Sensors */}
      {device.sensors.length > 0 && (
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

      {/* Actions */}
      {isActive && (
        <div className="mt-3 pt-3 border-t border-gray-600">
          <button
            onClick={(e) => {
              e.stopPropagation()
              onReset()
            }}
            disabled={isStreaming}
            className={`text-xs ${isStreaming ? 'text-gray-500 cursor-not-allowed' : 'text-red-400 hover:text-red-300'} transition-colors`}
          >
            Hardware Reset
          </button>
        </div>
      )}
    </div>
  )
}
