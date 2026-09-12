/**
 * The viewer's own controls against a real camera: the things a user reaches for after the
 * streams are up. Each check asserts the outcome (the overlay is drawn, the server stored the
 * ROI, the tiles kept their places, the 3D canvas has pixels), not that a button exists.
 *
 *   REAL_DEVICE=true npx playwright test --project=real-device tests/e2e/viewer-ui.spec.ts
 */

import { test, expect, suppressWhatsNew, getApiUrl } from './fixtures'
import { cameraCard, expectFramesFlowing, startDepth, startModule, stopAllStreams } from './helpers'

const api = () => `${getApiUrl()}/api/v1`

/** The stream label of every tile, top-left to bottom-right. */
async function tileOrder(page: import('@playwright/test').Page): Promise<string[]> {
  return page.locator('[data-testid="stream-tile"]').evaluateAll((tiles) =>
    tiles.map((t) => t.querySelector('[data-tile-drag-handle]')?.textContent?.trim() ?? '?'))
}

/** Fraction of the 3D canvas that is not background. */
async function canvasLit(page: import('@playwright/test').Page): Promise<number> {
  return page.evaluate(() => {
    const canvas = document.querySelector('canvas') as HTMLCanvasElement | null
    if (!canvas) return 0
    const gl = canvas.getContext('webgl2') ?? canvas.getContext('webgl')
    if (!gl) return 0
    const pixels = new Uint8Array(canvas.width * canvas.height * 4)
    gl.readPixels(0, 0, canvas.width, canvas.height, gl.RGBA, gl.UNSIGNED_BYTE, pixels)
    let lit = 0
    for (let i = 0; i < pixels.length; i += 4) if (pixels[i] > 12 || pixels[i + 1] > 12 || pixels[i + 2] > 12) lit++
    return lit / (pixels.length / 4)
  })
}

