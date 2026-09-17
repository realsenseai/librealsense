// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
//
// Body content (message list + settings preview + input) for the device-config chatbot mode,
// rendered inside the shared AssistantPanel shell. Reuses the untouched ChatBot message bubble
// and settings-preview components — only the header/frame and the input footer (ChatInput) are
// shared with the RealSense AI Assistant mode. Uses its own amber accent (instead of rs-blue) so
// the two modes are visually distinguishable at a glance, without having to read the header title.

import { useRef, useEffect } from 'react'
import { Loader2, Wrench } from 'lucide-react'
import { useAppStore } from '../../store'
import { ChatMessageBubble } from '../ChatBot/ChatMessage'
import { SettingsPreview } from '../ChatBot/SettingsPreview'
import { ChatInput } from './ChatInput'

export function ChatBotContent() {
  const { isChatLoading, chatMessages, pendingSettings, sendChatMessage, stopChatMessage } = useAppStore()

  const messagesEndRef = useRef<HTMLDivElement>(null)

  useEffect(() => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' })
  }, [chatMessages])

  return (
    <>
      <div className="flex-1 overflow-y-auto overflow-x-hidden p-4 space-y-4">
        {chatMessages.length === 0 ? (
          <div className="text-center mt-8 text-gray-400">
            <Wrench className="w-12 h-12 mx-auto mb-3 text-amber-700" />
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

      <ChatInput
        placeholder="Ask about camera settings..."
        accent="amber"
        isLoading={isChatLoading}
        attachmentsEnabled={false}
        onSend={(message) => sendChatMessage(message)}
        onStop={stopChatMessage}
      />
    </>
  )
}
