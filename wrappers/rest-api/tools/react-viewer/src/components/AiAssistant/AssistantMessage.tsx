import { useState } from 'react'
import { User, Bot, ExternalLink, ThumbsUp, ThumbsDown, Loader2, Copy, Check, RotateCw, FileText } from 'lucide-react'
import { useAppStore } from '../../store'
import type { AssistantChatMessage } from '../../api/assistantChat'

interface AssistantMessageBubbleProps {
  message: AssistantChatMessage
  isLatestAssistant: boolean
  theme: 'light' | 'dark'
}

/**
 * Individual assistant/user message bubble: code-block + bold/inline-code rendering,
 * citation chips, and (on the latest completed assistant turn only) reaction buttons.
 */
export function AssistantMessageBubble({ message, isLatestAssistant, theme }: AssistantMessageBubbleProps) {
  const sendAssistantReaction = useAppStore((s) => s.sendAssistantReaction)
  const regenerateLastAssistantMessage = useAppStore((s) => s.regenerateLastAssistantMessage)
  const [reactionSent, setReactionSent] = useState<1 | -1 | null>(null)
  const [copied, setCopied] = useState(false)
  const isUser = message.role === 'user'
  const isLight = theme === 'light'

  // The assistant embeds raw citation placeholders (e.g. "【4:1†source】") in the streamed
  // text, meant to be spliced into a rendered link. We show citations as chips below the
  // bubble instead (see the citations block further down), so just strip the placeholders
  // here rather than leaving them visible as noise.
  const stripCitationMarkers = (content: string) => {
    let stripped = content
    for (const citation of message.citations || []) {
      if (citation.textToReplace) stripped = stripped.split(citation.textToReplace).join('')
    }
    return stripped.replace(/【\d+(?::\d+)?†[^】]*】/g, '')
  }

  const renderContent = (content: string) => {
    const parts = content.split(/(```[\s\S]*?```)/g)

    return parts.map((part, i) => {
      if (part.startsWith('```')) {
        const match = part.match(/```(\w+)?\n?([\s\S]*?)```/)
        if (match) {
          const [, lang, code] = match
          return (
            <pre key={i} className={`mt-2 p-2 rounded text-xs whitespace-pre-wrap break-words ${isLight ? 'bg-gray-100' : 'bg-gray-900'}`}>
              <code className={`language-${lang || 'text'}`}>{code.trim()}</code>
            </pre>
          )
        }
      }

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

  const formatInline = (text: string) => {
    const parts: (string | JSX.Element)[] = []
    let remaining = text
    let key = 0

    while (remaining) {
      const boldMatch = remaining.match(/\*\*(.+?)\*\*/)
      const codeMatch = remaining.match(/`([^`]+)`/)

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
        if (earliestMatch.index! > 0) {
          parts.push(remaining.slice(0, earliestMatch.index))
        }
        if (type === 'bold') {
          parts.push(<strong key={key++} className="font-semibold">{earliestMatch[1]}</strong>)
        } else {
          parts.push(<code key={key++} className={`px-1 py-0.5 rounded text-xs ${isLight ? 'bg-gray-200' : 'bg-gray-800'}`}>{earliestMatch[1]}</code>)
        }
        remaining = remaining.slice(earliestMatch.index! + earliestMatch[0].length)
      } else {
        parts.push(remaining)
        break
      }
    }

    return parts
  }

  const handleReaction = (value: 1 | -1) => {
    setReactionSent(value)
    sendAssistantReaction(value)
  }

  const handleCopy = async () => {
    try {
      await navigator.clipboard.writeText(stripCitationMarkers(message.content))
      setCopied(true)
      setTimeout(() => setCopied(false), 2000)
    } catch (error) {
      console.warn('Failed to copy message:', error)
    }
  }

  const showReactions = isLatestAssistant && !message.isStreaming && !message.isError
  const showCopy = !isUser && !message.isStreaming && !message.isError
  const showRegenerate = !isUser && isLatestAssistant && !message.isStreaming
  const actionBtn = isLight ? 'text-gray-500 hover:text-gray-900' : 'text-gray-500 hover:text-white'

  return (
    <div className={`flex gap-3 ${isUser ? 'flex-row-reverse' : ''}`}>
      <div className={`flex-shrink-0 w-8 h-8 rounded-full flex items-center justify-center ${isUser ? 'bg-rs-blue' : isLight ? 'bg-gray-200' : 'bg-gray-700'}`}>
        {isUser ? <User className="w-4 h-4 text-white" /> : <Bot className={`w-4 h-4 ${isLight ? 'text-gray-600' : 'text-gray-300'}`} />}
      </div>

      <div className={`flex-1 min-w-0 max-w-[85%] ${isUser ? 'text-right' : ''}`}>
        {isUser && (message.attachments?.imageDataUris?.length || message.attachments?.fileDataUris?.length) ? (
          <div className="mb-1.5 flex flex-wrap gap-1.5 justify-end">
            {message.attachments.imageDataUris?.map((dataUri, i) => (
              <img key={`img-${i}`} src={dataUri} alt="Attached image" className="w-16 h-16 rounded-lg object-cover border border-gray-600" />
            ))}
            {message.attachments.fileDataUris?.map((file, i) => (
              <div
                key={`file-${i}`}
                className={`flex items-center gap-1 pl-2 pr-2 py-1 rounded border text-xs ${isLight ? 'bg-gray-100 border-gray-300 text-gray-700' : 'bg-gray-800 border-gray-600 text-gray-300'}`}
              >
                <FileText className="w-3.5 h-3.5 shrink-0" />
                <span className="max-w-[140px] truncate">{file.fileName}</span>
              </div>
            ))}
          </div>
        ) : null}

        <div className={`inline-block max-w-full break-words px-3 py-2 rounded-lg text-sm ${
          message.isError
            ? 'bg-red-950/60 border border-red-800 text-red-200 rounded-tl-none'
            : isUser
              ? 'bg-rs-blue text-white rounded-tr-none'
              : isLight
                ? 'bg-gray-100 text-gray-800 rounded-tl-none'
                : 'bg-gray-800 text-gray-200 rounded-tl-none'
        }`}>
          {message.isStreaming && !message.content ? (
            <Loader2 className="w-4 h-4 animate-spin text-gray-400" />
          ) : (
            renderContent(stripCitationMarkers(message.content))
          )}
        </div>

        {!isUser && message.citations && message.citations.length > 0 && (
          <div className="mt-2 flex flex-wrap gap-1.5 justify-start">
            {message.citations.map((c, i) => (
              <a
                key={i}
                href={c.url}
                target="_blank"
                rel="noopener noreferrer"
                title={c.quote || c.url}
                className={`inline-flex items-center gap-1 px-2 py-0.5 rounded-full text-[11px] text-rs-blue hover:text-blue-300 hover:border-rs-blue/50 transition-colors border max-w-full ${
                  isLight ? 'bg-gray-100 border-gray-300' : 'bg-gray-800 border-gray-700'
                }`}
              >
                <ExternalLink className="w-3 h-3 shrink-0" />
                <span className="truncate">{c.label || `Source ${i + 1}`}</span>
              </a>
            ))}
          </div>
        )}

        {(showCopy || showRegenerate || showReactions) && (
          <div className="mt-1.5 flex items-center gap-1">
            {showCopy && (
              <button
                onClick={handleCopy}
                title={copied ? 'Copied!' : 'Copy'}
                className={`p-1 transition-colors ${copied ? 'text-green-400' : actionBtn}`}
              >
                {copied ? <Check className="w-3.5 h-3.5" /> : <Copy className="w-3.5 h-3.5" />}
              </button>
            )}
            {showRegenerate && (
              <button
                onClick={() => regenerateLastAssistantMessage()}
                title="Regenerate response"
                className={`p-1 transition-colors ${actionBtn}`}
              >
                <RotateCw className="w-3.5 h-3.5" />
              </button>
            )}
            {showReactions && (
              <>
                <button
                  onClick={() => handleReaction(1)}
                  title="Good answer"
                  className={`p-1 transition-colors ${reactionSent === 1 ? 'text-green-400' : 'text-gray-500 hover:text-green-400'}`}
                >
                  <ThumbsUp className="w-3.5 h-3.5" />
                </button>
                <button
                  onClick={() => handleReaction(-1)}
                  title="Poor answer"
                  className={`p-1 transition-colors ${reactionSent === -1 ? 'text-red-400' : 'text-gray-500 hover:text-red-400'}`}
                >
                  <ThumbsDown className="w-3.5 h-3.5" />
                </button>
              </>
            )}
          </div>
        )}

        <div className={`text-[10px] mt-1 ${isLight ? 'text-gray-400' : 'text-gray-500'} ${isUser ? 'text-right' : ''}`}>
          {new Date(message.timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}
        </div>
      </div>
    </div>
  )
}
