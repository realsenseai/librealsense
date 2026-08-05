// RealSense AI Assistant state (hosted, anonymous product Q&A — separate from the legacy
// device-config chatbot slice that lives inline in index.ts).
import type { StateCreator } from 'zustand'
import {
  streamChatMessage,
  sendReaction as sendAssistantReactionApi,
  pingAssistantHealth as pingAssistantHealthApi,
  generateAssistantMessageId,
  AssistantRateLimitError,
  type AssistantChatMessage,
  type AssistantFileAttachment,
} from '../api/assistantChat'
import type { AppState } from './index'

// Tracks the in-flight assistant request so `stopAssistantMessage` can abort it.
let currentAssistantAbortController: AbortController | null = null

const ASSISTANT_CONVERSATION_ID_STORAGE_KEY = 'rsai_conversation_id'

function loadPersistedAssistantConversationId(): string | null {
  try {
    return localStorage.getItem(ASSISTANT_CONVERSATION_ID_STORAGE_KEY)
  } catch {
    return null
  }
}

function persistAssistantConversationId(id: string | null) {
  try {
    if (id) localStorage.setItem(ASSISTANT_CONVERSATION_ID_STORAGE_KEY, id)
    else localStorage.removeItem(ASSISTANT_CONVERSATION_ID_STORAGE_KEY)
  } catch {
    // localStorage may be unavailable — conversation just won't survive a reload
  }
}

const ASSISTANT_THEME_STORAGE_KEY = 'rsai_theme'

function loadPersistedAssistantTheme(): 'light' | 'dark' {
  try {
    return localStorage.getItem(ASSISTANT_THEME_STORAGE_KEY) === 'light' ? 'light' : 'dark'
  } catch {
    return 'dark'
  }
}

function persistAssistantTheme(theme: 'light' | 'dark') {
  try {
    localStorage.setItem(ASSISTANT_THEME_STORAGE_KEY, theme)
  } catch {
    // non-fatal — theme just won't survive a reload
  }
}

type SetAppState = (partial: Partial<AppState> | ((state: AppState) => Partial<AppState>)) => void
type GetAppState = () => AppState

/**
 * Shared streaming logic for both `sendAssistantMessage` and `regenerateLastAssistantMessage` —
 * the only difference between the two callers is whether a new user message was pushed first;
 * this just drives the SSE loop against an existing placeholder assistant message.
 */
async function streamAssistantTurn(
  set: SetAppState,
  get: GetAppState,
  assistantMessageId: string,
  content: string,
  attachments?: { imageDataUris?: string[]; fileDataUris?: AssistantFileAttachment[] }
) {
  const controller = new AbortController()
  currentAssistantAbortController = controller

  const patchAssistantMessage = (patch: Partial<AssistantChatMessage>) => {
    set((s) => ({
      assistantMessages: s.assistantMessages.map((m) =>
        m.id === assistantMessageId ? { ...m, ...patch } : m
      ),
    }))
  }

  try {
    for await (const evt of streamChatMessage({
      message: content,
      conversationId: get().assistantConversationId,
      imageDataUris: attachments?.imageDataUris,
      fileDataUris: attachments?.fileDataUris,
      signal: controller.signal,
    })) {
      switch (evt.type) {
        case 'conversationId':
          persistAssistantConversationId(evt.conversationId)
          set({ assistantConversationId: evt.conversationId })
          break
        case 'chunk':
          set((s) => ({
            assistantMessages: s.assistantMessages.map((m) =>
              m.id === assistantMessageId ? { ...m, content: m.content + evt.content } : m
            ),
          }))
          break
        case 'annotations':
          set((s) => ({
            assistantMessages: s.assistantMessages.map((m) =>
              m.id === assistantMessageId
                ? { ...m, citations: [...(m.citations || []), ...evt.annotations] }
                : m
            ),
          }))
          break
        case 'error':
          patchAssistantMessage({ content: evt.message, isStreaming: false, isError: true })
          break
        case 'done':
          patchAssistantMessage({ isStreaming: false })
          break
        case 'toolUse':
        case 'usage':
        case 'mcpApprovalRequest':
          // Not surfaced in the UI yet.
          break
      }
    }
  } catch (error) {
    if (error instanceof DOMException && error.name === 'AbortError') {
      // User-initiated stop — finalize whatever streamed so far, not an error state.
      patchAssistantMessage({ isStreaming: false })
    } else {
      const message =
        error instanceof AssistantRateLimitError
          ? error.message
          : `Sorry, I encountered an error: ${error instanceof Error ? error.message : 'Unknown error'}. Please try again.`
      patchAssistantMessage({ content: message, isStreaming: false, isError: true })
    }
  } finally {
    if (currentAssistantAbortController === controller) currentAssistantAbortController = null
    set({ isAssistantLoading: false })
  }
}