test.describe('@real-device Viewer controls', () => {
  test.beforeEach(async ({ testMode, page }) => {
    test.skip(testMode !== 'real', 'Real device tests require REAL_DEVICE=true')
    await suppressWhatsNew(page)
  })

  test('a page that opens while the camera streams shows the streams, in 2D and in 3D', async ({ page, waitForDevice, getDevices }) => {
    test.setTimeout(180000)
    const [device] = await getDevices()
    await page.goto('/')
    await waitForDevice(page)
    const card = cameraCard(page, device.serial_number)
    await expect(page.locator('[title="Loading..."]')).not.toBeVisible({ timeout: 30000 })

    try {
      await startDepth(page, card, device.device_id, true)
      const before = await tileOrder(page)
      expect(before.length).toBeGreaterThan(0)

      // The camera keeps streaming across the reload: the fresh page must adopt it rather
      // than show an idle camera whose tiles never appear and whose depth frames it drops.
      await page.reload()
      await waitForDevice(page)
      await expect(page.locator('[title="Loading..."]')).not.toBeVisible({ timeout: 30000 })
      await expect(page.locator('[data-testid="stream-tile"]').first()).toBeVisible({ timeout: 20000 })
      expect(await tileOrder(page)).toEqual(before)
      await expectFramesFlowing(device.device_id)
      await expect(cameraCard(page, device.serial_number).getByTestId('stop-streaming').first()).toBeVisible()

      await page.getByRole('button', { name: '3D View' }).click()
      await expect(page.locator('canvas').first()).toBeVisible({ timeout: 20000 })
      await expect.poll(() => canvasLit(page), { timeout: 30000, message: 'the 3D view drew nothing' }).toBeGreaterThan(0.01)
      await page.getByRole('button', { name: '2D View' }).click()
    } finally {
      await stopAllStreams(device.device_id)
    }
  })

  test('grid, zoom, ROI and the motion tile all answer the mouse', async ({ page, waitForDevice, getDevices }) => {
    test.setTimeout(180000)
    const [device] = await getDevices()
    await page.goto('/')
    await waitForDevice(page)
    const card = cameraCard(page, device.serial_number)
    await expect(page.locator('[title="Loading..."]')).not.toBeVisible({ timeout: 30000 })

    try {
      await startDepth(page, card, device.device_id)
      const order = await tileOrder(page)
      const depthTile = page.locator('[data-testid="stream-tile"]').filter({ hasText: 'DEPTH' }).first()

      // A press that wanders a few pixels used to start a tile drag and swallow the click.
      const gridButton = depthTile.locator('button[title="Show crosshair/grid overlay"]')
      const gb = (await gridButton.boundingBox())!
      await page.mouse.move(gb.x + gb.width / 2, gb.y + gb.height / 2)
      await page.mouse.down()
      await page.mouse.move(gb.x + gb.width / 2 + 4, gb.y + gb.height / 2 + 3)
      await page.mouse.up()
      await expect(depthTile.getByTestId('grid-overlay')).toBeVisible()
      await gridButton.click()
      await expect(depthTile.getByTestId('grid-overlay')).toHaveCount(0)

      // Zoom is reachable without knowing about the wheel
      const level = depthTile.getByTestId('zoom-level')
      await expect(level).toHaveText('100%')
      await depthTile.getByRole('button', { name: 'Zoom in' }).click()
      await expect(level).toHaveText('110%')
      await expect(depthTile.getByTestId('zoom-preview')).toBeVisible()
      await level.click()
      await expect(level).toHaveText('100%')

      // Drawing an ROI draws a rectangle; it does not rearrange the tiles
      await depthTile.getByRole('button', { name: 'Set auto-exposure ROI' }).click()
      const box = (await depthTile.locator('video').first().boundingBox())!
      await page.mouse.move(box.x + box.width * 0.3, box.y + box.height * 0.35)
      await page.mouse.down()
      await page.mouse.move(box.x + box.width * 0.7, box.y + box.height * 0.65, { steps: 8 })
      await page.mouse.up()
      const roiWidth = async () => {
        const roi = await (await fetch(`${api()}/devices/${device.device_id}/sensors/${device.device_id}-sensor-0/roi`)).json()
        return roi.max_x - roi.min_x
      }
      await expect.poll(roiWidth, { timeout: 10000 }).toBeLessThan(500)
      expect(await tileOrder(page)).toEqual(order)

      // Motion streams read out numbers rather than sitting on "waiting"
      const motionToggle = card.locator('[data-testid="toggle-stream-gyro"]')
      if (await motionToggle.count()) {
        await startModule(card, 'toggle-stream-gyro')
        const gyro = page.locator('[data-testid="stream-tile"]').filter({ hasText: 'GYRO' }).first()
        await expect(gyro.getByText(/‖ω‖/)).toBeVisible({ timeout: 20000 })
        await expect(gyro.getByText('Waiting for data…')).toHaveCount(0)
        // Every tile gets the same height: a motion readout must not squash the video tiles
        const heights = await page.locator('[data-testid="stream-tile"]').evaluateAll((tiles) =>
          tiles.map((t) => Math.round(t.getBoundingClientRect().height)))
        expect(Math.max(...heights) - Math.min(...heights)).toBeLessThanOrEqual(2)
      }
    } finally {
      await stopAllStreams(device.device_id)
    }
  })

  test('the recording pane records a clip and says what it is doing', async ({ page, waitForDevice, getDevices }) => {
    test.setTimeout(180000)
    const [device] = await getDevices()
    await page.goto('/')
    await waitForDevice(page)
    const card = cameraCard(page, device.serial_number)
    await expect(page.locator('[title="Loading..."]')).not.toBeVisible({ timeout: 30000 })

    try {
      await expect(card.getByTestId('record-start')).toBeDisabled() // nothing streams yet
      await startDepth(page, card, device.device_id)
      await expect(card.getByTestId('record-start')).toBeEnabled()

      await card.getByTestId('record-start').click()
      await expect(card.getByTestId('record-indicator')).toContainText('Recording', { timeout: 15000 })
      await expect(card.getByTestId('recording-pane')).toContainText('Writing to:')
      await page.waitForTimeout(3000)
      await card.getByTestId('record-stop').click()

      await expect(card.getByTestId('record-start')).toBeVisible({ timeout: 15000 })
      // The pane keeps naming the file it just wrote, and the server has it on disk
      await expect(card.getByTestId('recording-pane')).toContainText('Last file:')
      const written = (await card.getByTestId('recording-pane').innerText()).split('Last file:')[1].trim()
      const files = await (await fetch(`${api()}/playback/files`)).json()
      expect(files.map((f: { name: string }) => f.name)).toContain(written)
    } finally {
      await stopAllStreams(device.device_id)
    }
  })
})
