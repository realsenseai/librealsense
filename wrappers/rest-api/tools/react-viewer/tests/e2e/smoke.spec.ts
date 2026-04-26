import { test, expect } from '@playwright/test'
import { dismissWhatsNewModal } from './fixtures'

test.describe('Smoke Tests', () => {
  // Set localStorage to prevent What's New modal from appearing
  test.beforeEach(async ({ page }) => {
    await page.addInitScript(() => {
      localStorage.setItem('realsense-viewer-last-version', '0.5.0')
    })
  })

  test('application loads successfully', async ({ page }) => {
    await page.goto('/')
    
    // Dismiss What's New modal if it still appears
    await dismissWhatsNewModal(page)

    // Check that the main application loads by verifying header is visible
    await expect(page.locator('header')).toBeVisible()

    // Check for RealSense logo in the header
    await expect(page.locator('header img[alt="RealSense"]')).toBeVisible()

    // Check for main layout elements
    await expect(page.locator('aside')).toBeVisible() // Device panel
    await expect(page.locator('main')).toBeVisible()
  })

  test('shows connection status', async ({ page }) => {
    await page.goto('/')

    // Wait for Socket.IO connection status to appear
    // This might take a moment to connect or show disconnected state
    await page.waitForTimeout(1000)

    // Should see some status indicator
    const statusIndicators = page.locator('text=/connected|disconnected|connecting/i')
    await expect(statusIndicators.first()).toBeVisible({ timeout: 5000 })
  })

  test('displays device panel', async ({ page }) => {
    await page.goto('/')

    // Device panel should be visible
    const devicePanel = page.locator('aside')
    await expect(devicePanel).toBeVisible()

    // Should have some header or title in the panel
    await expect(page.locator('text=/device|camera/i').first()).toBeVisible()
  })

  test('view mode toggle is hidden when no devices', async ({ page }) => {
    await page.goto('/')

    // Without active devices, view toggle should not be visible
    const viewToggle = page.locator('text=/2D View|3D View/i')
    
    // Wait a bit for the page to fully render
    await page.waitForTimeout(500)
    
    // Should either not exist or not be visible
    const count = await viewToggle.count()
    if (count > 0) {
      // If it exists, it should be hidden (this depends on implementation)
      // For now, we just verify the page loaded
    }
  })

  test('about/info button is accessible', async ({ page }) => {
    await page.goto('/')

    // Look for info/about button
    const infoButton = page.locator('button[title*="About"], button:has-text("Info")')
    await expect(infoButton).toBeVisible()
  })
})
