import { defineConfig, devices } from '@playwright/test'

/**
 * E2E Test Configuration for React Viewer
 *
 * Playwright owns the API server: it starts it, waits for /health, and kills it. The
 * server also serves the built viewer (npm run build && npm run bundle), so these tests
 * exercise the artifact that ships rather than a dev server. Which suite runs is chosen
 * per invocation, not here:
 *
 *   npx playwright test --grep-invert @real-device   # no camera needed
 *   npx playwright test --grep @real-device          # needs a camera
 *
 * Environment variables:
 * - API_URL: where to run the server (default: http://localhost:8000)
 * - PYTHON_BIN: interpreter that has the SDK bindings (default: python3)
 * - DEVICE_SERIAL: drive this camera instead of whichever enumerates first
 */

const apiUrl = process.env.API_URL || 'http://localhost:8000'
const python = process.env.PYTHON_BIN || 'python3'

export default defineConfig({
  testDir: './tests/e2e',

  /* Fail the build on CI if you accidentally left test.only in the source code. */
  forbidOnly: !!process.env.CI,

  // One camera: a second worker would fight over it.
  workers: 1,

  timeout: 60000,
  expect: {
    timeout: 15000,
  },

  /* Reporter to use. See https://playwright.dev/docs/test-reporters */
  reporter: [
    ['list'],
    ...(process.env.CI ? [['github'] as ['github']] : []),
  ],

  use: {
    /* Base URL to use in actions like `await page.goto('/')`. */
    baseURL: apiUrl,

    trace: 'retain-on-failure',

    /* Take screenshot on failure */
    screenshot: 'only-on-failure',

    /* Capture video on failure */
    video: 'retain-on-failure',
  },

  // stdout:'pipe' puts the server's log in the same stream as the test output, so a
  // server error lands next to the test it broke instead of in a separate file.
  webServer: {
    command: `"${python}" -m uvicorn main:combined_app --host 127.0.0.1 --port ${new URL(apiUrl).port}`,
    cwd: '../..',
    url: `${apiUrl}/api/v1/health`,
    // The wrapper always sets API_URL, so CI never reuses a server it did not start; a local
    // run without it picks up the one the developer already has on :8000.
    reuseExistingServer: !process.env.API_URL,
    stdout: 'pipe',
    timeout: 120000,
  },

  projects: [
    {
      name: 'chromium',
      use: { ...devices['Desktop Chrome'] },
    },
  ],
})
