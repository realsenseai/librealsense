import { describe, it, expect, beforeEach } from 'vitest'
import { screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { http, HttpResponse } from 'msw'
import { server } from '../../mocks/server'
import { render, createMockDevice, createMockDeviceState } from '../../utils/test-utils'
import { OutputConsole, formatEntry } from '@/components/console/OutputConsole'
import { useConsoleStore } from '@/store/console'
import type { LogEntry } from '@/api/types'

const entry = (id: number, severity: string, message: string, source = 'sdk'): LogEntry => ({ id, ts: 0, severity, message, source })

describe('OutputConsole', () => {
  const device = createMockDevice()
  const withDevice = { initialStoreState: { devices: [device], deviceStates: { [device.device_id]: createMockDeviceState(device) } } }

  beforeEach(() => useConsoleStore.setState({
    entries: [entry(1, 'error', 'boom'), entry(2, 'warn', 'careful'), entry(3, 'info', 'hello')],
    isOpen: true, show: { error: true, warn: true, info: true }, search: '', commandHistory: [], fwLogs: {}, commandNames: [],
  }))

  it('formats a line with time, severity, source and location', () => {
    expect(formatEntry({ ...entry(1, 'warn', 'msg', 'server'), file: 'x.py', line: 3 })).toMatch(/\[WARN\] \[server\] x.py:3 msg$/)
  })

  it('collapses to a button that shows the error count', async () => {
    useConsoleStore.setState({ isOpen: false })
    render(<OutputConsole />, withDevice)
    expect(screen.getByRole('button', { name: 'Open output console' })).toHaveTextContent('1')
    await userEvent.click(screen.getByRole('button', { name: 'Open output console' }))
    expect(screen.getByTestId('output-console')).toBeInTheDocument()
  })

  it('lists lines and hides a severity when its counter is toggled', async () => {
    render(<OutputConsole />, withDevice)
    expect(screen.getByTestId('console-lines')).toHaveTextContent('boom')
    await userEvent.click(screen.getByRole('button', { name: /1 errors/ }))
    expect(screen.getByTestId('console-lines')).not.toHaveTextContent('boom')
    expect(screen.getByTestId('console-lines')).toHaveTextContent('careful')
  })

  it('sends a typed command to the device on Enter and recalls it with the arrow key', async () => {
    let sent: unknown = null
    server.use(http.post('/api/v1/devices/:deviceId/terminal', async ({ request }) => { sent = await request.json(); return HttpResponse.json({ output: 'x' }) }))
    render(<OutputConsole />, withDevice)
    const input = screen.getByLabelText('Console command')
    await userEvent.type(input, '10 00 00 00{Enter}')
    await waitFor(() => expect(sent).toEqual({ line: '10 00 00 00' }))
    expect(input).toHaveValue('')
    await userEvent.type(input, '{ArrowUp}')
    expect(input).toHaveValue('10 00 00 00')
  })

  it('completes a command name with Tab', async () => {
    useConsoleStore.setState({ commandNames: ['GVD', 'GLD'] })
    render(<OutputConsole />, withDevice)
    const input = screen.getByLabelText('Console command')
    await userEvent.type(input, 'gl{Tab}')
    expect(input).toHaveValue('GLD')
  })

  it('closes on Escape', async () => {
    render(<OutputConsole />, withDevice)
    await userEvent.keyboard('{Escape}')
    expect(screen.queryByTestId('output-console')).not.toBeInTheDocument()
  })
})
