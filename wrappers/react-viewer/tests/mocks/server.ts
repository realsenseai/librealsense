import { setupServer } from 'msw/node'
import { beforeAll, afterAll, afterEach } from 'vitest'
import { handlers } from './api-handlers'

// This configures a request mocking server with the given request handlers.
export const server = setupServer(...handlers)

// Lifecycle hooks for proper MSW integration
beforeAll(() => {
  // Start server before all tests
  server.listen({ onUnhandledRequest: 'warn' })
})

afterEach(() => {
  // Reset handlers after each test to ensure test isolation
  server.resetHandlers()
})

afterAll(() => {
  // Clean up after the tests are finished
  server.close()
})
