import { useState } from 'react'
import { Copy, Check } from 'lucide-react'
import { useAppStore } from '../../store'
import { generateCodeSnippet, type ProposedSettings } from '../../utils/chatPrompt'

interface CodeExportProps {
  settings: ProposedSettings
}

/**
 * Code export panel showing Python/C++ snippets for the proposed settings
 */
export function CodeExport({ settings }: CodeExportProps) {
  const [language, setLanguage] = useState<'python' | 'cpp'>('python')
  const [copied, setCopied] = useState(false)
  const { deviceStates } = useAppStore()

  // Find the device
  const deviceState = Object.values(deviceStates).find(
    ds => ds.device.serial_number === settings.deviceSerial
  )

  if (!deviceState) {
    return (
      <div className="mt-2 p-2 bg-gray-900 rounded text-xs text-gray-400">
        Device not found for code generation.
      </div>
    )
  }

  // Merge proposed configs with existing if needed
  const streamConfigs = settings.streamConfigs || deviceState.streamConfigs

  const code = generateCodeSnippet(
    language,
    {
      name: deviceState.device.name,
      serial_number: deviceState.device.serial_number,
    },
    streamConfigs,
    settings.optionChanges
  )

  const handleCopy = async () => {
    try {
      await navigator.clipboard.writeText(code)
      setCopied(true)
      setTimeout(() => setCopied(false), 2000)
    } catch (error) {
      console.error('Failed to copy code:', error)
    }
  }

  return (
    <div className="mt-2 border border-gray-700 rounded-lg overflow-hidden">
      {/* Language tabs */}
      <div className="flex items-center justify-between bg-gray-800 px-2 py-1 border-b border-gray-700">
        <div className="flex">
          <button
            onClick={() => setLanguage('python')}
            className={`px-3 py-1 text-xs rounded-t transition-colors ${
              language === 'python'
                ? 'bg-gray-900 text-white'
                : 'text-gray-400 hover:text-white'
            }`}
          >
            Python
          </button>
          <button
            onClick={() => setLanguage('cpp')}
            className={`px-3 py-1 text-xs rounded-t transition-colors ${
              language === 'cpp'
                ? 'bg-gray-900 text-white'
                : 'text-gray-400 hover:text-white'
            }`}
          >
            C++
          </button>
        </div>
        
        <button
          onClick={handleCopy}
          className="flex items-center gap-1 px-2 py-0.5 text-xs text-gray-400 hover:text-white transition-colors"
        >
          {copied ? (
            <>
              <Check className="w-3 h-3 text-green-400" />
              <span className="text-green-400">Copied!</span>
            </>
          ) : (
            <>
              <Copy className="w-3 h-3" />
              <span>Copy</span>
            </>
          )}
        </button>
      </div>

      {/* Code content */}
      <pre className="p-3 bg-gray-900 text-xs overflow-x-auto max-h-64">
        <code className={language === 'python' ? 'language-python' : 'language-cpp'}>
          {code}
        </code>
      </pre>
    </div>
  )
}
