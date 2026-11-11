import '@testing-library/jest-dom'
import { cleanup } from '@testing-library/react'
import { afterEach, beforeAll, afterAll, vi } from 'vitest'

// Import MSW server - this sets up the lifecycle hooks automatically
import '../mocks/server'

// Cleanup after each test
afterEach(() => {
  cleanup()
})

// jsdom has no backend to open the events socket to
vi.mock('@/api/events', () => ({ connectEvents: () => {}, onEvent: () => () => {} }))

// jsdom has no Worker or WebGL2: tiles receive no frames unless a test mocks this itself
vi.mock('@/api/frames', () => ({ openFrameStream: () => () => {} }))

beforeAll(() => {
  // Mock IntersectionObserver
  global.IntersectionObserver = vi.fn().mockImplementation(() => ({
    observe: vi.fn(),
    unobserve: vi.fn(),
    disconnect: vi.fn(),
  })) as any

  // Mock ResizeObserver
  global.ResizeObserver = vi.fn().mockImplementation(() => ({
    observe: vi.fn(),
    unobserve: vi.fn(),
    disconnect: vi.fn(),
  })) as any
})

afterAll(() => {
  vi.restoreAllMocks()
})
