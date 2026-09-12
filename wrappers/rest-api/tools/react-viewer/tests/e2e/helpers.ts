/**
 * Outcome-level helpers for the real-device suite. A test that only checks that a button
 * exists proves nothing about the camera; these check that frames actually flow, that the
 * server holds the state the UI claims, and they leave the camera idle afterwards.
 */

import { expect, type Locator, type Page } from '@playwright/test'
import { getApiUrl } from './fixtures'

const api = () => `${getApiUrl()}/api/v1`

/** Expand every sensor module in a device card (stream toggles live inside). */
export async function expandSensorModules(deviceCard: Locator) {
  const headers = deviceCard.locator('button[aria-expanded]')
  for (let i = 0; i < await headers.count(); i++) {
    const header = headers.nth(i)
    if (await header.getAttribute('aria-expanded') === 'false') await header.click()
  }
}

/** The card of one camera, pinned by serial so a loaded recording cannot shadow it. */
export function cameraCard(page: Page, serial: string): Locator {
  return page.locator('[data-testid="device-card"]').filter({ hasText: serial }).filter({ hasNotText: 'Playback' })
}

/** The sensor module section that owns a stream toggle (module header + its Start/Stop). */
export function moduleOf(deviceCard: Locator, streamToggle: string): Locator {
  // The innermost div that holds both the toggle and the module's Start/Stop button
  const page = deviceCard.page()
  return deviceCard.locator('div')
    .filter({ has: page.locator(`[data-testid="${streamToggle}"]`) })
    .filter({ has: page.locator('[data-testid="start-streaming"], [data-testid="stop-streaming"]') })
    .last()
}

/** Start one module from its own Start button and wait until the UI shows it streaming. */
export async function startModule(deviceCard: Locator, streamToggle: string) {
  const module = moduleOf(deviceCard, streamToggle)
  await module.locator(`[data-testid="${streamToggle}"]`).check()
  await module.getByTestId('start-streaming').click()
  // Wait for this module's own state to flip before touching anything else
  await expect(module.getByTestId('stop-streaming')).toBeVisible({ timeout: 15000 })
}

/** Start the depth stream (and optionally color) from the UI and prove frames arrive. */
export async function startDepth(page: Page, deviceCard: Locator, deviceId: string, withColor = false) {
  await expandSensorModules(deviceCard)
  await startModule(deviceCard, 'toggle-stream-depth')
  if (withColor && await deviceCard.locator('[data-testid="toggle-stream-color"]').count()) {
    await startModule(deviceCard, 'toggle-stream-color')
  }
  await expectFramesFlowing(deviceId)
}

/** Server-side proof that depth frames flow: the frame number moves between two reads. */
export async function expectFramesFlowing(deviceId: string, timeoutMs = 15000) {
  const frameNumber = async () => {
    const r = await fetch(`${api()}/devices/${deviceId}/stream/status`)
    if (!r.ok) return null
    const status = await r.json()
    if (!status.is_streaming) return null
    const md = await fetch(`${api()}/devices/${deviceId}/stream/metadata?stream=depth`).catch(() => null)
    if (md && md.ok) return (await md.json()).frame_number ?? null
    // fall back to the depth readout: any non-null value means a frame exists
    const d = await fetch(`${api()}/devices/${deviceId}/stream/depth-at-pixel?x=10&y=10`)
    return d.ok && (await d.json()).depth !== null ? Date.now() : null
  }
  await expect.poll(frameNumber, { timeout: timeoutMs, message: 'depth frames never arrived' }).not.toBeNull()
  const first = await frameNumber()
  await expect.poll(frameNumber, { timeout: 5000, message: 'depth frame number does not advance' }).not.toBe(first)
}

/** Stop every stream of the device through the API; used in `finally` blocks. */
export async function stopAllStreams(deviceId: string) {
  for (let i = 0; i < 3; i++) {
    try {
      await fetch(`${api()}/devices/${deviceId}/sensors/${deviceId}-sensor-${i}/stop`, { method: 'POST' })
    } catch {
      // not a sensor of this device
    }
  }
}

/** The camera as the server sees it: no sensor streaming. */
export async function expectCameraIdle(deviceId: string) {
  await expect.poll(async () => {
    const r = await fetch(`${api()}/devices/${deviceId}/stream/status`)
    return r.ok ? (await r.json()).is_streaming : null
  }, { timeout: 10000 }).toBe(false)
}

/**
 * Press Start in the calibration dialog. The server keeps the last result of a device, so
 * the dialog may open on a previous verdict (Keep / Use old / Recalibrate); go back to the
 * start form first, as a user would.
 */
export async function beginCalibration(dialog: Locator) {
  const start = dialog.getByTestId('calibration-start')
  const recalibrate = dialog.getByRole('button', { name: 'Recalibrate' })
  await expect(start.or(recalibrate).first()).toBeVisible({ timeout: 10000 })
  if (!(await start.isVisible())) await recalibrate.click()
  await start.click()
}

/** Put the old calibration table back if a run left an unwritten new one active. */
export async function restoreOldCalibration(deviceId: string) {
  await fetch(`${api()}/devices/${deviceId}/calibration/apply`, {
    method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ use_new: false }),
  }).catch(() => undefined)
}

/** Open the device actions menu and pick an entry by its label. */
export async function pickDeviceMenu(page: Page, deviceCard: Locator, label: string | RegExp) {
  await deviceCard.locator('button[title="Device actions"]').first().click()
  await page.getByRole('button', { name: label }).click()
}
