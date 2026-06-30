// Chat API client for AI-powered camera configuration
// Supports multiple providers: Groq (free), OpenAI, or any OpenAI-compatible API

import type { DeviceState } from './types'
import { buildSystemPrompt, parseProposedSettings, type ProposedSettings } from '../utils/chatPrompt'

// Provider configuration - Groq is free and the default
type Provider = 'groq' | 'openai' | 'custom'

interface ProviderConfig {
  url: string
  model: string
  modelsEndpoint?: string
}

const PROVIDERS: Record<Provider, ProviderConfig> = {
  groq: {
    url: 'https://api.groq.com/openai/v1/chat/completions',
    model: 'llama-3.3-70b-versatile', // Fast and capable, free tier
    modelsEndpoint: 'https://api.groq.com/openai/v1/models',
  },
  openai: {
    url: 'https://api.openai.com/v1/chat/completions',
    model: 'gpt-4o-mini',
    modelsEndpoint: 'https://api.openai.com/v1/models',
  },
  custom: {
    url: import.meta.env.VITE_LLM_API_URL || '',
    model: import.meta.env.VITE_LLM_MODEL || 'gpt-4o-mini',
  },
}

// Determine which provider to use based on available API keys
function getActiveProvider(): { provider: Provider; apiKey: string } | null {
  // Check for Groq first (free option)
  const groqKey = import.meta.env.VITE_GROQ_API_KEY
  if (groqKey) {
    return { provider: 'groq', apiKey: groqKey }
  }
  
  // Check for OpenAI
  const openaiKey = import.meta.env.VITE_OPENAI_API_KEY
  if (openaiKey) {
    return { provider: 'openai', apiKey: openaiKey }
  }
  
  // Check for custom provider
  const customKey = import.meta.env.VITE_LLM_API_KEY
  const customUrl = import.meta.env.VITE_LLM_API_URL
  if (customKey && customUrl) {
    return { provider: 'custom', apiKey: customKey }
  }
  
  return null
}

export interface ChatMessage {
  id: string
  role: 'user' | 'assistant' | 'system'
  content: string
  proposedSettings?: ProposedSettings
  timestamp: number
}

export interface ChatResponse {
  content: string
  proposedSettings?: ProposedSettings
}

/**
 * Check if the chat API is available (API key configured and endpoint reachable)
 */
export async function checkChatAvailability(): Promise<boolean> {
  const active = getActiveProvider()
  if (!active) {
    console.warn('No chat API key configured. Set VITE_GROQ_API_KEY (free) or VITE_OPENAI_API_KEY')
    return false
  }

  const config = PROVIDERS[active.provider]
  
  try {
    // Simple connectivity check
    const endpoint = config.modelsEndpoint || config.url
    const response = await fetch(endpoint, {
      method: 'GET',
      headers: {
        'Authorization': `Bearer ${active.apiKey}`,
      },
    })
    return response.ok
  } catch (error) {
    console.error('Chat API connectivity check failed:', error)
    return false
  }
}

/**
 * Get the name of the active provider for display
 */
export function getActiveProviderName(): string | null {
  const active = getActiveProvider()
  if (!active) return null
  
  const names: Record<Provider, string> = {
    groq: 'Groq (Llama 3.3)',
    openai: 'OpenAI (GPT-4o mini)',
    custom: 'Custom LLM',
  }
  return names[active.provider]
}

/**
 * Send a chat message and get AI response with optional settings proposal
 */
export async function sendChatMessage(
  messages: ChatMessage[],
  deviceStates: Record<string, DeviceState>
): Promise<ChatResponse> {
  const active = getActiveProvider()
  if (!active) {
    throw new Error('No chat API key configured')
  }

  const config = PROVIDERS[active.provider]
  const systemPrompt = buildSystemPrompt(deviceStates)
  
  const apiMessages = [
    { role: 'system', content: systemPrompt },
    ...messages.map(m => ({ role: m.role, content: m.content }))
  ]

  const response = await fetch(config.url, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Authorization': `Bearer ${active.apiKey}`,
    },
    body: JSON.stringify({
      model: config.model,
      messages: apiMessages,
      temperature: 0.7,
      max_tokens: 2048,
    }),
  })

  if (!response.ok) {
    if (response.status === 429) {
      const retryAfter = response.headers.get('retry-after')
      throw new Error(`Rate limited. ${retryAfter ? `Try again in ${retryAfter} seconds.` : 'Please try again later.'}`)
    }
    throw new Error(`API request failed: ${response.status} ${response.statusText}`)
  }

  const data = await response.json()
  const content = data.choices?.[0]?.message?.content || 'Sorry, I could not generate a response.'

  // Try to parse proposed settings from the response
  const proposedSettings = parseProposedSettings(content)

  return {
    content,
    proposedSettings,
  }
}

/**
 * Generate a unique message ID
 */
export function generateMessageId(): string {
  return `msg_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`
}
