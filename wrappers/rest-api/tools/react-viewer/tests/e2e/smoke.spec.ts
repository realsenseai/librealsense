import { test, expect } from './fixtures'

/**
 * No camera required. These run on GHA for every PR, where the real-device suite cannot:
 * they prove the bundle the server serves actually boots in a browser and reaches the
 * backend. The camera suite re-proves both, but only on a bench.
 */
test.describe('Smoke Tests', () => {
  test('application loads successfully', async ({ page }) => {
    await page.goto('/')

    await expect(page.locator('header')).toBeVisible()
    await expect(page.locator('header img[alt="RealSense"]')).toBeVisible()
    await expect(page.locator('aside')).toBeVisible() // Device panel
    await expect(page.locator('main')).toBeVisible()
  })

  test('reaches the backend over the socket', async ({ page }) => {
    await page.goto('/')

    // App.tsx shows the connecting splash until the Socket.IO handshake completes
    await expect(page.getByText(/^● Connected$/)).toBeVisible()
  })
})
