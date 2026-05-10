import { useState, useEffect } from 'react'

// Version of the React Viewer - update this with each release
export const APP_VERSION = '0.5.0'

// What's New content for each version
const WHATS_NEW: Record<string, { title: string; features: string[] }> = {
  '0.5.0': {
    title: "Auto Firmware Check + Update",
    features: [
      "Automatic firmware status check on device discovery",
      "Shows update banner when FW is outdated (like old viewer)",
      "One-click install using bundled signed image (no uploads)",
      "Graceful handling when FW bundle is missing — visible debug notice",
      "Safer updates: disabled while streaming; detailed backend errors",
    ],
  },
  '0.4.0': {
    title: "AI Configuration Assistant",
    features: [
      "New AI chatbot for natural language camera configuration",
      "Ask questions like 'Set up for 3D scanning' or 'Optimize for robotics'",
      "AI proposes settings changes with one-click apply",
      "Export configurations as Python or C++ code snippets",
      "Context-aware suggestions based on connected devices",
    ],
  },
  '0.3.0': {
    title: "Improved UI Layout & Device Actions",
    features: [
      "Controls moved under each device in the left sidebar - no more right panel",
      "Per-device Start/Stop streaming buttons near stream configuration",
      "Hamburger menu for device actions (Hardware Reset, Calibration)",
      "Stream types properly filtered based on actual sensor capabilities",
      "Compact controls design for better space utilization",
    ],
  },
  '0.2.0': {
    title: "Multi-Camera Support & Improved UI",
    features: [
      "Multi-camera support - activate and stream from multiple devices simultaneously",
      "Stream tiles show device name when using multiple cameras",
      "Per-device controls with collapsible accordion sections",
      "Sensor-specific stream filtering - only relevant streams shown per device",
      "Toggle switches for quick device activation/deactivation",
      "Improved streaming controls with device count indicators",
    ],
  },
  '0.1.0': {
    title: "Welcome to RealSense React Viewer!",
    features: [
      "Device discovery and selection",
      "Real-time video streaming via WebRTC (Depth, Color, Infrared)",
      "Camera controls with sensor options adjustment",
      "3D Point Cloud visualization with export to PLY",
      "IMU data visualization with real-time graphs",
      "CSV export for IMU data",
    ],
  },
}

const STORAGE_KEY = 'realsense-viewer-last-version'

interface WhatsNewModalProps {
  isOpen: boolean
  onClose: () => void
  version: string
}

function WhatsNewModal({ isOpen, onClose, version }: WhatsNewModalProps) {
  const content = WHATS_NEW[version]
  
  if (!isOpen || !content) return null

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm">
      <div className="bg-rs-dark border border-gray-600 rounded-xl shadow-2xl max-w-lg w-full mx-4 overflow-hidden">
        {/* Header */}
        <div className="bg-gradient-to-r from-rs-blue to-blue-600 px-6 py-4">
          <div className="flex items-center gap-3">
            <img 
              src="/realsense-logo.png" 
              alt="RealSense" 
              className="h-8 w-auto"
            />
            <div>
              <h2 className="text-xl font-bold text-white">What's New</h2>
              <p className="text-blue-100 text-sm">Version {version}</p>
            </div>
          </div>
        </div>

        {/* Content */}
        <div className="p-6">
          <h3 className="text-lg font-semibold text-white mb-4">{content.title}</h3>
          
          <ul className="space-y-3">
            {content.features.map((feature, index) => (
              <li key={index} className="flex items-start gap-3">
                <span className="flex-shrink-0 w-5 h-5 rounded-full bg-rs-blue/20 flex items-center justify-center mt-0.5">
                  <svg className="w-3 h-3 text-rs-blue" fill="currentColor" viewBox="0 0 20 20">
                    <path fillRule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clipRule="evenodd" />
                  </svg>
                </span>
                <span className="text-gray-300">{feature}</span>
              </li>
            ))}
          </ul>
        </div>

        {/* Footer */}
        <div className="px-6 py-4 bg-gray-800/50 flex justify-between items-center">
          <a 
            href="https://github.com/IntelRealSense/librealsense/releases" 
            target="_blank" 
            rel="noopener noreferrer"
            className="text-sm text-rs-blue hover:text-blue-400 transition-colors"
          >
            View Release Notes →
          </a>
          <button
            onClick={onClose}
            className="px-6 py-2 bg-rs-blue text-white rounded-lg hover:bg-blue-600 transition-colors font-medium"
          >
            Get Started
          </button>
        </div>
      </div>
    </div>
  )
}

export function WhatsNew() {
  const [showModal, setShowModal] = useState(false)
  const [displayVersion, setDisplayVersion] = useState(APP_VERSION)

  useEffect(() => {
    // Check if this is a new version
    const lastVersion = localStorage.getItem(STORAGE_KEY)
    
    if (lastVersion !== APP_VERSION) {
      // Show what's new for the current version
      setDisplayVersion(APP_VERSION)
      setShowModal(true)
    }
  }, [])

  const handleClose = () => {
    // Save the current version so we don't show again
    localStorage.setItem(STORAGE_KEY, APP_VERSION)
    setShowModal(false)
  }

  return (
    <WhatsNewModal 
      isOpen={showModal} 
      onClose={handleClose} 
      version={displayVersion} 
    />
  )
}

// Hook to manually trigger What's New (e.g., from a menu)
export function useWhatsNew() {
  const [isOpen, setIsOpen] = useState(false)
  
  const show = () => setIsOpen(true)
  const hide = () => setIsOpen(false)
  
  return { isOpen, show, hide, version: APP_VERSION }
}
