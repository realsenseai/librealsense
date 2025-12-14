import { useAppStore } from '../store'

export function Header() {
  const { viewMode, setViewMode, isStreaming, startStreaming, stopStreaming, selectedDevice } =
    useAppStore()

  return (
    <header className="bg-rs-dark border-b border-gray-700 px-4 py-3">
      <div className="flex items-center justify-between">
        {/* Logo and Title */}
        <div className="flex items-center gap-3">
          <img 
            src="/realsense-logo.png" 
            alt="RealSense" 
            className="h-8 w-auto"
          />
        </div>

        {/* View Mode Toggle */}
        {selectedDevice && (
          <div className="flex items-center gap-4">
            <div className="flex bg-gray-700 rounded-lg p-1">
              <button
                onClick={() => setViewMode('2d')}
                className={`px-4 py-1 rounded-md text-sm transition-colors ${
                  viewMode === '2d'
                    ? 'bg-rs-blue text-white'
                    : 'text-gray-300 hover:text-white'
                }`}
              >
                2D View
              </button>
              <button
                onClick={() => setViewMode('3d')}
                className={`px-4 py-1 rounded-md text-sm transition-colors ${
                  viewMode === '3d'
                    ? 'bg-rs-blue text-white'
                    : 'text-gray-300 hover:text-white'
                }`}
              >
                3D View
              </button>
            </div>

            {/* Streaming Controls */}
            <button
              onClick={isStreaming ? stopStreaming : startStreaming}
              className={`px-6 py-2 rounded-lg font-semibold transition-colors ${
                isStreaming
                  ? 'bg-red-600 hover:bg-red-700 text-white'
                  : 'bg-green-600 hover:bg-green-700 text-white'
              }`}
            >
              {isStreaming ? (
                <>
                  <span className="inline-block w-3 h-3 bg-white rounded-sm mr-2" />
                  Stop
                </>
              ) : (
                <>
                  <span className="inline-block w-0 h-0 border-t-[6px] border-t-transparent border-b-[6px] border-b-transparent border-l-[10px] border-l-white mr-2" />
                  Start
                </>
              )}
            </button>
          </div>
        )}
      </div>
    </header>
  )
}
