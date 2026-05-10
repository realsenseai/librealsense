import { useEffect } from 'react'
import { apiClient } from '../api/client'
import type { FirmwareState } from '../api/types'

interface FirmwareProgressModalProps {
  isOpen: boolean
  device: {
    device_id: string
    name: string
  }
  firmware: FirmwareState
  onClose: () => void
  onProgressUpdate: (progress: number) => void
  onSuccess: (firmwareVersion: string | null) => void
  onError: (error: string) => void
}

export function FirmwareProgressModal({
  isOpen,
  device,
  firmware,
  onClose,
  onProgressUpdate,
  onSuccess,
  onError,
}: FirmwareProgressModalProps) {
  useEffect(() => {
    if (!isOpen) return

    // Subscribe to progress updates
    const unsubscribeProgress = apiClient.onFirmwareProgress(device.device_id, (progress: number) => {
      onProgressUpdate(progress)
    })

    const unsubscribeSuccess = apiClient.onFirmwareSuccess(device.device_id, (fwVersion: string | null) => {
      onSuccess(fwVersion)
      // Close modal after 2 seconds to show completion
      setTimeout(() => {
        onClose()
      }, 2000)
    })

    const unsubscribeError = apiClient.onFirmwareError(device.device_id, (error: string) => {
      onError(error)
    })

    return () => {
      unsubscribeProgress()
      unsubscribeSuccess()
      unsubscribeError()
    }
  }, [isOpen, device.device_id, onProgressUpdate, onSuccess, onError, onClose])

  if (!isOpen) return null

  const progress = Math.round((firmware.progress || 0) * 100)

  return (
    <>
      {/* Backdrop */}
      <div className="fixed inset-0 bg-black/60 z-40" />

      {/* Modal */}
      <div className="fixed inset-0 flex items-center justify-center z-50">
        <div className="bg-gray-900 border border-gray-700 rounded-lg shadow-2xl p-6 max-w-md w-full mx-4">
          {/* Header */}
          <div className="mb-4">
            <h2 className="text-xl font-bold text-white">Firmware Update</h2>
            <p className="text-sm text-gray-400 mt-1">{device.name}</p>
          </div>

          {/* Status Message */}
          <div className="mb-6">
            {firmware.last_error ? (
              <div className="p-3 bg-red-900/50 border border-red-700 rounded text-red-200 text-sm">
                <div className="font-semibold mb-1">Update Failed</div>
                <div>{firmware.last_error}</div>
              </div>
            ) : progress === 100 ? (
              <div className="p-3 bg-green-900/50 border border-green-700 rounded text-green-200 text-sm">
                <div className="font-semibold">✓ Update Complete</div>
                <div className="text-sm mt-1">Firmware has been successfully installed</div>
              </div>
            ) : (
              <div className="text-gray-300 text-sm">
                <div className="font-semibold mb-1">Installing firmware...</div>
                <div className="text-gray-400">This may take 1-2 minutes. Please do not disconnect the device.</div>
              </div>
            )}
          </div>

          {/* Progress Bar */}
          <div className="mb-6">
            <div className="flex items-center justify-between mb-2">
              <span className="text-sm text-gray-400">Progress</span>
              <span className="text-sm font-semibold text-white">{progress}%</span>
            </div>
            <div className="w-full bg-gray-700 rounded-full h-2 overflow-hidden">
              <div
                className={`h-full transition-all duration-300 ${
                  firmware.last_error ? 'bg-red-500' : progress === 100 ? 'bg-green-500' : 'bg-rs-blue'
                }`}
                style={{ width: `${progress}%` }}
              />
            </div>
          </div>

          {/* Version Info */}
          {!firmware.last_error && (
            <div className="mb-6 p-3 bg-gray-800 rounded text-xs text-gray-300 space-y-1">
              <div>Current: <span className="text-white">{firmware.current || 'Unknown'}</span></div>
              <div>Target: <span className="text-white">{firmware.recommended || 'Unknown'}</span></div>
            </div>
          )}

          {/* Actions */}
          <div className="flex gap-3">
            {firmware.last_error ? (
              <button
                onClick={onClose}
                className="flex-1 px-4 py-2 bg-gray-700 hover:bg-gray-600 text-white rounded font-medium transition-colors"
              >
                Close
              </button>
            ) : progress === 100 ? (
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

          {/* Warning */}
          {!firmware.last_error && progress < 100 && (
            <div className="mt-4 p-3 bg-amber-900/40 border border-amber-600 rounded text-amber-200 text-xs">
              ⚠ Do not disconnect the device during the update
            </div>
          )}
        </div>
      </div>
    </>
  )
}
