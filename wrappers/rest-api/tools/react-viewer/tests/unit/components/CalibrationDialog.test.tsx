import { describe, it, expect, beforeEach } from 'vitest'
import { render, screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { http, HttpResponse } from 'msw'
import { server } from '../../mocks/server'
import { CalibrationDialog } from '@/components/calibration/CalibrationDialog'
import { useJobsStore } from '@/store/jobs'

const done = {
  kind: 'occ', state: 'done', health: [0.12, 0], verdict: 'good', has_new_table: true, active: 'new', written: false, error: null, started_at: 1,
}

describe('CalibrationDialog', () => {
  beforeEach(() => useJobsStore.setState({ jobs: {} }))

  it('starts an on-chip calibration with the chosen parameters and shows progress', async () => {
    const user = userEvent.setup()
    let sent: Record<string, unknown> | null = null
    server.use(http.post('/api/v1/devices/:deviceId/calibration/occ', async ({ request }) => {
      sent = (await request.json()) as Record<string, unknown>
      return HttpResponse.json({ id: 'j1', kind: 'calibration_occ', device_id: 'dev', state: 'running', progress: 0.05, message: 'Calibrating', result: null, error: null, created_at: 1, updated_at: 1 })
    }))
    render(<CalibrationDialog deviceId="dev" deviceName="D455" mode="occ" onClose={() => {}} />)
    await screen.findByLabelText('Speed')
    await user.selectOptions(screen.getByLabelText('Speed'), '4')
    await user.selectOptions(screen.getByLabelText('Accuracy'), '1')
    await user.click(screen.getByTestId('calibration-start'))
    await waitFor(() => expect(sent).toMatchObject({ speed: 4, accuracy: 1, apply_preset: true }))
    expect(await screen.findByTestId('calibration-progress')).toBeInTheDocument()
    expect(screen.getByRole('button', { name: 'Calibrating…' })).toBeDisabled()
  })

  it('shows the health verdict and lets the user keep or go back', async () => {
    const user = userEvent.setup()
    const calls: string[] = []
    server.use(
      http.get('/api/v1/devices/:deviceId/calibration/', () => HttpResponse.json(done)),
      http.post('/api/v1/devices/:deviceId/calibration/apply', async ({ request }) => {
        const body = (await request.json()) as { use_new: boolean }
        calls.push(`apply:${body.use_new}`)
        return HttpResponse.json({ ...done, active: body.use_new ? 'new' : 'old' })
      }),
      http.post('/api/v1/devices/:deviceId/calibration/keep', () => { calls.push('keep'); return HttpResponse.json({ ...done, written: true }) }),
    )
    render(<CalibrationDialog deviceId="dev" deviceName="D455" mode="occ" onClose={() => {}} />)
    expect(await screen.findByText('Good')).toBeInTheDocument()
    expect(screen.getByText('0.1200 / 0.0000')).toBeInTheDocument()
    await user.click(screen.getByRole('button', { name: 'Use old' }))
    await waitFor(() => expect(calls).toEqual(['apply:false']))
    await user.click(screen.getByRole('button', { name: 'Use new' }))
    await user.click(screen.getByRole('button', { name: 'Keep' }))
    await waitFor(() => expect(calls).toEqual(['apply:false', 'apply:true', 'keep']))
    expect(screen.getByText(/written to the device/)).toBeInTheDocument()
  })

  it('shows the last failure and offers a retry; tare asks for the ground truth', async () => {
    server.use(http.get('/api/v1/devices/:deviceId/calibration/', () => HttpResponse.json({
      ...done, state: 'failed', verdict: null, health: null, has_new_table: false, active: 'old', error: 'Not enough depth pixels! - low fill factor)',
    })))
    render(<CalibrationDialog deviceId="dev" deviceName="D455" mode="tare" onClose={() => {}} />)
    expect(await screen.findByTestId('calibration-error')).toHaveTextContent('Not enough depth pixels')
    expect(screen.getByTestId('calibration-start')).toHaveTextContent('Retry')
    expect(screen.getByLabelText('Ground truth')).toHaveValue(1000)
  })
})
