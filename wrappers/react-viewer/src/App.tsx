import { useEffect } from 'react'
import { DevicePanel } from './components/DevicePanel'
import { StreamViewer } from './components/StreamViewer'
import { ControlsPanel } from './components/ControlsPanel'
import { PointCloudViewer } from './components/PointCloudViewer'
import { IMUViewer } from './components/IMUViewer'
import { Header } from './components/Header'
import { useAppStore } from './store'
import { socketService } from './api/socket'

function App() {
  const { viewMode, selectedDevice, isConnected } = useAppStore()

  useEffect(() => {
    // Connect to Socket.IO on mount
    socketService.connect()
    
    // Don't disconnect on cleanup in dev mode (React strict mode double-mounts)
    // The socket service handles reconnection gracefully
    return () => {
      // Only disconnect if we're actually unmounting the app
      // In development with strict mode, this fires twice
    }
  }, [])

  return (
    <div className="min-h-screen bg-rs-darker flex flex-col">
      <Header />
      
      <div className="flex-1 flex">
        {/* Left Sidebar - Device Panel */}
        <aside className="w-72 bg-rs-dark border-r border-gray-700 overflow-y-auto">
          <DevicePanel />
        </aside>

        {/* Main Content Area */}
        <main className="flex-1 flex flex-col overflow-hidden">
          {selectedDevice ? (
            <>
              {/* Stream/PointCloud View */}
              <div className="flex-1 p-4 overflow-hidden">
                {viewMode === '2d' ? (
                  <StreamViewer />
                ) : (
                  <PointCloudViewer />
                )}
              </div>

              {/* IMU Viewer (collapsible) */}
              <IMUViewer />
            </>
          ) : (
            <div className="flex-1 flex items-center justify-center text-gray-500">
              <div className="text-center">
                <svg className="w-24 h-24 mx-auto mb-4 opacity-50" viewBox="0 0 100 100">
                  <circle cx="50" cy="50" r="45" fill="none" stroke="currentColor" strokeWidth="2"/>
                  <circle cx="35" cy="40" r="8" fill="currentColor" opacity="0.5"/>
                  <circle cx="65" cy="40" r="8" fill="currentColor" opacity="0.5"/>
                  <circle cx="50" cy="60" r="6" fill="currentColor" opacity="0.3"/>
                </svg>
                <p className="text-xl">No Device Selected</p>
                <p className="text-sm mt-2">Connect a RealSense device or select one from the sidebar</p>
              </div>
            </div>
          )}
        </main>

        {/* Right Sidebar - Controls Panel */}
        {selectedDevice && (
          <aside className="w-80 bg-rs-dark border-l border-gray-700 overflow-y-auto">
            <ControlsPanel />
          </aside>
        )}
      </div>

      {/* Connection Status */}
      <div className={`fixed bottom-4 right-4 px-3 py-1 rounded-full text-sm ${
        isConnected ? 'bg-green-600' : 'bg-red-600'
      }`}>
        {isConnected ? '● Connected' : '○ Disconnected'}
      </div>
    </div>
  )
}

export default App
