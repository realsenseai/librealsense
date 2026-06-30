/**
 * E2E Test Fixtures for React Viewer
 * 
 * Provides test fixtures for both mock and real device testing modes.
 * 
 * Usage:
 *   import { test, expect } from './fixtures'
 * 
 * Environment:
 *   REAL_DEVICE=true - Use real device instead of mocks
 *   DEVICE_SERIAL=xxx - Target specific device serial number
 */

import { test as base, expect, Page } from '@playwright/test'

/**
 * Test mode - mock (default) or real device
 */
export type TestMode = 'mock' | 'real'

/**
 * Device info from the API
 */
export interface TestDevice {
  device_id: string
  name: string
  serial_number: string
  firmware_version: string
}

/**
 * Custom test fixtures for RealSense viewer testing
 */
export interface TestFixtures {
  /** Current test mode */
  testMode: TestMode
  
  /** Wait for device to appear in the UI */
  waitForDevice: (page: Page, options?: { timeout?: number }) => Promise<void>
  
  /** Get list of connected devices (real mode only) */
  getDevices: () => Promise<TestDevice[]>
  
  /** Check if a specific device is connected */
  isDeviceConnected: (serial_number?: string) => Promise<boolean>
  
  /** Start streaming for a device */
  startStreaming: (page: Page, deviceId?: string) => Promise<void>
  
  /** Stop streaming for a device */
  stopStreaming: (page: Page, deviceId?: string) => Promise<void>
}

/**
 * Determine test mode from environment
 */
export const getTestMode = (): TestMode => {
  return process.env.REAL_DEVICE === 'true' ? 'real' : 'mock'
}

/**
 * API base URL for real device tests
 */
export const getApiUrl = (): string => {
  return process.env.API_URL || 'http://localhost:8000'
}

/**
 * Internal helper to dismiss "What's New" modal - used by fixtures
 */
async function dismissWhatsNewModalInternal(page: Page): Promise<void> {
  try {
    // Look for the "Get Started" button in the What's New modal
    const getStartedButton = page.locator('button:has-text("Get Started")')
    
    // Wait briefly for the modal to appear (it may not always show)
    await getStartedButton.waitFor({ state: 'visible', timeout: 2000 })
    await getStartedButton.click()
    
    // Wait for modal to close
    await page.waitForTimeout(300)
  } catch {
    // Modal didn't appear, which is fine - continue with test
  }
}

/**
 * Extended test with custom fixtures
 */
export const test = base.extend<TestFixtures>({
  testMode: async ({}, use) => {
    await use(getTestMode())
  },

  waitForDevice: async ({}, use) => {
    const waitFn = async (page: Page, options?: { timeout?: number }) => {
      const timeout = options?.timeout || 10000
      
      // First, dismiss What's New modal if it appears
      await dismissWhatsNewModalInternal(page)
      
      // Wait for device card to appear
      await page.waitForSelector('[data-testid="device-card"], .device-card', {
        timeout,
        state: 'visible',
      }).catch(() => {
        // Fallback: look for device name pattern
        return page.waitForSelector('text=/D4[0-9]{2}|Intel RealSense/i', { timeout })
      })
    }
    await use(waitFn)
  },

  getDevices: async ({}, use) => {
    const getFn = async (): Promise<TestDevice[]> => {
      if (getTestMode() === 'mock') {
        // Return mock devices in mock mode
        return [{
          device_id: 'mock-device-1',
          name: 'Intel RealSense D435',
          serial_number: '123456789',
          firmware_version: '5.16.0.1',
        }]
      }

      // Fetch real devices from API
      const response = await fetch(`${getApiUrl()}/api/v1/devices/`)
      if (!response.ok) {
        throw new Error(`Failed to fetch devices: ${response.status}`)
      }
      return response.json()
    }
    await use(getFn)
  },

  isDeviceConnected: async ({}, use) => {
    const checkFn = async (serial_number?: string): Promise<boolean> => {
      if (getTestMode() === 'mock') {
        return true // Mock mode always has devices
      }

      try {
        const response = await fetch(`${getApiUrl()}/api/v1/devices/`)
        if (!response.ok) return false
        
        const devices: TestDevice[] = await response.json()
        if (serial_number) {
          return devices.some(d => d.serial_number === serial_number)
        }
        return devices.length > 0
      } catch {
        return false
      }
    }
    await use(checkFn)
  },

  startStreaming: async ({ page }, use) => {
    const startFn = async (p: Page, deviceId?: string) => {
      // Click the start streaming button for the device
      const streamButton = p.locator('[data-testid="start-streaming-button"], button:has-text("Start")').first()
      await streamButton.click()
      
      // Wait for streaming to start
      await p.waitForSelector('video, [data-testid="stream-tile"]', { timeout: 10000 })
    }
    await use(startFn)
  },

  stopStreaming: async ({ page }, use) => {
    const stopFn = async (p: Page, deviceId?: string) => {
      // Click the stop streaming button
      const stopButton = p.locator('[data-testid="stop-streaming-button"], button:has-text("Stop")').first()
      if (await stopButton.isVisible()) {
        await stopButton.click()
      }
    }
    await use(stopFn)
  },
})

// Re-export expect
export { expect }

/**
 * Dismiss the "What's New" modal if it appears
 * This modal shows on first visit or after app version updates
 */
export async function dismissWhatsNewModal(page: Page): Promise<void> {
  return dismissWhatsNewModalInternal(page)
}

/**
 * Helper to skip test if real device is not available
 */
export async function skipIfNoRealDevice(testFn: typeof test) {
  const mode = getTestMode()
  if (mode === 'real') {
    try {
      const response = await fetch(`${getApiUrl()}/api/v1/devices/`)
      if (!response.ok || (await response.json()).length === 0) {
        testFn.skip(true, 'No real device connected')
      }
    } catch {
      testFn.skip(true, 'Cannot connect to API server')
    }
  }
}
