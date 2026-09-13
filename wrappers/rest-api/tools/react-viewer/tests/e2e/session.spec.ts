/**
 * One session, many features, in the order a user tries them - and after each one the
 * camera must still deliver frames. Every step asserts an outcome (frames, values, server
 * state), not the presence of a button.
 *
 *   REAL_DEVICE=true npx playwright test --project=real-device tests/e2e/session.spec.ts
 */

import { test, expect, suppressWhatsNew, getApiUrl } from './fixtures'
import { beginCalibration, cameraCard, expectCameraIdle, expectFramesFlowing, pickDeviceMenu, restoreOldCalibration, startDepth, stopAllStreams } from './helpers'

test.describe('@real-device Feature session', () => {
  test.beforeEach(async ({ testMode, page }) => {
    test.skip(testMode !== 'real', 'Real device tests require REAL_DEVICE=true')
    await suppressWhatsNew(page)
  })

  test('streams, metadata, ROI, calibration, 3D and restarts all keep the camera alive', async ({ page, waitForDevice, getDevices }) => {
    test.setTimeout(240000)
    const [device] = await getDevices()
    const api = `${getApiUrl()}/api/v1`
    const errors: string[] = []
    page.on('pageerror', (err) => errors.push(err.message))
    await page.goto('/')
    await waitForDevice(page)
    const card = cameraCard(page, device.serial_number)
    await expect(page.locator('[title="Loading..."]')).not.toBeVisible({ timeout: 30000 })

    try {
      // 1. Streaming: depth + color, frames flow (server-side proof, not just a <video>)
      await startDepth(page, card, device.device_id, true)
      const depthTile = page.locator('[data-testid="stream-tile"]').filter({ hasText: 'DEPTH' }).first()
      await expect(depthTile).toBeVisible()

      // 2. Metadata overlay shows live values that change
      await depthTile.locator('[title="Show frame metadata"]').click()
      const frameNumber = depthTile.getByText('Frame Number').locator('..').locator('span').last()
      await expect(frameNumber).not.toHaveText('', { timeout: 10000 })
      const first = await frameNumber.textContent()
      await expect.poll(() => frameNumber.textContent(), { timeout: 5000 }).not.toBe(first)
      const hwFps = depthTile.getByText('Hardware FPS').locator('..').locator('span').last()
      expect(Number(await hwFps.textContent())).toBeGreaterThan(0)
      await expect(depthTile.getByText('Clock Domain')).toBeVisible()
      await depthTile.locator('[title="Hide frame metadata"], [title="Show frame metadata"]').first().click()

      // 3. Auto-exposure ROI: drag a rectangle, the server stores it, the overlay shows it
      const roiButton = depthTile.getByRole('button', { name: 'Set auto-exposure ROI' })
      await expect(roiButton).toBeVisible({ timeout: 10000 })
      await roiButton.click()
      const box = (await depthTile.locator('video').first().boundingBox())!
      await page.mouse.move(box.x + box.width * 0.3, box.y + box.height * 0.3)
      await page.mouse.down()
      await page.mouse.move(box.x + box.width * 0.7, box.y + box.height * 0.7, { steps: 5 })
      await page.mouse.up()
      const roiWidth = async () => {
        const roi = await (await fetch(`${api}/devices/${device.device_id}/sensors/${device.device_id}-sensor-0/roi`)).json()
        return roi.max_x - roi.min_x
      }
      await expect.poll(roiWidth, { timeout: 10000 }).toBeLessThan(500) // a 40%-wide drag, well inside the frame
      const dragged = await roiWidth()
      // A finished drag leaves ROI mode (one rectangle per activation, as in the legacy
      // viewer); re-enter it to see the stored rectangle and the reset button
      await roiButton.click()
      await expect(depthTile.getByTestId('roi-rect')).toBeVisible()
      await depthTile.getByRole('button', { name: 'Reset ROI' }).click()
      // The firmware clamps "full frame" to its own maximum (636 px on a D455), so: wider than the drag
      await expect.poll(roiWidth, { timeout: 10000 }).toBeGreaterThan(dragged)
      await roiButton.click() // leave ROI mode
      await expectFramesFlowing(device.device_id)

      // 4. Depth readout on hover shows a distance
      await page.mouse.move(box.x + box.width * 0.5, box.y + box.height * 0.5)
      await page.mouse.move(box.x + box.width * 0.52, box.y + box.height * 0.5)
      await expect(depthTile.getByText(/Depth:/)).toBeVisible({ timeout: 10000 })

      // 5. On-chip calibration from the menu while streaming: outcome shown, streams back
      await pickDeviceMenu(page, card, 'On-Chip Calibration…')
      const dialog = page.getByRole('dialog', { name: 'On-Chip Calibration' })
      await beginCalibration(dialog)
      const outcome = dialog.getByTestId('calibration-result').or(dialog.getByTestId('calibration-error'))
      await expect(outcome.first()).toBeVisible({ timeout: 60000 })
      await dialog.getByRole('button', { name: 'Close' }).click()
      await expectFramesFlowing(device.device_id, 20000)

      // 6. 3D view renders the cloud, back to 2D still streams
      await page.getByRole('button', { name: '3D View' }).click()
      await expect(page.getByTestId('pointcloud-view').locator('canvas')).toBeVisible({ timeout: 20000 })
      await expect(page.getByTestId('3d-toolbar').getByRole('button', { name: 'Export PLY' })).toBeEnabled({ timeout: 20000 })
      await page.getByRole('button', { name: '2D View' }).click()
      await expectFramesFlowing(device.device_id)

      // 7. Stop everything and start again, twice: the failure a user hits when the handle dies
      for (let cycle = 0; cycle < 2; cycle++) {
        await stopAllStreams(device.device_id)
        await expectCameraIdle(device.device_id)
        await expect(card.getByTestId('start-streaming').first()).toBeVisible({ timeout: 10000 })
        await card.getByTestId('start-streaming').first().click()
        await expectFramesFlowing(device.device_id, 20000)
      }

      // 8. Dialogs that read the device all load real data
      await pickDeviceMenu(page, card, 'Calibration Table…')
      const table = page.getByRole('dialog', { name: 'Calibration Table' })
      const baseline = table.getByLabel('Baseline')
      await expect(baseline).toBeVisible({ timeout: 15000 })
      expect(Math.abs(Number(await baseline.inputValue()))).toBeGreaterThan(10) // millimetres, not zero
      await table.getByRole('button', { name: 'Close' }).click()
      await pickDeviceMenu(page, card, 'Software & Firmware Updates…')
      const updates = page.getByRole('dialog', { name: 'Updates' })
      await expect(updates.getByText(/UP TO DATE|RECOMMENDED|ESSENTIAL|UNKNOWN/).first()).toBeVisible({ timeout: 20000 })
      await updates.getByRole('button', { name: 'Close' }).click()

      expect(errors, errors.join(' | ')).toEqual([])
    } finally {
      await page.getByRole('button', { name: '2D View' }).click().catch(() => undefined)
      await stopAllStreams(device.device_id)
      await restoreOldCalibration(device.device_id)
    }
  })
})
