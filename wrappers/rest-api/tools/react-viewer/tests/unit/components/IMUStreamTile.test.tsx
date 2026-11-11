import { beforeEach, describe, it, expect, vi } from 'vitest'
import { screen, fireEvent, within } from '@testing-library/react'
import { StreamViewer } from '@/components/StreamViewer'
import { render, createMockDevice, createMockDeviceState, startedProfiles } from '../../utils/test-utils'
import type { StreamConfig } from '@/api/types'

const NOW = 1_700_000_000_000

type Sample = { x: number; y: number; z: number }

// Each tile's frame socket delivers one motion frame, as the backend sends it: float32 x, y, z.
const samples = vi.hoisted(() => new Map<string, Sample>())
vi.stubGlobal('requestAnimationFrame', (callback: FrameRequestCallback) => (callback(0), 1))
vi.mock('@/api/frames', () => ({
  openFrameStream: (deviceId: string, profile: { stream_type: string }, onFrame: (frame: unknown) => void) => {
    const sample = samples.get(`${deviceId}/${profile.stream_type}`)
    if (sample) {
      onFrame({
        header: { frame_number: 1, timestamp: NOW, timestamp_domain: 'global_time', format: 'motion_xyz32f', metadata: {} },
        pixels: new Float32Array([sample.x, sample.y, sample.z]).buffer,
      })
    }
    return () => {}
  },
}))

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

// `render` resets the store, so state has to arrive via initialStoreState.
function streamingDevice(deviceId: string, serial: string, accel?: Sample, gyro?: Sample) {
  if (accel) samples.set(`${deviceId}/accel`, accel)
  if (gyro) samples.set(`${deviceId}/gyro`, gyro)
  const device = createMockDevice({ device_id: deviceId, serial_number: serial })
  return createMockDeviceState(device, {
    isActive: true,
    isStreaming: true,
    // A stream is shown only when its sensor reports it running; pipeline mode is gone.
    sensorStreamingStatus: {
      'motion-sensor': { is_streaming: true, profiles: startedProfiles('accel', 'gyro') },
    },
    streamConfigs: [motionConfig('accel'), motionConfig('gyro')],
  })
}

function oneDevice() {
  return {
    deviceStates: {
      'device-1': streamingDevice('device-1', 'SN1', { x: 0.1, y: -9.8, z: 0.2 }, { x: 0.001, y: -0.002, z: 0.003 }),
    },
  }
}

const accelTiles = () =>
  screen.getAllByText('ACCEL').map((label) => label.closest('div.relative') as HTMLElement)
const accelTile = () => accelTiles()[0]

describe('IMUStreamTile', () => {
  beforeEach(() => samples.clear())

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
          'device-1': streamingDevice('device-1', 'SN1', { x: 0.1, y: -9.8, z: 0.2 }),
          'device-2': streamingDevice('device-2', 'SN2', { x: 1.5, y: -1.5, z: 7.7 }),
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
      },
    })

    expect(within(accelTile()).getByText(/Waiting for data/)).toBeInTheDocument()
  })
})
