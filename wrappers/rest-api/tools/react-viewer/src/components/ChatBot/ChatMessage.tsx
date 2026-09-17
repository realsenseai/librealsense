import { User, Bot, Code } from 'lucide-react'
import type { ChatMessage } from '../../api/chat'
import { useState } from 'react'
import { CodeExport } from './CodeExport'
import { renderMessageContent } from '../AiAssistant/messageFormatting'

interface ChatMessageBubbleProps {
  message: ChatMessage
}

/**
 * Individual chat message bubble. Markdown rendering (GFM tables, headers, lists, links,
 * code) is shared with the AI Assistant via messageFormatting.tsx.
 */
export function ChatMessageBubble({ message }: ChatMessageBubbleProps) {
  const [showCode, setShowCode] = useState(false)
  const isUser = message.role === 'user'

  // The proposed-settings JSON block is parsed out elsewhere (chatPrompt.ts) to drive
  // SettingsPreview, but never removed from the stored message — strip it here so it
  // isn't also dumped as raw text in the bubble.
  const cleanContent = message.content.replace(/```settings[\s\S]*?```/g, '').trim()

  return (
    <div className={`flex gap-3 ${isUser ? 'flex-row-reverse' : ''}`}>
      {/* Avatar */}
      <div className={`flex-shrink-0 w-8 h-8 rounded-full flex items-center justify-center ${
        isUser ? 'bg-rs-blue' : 'bg-gray-700'
      }`}>
        {isUser ? (
          <User className="w-4 h-4 text-white" />
        ) : (
          <Bot className="w-4 h-4 text-gray-300" />
        )}
      </div>

      {/* Message content */}
      <div className={`flex-1 max-w-[85%] ${isUser ? 'text-right' : ''}`}>
        <div className={`inline-block px-3 py-2 rounded-lg text-sm ${
          isUser 
            ? 'bg-rs-blue text-white rounded-tr-none' 
            : 'bg-gray-800 text-gray-200 rounded-tl-none'
        }`}>
          {renderMessageContent(cleanContent)}
        </div>
        
        {/* Show code export button for assistant messages with proposed settings */}
        {!isUser && message.proposedSettings && (
          <div className="mt-2">
            <button
              onClick={() => setShowCode(!showCode)}
              className="flex items-center gap-1 text-xs text-gray-400 hover:text-white transition-colors"
            >
              <Code className="w-3 h-3" />
              {showCode ? 'Hide code' : 'Export as code'}
            </button>
            
            {showCode && <CodeExport settings={message.proposedSettings} />}
          </div>
        )}
        
        {/* Timestamp */}
        <div className={`text-[10px] text-gray-500 mt-1 ${isUser ? 'text-right' : ''}`}>
          {new Date(message.timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}
        </div>
      </div>
    </div>
  )
}
