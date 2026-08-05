import { useState, useRef, useEffect, useMemo } from 'react'
import { Send, PlusCircle, Loader2, Sparkles, Paperclip, Square, Mic, MicOff, X, FileText } from 'lucide-react'
import { useAppStore } from '../../store'
import { AssistantMessageBubble } from './AssistantMessage'
import type { AssistantFileAttachment } from '../../api/assistantChat'

// Minimal shape of the (still vendor-prefixed) Web Speech API — not in lib.dom.d.ts.
interface MinimalSpeechRecognition extends EventTarget {
  continuous: boolean
  interimResults: boolean
  lang: string
  onresult: ((event: { results: ArrayLike<ArrayLike<{ transcript: string }>> }) => void) | null
  onerror: (() => void) | null
  onend: (() => void) | null
  start: () => void
  stop: () => void
}

function getSpeechRecognitionCtor(): (new () => MinimalSpeechRecognition) | null {
  const w = window as unknown as {
    SpeechRecognition?: new () => MinimalSpeechRecognition
    webkitSpeechRecognition?: new () => MinimalSpeechRecognition
  }
  return w.SpeechRecognition || w.webkitSpeechRecognition || null
}

function readFileAsDataUri(file: File): Promise<string> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader()
    reader.onload = () => resolve(reader.result as string)
    reader.onerror = () => reject(reader.error)
    reader.readAsDataURL(file)
  })
}

// Icons matching the reference widget (widget.js) exactly, rather than the closest lucide-react
// equivalents, so the panel chrome looks identical to the production embed.
const iconProps = { width: 16, height: 16, viewBox: '0 0 24 24', fill: 'none', 'aria-hidden': true } as const

