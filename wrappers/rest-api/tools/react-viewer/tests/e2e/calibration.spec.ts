/**
 * On-chip calibration E2E against a real camera (WP6): the wizard drives a firmware
 * calibration job and shows either the health verdict or the firmware's refusal.
 *
 *   REAL_DEVICE=true npx playwright test --project=real-device tests/e2e/calibration.spec.ts
 */

import { test, expect, suppressWhatsNew } from './fixtures'
import { beginCalibration, restoreOldCalibration } from './helpers'

test.describe('@real-device Calibration', () => {
  test.beforeEach(async ({ testMode, page }) => {
    test.skip(testMode !== 'real', 'Real device tests require REAL_DEVICE=true')
    await suppressWhatsNew(page)
  })

  test('runs on-chip calibration from the device menu and reports the outcome', async ({ page, waitForDevice, getDevices }) => {
    test.setTimeout(120000)
    const [device] = await getDevices()
    await page.goto('/')
    await waitForDevice(page)
    const deviceCard = page.locator('[data-testid="device-card"]').filter({ hasText: device.serial_number })
    await expect(page.locator('[title="Loading..."]')).not.toBeVisible({ timeout: 30000 })

    await deviceCard.locator('button[title="Device actions"]').first().click()
    await page.getByRole('button', { name: 'On-Chip Calibration…' }).click()
    const dialog = page.getByRole('dialog', { name: 'On-Chip Calibration' })
    await expect(dialog).toBeVisible()
    try {
      await beginCalibration(dialog)
      await expect(dialog.getByTestId('calibration-progress')).toBeVisible({ timeout: 10000 })

      // The firmware either converges (verdict) or refuses this scene (error); both must land
      const outcome = dialog.getByTestId('calibration-result').or(dialog.getByTestId('calibration-error'))
      await expect(outcome.first()).toBeVisible({ timeout: 60000 })
      await expect(dialog.getByRole('button', { name: 'Close' })).toBeEnabled()

      // The camera is idle again afterwards
      await expect.poll(async () => {
        const status = await (await fetch(`http://localhost:8000/api/v1/devices/${device.device_id}/sensors/${device.device_id}-sensor-0/status`)).json()
        return status.is_streaming
      }, { timeout: 15000 }).toBe(false)
      await dialog.getByRole('button', { name: 'Close' }).click()
    } finally {
      await restoreOldCalibration(device.device_id)
    }
  })
})
