/**
 * Real Device E2E Tests
 *
 * Need a camera, and the viewer built and bundled so the server serves it
 * (npm run build && npm run bundle). Playwright starts the server; see playwright.config.ts.
 *
 *   npx playwright test --grep @real-device
 */

import { test, expect, dismissToasts } from './fixtures'
import type { Locator, Page } from '@playwright/test'
import type { DeviceInfo, SensorInfo } from '../../src/api/types'

// Start/stop is rendered once per sensor module, so it has to be scoped to the module
// holding the depth toggle - .first() can land on the colour sensor's disabled button.
function depthSensorModule(page: Page): Locator {
  return page.locator('[data-testid="sensor-module"]')
    .filter({ has: page.locator('[data-testid="toggle-stream-depth"]') })
}

/**
 * Opens the card for the camera under test, selected by serial rather than by position,
 * and expands its sensor modules - stream toggles and controls only render inside one.
 */
async function openDeviceCard(page: Page, device: DeviceInfo): Promise<void> {
  const deviceCard = page.locator('[data-testid="device-card"]')
    .filter({ hasText: device.serial_number })
  // Drive the toggle, not the card: an active card has no click handler, so clicking it
  // delivers the click to whatever control sits at its centre.
  const toggle = deviceCard.locator('button[title$="device"]')
  if (await toggle.getAttribute('title') === 'Activate device') await toggle.click()
  await expect(deviceCard.locator('[title="Deactivate device"]')).toBeVisible()
  await expect(page.locator('[title="Loading..."]')).toHaveCount(0, { timeout: 15000 })
  await dismissToasts(page)

  // Only the sensor headers: an unscoped button[aria-expanded] also matches the control
  // sections and groups nested inside them, and opening one renders more of them.
  const closed = deviceCard.locator('[data-testid="sensor-module"] > div > button[aria-expanded="false"]')
  while (await closed.count()) await closed.first().click()
}

async function startDepthStream(page: Page, device: DeviceInfo): Promise<void> {
  await openDeviceCard(page, device)
  await page.locator('[data-testid="toggle-stream-depth"]').first().check()

  const startButton = depthSensorModule(page).locator('[data-testid="start-streaming"]')
  await expect(startButton).toBeEnabled()
  await startButton.click()
  await expect(page.locator('video.stream-video').first()).toBeVisible({ timeout: 20000 })
}

async function stopDepthStream(page: Page): Promise<void> {
  const stopButton = depthSensorModule(page).locator('[data-testid="stop-streaming"]')
  await stopButton.click()
  await expect(stopButton).toHaveCount(0, { timeout: 15000 })
}

/** Opens a sensor's controls and filters them, so the wanted option is on screen. */
async function openControls(page: Page, device: DeviceInfo, sensorName: string, query: string): Promise<Locator> {
  await openDeviceCard(page, device)
  // Scope to the module for the sensor the API gave us, not whichever renders first.
  const sensorModule = page.locator('[data-testid="sensor-module"]').filter({ hasText: sensorName })
  // A query force-opens the control sections, which are collapsed by default.
  await sensorModule.getByPlaceholder('Search controls').fill(query)
  return sensorModule
}

// A failed test can leave the camera streaming server-side with no Stop button rendered,
// and the next test then cannot start it. Stop via the API, which covers that case too.
test.afterEach(async ({ device, baseURL }) => {
  const sensorsUrl = `${baseURL}/api/v1/devices/${device.device_id}/sensors/`
  const sensors: SensorInfo[] = await (await fetch(sensorsUrl)).json()
  for (const sensor of sensors) {
    await fetch(`${sensorsUrl}${sensor.sensor_id}/stop`, { method: 'POST' })
  }
})