function ExpandIcon() {
  return (
    <svg {...iconProps}>
      <path d="M4 10V4h6M20 14v6h-6M4 4l7 7M20 20l-7-7" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" />
    </svg>
  )
}
function CollapseIcon() {
  return (
    <svg {...iconProps}>
      <path d="M10 4v6H4M14 20v-6h6M10 10L4 4M14 14l6 6" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" />
    </svg>
  )
}
function SunIcon() {
  return (
    <svg {...iconProps}>
      <circle cx="12" cy="12" r="4" stroke="currentColor" strokeWidth="2" />
      <path d="M12 2v2M12 20v2M4.93 4.93l1.41 1.41M17.66 17.66l1.41 1.41M2 12h2M20 12h2M4.93 19.07l1.41-1.41M17.66 6.34l1.41-1.41" stroke="currentColor" strokeWidth="2" strokeLinecap="round" />
    </svg>
  )
}
function MoonIcon() {
  return (
    <svg {...iconProps}>
      <path d="M20 14.5A8 8 0 0 1 9.5 4a8 8 0 1 0 10.5 10.5z" stroke="currentColor" strokeWidth="2" strokeLinejoin="round" />
    </svg>
  )
}
function CloseIcon() {
  return (
    <svg width={16} height={16} viewBox="0 0 18 18" aria-hidden="true">
      <path d="M4 4L14 14M14 4L4 14" stroke="currentColor" strokeWidth="2" strokeLinecap="round" />
    </svg>
  )
}

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
    toggleAssistantTheme,
    toggleAssistantSize,
  } = useAppStore()

  const isLight = assistantTheme === 'light'
  const isWide = assistantSize === 'wide'

  const [inputValue, setInputValue] = useState('')
  const [pendingImages, setPendingImages] = useState<string[]>([])
  const [pendingFiles, setPendingFiles] = useState<AssistantFileAttachment[]>([])
  const [isListening, setIsListening] = useState(false)
  const messagesEndRef = useRef<HTMLDivElement>(null)
  const inputRef = useRef<HTMLInputElement>(null)
  const fileInputRef = useRef<HTMLInputElement>(null)
  const recognitionRef = useRef<MinimalSpeechRecognition | null>(null)
  const wasOpenRef = useRef(isAssistantOpen)
  const micSupported = useMemo(() => getSpeechRecognitionCtor() !== null, [])

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

  useEffect(() => {
    if (wasOpenRef.current && !isAssistantOpen) {
      document.getElementById('rsai-launcher-button')?.focus()
    }
    wasOpenRef.current = isAssistantOpen
  }, [isAssistantOpen])

  // Stop any active speech recognition when the panel unmounts.
  useEffect(() => {
    return () => recognitionRef.current?.stop()
  }, [])

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault()
    const message = inputValue.trim()
    if ((!message && pendingImages.length === 0 && pendingFiles.length === 0) || isAssistantLoading) return
    setInputValue('')
    const attachments = (pendingImages.length || pendingFiles.length)
      ? { imageDataUris: pendingImages, fileDataUris: pendingFiles }
      : undefined
    setPendingImages([])
    setPendingFiles([])
    sendAssistantMessage(message, attachments)
  }

  const handleFileChange = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = Array.from(e.target.files || [])
    e.target.value = '' // allow re-selecting the same file later
    for (const file of files) {
      try {
        const dataUri = await readFileAsDataUri(file)
        if (file.type.startsWith('image/')) {
          setPendingImages((prev) => [...prev, dataUri])
        } else {
          setPendingFiles((prev) => [...prev, { dataUri, fileName: file.name, mimeType: file.type || 'application/octet-stream' }])
        }
      } catch (error) {
        console.warn('Failed to read attached file:', error)
      }
    }
  }

  const toggleListening = () => {
    if (isListening) {
      recognitionRef.current?.stop()
      return
    }
    const SpeechRecognitionCtor = getSpeechRecognitionCtor()
    if (!SpeechRecognitionCtor) return

    const recognition = new SpeechRecognitionCtor()
    recognition.continuous = false
    recognition.interimResults = false
    recognition.lang = 'en-US'
    recognition.onresult = (event) => {
      const transcript = Array.from(event.results).map((r) => r[0]?.transcript || '').join(' ')
      setInputValue((prev) => (prev ? `${prev} ${transcript}` : transcript))
    }
    recognition.onerror = () => setIsListening(false)
    recognition.onend = () => setIsListening(false)

    recognitionRef.current = recognition
    setIsListening(true)
    recognition.start()
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
      <div className={`flex items-center justify-between px-4 py-3 border-b ${headerBg}`}>
        <div className="flex items-center gap-3 min-w-0">
          <span className="flex items-center justify-center w-9 h-9 rounded-full bg-white overflow-hidden shrink-0 ring-2 ring-rs-blue/40">
            {/* See AssistantButton.tsx for why this crops with object-cover instead of object-contain. */}
            <img src="/realsense-logo.png" alt="" className="w-full h-full object-cover object-left" />
          </span>
          <div className="min-w-0">
            <h3 className={`font-semibold text-sm truncate ${titleText}`}>RealSense AI Assistant</h3>
            <div className={`flex items-center gap-1.5 text-[11px] ${mutedText}`}>
              <span className="relative flex w-1.5 h-1.5">
                {isAssistantOnline && (
                  <span className="absolute inline-flex h-full w-full rounded-full bg-green-400 opacity-75 animate-ping motion-reduce:animate-none" />
                )}
                <span className={`relative inline-flex rounded-full w-1.5 h-1.5 ${isAssistantOnline ? 'bg-green-500' : 'bg-gray-500'}`} />
              </span>
              {isAssistantOnline ? 'Online' : 'Reconnecting…'} · powered by RealSense AI
            </div>
          </div>
        </div>
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
            onClick={clearAssistantChat}
            className={`p-1.5 rounded transition-colors ${iconBtn}`}
            title="New chat"
          >
            <PlusCircle className="w-4 h-4" />
          </button>
          <button
            onClick={toggleAssistant}
            className={`p-1.5 rounded transition-colors ${iconBtn}`}
            title="Close"
          >
            <CloseIcon />
          </button>
        </div>
      </div>

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
                  onClick={() => setPendingImages((prev) => prev.filter((_, idx) => idx !== i))}
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
                  onClick={() => setPendingFiles((prev) => prev.filter((_, idx) => idx !== i))}
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
    </div>
  )
}
