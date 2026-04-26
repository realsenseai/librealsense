import { defineConfig, devices } from '@playwright/test'

/**
 * E2E Test Configuration for React Viewer
 * 
 * Dual-mode testing support:
 * - Mock mode (default): Uses MSW to mock API responses
 * - Real device mode: Tests against actual RealSense devices
 * 
 * Environment variables:
 * - REAL_DEVICE=true: Enable real device testing
 * - DEVICE_SERIAL: Target a specific device by serial number
 * - API_URL: Override the API base URL (default: http://localhost:8000)
 * 
 * Usage:
 *   npm run test:e2e                    # Mock mode (default)
 *   REAL_DEVICE=true npm run test:e2e   # Real device mode
 */

const isRealDeviceMode = process.env.REAL_DEVICE === 'true'
const apiUrl = process.env.API_URL || 'http://localhost:8000'

export default defineConfig({
  testDir: './tests/e2e',
  
  /* Run tests in files in parallel */
  fullyParallel: !isRealDeviceMode, // Serial execution for real devices to avoid conflicts
  
  /* Fail the build on CI if you accidentally left test.only in the source code. */
  forbidOnly: !!process.env.CI,
  
  /* Retry on CI only, more retries for real device tests */
  retries: process.env.CI ? (isRealDeviceMode ? 3 : 2) : 0,
  
  /* Single worker for real device tests to avoid resource conflicts */
  workers: isRealDeviceMode ? 1 : (process.env.CI ? 1 : undefined),
  
  /* Longer timeout for real device operations */
  timeout: isRealDeviceMode ? 60000 : 30000,
  expect: {
    timeout: isRealDeviceMode ? 15000 : 5000,
  },
  
  /* Reporter to use. See https://playwright.dev/docs/test-reporters */
  reporter: [
    ['html'],
    ['list'],
    ...(process.env.CI ? [['github'] as ['github']] : []),
  ],
  
  /* Shared settings for all the projects below. See https://playwright.dev/docs/api/class-testoptions. */
  use: {
    /* Base URL to use in actions like `await page.goto('/')`. */
    baseURL: 'http://localhost:3000',
    
    /* Collect trace when retrying the failed test. See https://playwright.dev/docs/trace-viewer */
    trace: 'on-first-retry',
    
    /* Take screenshot on failure */
    screenshot: 'only-on-failure',
    
    /* Capture video on failure */
    video: 'retain-on-failure',
  },

  /* Configure projects for major browsers */
  projects: [
    {
      name: 'chromium',
      use: { ...devices['Desktop Chrome'] },
    },

    {
      name: 'firefox',
      use: { ...devices['Desktop Firefox'] },
    },

    // Real device project - runs only chromium for consistency
    {
      name: 'real-device',
      use: { 
        ...devices['Desktop Chrome'],
      },
      // Only run tests tagged with @real-device when in real device mode
      grep: /@real-device/,
    },

    // Uncomment for webkit/safari testing
    // {
    //   name: 'webkit',
    //   use: { ...devices['Desktop Safari'] },
    // },
  ],

  /* Run your local dev server before starting the tests */
  webServer: {
    command: 'npm run dev',
    url: 'http://localhost:3000',
    reuseExistingServer: !process.env.CI,
    timeout: 120000,
  },
})
