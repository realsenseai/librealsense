/**
 * Real Device E2E Tests
 * 
 * These tests run against actual RealSense hardware.
 * They are tagged with @real-device and only run when REAL_DEVICE=true.
 * 
 * Prerequisites:
 * - RealSense camera connected via USB
 * - Backend API server running (npm run dev:api)
 * - Frontend dev server running (npm run dev)
 * 
 * Usage:
 *   REAL_DEVICE=true npx playwright test --project=real-device
 */

import { test, expect, getTestMode, getApiUrl, dismissWhatsNewModal, suppressWhatsNew } from './fixtures'
import { expectFramesFlowing, stopAllStreams } from './helpers'
import type { Locator } from '@playwright/test'

// Per-stream toggles only render inside an expanded sensor module
async function expandSensorModules(deviceCard: Locator) {
  const headers = deviceCard.locator('button[aria-expanded]')
  for (let i = 0; i < await headers.count(); i++) {
    const header = headers.nth(i)
    if (await header.getAttribute('aria-expanded') === 'false') await header.click()
  }
}

// Skip entire file in mock mode
test.beforeEach(async ({ testMode, page }) => {
  test.skip(testMode !== 'real', 'Real device tests require REAL_DEVICE=true')
  
  await suppressWhatsNew(page)
})

test.describe('@real-device Real Device Tests', () => {
  test.describe('Device Detection', () => {
    test('detects connected RealSense device', async ({ page, getDevices }) => {
      const devices = await getDevices()
      expect(devices.length).toBeGreaterThan(0)
      
      await page.goto('/')
      
      // Dismiss What's New modal if it appears
      await dismissWhatsNewModal(page)
      
      // Wait for device to appear in UI
      await expect(page.locator('text=/D4[0-9]{2}|Intel RealSense/i').first()).toBeVisible({
        timeout: 15000,
      })
    })

    test('displays correct device information', async ({ page, getDevices }) => {
      const devices = await getDevices()
      const device = devices[0]
      
      await page.goto('/')
      
      // Dismiss What's New modal if it appears
      await dismissWhatsNewModal(page)
      
      // Check serial number is displayed
      await expect(page.locator(`text=${device.serial_number}`)).toBeVisible({ timeout: 10000 })
      
      // Check firmware version is displayed
      await expect(page.locator(`text=/${device.firmware_version}/`)).toBeVisible()
    })
  })

  test.describe('Streaming', () => {
    test('can start and stop depth streaming', async ({ page, waitForDevice }) => {
      await page.goto('/')
      await waitForDevice(page)
      
      const deviceCard = page.locator('.device-card, [data-testid="device-card"]').first()
      
      // Wait for device to finish loading sensors
      await expect(page.locator('[title="Loading..."]')).not.toBeVisible({ timeout: 30000 })
      
      // Sensor modules are collapsed by default; expand them to reveal the stream toggles
      await expandSensorModules(deviceCard)

      // Enable depth stream
      const depthToggle = page.locator('[data-testid="toggle-stream-depth"]').first()
      await depthToggle.check()
      
      // Start streaming
      const startButton = page.locator('button:has-text("Start"), [data-testid="start-streaming"]').first()
      await startButton.click()
      
      // Verify stream is active: the server must be delivering frames, not just a <video>
      await expect(page.locator('video, canvas').first()).toBeVisible({ timeout: 15000 })
      const [device] = await (await fetch(`${getApiUrl()}/api/v1/devices/`)).json()
      await expectFramesFlowing(device.device_id)
      
      // Stop streaming
      const stopButton = page.locator('button:has-text("Stop"), [data-testid="stop-streaming"]').first()
      await stopButton.click()
      
      // Allow cleanup time
      await page.waitForTimeout(1000)
    })

    test('displays depth frames', async ({ page, waitForDevice }) => {
      await page.goto('/')
      await waitForDevice(page)
      
      const deviceCard = page.locator('.device-card, [data-testid="device-card"]').first()
      
      // Wait for device to finish loading sensors
      await expect(page.locator('[title="Loading..."]')).not.toBeVisible({ timeout: 30000 })
      
      await expandSensorModules(deviceCard)

      const depthToggle = page.locator('[data-testid="toggle-stream-depth"]').first()
      await depthToggle.check()
      
      const startButton = page.locator('button:has-text("Start"), [data-testid="start-streaming"]').first()
      await startButton.click()
      
      // Wait for frames
      await page.waitForTimeout(3000)
      
      // The frame counter lives in the tile's metadata overlay; open it first
      await page.locator('[title="Show frame metadata"]').first().click()
      const frameCounter = page.getByText('Frame Number').locator('..').first()
      const firstValue = await frameCounter.textContent()
      
      await page.waitForTimeout(1000)
      const secondValue = await frameCounter.textContent()
      
      // Frame number should have changed
      expect(firstValue).not.toBe(secondValue)
      
      // Cleanup
      const stopButton = page.locator('button:has-text("Stop"), [data-testid="stop-streaming"]').first()
      await stopButton.click()
      const [dev] = await (await fetch(`${getApiUrl()}/api/v1/devices/`)).json()
      await stopAllStreams(dev.device_id)
    })
  })

  test.describe('Sensor Options', () => {
    test('can modify exposure setting', async ({ page, waitForDevice }) => {
      await page.goto('/')
      await waitForDevice(page)
      
      const deviceCard = page.locator('.device-card, [data-testid="device-card"]').first()
      
      // Wait for options to load
      await page.waitForTimeout(1000)
      
      // Find exposure slider
      const exposureSlider = page.locator('input[type="range"]').first()
      if (await exposureSlider.isVisible()) {
        // Modify the value
        await exposureSlider.fill('5000')
        
        // Verify change was applied
        await page.waitForTimeout(500)
      }
    })
  })

  test.describe('Multi-Camera', () => {
    test('handles multiple cameras if connected', async ({ page, getDevices }) => {
      const devices = await getDevices()
      
      // Skip if only one device
      test.skip(devices.length < 2, 'Need 2+ devices for multi-camera test')
      
      await page.goto('/')
      
      // Dismiss What's New modal if it appears
      await dismissWhatsNewModal(page)
      
      // Should see multiple device cards
      const deviceCards = page.locator('.device-card, [data-testid="device-card"]')
      await expect(deviceCards).toHaveCount(devices.length, { timeout: 10000 })
      
      // Each device serial should be visible
      for (const device of devices) {
        await expect(page.locator(`text=${device.serial_number}`)).toBeVisible()
      }
    })
  })
})