export type AssistantSlice = Pick<AppState,
  | 'isAssistantOpen'
  | 'isAssistantOnline'
  | 'isAssistantLoading'
  | 'assistantMessages'
  | 'assistantConversationId'
  | 'assistantTheme'
  | 'assistantSize'
  | 'toggleAssistant'
  | 'pingAssistantHealth'
  | 'sendAssistantMessage'
  | 'regenerateLastAssistantMessage'
  | 'stopAssistantMessage'
  | 'sendAssistantReaction'
  | 'clearAssistantChat'
  | 'toggleAssistantTheme'
  | 'toggleAssistantSize'
>

export const createAssistantSlice: StateCreator<AppState, [], [], AssistantSlice> = (set, get) => ({
  isAssistantOpen: false,
  isAssistantOnline: true, // optimistic default; corrected by pingAssistantHealth()
  isAssistantLoading: false,
  assistantMessages: [],
  assistantConversationId: loadPersistedAssistantConversationId(),
  assistantTheme: loadPersistedAssistantTheme(),
  assistantSize: 'compact',

  toggleAssistant: () => set((state) => ({ isAssistantOpen: !state.isAssistantOpen })),

  toggleAssistantTheme: () =>
    set((state) => {
      const next = state.assistantTheme === 'light' ? 'dark' : 'light'
      persistAssistantTheme(next)
      return { assistantTheme: next }
    }),

  toggleAssistantSize: () =>
    set((state) => ({ assistantSize: state.assistantSize === 'wide' ? 'compact' : 'wide' })),

  pingAssistantHealth: async () => {
    const online = await pingAssistantHealthApi()
    set({ isAssistantOnline: online })
  },

  sendAssistantMessage: async (content, attachments) => {
    const userMessage: AssistantChatMessage = {
      id: generateAssistantMessageId(),
      role: 'user',
      content,
      attachments: (attachments?.imageDataUris?.length || attachments?.fileDataUris?.length) ? attachments : undefined,
      timestamp: Date.now(),
    }
    const assistantMessageId = generateAssistantMessageId()
    const placeholderMessage: AssistantChatMessage = {
      id: assistantMessageId,
      role: 'assistant',
      content: '',
      isStreaming: true,
      timestamp: Date.now(),
    }

    set((s) => ({
      assistantMessages: [...s.assistantMessages, userMessage, placeholderMessage],
      isAssistantLoading: true,
    }))

    await streamAssistantTurn(set, get, assistantMessageId, content, attachments)
  },

  regenerateLastAssistantMessage: async () => {
    const lastUserMessage = [...get().assistantMessages].reverse().find((m) => m.role === 'user')
    if (!lastUserMessage) return

    const assistantMessageId = generateAssistantMessageId()
    const placeholderMessage: AssistantChatMessage = {
      id: assistantMessageId,
      role: 'assistant',
      content: '',
      isStreaming: true,
      timestamp: Date.now(),
    }

    set((s) => {
      const messages = [...s.assistantMessages]
      if (messages[messages.length - 1]?.role === 'assistant') messages.pop()
      return { assistantMessages: [...messages, placeholderMessage], isAssistantLoading: true }
    })

    await streamAssistantTurn(set, get, assistantMessageId, lastUserMessage.content)
  },

  stopAssistantMessage: () => {
    currentAssistantAbortController?.abort()
  },

  sendAssistantReaction: async (value: 1 | -1 | 0) => {
    const { assistantConversationId } = get()
    if (!assistantConversationId) return
    try {
      await sendAssistantReactionApi(assistantConversationId, value)
    } catch (error) {
      console.warn('Failed to send assistant reaction:', error)
    }
  },

  clearAssistantChat: () => {
    persistAssistantConversationId(null)
    set({
      assistantMessages: [],
      assistantConversationId: null,
    })
  },
})
