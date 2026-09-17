// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import { useState, useRef, useEffect } from 'react'
import { PlusCircle, Sparkles, Wrench, Maximize2, Minimize2, X } from 'lucide-react'
import { useAppStore } from '../../store'
import { getActiveProviderName } from '../../api/chat'
import { AssistantContent } from './AssistantContent'
import { ChatBotContent } from './ChatBotContent'

type PanelMode = 'assistant' | 'chatbot'

/**
 * Slide-out panel for the RealSense AI Assistant. Always mounted (not conditionally
 * rendered) so open/close can animate via opacity/transform rather than mount/unmount.
 * Only the header/frame here is shared between modes — AssistantContent/ChatBotContent
 * each own their message list and input.
 */
export function AssistantPanel() {
  const {
    isAssistantOpen,
    isAssistantOnline,
    assistantSize,
    clearAssistantChat,
    toggleAssistant,
    toggleAssistantSize,
    isChatAvailable,
    clearChat,
  } = useAppStore()

  const isWide = assistantSize === 'wide'

  const [mode, setMode] = useState<PanelMode>('assistant')
  const isChatbotMode = mode === 'chatbot'
  const providerName = getActiveProviderName()

  const wasOpenRef = useRef(isAssistantOpen)
  const panelRef = useRef<HTMLDivElement>(null)

  // Escape closes the panel; when it closes, return focus to the launcher pill.
  useEffect(() => {
    if (!isAssistantOpen) return
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Escape') toggleAssistant()
    }
    document.addEventListener('keydown', handleKeyDown)
    return () => document.removeEventListener('keydown', handleKeyDown)
  }, [isAssistantOpen, toggleAssistant])

  // aria-hidden alone doesn't stop Tab from focusing elements inside a hidden, only
  // pointer-events-none'd panel — `inert` removes it from the tab order too.
  useEffect(() => {
    if (panelRef.current) panelRef.current.inert = !isAssistantOpen
  }, [isAssistantOpen])

  useEffect(() => {
    if (wasOpenRef.current && !isAssistantOpen) {
      document.getElementById('rsai-launcher-button')?.focus()
    }
    wasOpenRef.current = isAssistantOpen
  }, [isAssistantOpen])

  const panelBg = 'bg-rs-dark border-gray-700'
  const headerBg = 'bg-rs-darker border-gray-700'
  const titleText = 'text-white'
  const mutedText = 'text-gray-400'
  const iconBtn = 'text-gray-400 hover:text-white hover:bg-gray-700'
  const panelSize = isWide
    ? 'sm:w-[640px] sm:h-[700px] sm:max-h-[85vh]'
    : 'sm:w-96 sm:h-[600px] sm:max-h-[75vh]'

  return (
    <div
      ref={panelRef}
      className={`
        fixed inset-0 z-40 w-full h-full rounded-none border-0
        sm:inset-auto sm:right-6 sm:bottom-24 sm:rounded-2xl sm:border ${panelSize}
        ${panelBg} shadow-2xl
        flex flex-col overflow-hidden
        origin-bottom-right transition-all duration-200 motion-reduce:transition-none
        ${isAssistantOpen
          ? 'opacity-100 translate-y-0 scale-100 pointer-events-auto'
          : 'opacity-0 translate-y-2 scale-95 pointer-events-none'}
      `}
      role="dialog"
      aria-label="RealSense AI Assistant"
      aria-hidden={!isAssistantOpen}
    >
      {/* Header */}
      <div className={`border-b ${headerBg}`}>
        <div className="flex items-center justify-between gap-2 px-4 pt-3 pb-1.5">
          <div className="flex items-center gap-3 min-w-0">
            <span
              className={`flex items-center justify-center w-9 h-9 rounded-full overflow-hidden shrink-0 ring-2 transition-colors ${
                isChatbotMode ? 'bg-amber-500 ring-amber-500/40' : 'bg-white ring-rs-blue/40'
              }`}
            >
              {isChatbotMode ? (
                <Wrench className="w-4 h-4 text-white" />
              ) : (
                // See AssistantButton.tsx for why this crops with object-cover instead of object-contain.
                <img src="/realsense-logo.png" alt="" className="w-full h-full object-cover object-left" />
              )}
            </span>
            <div className="min-w-0">
              <h3 className={`font-semibold text-sm truncate ${titleText}`}>
                {isChatbotMode ? 'Device Config Chatbot' : 'RealSense AI Assistant'}
              </h3>
              <div className={`flex items-center gap-1.5 text-[11px] whitespace-nowrap ${mutedText}`}>
                {isChatbotMode ? (
                  <>
                    <span className="relative inline-flex rounded-full w-1.5 h-1.5 bg-amber-500 shrink-0" />
                    <span className="truncate">{providerName ? `Using ${providerName}` : 'Local camera settings'}</span>
                  </>
                ) : (
                  <>
                    <span className="relative flex w-1.5 h-1.5 shrink-0">
                      {isAssistantOnline && (
                        <span className="absolute inline-flex h-full w-full rounded-full bg-green-400 opacity-75 animate-ping motion-reduce:animate-none" />
                      )}
                      <span className={`relative inline-flex rounded-full w-1.5 h-1.5 ${isAssistantOnline ? 'bg-green-500' : 'bg-gray-500'}`} />
                    </span>
                    <span className="truncate">{isAssistantOnline ? 'Online' : 'Reconnecting…'} · powered by RealSense AI</span>
                  </>
                )}
              </div>
            </div>
          </div>
          <button
            onClick={toggleAssistant}
            className={`p-1.5 rounded transition-colors shrink-0 ${iconBtn}`}
            title="Close"
          >
            <X className="w-4 h-4" />
          </button>
        </div>

        <div className="flex items-center justify-between gap-2 px-4 pb-2">
          {isChatAvailable ? (
            <div
              role="group"
              aria-label="Choose assistant mode"
              className="flex items-center rounded-full p-0.5 gap-0.5 bg-gray-700"
            >
              <button
                onClick={() => setMode('assistant')}
                title="Switch to the RealSense AI Assistant (product Q&A)"
                aria-pressed={!isChatbotMode}
                className={`flex items-center gap-1 px-2 py-1 rounded-full text-[11px] font-medium transition-colors ${
                  !isChatbotMode ? 'bg-rs-blue text-white' : 'text-gray-400 hover:text-white hover:bg-gray-600'
                }`}
              >
                <Sparkles className="w-3 h-3 shrink-0" />
                AI Assistant
              </button>
              <button
                onClick={() => setMode('chatbot')}
                title="Switch to the device-config Chatbot (camera settings)"
                aria-pressed={isChatbotMode}
                className={`flex items-center gap-1 px-2 py-1 rounded-full text-[11px] font-medium transition-colors ${
                  isChatbotMode ? 'bg-amber-600 text-white' : 'text-gray-400 hover:text-white hover:bg-gray-600'
                }`}
              >
                <Wrench className="w-3 h-3 shrink-0" />
                Chatbot
              </button>
            </div>
          ) : (
            <span />
          )}
          <div className="flex items-center gap-1 shrink-0">
            <button
              onClick={toggleAssistantSize}
              className={`p-1.5 rounded transition-colors hidden sm:inline-flex ${iconBtn}`}
              title={isWide ? 'Collapse panel' : 'Expand panel'}
            >
              {isWide ? <Minimize2 className="w-4 h-4" /> : <Maximize2 className="w-4 h-4" />}
            </button>
            <button
              onClick={isChatbotMode ? clearChat : clearAssistantChat}
              className={`p-1.5 rounded transition-colors ${iconBtn}`}
              title="New chat"
            >
              <PlusCircle className="w-4 h-4" />
            </button>
          </div>
        </div>
      </div>

      {isChatbotMode ? <ChatBotContent /> : <AssistantContent />}
    </div>
  )
}
