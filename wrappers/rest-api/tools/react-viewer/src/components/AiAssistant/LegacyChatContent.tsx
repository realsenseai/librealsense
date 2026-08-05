// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
//
// Body content (message list + settings preview + input) for the legacy device-config chatbot
// mode, rendered inside the shared AssistantPanel shell. Reuses the untouched ChatBot message
// bubble and settings-preview components — only the header/frame around it is shared with the
// RealSense AI Assistant mode.

import { useState, useRef, useEffect } from 'react'
import { Send, Loader2, Sparkles } from 'lucide-react'
import { useAppStore } from '../../store'
import { ChatMessageBubble } from '../ChatBot/ChatMessage'
import { SettingsPreview } from '../ChatBot/SettingsPreview'

export function LegacyChatContent() {
  const { isChatLoading, chatMessages, pendingSettings, sendChatMessage } = useAppStore()

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
          <div className="text-center text-gray-500 mt-8">
            <Sparkles className="w-12 h-12 mx-auto mb-3 text-gray-600" />
            <p className="text-sm">Hi! I can help you configure your RealSense cameras.</p>
            <p className="text-xs mt-2 text-gray-600">Try: "Set up for 3D scanning" or "Optimize for robotics"</p>
          </div>
        ) : (
          chatMessages.map((message) => <ChatMessageBubble key={message.id} message={message} />)
        )}

        {isChatLoading && (
          <div className="flex items-center gap-2 text-gray-400">
            <Loader2 className="w-4 h-4 animate-spin" />
            <span className="text-sm">Thinking...</span>
          </div>
        )}

        <div ref={messagesEndRef} />
      </div>

      {pendingSettings && <SettingsPreview settings={pendingSettings} />}

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
    </>
  )
}
