import { useState, useRef, useEffect } from 'react'
import { Send, Trash2, Loader2, Sparkles } from 'lucide-react'
import { useAppStore } from '../../store'
import { ChatMessageBubble } from './ChatMessage'
import { SettingsPreview } from './SettingsPreview'
import { getActiveProviderName } from '../../api/chat'

/**
 * Slide-out chat panel for AI assistant
 */
export function ChatPanel() {
  const {
    isChatOpen,
    isChatLoading,
    chatMessages,
    pendingSettings,
    sendChatMessage,
    clearChat,
    deviceStates,
  } = useAppStore()

  const [inputValue, setInputValue] = useState('')
  const messagesEndRef = useRef<HTMLDivElement>(null)
  const inputRef = useRef<HTMLInputElement>(null)
  const providerName = getActiveProviderName()

  // Scroll to bottom when new messages arrive
  useEffect(() => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' })
  }, [chatMessages])

  // Focus input when panel opens
  useEffect(() => {
    if (isChatOpen) {
      inputRef.current?.focus()
    }
  }, [isChatOpen])

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault()
    const message = inputValue.trim()
    if (!message || isChatLoading) return
    
    setInputValue('')
    sendChatMessage(message)
  }

  const activeDeviceCount = Object.values(deviceStates).filter(ds => ds.isActive).length

  if (!isChatOpen) return null

  return (
    <div className="fixed right-24 bottom-6 z-40 w-96 h-[600px] max-h-[80vh] bg-rs-dark border border-gray-700 rounded-lg shadow-2xl flex flex-col overflow-hidden">
      {/* Header */}
      <div className="flex items-center justify-between px-4 py-3 bg-rs-darker border-b border-gray-700">
        <div className="flex items-center gap-2">
          <Sparkles className="w-5 h-5 text-rs-blue" />
          <div>
            <h3 className="font-semibold text-white text-sm">AI Assistant</h3>
            {providerName && (
              <span className="text-[10px] text-gray-500">{providerName}</span>
            )}
          </div>
        </div>
        <div className="flex items-center gap-2">
          <span className="text-xs text-gray-400">
            {activeDeviceCount} device{activeDeviceCount !== 1 ? 's' : ''} active
          </span>
          <button
            onClick={clearChat}
            className="p-1.5 text-gray-400 hover:text-white hover:bg-gray-700 rounded transition-colors"
            title="Clear chat"
          >
            <Trash2 className="w-4 h-4" />
          </button>
        </div>
      </div>

      {/* Messages */}
      <div className="flex-1 overflow-y-auto p-4 space-y-4">
        {chatMessages.length === 0 ? (
          <div className="text-center text-gray-500 mt-8">
            <Sparkles className="w-12 h-12 mx-auto mb-3 text-gray-600" />
            <p className="text-sm">
              Hi! I can help you configure your RealSense cameras.
            </p>
            <p className="text-xs mt-2 text-gray-600">
              Try: "Set up for 3D scanning" or "Optimize for robotics"
            </p>
          </div>
        ) : (
          <>
            {chatMessages.map((message) => (
              <ChatMessageBubble key={message.id} message={message} />
            ))}
          </>
        )}
        
        {/* Loading indicator */}
        {isChatLoading && (
          <div className="flex items-center gap-2 text-gray-400">
            <Loader2 className="w-4 h-4 animate-spin" />
            <span className="text-sm">Thinking...</span>
          </div>
        )}
        
        <div ref={messagesEndRef} />
      </div>

      {/* Pending settings preview */}
      {pendingSettings && <SettingsPreview settings={pendingSettings} />}

      {/* Input */}
      <form onSubmit={handleSubmit} className="p-3 border-t border-gray-700 bg-rs-darker">
        <div className="flex items-center gap-2">
          <input
            ref={inputRef}
            type="text"
            value={inputValue}
            onChange={(e) => setInputValue(e.target.value)}
            placeholder="Ask about camera settings..."
            disabled={isChatLoading}
            className="flex-1 px-3 py-2 bg-gray-800 border border-gray-600 rounded-lg text-white placeholder-gray-500 focus:outline-none focus:border-rs-blue text-sm"
          />
          <button
            type="submit"
            disabled={!inputValue.trim() || isChatLoading}
            className="p-2 bg-rs-blue text-white rounded-lg hover:bg-blue-600 disabled:opacity-50 disabled:cursor-not-allowed transition-colors"
          >
            <Send className="w-4 h-4" />
          </button>
        </div>
      </form>
    </div>
  )
}
