import { User, Bot, Code } from 'lucide-react'
import type { ChatMessage } from '../../api/chat'
import { useState } from 'react'
import { CodeExport } from './CodeExport'

interface ChatMessageBubbleProps {
  message: ChatMessage
}

/**
 * Individual chat message bubble with markdown-like rendering
 */
export function ChatMessageBubble({ message }: ChatMessageBubbleProps) {
  const [showCode, setShowCode] = useState(false)
  const isUser = message.role === 'user'

  // Simple markdown-like rendering
  const renderContent = (content: string) => {
    // Remove settings JSON blocks from display (they're shown in SettingsPreview)
    const cleanContent = content.replace(/```settings[\s\S]*?```/g, '').trim()
    
    // Split by code blocks
    const parts = cleanContent.split(/(```[\s\S]*?```)/g)
    
    return parts.map((part, i) => {
      if (part.startsWith('```')) {
        // Code block
        const match = part.match(/```(\w+)?\n?([\s\S]*?)```/)
        if (match) {
          const [, lang, code] = match
          return (
            <pre key={i} className="mt-2 p-2 bg-gray-900 rounded text-xs overflow-x-auto">
              <code className={`language-${lang || 'text'}`}>{code.trim()}</code>
            </pre>
          )
        }
      }
      
      // Regular text with inline formatting
      return (
        <span key={i}>
          {part.split('\n').map((line, j) => (
            <span key={j}>
              {j > 0 && <br />}
              {formatInline(line)}
            </span>
          ))}
        </span>
      )
    })
  }

  // Format inline elements like **bold** and `code`
  const formatInline = (text: string) => {
    const parts: (string | JSX.Element)[] = []
    let remaining = text
    let key = 0
    
    while (remaining) {
      // Bold
      const boldMatch = remaining.match(/\*\*(.+?)\*\*/)
      // Inline code
      const codeMatch = remaining.match(/`([^`]+)`/)
      
      // Find earliest match
      let earliestMatch: RegExpMatchArray | null = null
      let type: 'bold' | 'code' | null = null
      
      if (boldMatch && (!codeMatch || boldMatch.index! < codeMatch.index!)) {
        earliestMatch = boldMatch
        type = 'bold'
      } else if (codeMatch) {
        earliestMatch = codeMatch
        type = 'code'
      }
      
      if (earliestMatch && type) {
        // Add text before match
        if (earliestMatch.index! > 0) {
          parts.push(remaining.slice(0, earliestMatch.index))
        }
        
        // Add formatted element
        if (type === 'bold') {
          parts.push(<strong key={key++} className="font-semibold">{earliestMatch[1]}</strong>)
        } else if (type === 'code') {
          parts.push(<code key={key++} className="px-1 py-0.5 bg-gray-800 rounded text-xs">{earliestMatch[1]}</code>)
        }
        
        remaining = remaining.slice(earliestMatch.index! + earliestMatch[0].length)
      } else {
        parts.push(remaining)
        break
      }
    }
    
    return parts
  }

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
          {renderContent(message.content)}
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
