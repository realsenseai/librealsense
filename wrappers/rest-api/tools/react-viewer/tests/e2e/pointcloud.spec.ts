/**
 * 3D view E2E against a real camera (WP4): depth frames reach the browser and the GPU point
 * cloud renders with its toolbar.
 *
 *   REAL_DEVICE=true npx playwright test --project=real-device tests/e2e/pointcloud.spec.ts
 */

import { test, expect, suppressWhatsNew } from './fixtures'
import type { Locator } from '@playwright/test'

async function expandSensorModules(deviceCard: Locator) {
  const headers = deviceCard.locator('button[aria-expanded]')
  for (let i = 0; i < await headers.count(); i++) {
    const header = headers.nth(i)
    if (await header.getAttribute('aria-expanded') === 'false') await header.click()
  }
}

test.describe('@real-device 3D view', () => {
  test.beforeEach(async ({ testMode, page }) => {
    test.skip(testMode !== 'real', 'Real device tests require REAL_DEVICE=true')
    await suppressWhatsNew(page)
  })

  test('renders the point cloud from depth frames with texture and shading controls', async ({ page, waitForDevice, getDevices }) => {
    test.setTimeout(120000)
    const [device] = await getDevices()
    const errors: string[] = []
    page.on('console', (msg) => { if (msg.type() === 'error') errors.push(msg.text()) })
    page.on('pageerror', (err) => errors.push(err.message))
    await page.goto('/')
    await waitForDevice(page)
    const deviceCard = page.locator('[data-testid="device-card"]').filter({ hasText: device.serial_number })
    await expect(page.locator('[title="Loading..."]')).not.toBeVisible({ timeout: 30000 })
    await expandSensorModules(deviceCard)

    // Depth + color so the cloud can be textured: each sensor module has its own Start
    await deviceCard.locator('[data-testid="toggle-stream-depth"]').first().check()
    const color = deviceCard.locator('[data-testid="toggle-stream-color"]').first()
    const hasColor = (await color.count()) > 0
    if (hasColor) await color.check()
    const starts = deviceCard.locator('button:has-text("Start")')
    const startCount = await starts.count()
    for (let i = 0; i < startCount; i++) {
      await deviceCard.locator('button:has-text("Start")').first().click()
      await page.waitForTimeout(1500)
    }
    await expect(page.locator('video').first()).toBeVisible({ timeout: 15000 })

    await page.getByRole('button', { name: '3D View' }).click()
    const toolbar = page.getByTestId('3d-toolbar')
    await expect(toolbar).toBeVisible()
    // A frame and its geometry arrived: the canvas is up and export is possible
    await expect(page.locator('canvas').first()).toBeVisible({ timeout: 20000 })
    await expect(toolbar.getByRole('button', { name: 'Export PLY' })).toBeEnabled({ timeout: 20000 })
    await expect(toolbar.getByLabel('Depth source')).toContainText(device.serial_number)

    // Texture defaults to color when it streams; shading can be switched
    const texture = toolbar.getByLabel('Texture source')
    if (hasColor) await expect(texture).toHaveValue('color', { timeout: 15000 })
    await toolbar.getByLabel('Shading').selectOption('points')
    await expect(page.locator('canvas').first()).toBeVisible()
    await toolbar.getByLabel('Shading').selectOption('diffuse')
    await texture.selectOption('')
    await expect(page.locator('canvas').first()).toBeVisible()

    // Measurement: two clicks on the cloud draw a ruler with a distance label; Z undoes
    const canvas = page.locator('canvas').first()
    const box = (await canvas.boundingBox())!
    await page.mouse.click(box.x + box.width * 0.45, box.y + box.height * 0.5)
    await page.mouse.click(box.x + box.width * 0.55, box.y + box.height * 0.5)
    await expect(page.getByTestId('ruler-label').first()).toBeVisible({ timeout: 10000 })
    await page.keyboard.press('z')
    await expect(page.getByTestId('ruler-label')).toHaveCount(0)
    await page.getByRole('button', { name: 'Clear ruler' }).click()

    // Server-side PLY export downloads a file
    await toolbar.getByRole('button', { name: 'Export PLY' }).click()
    const download = page.waitForEvent('download', { timeout: 30000 })
    await page.getByTestId('ply-export-menu').getByRole('button', { name: 'Export' }).click()
    expect((await download).suggestedFilename()).toMatch(/\.ply$/)

    await page.getByRole('button', { name: '2D View' }).click()
    while (await deviceCard.locator('button:has-text("Stop")').count()) {
      await deviceCard.locator('button:has-text("Stop")').first().click()
      await page.waitForTimeout(1000)
    }
    const shaderErrors = errors.filter((e) => /shader|webgl|program/i.test(e))
    expect(shaderErrors, shaderErrors.join(' | ')).toEqual([])
  })
})
