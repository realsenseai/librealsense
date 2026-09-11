import { describe, it, expect, vi } from 'vitest'
import { render, screen } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { UpdatesDialog } from '@/components/updates/UpdatesDialog'

describe('UpdatesDialog', () => {
  it('shows both sections with badges and offers to install a recommended firmware', async () => {
    const install = vi.fn()
    render(<UpdatesDialog deviceId="dev" deviceName="RealSense D455" onClose={() => {}} onInstallFirmware={install} />)
    expect(await screen.findByText('RECOMMENDED')).toBeInTheDocument()
    expect(screen.getByText('UP TO DATE')).toBeInTheDocument()
    expect(screen.getByText('5.17.0.10')).toBeInTheDocument()
    expect(screen.getByRole('link', { name: 'Release notes' })).toHaveAttribute('href', 'https://x/notes')
    await userEvent.click(screen.getByRole('button', { name: 'Install' }))
    expect(install).toHaveBeenCalled()
  })
})
