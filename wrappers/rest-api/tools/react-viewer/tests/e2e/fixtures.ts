/**
 * E2E Test Fixtures for React Viewer
 *
 * Usage:
 *   import { test, expect } from './fixtures'
 *
 * The server is started by playwright.config.ts; see it for the environment variables.
 */

import { test as base, expect, Page } from '@playwright/test'
import type { DeviceInfo } from '../../src/api/types'

export interface TestFixtures {
  /** The camera under test. */
  device: DeviceInfo
}

export const test = base.extend<TestFixtures>({
  // Keep the welcome modal from opening at all, rather than racing to dismiss it.
  // WhatsNew.tsx reopens whenever the stored version differs from the one /health reports,
  // so the marker has to carry the live value.
  page: async ({ page, baseURL }, use) => {
    const { sdk_version } = await (await fetch(`${baseURL}/api/v1/health`)).json()
    await page.addInitScript((version: string) => {
      localStorage.setItem('rs-sdk-last-shown', version)
    }, sdk_version)
    await use(page)
  },

  // A bench can have several cameras attached and their order is not stable, so without
  // DEVICE_SERIAL the tests would silently run against a camera nobody asked for.
  device: async ({ baseURL }, use) => {
    const devices: DeviceInfo[] = await (await fetch(`${baseURL}/api/v1/devices/`)).json()
    const serial = process.env.DEVICE_SERIAL
    const device = serial ? devices.find(d => d.serial_number === serial) : devices[0]
    expect(device, `DEVICE_SERIAL=${serial ?? '(first available)'} did not enumerate`).toBeTruthy()
    await use(device!)
  },
})

export { expect }

/**
 * Toasts render fixed over the whole app, so a firmware-update prompt can swallow
 * a click meant for the UI underneath.
 */
export async function dismissToasts(page: Page): Promise<void> {
  const closeButtons = page.getByRole('button', { name: 'Close' })
  for (let remaining = await closeButtons.count(); remaining > 0; remaining--) {
    await closeButtons.first().click()
  }
}
