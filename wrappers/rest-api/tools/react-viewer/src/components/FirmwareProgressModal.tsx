import { useEffect } from 'react'
import { onEvent } from '../api/events'
import type { DeviceInfo, FirmwarePhase, FirmwareState } from '../api/types'

// What each step of an update shows: its title, and what the user waits for
const STEPS: Record<FirmwarePhase, [string, string]> = {
  downloading: ['Downloading firmware…', 'Fetching the recommended image. This can be slow on some networks.'],
  checking: ['Checking the image…', 'The SDK checks that this image fits the camera.'],
  recovery: ['Switching to recovery mode…', 'The camera disconnects and comes back as a recovery device.'],
  installing: ['Installing firmware…', 'This may take 1-2 minutes. Please do not disconnect the device.'],
  restarting: ['Restarting the camera…', 'Written. Waiting for the camera to come back with its new firmware.'],
}

interface FirmwareProgressModalProps {
  isOpen: boolean
  device: DeviceInfo
  firmware: FirmwareState
  fileName?: string
  onClose: () => void
  onProgressUpdate: (progress: number) => void
}

export function FirmwareProgressModal({
  isOpen,
  device,
  firmware,
  fileName,
  onClose,
  onProgressUpdate,
}: FirmwareProgressModalProps) {
  useEffect(() => {
    if (!isOpen) return

    return onEvent<{ device_id: string; progress: number }>('firmware_progress',
      (m) => m.device_id === device.device_id && onProgressUpdate(m.progress))
  }, [isOpen, device.device_id, onProgressUpdate])

  if (!isOpen) return null

  // Each phase fills the bar once; its label says which one it is
  const progress = Math.round((firmware.progress || 0) * 100)
  // Only the update's outcome means done: the SDK hits 100% long before the camera is back.
  const done = firmware.is_updating === false && !firmware.last_error

  return (
    <>
      <div className="fixed inset-0 bg-black/60 z-40" />
      <div className="fixed inset-0 flex items-center justify-center z-50">
        <div className="bg-gray-900 border border-gray-700 rounded-lg shadow-2xl p-6 max-w-md w-full mx-4">
          <div className="mb-4">
            <h2 className="text-xl font-bold text-white">Firmware Update</h2>
            <p className="text-sm text-gray-400 mt-1">{device.info.name}</p>
            {fileName && (
              <p className="text-xs text-gray-500 mt-1 truncate" title={fileName}>From file: {fileName}</p>
            )}
          </div>

          <div className="mb-6">
            {firmware.last_error ? (
              <div className="p-3 bg-red-900/50 border border-red-700 rounded text-red-200 text-sm">
                <div className="font-semibold mb-1">Update Failed</div>
                <div>{firmware.last_error}</div>
              </div>
            ) : done ? (
              <div className="p-3 bg-green-900/50 border border-green-700 rounded text-green-200 text-sm">
                <div className="font-semibold">✓ Update Complete</div>
                <div className="text-sm mt-1">Firmware has been successfully installed</div>
              </div>
            ) : (
              <div className="text-gray-300 text-sm">
                <div className="font-semibold mb-1">{STEPS[firmware.phase ?? 'installing'][0]}</div>
                <div className="text-gray-400">{STEPS[firmware.phase ?? 'installing'][1]}</div>
              </div>
            )}
          </div>

          <div className="mb-6">
            <div className="flex items-center justify-between mb-2">
              <span className="text-sm text-gray-400">Progress</span>
              <span className="text-sm font-semibold text-white">{progress}%</span>
            </div>
            <div className="w-full bg-gray-700 rounded-full h-2 overflow-hidden">
              <div
                className={`h-full transition-all duration-300 ${
                  firmware.last_error ? 'bg-red-500' : done ? 'bg-green-500' : 'bg-rs-blue'
                }`}
                style={{ width: `${progress}%` }}
              />
            </div>
          </div>

          {!firmware.last_error && device.info.firmware_version && (
            <div className="mb-6 p-3 bg-gray-800 rounded text-xs text-gray-300">
              <div>Current: <span className="text-white">{device.info.firmware_version}</span></div>
            </div>
          )}

          <div className="flex gap-3">
            {firmware.last_error ? (
              <button
                onClick={onClose}
                className="flex-1 px-4 py-2 bg-gray-700 hover:bg-gray-600 text-white rounded font-medium transition-colors"
              >
                Close
              </button>
            ) : done ? (
              <button
                onClick={onClose}
                className="flex-1 px-4 py-2 bg-green-600 hover:bg-green-500 text-white rounded font-medium transition-colors"
              >
                Done
              </button>
            ) : (
              <div className="flex-1 px-4 py-2 bg-gray-700 text-gray-300 rounded font-medium text-center">
                <div className="inline-block w-4 h-4 border-2 border-rs-blue border-t-transparent rounded-full animate-spin mr-2" />
                In Progress
              </div>
            )}
          </div>

        </div>
      </div>
    </>
  )
}
