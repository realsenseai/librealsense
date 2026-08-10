// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
//
// Body content (message list + settings preview + input) for the device-config chatbot mode,
// rendered inside the shared AssistantPanel shell. Reuses the untouched ChatBot message bubble
// and settings-preview components — only the header/frame around it is shared with the
// RealSense AI Assistant mode. Uses its own amber accent (instead of rs-blue) so the two modes
// are visually distinguishable at a glance, without having to read the header title.

import { useState, useRef, useEffect } from 'react'
import { Send, Loader2, Square, Wrench } from 'lucide-react'
import { useAppStore } from '../../store'
import { ChatMessageBubble } from '../ChatBot/ChatMessage'
import { SettingsPreview } from '../ChatBot/SettingsPreview'

interface ChatBotContentProps {
  theme: 'light' | 'dark'
}

export function ChatBotContent({ theme }: ChatBotContentProps) {
  const isLight = theme === 'light'
  const { isChatLoading, chatMessages, pendingSettings, sendChatMessage, stopChatMessage } = useAppStore()

  const [inputValue, setInputValue] = useState('')
  const messagesEndRef = useRef<HTMLDivElement>(null)
  const inputRef = useRef<HTMLInputElement>(null)

  useEffect(() => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' })
  }, [chatMessages])

  useEffect(() => {
    inputRef.current?.focus()
  }, [])

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault()
    const message = inputValue.trim()
    if (!message || isChatLoading) return
    setInputValue('')
    sendChatMessage(message)
  }

  return (
    <>
      <div className="flex-1 overflow-y-auto overflow-x-hidden p-4 space-y-4">
        {chatMessages.length === 0 ? (
          <div className={`text-center mt-8 ${isLight ? 'text-gray-500' : 'text-gray-400'}`}>
            <Wrench className={`w-12 h-12 mx-auto mb-3 ${isLight ? 'text-amber-400' : 'text-amber-700'}`} />
            <p className="text-sm">Hi! I can help you configure your RealSense cameras.</p>
            <p className={`text-xs mt-2 ${isLight ? 'text-gray-400' : 'text-gray-600'}`}>Try: "Set up for 3D scanning" or "Optimize for robotics"</p>
          </div>
        ) : (
          chatMessages.map((message) => <ChatMessageBubble key={message.id} message={message} />)
        )}

        {isChatLoading && (
          <div className={`flex items-center gap-2 ${isLight ? 'text-gray-500' : 'text-gray-400'}`}>
            <Loader2 className="w-4 h-4 animate-spin" />
            <span className="text-sm">Thinking...</span>
          </div>
        )}

        <div ref={messagesEndRef} />
      </div>

      {pendingSettings && <SettingsPreview settings={pendingSettings} />}

      <form onSubmit={handleSubmit} className={`p-3 border-t ${isLight ? 'bg-gray-50 border-gray-200' : 'bg-rs-darker border-gray-700'}`}>
        <div className="flex items-center gap-2">
          <input
            ref={inputRef}
            type="text"
            value={inputValue}
            onChange={(e) => setInputValue(e.target.value)}
            placeholder="Ask about camera settings..."
            disabled={isChatLoading}
            className={`flex-1 px-3 py-2 border rounded-lg focus:outline-none focus:border-amber-500 text-sm ${
              isLight
                ? 'bg-gray-100 border-gray-300 text-gray-900 placeholder-gray-400'
                : 'bg-gray-800 border-gray-600 text-white placeholder-gray-500'
            }`}
          />
          <button
            type="submit"
            disabled={!inputValue.trim() || isChatLoading}
            className="p-2 bg-amber-600 text-white rounded-lg hover:bg-amber-500 disabled:opacity-50 disabled:cursor-not-allowed transition-colors"
          >
            <Send className="w-4 h-4" />
          </button>
        </div>

        <div className="flex items-center gap-1 mt-2">
          <button
            type="button"
            onClick={stopChatMessage}
            disabled={!isChatLoading}
            title="Stop generating"
            className={`p-1.5 rounded transition-colors disabled:opacity-30 disabled:cursor-not-allowed ${
              isLight ? 'text-gray-500 hover:text-gray-900 hover:bg-gray-200' : 'text-gray-400 hover:text-white hover:bg-gray-700'
            }`}
          >
            <Square className="w-4 h-4" />
          </button>
        </div>
      </form>
    </>
  )
}
