import { Check, X, Settings } from 'lucide-react'
import { useAppStore } from '../../store'
import type { ProposedSettings } from '../../utils/chatPrompt'

interface SettingsPreviewProps {
  settings: ProposedSettings
}

/**
 * Preview panel for proposed settings with Apply/Dismiss buttons
 */
export function SettingsPreview({ settings }: SettingsPreviewProps) {
  const { applyProposedSettings, dismissProposedSettings, deviceStates } = useAppStore()

  // Find the device
  const device = Object.values(deviceStates).find(
    ds => ds.device.serial_number === settings.deviceSerial
  )

  const hasStreamChanges = settings.streamConfigs && settings.streamConfigs.length > 0
  const hasOptionChanges = settings.optionChanges && settings.optionChanges.length > 0

  if (!hasStreamChanges && !hasOptionChanges) return null

  return (
    <div className="mx-3 mb-3 p-3 bg-gray-800 border border-rs-blue/50 rounded-lg">
      <div className="flex items-center justify-between mb-2">
        <div className="flex items-center gap-2">
          <Settings className="w-4 h-4 text-rs-blue" />
          <span className="text-sm font-medium text-white">Proposed Changes</span>
        </div>
        <span className="text-xs text-gray-400">
          {device?.device.name || settings.deviceSerial}
        </span>
      </div>

      {/* Changes summary */}
      <div className="space-y-2 text-xs mb-3">
        {hasStreamChanges && (
          <div className="text-gray-300">
            <span className="text-gray-500">Streams:</span>
            <ul className="ml-3 mt-1 space-y-0.5">
              {settings.streamConfigs!.map((config, i) => (
                <li key={i} className="flex items-center gap-1">
                  <span className={config.enable ? 'text-green-400' : 'text-red-400'}>
                    {config.enable ? '+ Enable' : '- Disable'}
                  </span>
                  <span>
                    {config.stream_type} {config.resolution.width}x{config.resolution.height} @ {config.framerate}fps
                  </span>
                </li>
              ))}
            </ul>
          </div>
        )}

        {hasOptionChanges && (
          <div className="text-gray-300">
            <span className="text-gray-500">Options:</span>
            <ul className="ml-3 mt-1 space-y-0.5">
              {settings.optionChanges!.map((change, i) => (
                <li key={i}>
                  <span className="text-gray-400">{change.sensorId}:</span>{' '}
                  <span className="text-yellow-400">{change.optionId}</span> → {String(change.value)}
                </li>
              ))}
            </ul>
          </div>
        )}

        {settings.explanation && (
          <p className="text-gray-400 italic mt-2">{settings.explanation}</p>
        )}
      </div>

      {/* Action buttons */}
      <div className="flex items-center gap-2">
        <button
          onClick={applyProposedSettings}
          className="flex-1 flex items-center justify-center gap-1 px-3 py-1.5 bg-rs-blue text-white text-sm rounded hover:bg-blue-600 transition-colors"
        >
          <Check className="w-4 h-4" />
          Apply
        </button>
        <button
          onClick={dismissProposedSettings}
          className="flex items-center justify-center px-3 py-1.5 bg-gray-700 text-gray-300 text-sm rounded hover:bg-gray-600 transition-colors"
        >
          <X className="w-4 h-4" />
        </button>
      </div>
    </div>
  )
}
