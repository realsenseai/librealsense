import { describe, it, expect, vi, beforeEach } from 'vitest'
import { render, screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { http, HttpResponse } from 'msw'
import { server } from '../../mocks/server'
import { SettingsDialog } from '@/components/settings/SettingsDialog'
import { useSettingsStore } from '@/store/settings'
import { mockSettings } from '../../mocks/fixtures/settings'

describe('SettingsDialog', () => {
  beforeEach(() => {
    useSettingsStore.setState({ settings: structuredClone(mockSettings), isLoading: false, error: null })
    localStorage.removeItem('rs-settings-tab')
  })

  it('renders nothing while closed', () => {
    render(<SettingsDialog isOpen={false} onClose={() => {}} />)
    expect(screen.queryByRole('dialog')).not.toBeInTheDocument()
  })

  it('shows the three legacy tabs and remembers the selected one', async () => {
    const user = userEvent.setup()
    render(<SettingsDialog isOpen onClose={() => {}} />)

    expect(screen.getByRole('tab', { name: 'Playback & Record' })).toBeInTheDocument()
    await user.click(screen.getByRole('tab', { name: 'Online' }))

    expect(screen.getByLabelText(/Custom versions database URL/)).toBeInTheDocument()
    expect(localStorage.getItem('rs-settings-tab')).toBe('Online')
  })

  it('sends the edited draft on Save & Close and closes', async () => {
    const user = userEvent.setup()
    const onClose = vi.fn()
    let sent: Record<string, unknown> | null = null
    server.use(
      http.put('/api/v1/settings/', async ({ request }) => {
        sent = (await request.json()) as Record<string, unknown>
        return HttpResponse.json({ ...mockSettings, viewer: { metric_system: false } })
      })
    )
    render(<SettingsDialog isOpen onClose={onClose} />)

    await user.click(screen.getByRole('tab', { name: 'General' }))
    await user.selectOptions(screen.getByLabelText('Units of measurement'), 'imperial')
    await user.click(screen.getByRole('button', { name: /Save & Close/ }))

    await waitFor(() => expect(onClose).toHaveBeenCalled())
    expect(sent).toMatchObject({ viewer: { metric_system: false } })
    expect(useSettingsStore.getState().settings?.viewer.metric_system).toBe(false)
  })

  it('shows the server error and stays open when saving fails', async () => {
    const user = userEvent.setup()
    const onClose = vi.fn()
    server.use(http.put('/api/v1/settings/', () => HttpResponse.json({ detail: 'nope' }, { status: 422 })))
    render(<SettingsDialog isOpen onClose={onClose} />)

    await user.click(screen.getByRole('button', { name: 'Apply' }))

    await waitFor(() => expect(screen.getByText(/Request failed|nope/)).toBeInTheDocument())
    expect(onClose).not.toHaveBeenCalled()
  })

  it('cancel closes without saving', async () => {
    const user = userEvent.setup()
    const onClose = vi.fn()
    const put = vi.fn()
    server.use(http.put('/api/v1/settings/', () => { put(); return HttpResponse.json(mockSettings) }))
    render(<SettingsDialog isOpen onClose={onClose} />)

    await user.click(screen.getByRole('button', { name: 'Cancel' }))

    expect(onClose).toHaveBeenCalled()
    expect(put).not.toHaveBeenCalled()
  })
})