test.describe('@real-device Real Device Tests', () => {
  test('displays correct device information', async ({ page, device }) => {
    await page.goto('/')

    // Generous: the card only renders once the socket is up, measured at ~16s on a cold start
    const card = page.locator('[data-testid="device-card"]').filter({ hasText: device.serial_number })
    await expect(card).toBeVisible({ timeout: 40000 })
    await expect(card).toContainText(device.firmware_version!)
  })

  test('displays depth frames', async ({ page, device }) => {
    await page.goto('/')

    await startDepthStream(page, device)

    // The frame number lives only in the metadata overlay, behind its toggle
    const tile = page.locator('video.stream-video').first().locator('..')
    await tile.locator('[data-testid="toggle-metadata"]').click({ timeout: 20000 })

    // Frame number must keep climbing, and the browser must really decode the frames
    const frameNumber = tile.locator('[data-testid="metadata-frame-number"]')
    const first = Number(await frameNumber.textContent())
    await expect
      .poll(async () => Number(await frameNumber.textContent()), { timeout: 15000 })
      .toBeGreaterThan(first)

    // The tile renders before WebRTC has negotiated, so this has to be polled
    await expect
      .poll(() => page.locator('video.stream-video').first()
        .evaluate((v: HTMLVideoElement) => v.getVideoPlaybackQuality().totalVideoFrames))
      .toBeGreaterThan(0)

    await stopDepthStream(page)
  })

  test('renders a point cloud in 3D', async ({ page, device }) => {
    await page.goto('/')
    await startDepthStream(page, device)

    const video = page.locator('video.stream-video').first()
    const activated = page.waitForResponse(r => r.url().includes('/point_cloud/activate'))
    await page.getByRole('button', { name: '3D View' }).click()
    expect((await activated).ok()).toBe(true)
    await expect(video).toBeHidden()

    // Export PLY enables only once vertices reach the store, so it stands for the whole
    // path: SDK point cloud -> socket -> viewer. The canvas itself cannot be read back,
    // as three.js renders without preserveDrawingBuffer.
    await expect(page.getByRole('button', { name: 'Export PLY' })).toBeEnabled({ timeout: 20000 })
    await expect(page.locator('canvas')).toBeVisible()

    await page.getByRole('button', { name: '2D View' }).click()
    await expect(video).toBeVisible()

    await stopDepthStream(page)
  })

  // A stream not torn down cleanly leaves the device unable to restart.
  test('can restart a stream after stopping it', async ({ page, device }) => {
    await page.goto('/')

    for (let attempt = 1; attempt <= 2; attempt++) {
      await startDepthStream(page, device)
      await stopDepthStream(page)
      await expect(page.locator('video.stream-video')).toHaveCount(0)
    }
  })

  test('can modify exposure setting', async ({ page, device, baseURL }) => {
    await page.goto('/')

    const sensorsUrl = `${baseURL}/api/v1/devices/${device.device_id}/sensors/`
    const sensors = await (await fetch(sensorsUrl)).json()
    const sensor = sensors.find((s: SensorInfo) => s.options.some(o => o.option_id === 'exposure'))
    expect(sensor, 'no sensor exposes an exposure control').toBeTruthy()
    const exposure = sensor!.options.find(o => o.option_id === 'exposure')!
    const target = Math.min(exposure.max_value, Math.round(Number(exposure.default_value) * 2))

    const sensorModule = await openControls(page, device, sensor!.name, 'exposure')

    // No need to switch auto-exposure off first: the SDK registers exposure as an
    // auto_disabling_control on both depth and colour, so writing it turns AE off.
    const slider = sensorModule.locator('[data-testid="option-exposure"] input[type="range"]')
    await expect(slider).toBeVisible()
    await slider.fill(String(target))
    await slider.dispatchEvent('mouseup')   // the PUT is only sent on mouseup

    // BE + SDK took it...
    await expect
      .poll(async () => {
        const url = `${sensorsUrl}${sensor!.sensor_id}/options/exposure/`
        return (await (await fetch(url)).json()).current_value
      }, { timeout: 10000 })
      .toBe(target)

    // ...and the FE shows it after a reload, i.e. read back from the server rather than
    // left over from the fill. Asserting the slider before this only re-reads our typing.
    await page.reload()
    const reopened = await openControls(page, device, sensor!.name, 'exposure')
    await expect(reopened.locator('[data-testid="option-exposure"] input[type="range"]'))
      .toHaveValue(String(target))
  })
})
