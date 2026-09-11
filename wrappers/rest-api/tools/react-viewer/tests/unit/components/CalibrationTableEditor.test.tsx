import { describe, it, expect } from 'vitest'
import { render, screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { http, HttpResponse } from 'msw'
import { server } from '../../mocks/server'
import { CalibrationTableEditor } from '@/components/calibration/CalibrationTableEditor'
import { mockCalibrationTable } from '../../mocks/api-handlers'

describe('CalibrationTableEditor', () => {
  it('shows the table, highlights an edit and applies it without writing', async () => {
    const user = userEvent.setup()
    let sent: { baseline?: number; write?: boolean; rect_params?: { index: number; fx?: number }[] } | null = null
    server.use(http.put('/api/v1/devices/:deviceId/calibration/table', async ({ request }) => {
      sent = (await request.json()) as typeof sent
      return HttpResponse.json({ ...mockCalibrationTable(), baseline: sent!.baseline ?? 95 })
    }))
    render(<CalibrationTableEditor deviceId="dev" deviceName="D455" onClose={() => {}} />)
    const baseline = await screen.findByLabelText('Baseline')
    expect(baseline).toHaveValue(95)
    expect(screen.getByText(/table v2.1 · CRC ok/)).toBeInTheDocument()
    expect(screen.getByRole('button', { name: 'Apply' })).toBeDisabled()

    await user.clear(baseline)
    await user.type(baseline, '96')
    expect(baseline.className).toContain('border-yellow-500')
    await user.selectOptions(screen.getByLabelText('Rectified resolution'), '1')
    expect(screen.getByLabelText('Rectified fx')).toHaveValue(401)

    await user.click(screen.getByRole('button', { name: 'Apply' }))
    await waitFor(() => expect(sent).not.toBeNull())
    expect(sent!.baseline).toBe(96)
    expect(sent!.write).toBe(false)
    expect(sent!.rect_params).toHaveLength(16)
    await waitFor(() => expect(screen.getByRole('button', { name: 'Apply' })).toBeDisabled()) // synced with the device again
  })

  it('reports a refused write', async () => {
    const user = userEvent.setup()
    server.use(http.put('/api/v1/devices/:deviceId/calibration/table', () =>
      HttpResponse.json({ detail: 'Writing calibration to the device is disabled in Settings' }, { status: 403 })))
    render(<CalibrationTableEditor deviceId="dev" deviceName="D455" onClose={() => {}} />)
    await screen.findByLabelText('Baseline')
    await user.click(screen.getByRole('button', { name: 'Write' }))
    expect(await screen.findByRole('alert')).toHaveTextContent('disabled in Settings')
  })
})
