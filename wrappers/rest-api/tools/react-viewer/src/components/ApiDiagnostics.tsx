import { useEffect, useState } from 'react'
import { invoke } from '@tauri-apps/api/tauri'

interface BackendStatus {
  is_running: boolean
  port: number
  log_count: number
  last_logs: string[]
}

// Detect if running in Tauri desktop app
const isDesktopApp = typeof window !== 'undefined' && (window as any).__TAURI__ !== undefined;

export function ApiDiagnostics() {
  const [status, setStatus] = useState<'checking' | 'connected' | 'error'>('checking')
  const [message, setMessage] = useState('')
  const [showDetails, setShowDetails] = useState(false)
  const [backendStatus, setBackendStatus] = useState<BackendStatus | null>(null)
  const [backendLogs, setBackendLogs] = useState<string[]>([])
  const [retrying, setRetrying] = useState(false)

  const testConnection = async () => {
    if (!isDesktopApp) {
      setStatus('error');
      setMessage('Backend diagnostics are only available in the desktop app.');
      return;
    }
    try {
      const result = await invoke<string>('test_api_connection')
      setStatus('connected')
      setMessage(result)
    } catch (error: any) {
      setStatus('error')
      setMessage(error || 'Unknown error')
      
      // Fetch backend status and logs for debugging
      try {
        const bStatus = await invoke<BackendStatus>('get_backend_status')
        setBackendStatus(bStatus)
        
        const logs = await invoke<string[]>('get_backend_logs')
        setBackendLogs(logs)
      } catch (e) {
        console.error('Failed to fetch backend diagnostics:', e)
      }
    }
  }

  const handleRetry = async () => {
    setRetrying(true)
    await testConnection()
    setRetrying(false)
  }

  useEffect(() => {
    testConnection()
    // Re-test every 10 seconds if error
    const interval = setInterval(() => {
      if (status === 'error') {
        testConnection()
      }
    }, 10000)

    return () => clearInterval(interval)
  }, [status])

  if (!isDesktopApp) {
    return null;
  }

  if (status === 'connected') {
    return null // Hide when connected
  }

  return (
    <div className="fixed bottom-4 right-4 max-w-md z-50">
      <div
        className={`rounded-lg shadow-lg p-4 ${
          status === 'checking'
            ? 'bg-yellow-50 border border-yellow-200'
            : 'bg-red-50 border border-red-200'
        }`}
      >
        <div className="flex items-start justify-between">
          <div className="flex-1">
            <p className={`font-semibold ${status === 'checking' ? 'text-yellow-900' : 'text-red-900'}`}>
              {status === 'checking' ? '⏳ Connecting to Backend...' : '⚠️ Backend Connection Error'}
            </p>
            {message && (
              <p className={`text-xs mt-2 font-mono ${status === 'checking' ? 'text-yellow-800' : 'text-red-800'}`}>
                {message}
              </p>
            )}
          </div>
          <button
            onClick={() => setShowDetails(!showDetails)}
            className={`text-sm font-medium ml-2 ${status === 'checking' ? 'text-yellow-600' : 'text-red-600'}`}
          >
            {showDetails ? '▼' : '▶'}
          </button>
        </div>

        {showDetails && status === 'error' && (
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

            {backendStatus && (
              <div className="mt-3 p-2 bg-red-100 rounded text-xs text-red-900">
                <p><strong>Backend Status:</strong></p>
                <p>• Process running: {backendStatus.is_running ? '✅ Yes' : '❌ No'}</p>
                <p>• Port: {backendStatus.port}</p>
                <p>• Log entries: {backendStatus.log_count}</p>
              </div>
            )}

            {backendLogs.length > 0 && (
              <div className="mt-3">
                <p className="text-sm font-semibold text-red-900 mb-1">Backend Logs (last 10):</p>
                <div className="bg-red-100 rounded p-2 max-h-48 overflow-y-auto">
                  <pre className="text-xs text-red-900 font-mono whitespace-pre-wrap">
                    {backendLogs.slice(-10).join('\n')}
                  </pre>
                </div>
              </div>
            )}

            <button
              onClick={handleRetry}
              disabled={retrying}
              className="mt-3 w-full bg-red-600 text-white py-2 px-4 rounded hover:bg-red-700 disabled:opacity-50 text-sm font-medium"
            >
              {retrying ? '🔄 Retrying...' : '🔄 Retry Connection'}
            </button>
          </>
        )}
      </div>
    </div>
  )
}