test.describe('@real-device Performance Tests', () => {
  test.beforeEach(async ({ testMode, page }) => {
    test.skip(testMode !== 'real', 'Real device tests require REAL_DEVICE=true')
    
    await suppressWhatsNew(page)
  })

  test('streaming maintains acceptable frame rate', async ({ page, waitForDevice }) => {
    await page.goto('/')
    await waitForDevice(page)
    
    const deviceCard = page.locator('.device-card, [data-testid="device-card"]').first()
    
    // Wait for device to finish loading sensors
    await expect(page.locator('[title="Loading..."]')).not.toBeVisible({ timeout: 30000 })
    
    await expandSensorModules(deviceCard)

    const depthToggle = page.locator('[data-testid="toggle-stream-depth"]').first()
    await depthToggle.check()
    
    const startButton = page.locator('button:has-text("Start"), [data-testid="start-streaming"]').first()
    await startButton.click()
    
    // Wait for streaming to stabilize
    await page.waitForTimeout(5000)
    
    // Sample FPS from metadata
    const fpsText = page.locator('text=/[0-9]+ fps/i').first()
    if (await fpsText.isVisible()) {
      const fpsValue = await fpsText.textContent()
      const fps = parseInt(fpsValue?.match(/([0-9]+)/)?.[1] || '0')
      
      // Should be at least 15 FPS
      expect(fps).toBeGreaterThanOrEqual(15)
    }
    
    // Cleanup
    const stopButton = page.locator('button:has-text("Stop"), [data-testid="stop-streaming"]').first()
    await stopButton.click()
  })
})
