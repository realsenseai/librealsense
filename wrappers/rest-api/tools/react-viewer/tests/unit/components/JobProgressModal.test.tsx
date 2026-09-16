import { describe, it, expect, vi } from 'vitest'
import { render, screen } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { http, HttpResponse } from 'msw'
import { server } from '../../mocks/server'
import { JobProgressModal } from '@/components/jobs/JobProgressModal'
import type { JobInfo } from '@/api/types'

const job = (over: Partial<JobInfo> = {}): JobInfo => ({
  id: 'j1', kind: 'calibration', device_id: 'dev', state: 'running', progress: 0.4,
  message: 'scanning', result: null, error: null, created_at: 1, updated_at: 1, ...over,
})

describe('JobProgressModal', () => {
  it('renders nothing without a job', () => {
    render(<JobProgressModal job={undefined} title="Calibration" onClose={() => {}} />)
    expect(screen.queryByRole('dialog')).not.toBeInTheDocument()
  })

  it('shows progress and the phase message while running, with no close button', () => {
    render(<JobProgressModal job={job()} title="Calibration" subtitle="D455" onClose={() => {}} />)
    expect(screen.getByText('40%')).toBeInTheDocument()
    expect(screen.getByText('scanning')).toBeInTheDocument()
    expect(screen.queryByRole('button', { name: /Close|Done/ })).not.toBeInTheDocument()
  })

  it('offers Cancel only when cancellable and posts the cancel', async () => {
    let cancelled = false
    server.use(http.post('/api/v1/jobs/j1/cancel', () => { cancelled = true; return HttpResponse.json(job()) }))
    const { rerender } = render(<JobProgressModal job={job()} title="Calibration" onClose={() => {}} />)
    expect(screen.queryByRole('button', { name: 'Cancel' })).not.toBeInTheDocument()

    rerender(<JobProgressModal job={job()} title="Calibration" cancellable onClose={() => {}} />)
    await userEvent.setup().click(screen.getByRole('button', { name: 'Cancel' }))
    expect(cancelled).toBe(true)
  })

  it('shows the error and a Close button when failed', async () => {
    const onClose = vi.fn()
    render(<JobProgressModal job={job({ state: 'failed', error: 'boom' })} title="Calibration" onClose={onClose} />)
    expect(screen.getByText('boom')).toBeInTheDocument()
    await userEvent.setup().click(screen.getByRole('button', { name: 'Close' }))
    expect(onClose).toHaveBeenCalled()
  })

  it('shows Done at 100% when complete', () => {
    render(<JobProgressModal job={job({ state: 'done', progress: 1 })} title="Export" onClose={() => {}} />)
    expect(screen.getByText('100%')).toBeInTheDocument()
    expect(screen.getByRole('button', { name: 'Done' })).toBeInTheDocument()
  })
})
