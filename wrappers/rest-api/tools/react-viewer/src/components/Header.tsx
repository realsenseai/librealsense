import { useEffect, useState } from 'react'
import { useAppStore } from '../store'
import { apiClient } from '../api'
import { SettingsDialog } from './settings/SettingsDialog'
import { reportIssueUrl } from '../store/notifications'

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
            <a href="https://github.com/realsenseai/librealsense/blob/master/LICENSE" target="_blank" rel="noopener noreferrer" className="text-rs-blue hover:underline">Apache 2.0</a>
          </div>
          <p className="text-xs text-gray-500">
            Licensed under the Apache License, Version 2.0. You may not use this software except in
            compliance with the License. Software distributed under the License is distributed on an
            &quot;AS IS&quot; BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND.
          </p>
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
    getDeviceStates,
  } = useAppStore()
  const [showAbout, setShowAbout] = useState(false)
  const [showSettings, setShowSettings] = useState(false)
  const [showHelp, setShowHelp] = useState(false)
  const [sdkVersion, setSdkVersion] = useState<string | undefined>()
  const deviceStates = useAppStore((s) => s.deviceStates)
  useEffect(() => { apiClient.getHealth().then((h) => setSdkVersion(h.sdk_version)).catch(() => undefined) }, [])

  const hasActiveDevices = getDeviceStates().length > 0

  return (
    <>
      <AboutModal isOpen={showAbout} onClose={() => setShowAbout(false)} />
      <SettingsDialog isOpen={showSettings} onClose={() => setShowSettings(false)} />
      
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
            </div>
          )}

          {/* Right side - Help, Settings and About */}
          <div className="flex items-center gap-1 relative">
          <button
            onClick={() => setShowHelp((v) => !v)}
            className="p-2 text-gray-400 hover:text-white hover:bg-gray-700 rounded-lg transition-colors"
            title="Help"
            aria-label="Help"
          >
            <svg className="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M8.228 9c.549-1.165 2.03-2 3.772-2 2.21 0 4 1.343 4 3 0 1.4-1.278 2.575-3.006 2.907-.542.104-.994.54-.994 1.093m0 3h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
            </svg>
          </button>
          {showHelp && (
            <>
              <div className="fixed inset-0 z-10" onClick={() => setShowHelp(false)} />
              <div className="absolute right-0 top-10 w-52 bg-gray-800 border border-gray-600 rounded-lg shadow-xl z-20 py-1 text-sm" role="menu">
                <a href={reportIssueUrl(Object.values(deviceStates).map((ds) => ds.device), sdkVersion)} target="_blank" rel="noopener noreferrer"
                  className="block px-4 py-2 hover:bg-gray-700" onClick={() => setShowHelp(false)}>Report Issue</a>
                <a href="https://store.realsenseai.com/" target="_blank" rel="noopener noreferrer"
                  className="block px-4 py-2 hover:bg-gray-700" onClick={() => setShowHelp(false)}>RealSense Store</a>
                <a href="https://github.com/realsenseai/librealsense/wiki/Release-Notes" target="_blank" rel="noopener noreferrer"
                  className="block px-4 py-2 hover:bg-gray-700" onClick={() => setShowHelp(false)}>Release Notes</a>
              </div>
            </>
          )}
          <button
            onClick={() => setShowSettings(true)}
            className="p-2 text-gray-400 hover:text-white hover:bg-gray-700 rounded-lg transition-colors"
            title="Settings"
            aria-label="Settings"
          >
            <svg className="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M10.325 4.317c.426-1.756 2.924-1.756 3.35 0a1.724 1.724 0 002.573 1.066c1.543-.94 3.31.826 2.37 2.37a1.724 1.724 0 001.065 2.572c1.756.426 1.756 2.924 0 3.35a1.724 1.724 0 00-1.066 2.573c.94 1.543-.826 3.31-2.37 2.37a1.724 1.724 0 00-2.572 1.065c-.426 1.756-2.924 1.756-3.35 0a1.724 1.724 0 00-2.573-1.066c-1.543.94-3.31-.826-2.37-2.37a1.724 1.724 0 00-1.065-2.572c-1.756-.426-1.756-2.924 0-3.35a1.724 1.724 0 001.066-2.573c-.94-1.543.826-3.31 2.37-2.37.996.608 2.296.07 2.572-1.065z" />
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M15 12a3 3 0 11-6 0 3 3 0 016 0z" />
            </svg>
          </button>
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
        </div>
      </header>
    </>
  )
}
