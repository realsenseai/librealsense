/**
 * Record & playback E2E against a real camera (WP3.4).
 *
 *   REAL_DEVICE=true npx playwright test --project=real-device tests/e2e/playback.spec.ts
 *
 * Records a few seconds from the first camera, loads the file back through the API (the
 * UI's picker uploads a browser-side file; the recording already sits on the server) and
 * drives the transport in the playback device's card.
 */

import { test, expect, getApiUrl, suppressWhatsNew } from './fixtures'
import type { Locator } from '@playwright/test'

async function expandSensorModules(deviceCard: Locator) {
  const headers = deviceCard.locator('button[aria-expanded]')
  for (let i = 0; i < await headers.count(); i++) {
    const header = headers.nth(i)
    if (await header.getAttribute('aria-expanded') === 'false') await header.click()
  }
}

test.describe('@real-device Record and playback', () => {
  test.beforeEach(async ({ testMode, page }) => {
    test.skip(testMode !== 'real', 'Real device tests require REAL_DEVICE=true')
    await suppressWhatsNew(page)
  })

  test('records a clip and plays it back with the transport', async ({ page, waitForDevice, getDevices }) => {
    test.setTimeout(180000)
    const [device] = await getDevices()
    const api = `${getApiUrl()}/api/v1`

    await page.goto('/')
    await waitForDevice(page)
    // Pin the camera's card: once the recording loads, its card sits first in the list
    const deviceCard = page.locator('[data-testid="device-card"]').filter({ hasText: device.serial_number }).filter({ hasNotText: 'Playback' })
    await expect(page.locator('[title="Loading..."]')).not.toBeVisible({ timeout: 30000 })
    await expandSensorModules(deviceCard)

    await page.locator('[data-testid="toggle-stream-depth"]').first().check()
    await page.locator('button:has-text("Start")').first().click()
    await expect(page.locator('video').first()).toBeVisible({ timeout: 15000 })

    // Record for a few seconds
    await deviceCard.getByRole('button', { name: 'Record', exact: true }).click()
    const stopRecording = deviceCard.getByRole('button', { name: 'Stop recording' })
    await expect(stopRecording).toBeVisible({ timeout: 10000 })
    const recording = await (await fetch(`${api}/devices/${device.device_id}/record/`)).json()
    expect(recording.recording).toBe(true)
    const file: string = recording.file
    expect(file).toMatch(/\.db3$/)
    await page.waitForTimeout(6000)
    await stopRecording.click()
    await expect(deviceCard.getByRole('button', { name: 'Record', exact: true })).toBeVisible({ timeout: 10000 })
    await deviceCard.locator('button:has-text("Stop")').first().click()
    await expect(deviceCard.locator('button:has-text("Stop")')).toHaveCount(0, { timeout: 10000 })

    // Load it back as a device
    const load = await fetch(`${api}/playback/load`, {
      method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify({ path: file }),
    })
    expect(load.ok).toBe(true)
    const playback = await load.json()
    try {
      await page.getByRole('button', { name: 'Refresh devices' }).click()
      const playbackCard = page.locator('[data-testid="device-card"]').filter({ hasText: 'Playback' })
      await expect(playbackCard).toBeVisible({ timeout: 15000 })
      const transport = playbackCard.getByTestId('playback-transport')
      await expect(transport).toBeVisible({ timeout: 10000 })
      await expect(transport).toContainText(file.split(/[\\/]/).pop()!)

      // Starting the recorded depth stream starts playback (the SDK plays as soon as a
      // sensor streams), so the transport flips to Pause and the position moves.
      await expandSensorModules(playbackCard)
      await playbackCard.locator('[data-testid="toggle-stream-depth"]').first().check()
      await playbackCard.locator('button:has-text("Start")').first().click()
      await expect(transport.getByRole('button', { name: 'Pause' })).toBeVisible({ timeout: 10000 })
      const seek = transport.getByLabel('Seek')
      await expect.poll(async () => Number(await seek.inputValue()), { timeout: 10000 }).toBeGreaterThan(0)

      // Pause, step, seek back to the start, play again, repeat
      await transport.getByRole('button', { name: 'Pause' }).click()
      await expect(transport.getByRole('button', { name: 'Play' })).toBeVisible({ timeout: 10000 })
      const paused = Number(await seek.inputValue())
      await transport.getByRole('button', { name: 'Step forward' }).click()
      await expect.poll(async () => Number(await seek.inputValue()), { timeout: 5000 }).toBeGreaterThanOrEqual(paused)

      await seek.evaluate((el: HTMLInputElement) => {
        // React tracks the value it last set; go through the prototype setter so the
        // input event is seen as a real change, then release the thumb.
        Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value')!.set!.call(el, '0')
        el.dispatchEvent(new Event('input', { bubbles: true }))
        el.dispatchEvent(new MouseEvent('mouseup', { bubbles: true }))
      })
      await expect.poll(async () => Number(await seek.inputValue()), { timeout: 5000 }).toBeLessThan(Math.max(paused, 1))

      await transport.getByRole('button', { name: 'Play' }).click()
      await expect(transport.getByRole('button', { name: 'Pause' })).toBeVisible({ timeout: 10000 })

      await transport.getByRole('button', { name: 'Repeat' }).click()
      await expect(transport.getByRole('button', { name: 'Repeat' })).toHaveAttribute('aria-pressed', 'true')

      await transport.getByRole('button', { name: 'Close recording' }).click()
      await expect(playbackCard).not.toBeVisible({ timeout: 15000 })
    } finally {
      await fetch(`${api}/playback/${playback.device_id}`, { method: 'DELETE' }).catch(() => undefined)
    }
  })
})
