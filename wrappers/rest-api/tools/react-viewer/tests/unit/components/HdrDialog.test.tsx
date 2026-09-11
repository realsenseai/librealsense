import { describe, it, expect } from 'vitest'
import { render, screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { http, HttpResponse } from 'msw'
import { server } from '../../mocks/server'
import { HdrDialog } from '@/components/hdr/HdrDialog'

describe('HdrDialog', () => {
  it('says so for a device without HDR presets', async () => {
    server.use(http.get('/api/v1/devices/:deviceId/hdr/', () => HttpResponse.json({
      supported: false, preset: null, exposure_range: null, gain_range: null, hdr_enabled: null,
    })))
    render(<HdrDialog deviceId="dev" deviceName="D455" onClose={() => {}} />)
    expect(await screen.findByText(/no HDR preset support/)).toBeInTheDocument()
    expect(screen.queryByRole('button', { name: 'Apply' })).not.toBeInTheDocument()
  })

  it('edits the sequence within the legacy limits and applies it', async () => {
    const user = userEvent.setup()
    let sent: { items: unknown[]; control_type_auto: boolean } | null = null
    server.use(http.put('/api/v1/devices/:deviceId/hdr/', async ({ request }) => {
      sent = (await request.json()) as typeof sent
      return HttpResponse.json({ supported: true, preset: sent, exposure_range: null, gain_range: null, hdr_enabled: false })
    }))
    render(<HdrDialog deviceId="dev" deviceName="D455" onClose={() => {}} />)

    expect(await screen.findByText('Preset Item 1')).toBeInTheDocument()
    const remove = screen.getByRole('button', { name: 'Remove last item' })
    const add = screen.getByRole('button', { name: 'Add item' })
    expect(remove).toBeDisabled() // never below two items
    for (let i = 0; i < 4; i++) await user.click(add)
    expect(add).toBeDisabled() // never above six
    expect(screen.getByText('Preset Item 6')).toBeInTheDocument()

    // Clamp to the gain range from the device
    const gainField = screen.getByLabelText('Gain Value')
    await user.clear(gainField)
    await user.type(gainField, '9999')
    expect(gainField).toHaveValue(248)

    await user.click(screen.getByLabelText('Auto HDR'))
    expect(screen.getByLabelText('Gain Delta')).toBeInTheDocument()

    await user.click(screen.getByRole('button', { name: 'Apply' }))
    await waitFor(() => expect(sent).not.toBeNull())
    expect(sent!.items).toHaveLength(6)
    expect(sent!.control_type_auto).toBe(true)
  })

  it('resets to the legacy defaults', async () => {
    const user = userEvent.setup()
    render(<HdrDialog deviceId="dev" deviceName="D455" onClose={() => {}} />)
    await screen.findByText('Preset Item 1')
    await user.click(screen.getByRole('button', { name: 'Add item' }))
    expect(screen.getByText('Preset Item 3')).toBeInTheDocument()
    await user.click(screen.getByRole('button', { name: 'Load defaults' }))
    expect(screen.queryByText('Preset Item 3')).not.toBeInTheDocument()
  })
})
