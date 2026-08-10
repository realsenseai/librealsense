// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
//
// Client for the hosted RealSense AI Assistant (general product Q&A, anonymous, no API key).
// See INTEGRATION.md / API.md: POST /api/chat/stream returns a text/event-stream of typed frames.
// The assistant's CORS allowlist permits any origin, so we call the Front Door endpoint directly.
const ASSISTANT_ENDPOINT = 'https://rs-chat-hnd6gchgesc9fre6.a02.azurefd.net'

function endpointUrl(path: 'chat/stream' | 'reactions' | 'health'): string {
  return `${ASSISTANT_ENDPOINT}/api/${path}`
}

const JSON_HEADERS = { 'Content-Type': 'application/json', 'X-RS-Integration': 'viewer' }

export interface AssistantCitation {
  type: 'url_citation'
  label: string
  url: string
  fileId?: string | null
  containerId?: string | null
  textToReplace?: string
  startIndex?: number
  endIndex?: number
  quote?: string
}

export interface AssistantMessageAttachments {
  imageDataUris?: string[]
  fileDataUris?: AssistantFileAttachment[]
}

export interface AssistantChatMessage {
  id: string
  role: 'user' | 'assistant'
  content: string
  citations?: AssistantCitation[]
  attachments?: AssistantMessageAttachments
  isStreaming?: boolean
  isError?: boolean
  timestamp: number
}

export type AssistantStreamEvent =
  | { type: 'conversationId'; conversationId: string }
  | { type: 'toolUse'; toolName: string }
  | { type: 'chunk'; content: string }
  | { type: 'annotations'; annotations: AssistantCitation[] }
  | { type: 'usage'; [key: string]: unknown }
  | { type: 'mcpApprovalRequest'; [key: string]: unknown }
  | { type: 'done' }
  | { type: 'error'; message: string }

export class AssistantRateLimitError extends Error {
  retryAfterSeconds?: number

  constructor(retryAfterSeconds?: number) {
    super(
      retryAfterSeconds
        ? `Rate limited. Try again in ${retryAfterSeconds} seconds.`
        : 'Rate limited. Please wait a moment before trying again.'
    )
    this.name = 'AssistantRateLimitError'
    this.retryAfterSeconds = retryAfterSeconds
  }
}

export interface AssistantFileAttachment {
  dataUri: string
  fileName: string
  mimeType: string
}

export interface StreamChatOptions {
  message: string
  conversationId?: string | null
  imageDataUris?: string[]
  fileDataUris?: AssistantFileAttachment[]
  signal?: AbortSignal
}

/**
 * Streams a chat turn from the hosted assistant, yielding each parsed SSE event as it arrives.
 * Frames are `data: <json>\n\n`; a frame may straddle two reader chunks, so we buffer until a
 * full "\n\n" separator is seen before parsing.
 */
export async function* streamChatMessage(
  options: StreamChatOptions
): AsyncGenerator<AssistantStreamEvent, void, unknown> {
  const response = await fetch(endpointUrl('chat/stream'), {
    method: 'POST',
    headers: JSON_HEADERS,
    body: JSON.stringify({
      message: options.message,
      conversationId: options.conversationId || undefined,
      imageDataUris: options.imageDataUris?.length ? options.imageDataUris : undefined,
      fileDataUris: options.fileDataUris?.length ? options.fileDataUris : undefined,
    }),
    signal: options.signal,
  })

  if (!response.ok) {
    if (response.status === 429) {
      const retryAfter = response.headers.get('retry-after')
      throw new AssistantRateLimitError(retryAfter ? Number(retryAfter) : undefined)
    }
    if (response.status === 403) {
      throw new Error('Assistant endpoint rejected the request (must call the Front Door URL, not a direct container URL).')
    }
    throw new Error(`Assistant request failed: ${response.status} ${response.statusText}`)
  }
  if (!response.body) {
    throw new Error('Assistant response had no readable body (streaming unsupported in this environment)')
  }

  const reader = response.body.getReader()
  const decoder = new TextDecoder()
  let buffer = ''

  try {
    while (true) {
      const { value, done } = await reader.read()
      if (done) break
      // Normalize CRLF to LF on the whole buffer (not just the new chunk) so a "\r\n\r\n"
      // frame separator straddling two reads still gets caught.
      buffer = (buffer + decoder.decode(value, { stream: true })).replace(/\r\n/g, '\n')

      let separatorIndex = buffer.indexOf('\n\n')
      while (separatorIndex !== -1) {
        const rawFrame = buffer.slice(0, separatorIndex)
        buffer = buffer.slice(separatorIndex + 2)

        const dataLine = rawFrame.split('\n').find((line) => line.startsWith('data:'))
        if (dataLine) {
          const jsonStr = dataLine.slice(5).trim()
          if (jsonStr) {
            try {
              const parsed = JSON.parse(jsonStr) as AssistantStreamEvent
              yield parsed
              if (parsed.type === 'done' || parsed.type === 'error') return
            } catch (error) {
              console.warn('Failed to parse assistant SSE frame:', jsonStr, error)
            }
          }
        }

        separatorIndex = buffer.indexOf('\n\n')
      }
    }
  } finally {
    reader.releaseLock()
  }
}

/** Thumbs up/down feedback on the latest answer in a conversation (conversation-scoped, not per-message). */
export async function sendReaction(conversationId: string, value: 1 | -1 | 0): Promise<void> {
  const response = await fetch(endpointUrl('reactions'), {
    method: 'POST',
    headers: JSON_HEADERS,
    body: JSON.stringify({ conversationId, value }),
  })
  if (!response.ok) {
    throw new Error(`Failed to send reaction: ${response.status}`)
  }
}

/** Cosmetic status check only — never gates the UI (the assistant is anonymous, always-on infra). */
export async function pingAssistantHealth(signal?: AbortSignal): Promise<boolean> {
  try {
    const response = await fetch(endpointUrl('health'), { signal })
    return response.ok
  } catch {
    return false
  }
}

/** Generate a unique message id for locally-created chat messages. */
export function generateAssistantMessageId(): string {
  return `rsai_${Date.now()}_${Math.random().toString(36).slice(2, 9)}`
}
