import { useEffect, useState } from 'react'
import { apiClient } from '../api'

/**
 * Welcome banner shown once per librealsense SDK version, mirroring the
 * `version_upgrade_model` flow in the C++ realsense-viewer
 * (see common/viewer.cpp and common/notifications.cpp).
 *
 * The first time the viewer opens against a new SDK version, a generic
 * "Welcome to librealsense X.Y.Z" modal pops up with a link to the
 * matching Release Notes wiki entry. The version we have shown is then
 * persisted to localStorage so the modal does not re-appear on subsequent
 * launches of the same SDK version.
 *
 * No per-release feature list is maintained here; the wiki link is the
 * single source of truth (same as the C++ viewer).
 */

const STORAGE_KEY = 'rs-sdk-last-shown'

function releaseNotesUrl(version: string): string {
  // The C++ viewer uses the SDK API integer (e.g. 21701) as the anchor.
  // We use the dotted version stripped of dots (2.57.0 -> 2570), which is
  // how the Release-Notes wiki page tags each release.
  const anchor = version.replace(/\./g, '')
  return `https://github.com/realsenseai/librealsense/wiki/Release-Notes#release-${anchor}`
}

interface WelcomeModalProps {
  isOpen: boolean
  sdkVersion: string
  onClose: () => void
}

function WelcomeModal({ isOpen, sdkVersion, onClose }: WelcomeModalProps) {
  if (!isOpen) return null

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm">
      <div className="bg-rs-dark border border-gray-600 rounded-xl shadow-2xl max-w-md w-full mx-4 overflow-hidden">
        {/* Header */}
        <div className="bg-gradient-to-r from-rs-blue to-blue-600 px-6 py-5">
          <div className="flex items-center gap-3">
            <img src="/realsense-logo.png" alt="RealSense" className="h-8 w-auto" />
            <div>
              <h2 className="text-xl font-bold text-white">Welcome to librealsense</h2>
              <p className="text-blue-100 text-sm font-mono">{sdkVersion}</p>
            </div>
          </div>
        </div>

        {/* Content */}
        <div className="p-6 space-y-3 text-sm text-gray-300">
          <p>
            You are running the RealSense Viewer against librealsense SDK{' '}
            <span className="font-mono text-white">{sdkVersion}</span>.
          </p>
          <p>
            For the list of changes in this release, see the Release Notes on the
            librealsense wiki.
          </p>
        </div>

        {/* Footer */}
        <div className="px-6 py-4 bg-gray-800/50 flex justify-between items-center">
          <a
            href={releaseNotesUrl(sdkVersion)}
            target="_blank"
            rel="noopener noreferrer"
            className="text-sm text-rs-blue hover:text-blue-400 transition-colors"
          >
            What&apos;s new in this release? →
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
  const [sdkVersion, setSdkVersion] = useState<string | null>(null)
  const [showModal, setShowModal] = useState(false)

  useEffect(() => {
    let cancelled = false
    apiClient
      .getHealth()
      .then((h) => {
        if (cancelled) return
        const v = h.sdk_version
        if (!v || v === 'unknown') return
        setSdkVersion(v)
        const lastShown = localStorage.getItem(STORAGE_KEY)
        if (lastShown !== v) {
          setShowModal(true)
        }
      })
      .catch(() => {
        // Backend not reachable yet (or older version without sdk_version);
        // skip the welcome silently — the user will see it next launch.
      })
    return () => {
      cancelled = true
    }
  }, [])

  const handleClose = () => {
    if (sdkVersion) {
      localStorage.setItem(STORAGE_KEY, sdkVersion)
    }
    setShowModal(false)
  }

  if (!sdkVersion) return null

  return <WelcomeModal isOpen={showModal} sdkVersion={sdkVersion} onClose={handleClose} />
}
