import { useEffect, useState } from 'react'
import { invoke } from '@tauri-apps/api/tauri'
import { isDesktopApp } from '../api/backend'
import { useAppStore } from '../store'

interface BackendStatus {
  is_running: boolean
  port: number
  log_count: number
  last_logs: string[]
}

export function ApiDiagnostics() {
  const [showDetails, setShowDetails] = useState(false)
  const [backendStatus, setBackendStatus] = useState<BackendStatus | null>(null)
  const isConnected = useAppStore((s) => s.connection === 'connected')

  const testConnection = async () => {
    try {
      setBackendStatus(await invoke<BackendStatus>('get_backend_status'))
    } catch (e) {
      console.error('Failed to fetch backend diagnostics:', e)
    }
  }

  useEffect(() => {
    if (!isDesktopApp || isConnected) return
    const interval = setInterval(testConnection, 2000) // no immediate check: backend may not be spawned yet

    return () => clearInterval(interval)
  }, [isConnected])

  // A live backend process is still starting; only a dead one is an error.
  if (isConnected || !backendStatus || backendStatus.is_running) {
    return null
  }

  return (
    <div className="fixed bottom-4 right-4 max-w-md z-50">
      <div className="rounded-lg shadow-lg p-4 bg-red-50 border border-red-200">
        <div className="flex items-start justify-between">
          <div className="flex-1">
            <p className="font-semibold text-red-900">⚠️ Backend Connection Error</p>
          </div>
          <button
            onClick={() => setShowDetails(!showDetails)}
            className="text-sm font-medium ml-2 text-red-600"
          >
            {showDetails ? '▼' : '▶'}
          </button>
        </div>

        {showDetails && (
          <>
            <div className="mt-3 text-sm text-red-800 space-y-1">
              <p>
                <strong>Troubleshooting:</strong>
              </p>
              <ul className="list-disc list-inside space-y-1">
                <li>Ensure FastAPI backend is running</li>
                <li>Check if RealSense SDK is installed</li>
                <li>Verify USB devices are connected</li>
                <li>Port 8000 is not used by another app</li>
              </ul>
            </div>

            <div className="mt-3 p-2 bg-red-100 rounded text-xs text-red-900">
              <p><strong>Backend Status:</strong></p>
              <p>• Port: {backendStatus.port}</p>
              <p>• Log entries: {backendStatus.log_count}</p>
            </div>

            {backendStatus.last_logs.length > 0 && (
              <div className="mt-3">
                <p className="text-sm font-semibold text-red-900 mb-1">Backend Logs (last 10):</p>
                <div className="bg-red-100 rounded p-2 max-h-48 overflow-y-auto">
                  <pre className="text-xs text-red-900 font-mono whitespace-pre-wrap">
                    {backendStatus.last_logs.join('\n')}
                  </pre>
                </div>
              </div>
            )}
          </>
        )}
      </div>
    </div>
  )
}
