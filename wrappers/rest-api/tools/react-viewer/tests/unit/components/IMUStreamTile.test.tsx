import { describe, it, expect, beforeAll } from 'vitest'
import { screen, fireEvent, within } from '@testing-library/react'
import { StreamViewer } from '@/components/StreamViewer'
import { render, createMockDevice, createMockDeviceState } from '../../utils/test-utils'
import type { StreamConfig } from '@/api/types'

const NOW = 1_700_000_000_000

function motionConfig(streamType: 'accel' | 'gyro'): StreamConfig {
  return {
    sensor_id: 'motion-sensor',
    stream_type: streamType,
    format: 'motion_xyz32f',
    resolution: { width: 0, height: 0 },
    framerate: 200,
    enable: true,
  }
}

function samples(x: number, y: number, z: number, count = 5) {
  return Array.from({ length: count }, (_, i) => ({ timestamp: NOW + i * 50, x, y, z }))
}

// `render` resets the store, so state has to arrive via initialStoreState.
function streamingDevice(deviceId: string, serial: string) {
  const device = createMockDevice({ device_id: deviceId, serial_number: serial })
  return createMockDeviceState(device, {
    isActive: true,
    isStreaming: true,
    // A stream is shown only when its sensor reports it running; pipeline mode is gone.
    sensorStreamingStatus: {
      'motion-sensor': { is_streaming: true, stream_types: ['accel', 'gyro'] },
    },
    streamConfigs: [motionConfig('accel'), motionConfig('gyro')],
    streamMetadata: {
      accel: { frame_number: 1, timestamp: NOW, width: 0, height: 0 },
      gyro: { frame_number: 1, timestamp: NOW, width: 0, height: 0 },
    },
  })
}

function oneDevice() {
  return {
    deviceStates: { 'device-1': streamingDevice('device-1', 'SN1') },
    imuHistory: {
      'device-1': { accel: samples(0.1, -9.8, 0.2), gyro: samples(0.001, -0.002, 0.003) },
    },
  }
}

const accelTiles = () =>
  screen.getAllByText('ACCEL').map((label) => label.closest('div.relative') as HTMLElement)
const accelTile = () => accelTiles()[0]

describe('IMUStreamTile', () => {
  // The chart is a lazy import. Resolve it once up front so the first test to open
  // the graph does not race the module load.
  beforeAll(async () => {
    await import('@/components/IMUChart')
  })

  it('renders the orientation view by default', () => {
    render(<StreamViewer />, { initialStoreState: oneDevice() })

    const tile = accelTile()
    // The wireframe labels the magnitude; ‖(0.1, -9.8, 0.2)‖ = 9.803.
    expect(within(tile).getByRole('img', { name: /magnitude 9\.803 m\/s²/ })).toBeInTheDocument()
    // Each axis carries its unit, and nothing does in the header.
    expect(within(tile).getByText(/Y -9\.800/)).toHaveTextContent('Y -9.800 m/s²')
    expect(within(tile).getAllByText('m/s²')).toHaveLength(3)
    expect(within(tile).queryByTitle('Hide X')).not.toBeInTheDocument()
  })

  it('switches to the graph view and back', async () => {
    render(<StreamViewer />, { initialStoreState: oneDevice() })

    const tile = accelTile()
    const toggle = within(tile).getByTitle('Open graph view')
    expect(toggle).toHaveAttribute('aria-pressed', 'false')

    fireEvent.click(toggle)

    // The chart is a lazy import; the first test to open it pays the module load.
    await within(tile).findByTitle('Hide X')
    const close = within(tile).getByTitle('Close graph view')
    expect(close).toHaveAttribute('aria-pressed', 'true')
    // The orientation wireframe is gone while the graph is open.
    expect(within(tile).queryByRole('img', { name: /magnitude/ })).not.toBeInTheDocument()

    fireEvent.click(close)
    expect(within(tile).getByTitle('Open graph view')).toBeInTheDocument()
  })

  it('plots the magnitude series alongside the three axes', async () => {
    render(<StreamViewer />, { initialStoreState: oneDevice() })

    const tile = accelTile()
    fireEvent.click(within(tile).getByTitle('Open graph view'))
    // The header toggle flips synchronously; the chart itself is a lazy chunk.
    await within(tile).findByTitle('Hide X')

    for (const axis of ['X', 'Y', 'Z', 'N']) {
      expect(within(tile).getByTitle(`Hide ${axis}`)).toHaveAttribute('aria-pressed', 'true')
    }

    fireEvent.click(within(tile).getByTitle('Hide N'))
    expect(within(tile).getByTitle('Show N')).toHaveAttribute('aria-pressed', 'false')
    // The axes are unaffected by hiding the magnitude.
    expect(within(tile).getByTitle('Hide X')).toHaveAttribute('aria-pressed', 'true')
  })

  it('keeps each device on its own history', () => {
    render(<StreamViewer />, {
      initialStoreState: {
        deviceStates: {
          'device-1': streamingDevice('device-1', 'SN1'),
          'device-2': streamingDevice('device-2', 'SN2'),
        },
        imuHistory: {
          'device-1': { accel: samples(0.1, -9.8, 0.2), gyro: [] },
          'device-2': { accel: samples(1.5, -1.5, 7.7), gyro: [] },
        },
      },
    })

    const [first, second] = accelTiles()
    // Two devices, so each tile names its own camera.
    expect(within(first).getByText(/SN1/)).toBeInTheDocument()
    expect(within(second).getByText(/SN2/)).toBeInTheDocument()

    expect(within(first).getByText(/Y -9\.800/)).toBeInTheDocument()
    expect(within(first).queryByText(/Z 7\.700/)).not.toBeInTheDocument()
    expect(within(second).getByText(/Z 7\.700/)).toBeInTheDocument()
    expect(within(second).queryByText(/Y -9\.800/)).not.toBeInTheDocument()
  })

  it('shows a placeholder until the first sample arrives', () => {
    render(<StreamViewer />, {
      initialStoreState: {
        deviceStates: { 'device-1': streamingDevice('device-1', 'SN1') },
        imuHistory: {},
      },
    })

    expect(within(accelTile()).getByText(/Waiting for data/)).toBeInTheDocument()
  })
})
