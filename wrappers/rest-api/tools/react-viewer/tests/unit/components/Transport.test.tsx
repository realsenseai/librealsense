import { describe, it, expect } from 'vitest'
import { screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { render, createMockDevice, createMockDeviceState } from '../../utils/test-utils'
import { Transport, formatNs } from '@/components/playback/Transport'
import { useAppStore } from '@/store'

describe('Transport', () => {
  const device = createMockDevice({ device_id: 'playback-clip.db3', is_playback: true, file_name: 'C:/recs/clip.db3' })
  const withDevice = { initialStoreState: { devices: [device], deviceStates: { [device.device_id]: createMockDeviceState(device) } } }

  it('formats positions as hh:mm:ss.mmm', () => {
    expect(formatNs(0)).toBe('00:00:00.000')
    expect(formatNs(61_250_000_000)).toBe('00:01:01.250')
  })

  it('shows the position, duration and file once the status is fetched', async () => {
    render(<Transport deviceId={device.device_id} />, withDevice)
    await waitFor(() => expect(screen.getByText('00:00:01.500')).toBeInTheDocument())
    expect(screen.getByText('00:00:04.000')).toBeInTheDocument()
    expect(screen.getByRole('button', { name: 'Play' })).toBeInTheDocument()
    expect(screen.getByRole('button', { name: 'Step forward' })).not.toBeDisabled()
  })

  it('plays, then pauses, and changes speed', async () => {
    const user = userEvent.setup()
    render(<Transport deviceId={device.device_id} />, withDevice)
    await user.click(await screen.findByRole('button', { name: 'Play' }))
    await waitFor(() => expect(useAppStore.getState().deviceStates[device.device_id].playback?.state).toBe('playing'))
    expect(screen.getByRole('button', { name: 'Pause' })).toBeInTheDocument()

    await user.selectOptions(screen.getByLabelText('Playback speed'), '0.5')
    await waitFor(() => expect(useAppStore.getState().deviceStates[device.device_id].playback?.speed).toBe(0.5))
  })
})
