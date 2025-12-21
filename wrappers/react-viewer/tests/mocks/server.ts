import { setupServer } from 'msw/node'
import { handlers } from './api-handlers'

// This configures a request mocking server with the given request handlers.
export const server = setupServer(...handlers)

// Start server before all tests
server.listen({ onUnhandledRequest: 'warn' })

// Reset handlers after each test
server.resetHandlers()

// Clean up after the tests are finished.
server.close()
