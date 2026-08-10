// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import { useState, useRef, useEffect } from 'react'
import { Send, PlusCircle, Loader2, Sparkles, Paperclip, Square, Mic, MicOff, X, FileText, Wrench } from 'lucide-react'
import { useAppStore } from '../../store'
import { getActiveProviderName } from '../../api/chat'
import { AssistantMessageBubble } from './AssistantMessage'
import { ExpandIcon, CollapseIcon, SunIcon, MoonIcon, CloseIcon } from './icons'
import { useVoiceInput } from './useVoiceInput'
import { usePendingAttachments } from './usePendingAttachments'
import { ChatBotContent } from './ChatBotContent'

type PanelMode = 'assistant' | 'chatbot'

/**
 * Slide-out panel for the RealSense AI Assistant. Always mounted (not conditionally
 * rendered) so open/close can animate via opacity/transform rather than mount/unmount.
 */
export function AssistantPanel() {
  const {
    isAssistantOpen,
    isAssistantOnline,
    isAssistantLoading,
    assistantMessages,
    assistantTheme,
    assistantSize,
    sendAssistantMessage,
    stopAssistantMessage,
    clearAssistantChat,
    toggleAssistant,
    setError,
    toggleAssistantTheme,
    toggleAssistantSize,
    isChatAvailable,
    clearChat,
  } = useAppStore()

  const isLight = assistantTheme === 'light'
  const isWide = assistantSize === 'wide'

  const [mode, setMode] = useState<PanelMode>('assistant')
  const isChatbotMode = mode === 'chatbot'
  const providerName = getActiveProviderName()

  const [inputValue, setInputValue] = useState('')
  const messagesEndRef = useRef<HTMLDivElement>(null)
  const inputRef = useRef<HTMLInputElement>(null)
  const fileInputRef = useRef<HTMLInputElement>(null)
  const wasOpenRef = useRef(isAssistantOpen)
  const panelRef = useRef<HTMLDivElement>(null)

  const { pendingImages, pendingFiles, handleFileChange, removeImage, removeFile, clear: clearAttachments } =
    usePendingAttachments(setError)
  const { isListening, micSupported, toggleListening } = useVoiceInput((transcript) =>
    setInputValue((prev) => (prev ? `${prev} ${transcript}` : transcript))
  )

  useEffect(() => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' })
  }, [assistantMessages])

  useEffect(() => {
    if (isAssistantOpen) inputRef.current?.focus()
  }, [isAssistantOpen])

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

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault()
    const message = inputValue.trim()
    if ((!message && pendingImages.length === 0 && pendingFiles.length === 0) || isAssistantLoading) return
    setInputValue('')
    const attachments = (pendingImages.length || pendingFiles.length)
      ? { imageDataUris: pendingImages, fileDataUris: pendingFiles }
      : undefined
    clearAttachments()
    sendAssistantMessage(message, attachments)
  }

  const panelBg = isLight ? 'bg-white border-gray-200' : 'bg-rs-dark border-gray-700'
  const headerBg = isLight ? 'bg-gray-50 border-gray-200' : 'bg-rs-darker border-gray-700'
  const titleText = isLight ? 'text-gray-900' : 'text-white'
  const mutedText = isLight ? 'text-gray-500' : 'text-gray-400'
  const iconBtn = isLight
    ? 'text-gray-500 hover:text-gray-900 hover:bg-gray-200'
    : 'text-gray-400 hover:text-white hover:bg-gray-700'
  const inputClasses = isLight
    ? 'bg-gray-100 border-gray-300 text-gray-900 placeholder-gray-400'
    : 'bg-gray-800 border-gray-600 text-white placeholder-gray-500'
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
            <CloseIcon />
          </button>
        </div>

        <div className="flex items-center justify-between gap-2 px-4 pb-2">
          {isChatAvailable ? (
            <div
              role="group"
              aria-label="Choose assistant mode"
              className={`flex items-center rounded-full p-0.5 gap-0.5 ${isLight ? 'bg-gray-200' : 'bg-gray-700'}`}
            >
              <button
                onClick={() => setMode('assistant')}
                title="Switch to the RealSense AI Assistant (product Q&A)"
                aria-pressed={!isChatbotMode}
                className={`flex items-center gap-1 px-2 py-1 rounded-full text-[11px] font-medium transition-colors ${
                  !isChatbotMode
                    ? 'bg-rs-blue text-white'
                    : isLight ? 'text-gray-500 hover:text-gray-900 hover:bg-gray-300' : 'text-gray-400 hover:text-white hover:bg-gray-600'
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
                  isChatbotMode
                    ? 'bg-amber-600 text-white'
                    : isLight ? 'text-gray-500 hover:text-gray-900 hover:bg-gray-300' : 'text-gray-400 hover:text-white hover:bg-gray-600'
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
              onClick={toggleAssistantTheme}
              className={`p-1.5 rounded transition-colors hidden sm:inline-flex ${iconBtn}`}
              title={isLight ? 'Switch to dark theme' : 'Switch to light theme'}
            >
              {isLight ? <MoonIcon /> : <SunIcon />}
            </button>
            <button
              onClick={toggleAssistantSize}
              className={`p-1.5 rounded transition-colors hidden sm:inline-flex ${iconBtn}`}
              title={isWide ? 'Collapse panel' : 'Expand panel'}
            >
              {isWide ? <CollapseIcon /> : <ExpandIcon />}
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

      {isChatbotMode ? (
        <ChatBotContent theme={assistantTheme} />
      ) : (
        <>
      {/* Messages */}
      <div className="flex-1 overflow-y-auto overflow-x-hidden p-4 space-y-4">
        {assistantMessages.length === 0 ? (
          <div className={`text-center mt-8 ${mutedText}`}>
            <Sparkles className={`w-12 h-12 mx-auto mb-3 ${isLight ? 'text-gray-300' : 'text-gray-600'}`} />
            <p className="text-sm">Ask me anything about RealSense products.</p>
            <p className={`text-xs mt-2 ${isLight ? 'text-gray-400' : 'text-gray-600'}`}>Try: "What's the depth range of the D435i?"</p>
          </div>
        ) : (
          assistantMessages.map((message, i) => (
            <AssistantMessageBubble
              key={message.id}
              message={message}
              isLatestAssistant={message.role === 'assistant' && i === assistantMessages.length - 1}
              theme={assistantTheme}
            />
          ))
        )}

        {isAssistantLoading && !assistantMessages.some((m) => m.isStreaming && m.content) && (
          <div className={`flex items-center gap-2 ${mutedText}`}>
            <Loader2 className="w-4 h-4 animate-spin" />
            <span className="text-sm">Thinking...</span>
          </div>
        )}

        <div ref={messagesEndRef} />
      </div>

      {/* Input */}
      <form onSubmit={handleSubmit} className={`p-3 border-t ${headerBg}`}>
        {(pendingImages.length > 0 || pendingFiles.length > 0) && (
          <div className="flex flex-wrap gap-1.5 mb-2">
            {pendingImages.map((dataUri, i) => (
              <div key={i} className="relative w-12 h-12 rounded overflow-hidden border border-gray-600 shrink-0">
                <img src={dataUri} alt="" className="w-full h-full object-cover" />
                <button
                  type="button"
                  onClick={() => removeImage(i)}
                  className="absolute top-0 right-0 bg-black/60 text-white rounded-bl p-0.5"
                  title="Remove image"
                >
                  <X className="w-3 h-3" />
                </button>
              </div>
            ))}
            {pendingFiles.map((file, i) => (
              <div
                key={i}
                className={`flex items-center gap-1 pl-2 pr-1 py-1 rounded border text-xs ${isLight ? 'bg-gray-100 border-gray-300 text-gray-700' : 'bg-gray-800 border-gray-600 text-gray-300'}`}
              >
                <FileText className="w-3.5 h-3.5 shrink-0" />
                <span className="max-w-[120px] truncate">{file.fileName}</span>
                <button
                  type="button"
                  onClick={() => removeFile(i)}
                  className={iconBtn}
                  title="Remove file"
                >
                  <X className="w-3.5 h-3.5" />
                </button>
              </div>
            ))}
          </div>
        )}

        <div className="flex items-center gap-2">
          <input
            ref={inputRef}
            type="text"
            value={inputValue}
            onChange={(e) => setInputValue(e.target.value)}
            placeholder="Ask about RealSense products..."
            disabled={isAssistantLoading}
            className={`flex-1 px-3 py-2 border rounded-lg focus:outline-none focus:border-rs-blue text-sm ${inputClasses}`}
          />
          <button
            type="submit"
            disabled={(!inputValue.trim() && pendingImages.length === 0 && pendingFiles.length === 0) || isAssistantLoading}
            className="p-2 bg-rs-blue text-white rounded-lg hover:bg-blue-600 disabled:opacity-50 disabled:cursor-not-allowed transition-colors"
          >
            <Send className="w-4 h-4" />
          </button>
        </div>

        <div className="flex items-center gap-1 mt-2">
          <input
            ref={fileInputRef}
            type="file"
            multiple
            onChange={handleFileChange}
            className="hidden"
            accept="image/*,.pdf,.doc,.docx,.txt"
          />
          <button
            type="button"
            onClick={() => fileInputRef.current?.click()}
            title="Attach a file"
            className={`p-1.5 rounded transition-colors ${iconBtn}`}
          >
            <Paperclip className="w-4 h-4" />
          </button>
          <button
            type="button"
            onClick={stopAssistantMessage}
            disabled={!isAssistantLoading}
            title="Stop generating"
            className={`p-1.5 rounded transition-colors ${iconBtn} disabled:opacity-30 disabled:cursor-not-allowed`}
          >
            <Square className="w-4 h-4" />
          </button>
          {micSupported && (
            <button
              type="button"
              onClick={toggleListening}
              title={isListening ? 'Stop voice input' : 'Voice input'}
              className={`p-1.5 rounded transition-colors ${isListening ? 'text-red-400' : iconBtn}`}
            >
              {isListening ? <MicOff className="w-4 h-4" /> : <Mic className="w-4 h-4" />}
            </button>
          )}
        </div>
      </form>
        </>
      )}
    </div>
  )
}
