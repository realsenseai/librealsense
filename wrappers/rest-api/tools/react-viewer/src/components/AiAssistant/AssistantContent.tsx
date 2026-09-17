// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
//
// Body content (message list + input) for the RealSense AI Assistant mode, rendered inside the
// shared AssistantPanel shell — only the header/frame around it is shared with the Chatbot mode.
// Mirrors ChatBotContent.tsx's self-contained shape (reads its own store slice, owns its own
// input state) for symmetry between the two modes.

import { useRef, useEffect } from 'react'
import { Loader2, Sparkles } from 'lucide-react'
import { useAppStore } from '../../store'
import { AssistantMessageBubble } from './AssistantMessage'
import { ChatInput } from './ChatInput'

export function AssistantContent() {
  const { isAssistantLoading, assistantMessages, sendAssistantMessage, stopAssistantMessage, setError } = useAppStore()

  const messagesEndRef = useRef<HTMLDivElement>(null)

  useEffect(() => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' })
  }, [assistantMessages])

  return (
    <>
      <div className="flex-1 overflow-y-auto overflow-x-hidden p-4 space-y-4">
        {assistantMessages.length === 0 ? (
          <div className="text-center mt-8 text-gray-400">
            <Sparkles className="w-12 h-12 mx-auto mb-3 text-gray-600" />
            <p className="text-sm">Ask me anything about RealSense products.</p>
            <p className="text-xs mt-2 text-gray-600">Try: "What's the depth range of the D435i?"</p>
          </div>
        ) : (
          assistantMessages.map((message, i) => (
            <AssistantMessageBubble
              key={message.id}
              message={message}
              isLatestAssistant={message.role === 'assistant' && i === assistantMessages.length - 1}
            />
          ))
        )}

        {isAssistantLoading && !assistantMessages.some((m) => m.isStreaming && m.content) && (
          <div className="flex items-center gap-2 text-gray-400">
            <Loader2 className="w-4 h-4 animate-spin" />
            <span className="text-sm">Thinking...</span>
          </div>
        )}

        <div ref={messagesEndRef} />
      </div>

      <ChatInput
        placeholder="Ask about RealSense products..."
        accent="blue"
        isLoading={isAssistantLoading}
        attachmentsEnabled
        onSend={sendAssistantMessage}
        onStop={stopAssistantMessage}
        onAttachmentError={setError}
      />
    </>
  )
}
