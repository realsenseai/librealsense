import { useEffect, useState } from 'react'
import { useAppStore } from '../store'
import { apiClient } from '../api'

interface WhatsNewModalProps {
  isOpen: boolean
  onClose: () => void
}

function AboutModal({ isOpen, onClose }: WhatsNewModalProps) {
  const [sdkVersion, setSdkVersion] = useState<string | null>(null)

  useEffect(() => {
    if (!isOpen) return
    let cancelled = false
    apiClient
      .getHealth()
      .then((h) => {
        if (!cancelled) setSdkVersion(h.sdk_version || 'unknown')
      })
      .catch(() => {
        if (!cancelled) setSdkVersion('unknown')
      })
    return () => {
      cancelled = true
    }
  }, [isOpen])

  if (!isOpen) return null

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm">
      <div className="bg-rs-dark border border-gray-600 rounded-xl shadow-2xl max-w-md w-full mx-4 overflow-hidden">
        {/* Header */}
        <div className="bg-gradient-to-r from-rs-blue to-blue-600 px-6 py-4">
          <div className="flex items-center gap-3">
            <img 
              src="/realsense-logo.png" 
              alt="RealSense" 
              className="h-8 w-auto"
            />
            <div>
              <h2 className="text-xl font-bold text-white">About</h2>
              <p className="text-blue-100 text-sm">RealSense React Viewer</p>
            </div>
          </div>
        </div>

        {/* Content */}
        <div className="p-6 space-y-4">
          <div className="flex justify-between text-sm">
            <span className="text-gray-400">librealsense SDK</span>
            <span className="text-white font-mono">{sdkVersion ?? '…'}</span>
          </div>
          <div className="flex justify-between text-sm">
            <span className="text-gray-400">License</span>
            <span className="text-white">Apache 2.0</span>
          </div>
          <div className="pt-2 border-t border-gray-700">
            <p className="text-gray-400 text-sm">
              A modern React-based web UI for RealSense Cameras, 
              leveraging the REST API backend for device control and streaming.
            </p>
          </div>
        </div>

        {/* Footer */}
        <div className="px-6 py-4 bg-gray-800/50 flex justify-between items-center">
          <a 
            href="https://github.com/realsenseai/librealsense" 
            target="_blank" 
            rel="noopener noreferrer"
            className="text-sm text-rs-blue hover:text-blue-400 transition-colors"
          >
            GitHub Repository →
          </a>
          <button
            onClick={onClose}
            className="px-6 py-2 bg-rs-blue text-white rounded-lg hover:bg-blue-600 transition-colors font-medium"
          >
            Close
          </button>
        </div>
      </div>
    </div>
  )
}

export function Header() {
  const { 
    viewMode, 
    setViewMode, 
    getActiveDevices,
  } = useAppStore()
  const [showAbout, setShowAbout] = useState(false)

  const activeDevices = getActiveDevices()
  const hasActiveDevices = activeDevices.length > 0

  return (
    <>
      <AboutModal isOpen={showAbout} onClose={() => setShowAbout(false)} />
      
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

          {/* Center Controls - View Mode Toggle */}
          {hasActiveDevices && (
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
                  disabled
                  title="3D View feature coming soon"
                  className={`px-4 py-1 rounded-md text-sm transition-colors cursor-not-allowed ${
                    viewMode === '3d'
                      ? 'bg-rs-blue text-white'
                      : 'text-gray-500 opacity-50'
                  }`}
                >
                  3D View
                </button>
              </div>
            </div>
          )}

          {/* Right side - Info button */}
          <button
            onClick={() => setShowAbout(true)}
            className="p-2 text-gray-400 hover:text-white hover:bg-gray-700 rounded-lg transition-colors"
            title="About"
          >
            <svg className="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M13 16h-1v-4h-1m1-4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
            </svg>
          </button>
        </div>
      </header>
    </>
  )
}
