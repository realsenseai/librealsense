// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import { useEffect } from 'react'
import { X } from 'lucide-react'
import { useAppStore } from '../../store'

/**
 * Floating pill launcher for the RealSense AI Assistant. While the panel is open it collapses
 * into a compact circular close (X) button instead of disappearing, so there's always a visible
 * way to close the panel without reaching for its in-panel close button.
 * Always clickable — unlike the old chatbot, there's no API key to misconfigure,
 * so the status dot is cosmetic only and never gates opening the panel.
 */
export function AssistantButton() {
  const {
    isAssistantOpen,
    isAssistantOnline,
    toggleAssistant,
    pingAssistantHealth,
    checkChatAvailability,
  } = useAppStore()

  useEffect(() => {
    pingAssistantHealth()
    // Populates isChatAvailable so the panel can show a toggle into the device-config chatbot
    // mode only when it's actually configured (a provider API key present).
    checkChatAvailability()
  }, [pingAssistantHealth, checkChatAvailability])

  return (
    <button
      id="rsai-launcher-button"
      onClick={toggleAssistant}
      title={isAssistantOpen ? 'Close AI Assistant' : 'Ask RealSenseAI'}
      className={`
        fixed bottom-6 right-6 z-50
        flex items-center justify-center rounded-full
        backdrop-blur-md border
        shadow-lg shadow-black/40
        transition-all duration-200 motion-reduce:transition-none
        hover:-translate-y-0.5
        bg-rs-dark/90 border-gray-700 hover:border-rs-blue/60
        ${isAssistantOpen
          ? // On mobile the panel is full-screen (inset-0), so this fixed button would sit on top
            // of the panel's own content — stay hidden there, only collapse to a close (X) button
            // from sm: up, where the panel floats above bottom-6 and never overlaps it.
            'opacity-0 scale-90 pointer-events-none sm:opacity-100 sm:scale-100 sm:pointer-events-auto sm:w-12 sm:h-12'
          : 'gap-2 pl-2 pr-4 py-2'}
      `}
    >
      {isAssistantOpen ? (
        <X className="w-5 h-5 text-rs-light" />
      ) : (
        <>
          <span className="relative flex items-center justify-center w-8 h-8 rounded-full bg-white overflow-hidden shrink-0">
            {/* realsense-logo.png is a wide icon+wordmark lockup (2000x327) — cover+object-left
                crops in on just the square shutter mark instead of shrinking the whole wordmark
                into this small circle (which made the icon nearly invisible). */}
            <img src="/realsense-logo.png" alt="" className="w-full h-full object-cover object-left" />
          </span>
          <span className="flex items-center gap-2 text-sm font-medium whitespace-nowrap text-rs-light">
            Ask RealSenseAI
            <span className="relative flex w-2 h-2">
              {isAssistantOnline && (
                <span className="absolute inline-flex h-full w-full rounded-full bg-green-400 opacity-75 animate-ping motion-reduce:animate-none" />
              )}
              <span className={`relative inline-flex rounded-full w-2 h-2 ${isAssistantOnline ? 'bg-green-500' : 'bg-gray-500'}`} />
            </span>
          </span>
        </>
      )}
    </button>
  )
}
