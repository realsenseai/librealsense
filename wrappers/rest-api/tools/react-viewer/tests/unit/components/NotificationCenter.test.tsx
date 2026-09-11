import { describe, it, expect, beforeEach } from 'vitest'
import { render, screen } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { NotificationCenter } from '@/components/notifications/NotificationCenter'
import { useNotificationsStore } from '@/store/notifications'

describe('NotificationCenter', () => {
  beforeEach(() => useNotificationsStore.setState({ notices: [], snoozed: {} }))

  it('renders nothing when there is nothing to show', () => {
    render(<NotificationCenter />)
    expect(screen.queryByTestId('notice')).not.toBeInTheDocument()
  })

  it('shows a card and dismisses it', async () => {
    useNotificationsStore.getState().push({ severity: 'warn', title: 'D455: hardware event', message: 'Laser turned on' })
    render(<NotificationCenter />)
    expect(screen.getByText('Laser turned on')).toBeInTheDocument()
    await userEvent.click(screen.getByRole('button', { name: 'Dismiss' }))
    expect(screen.queryByTestId('notice')).not.toBeInTheDocument()
  })

  it('offers the three legacy dismissals for a snoozable card', async () => {
    useNotificationsStore.getState().push({ kind: 'fw:1', severity: 'info', title: 'Firmware', message: 'Update available' })
    render(<NotificationCenter />)
    await userEvent.click(screen.getByRole('button', { name: 'Dismiss options' }))
    await userEvent.click(screen.getByText("Don't show again"))
    expect(screen.queryByTestId('notice')).not.toBeInTheDocument()
    expect(useNotificationsStore.getState().isSnoozed('fw:1')).toBe(true)
  })
})
