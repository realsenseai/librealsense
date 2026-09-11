import { describe, it, expect, vi, beforeEach } from 'vitest'
import { screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { http, HttpResponse } from 'msw'
import { server } from '../../mocks/server'
import { render, createMockDevice, createMockDeviceState, createMockStreamConfig } from '../../utils/test-utils'
import { StreamViewer } from '@/components/StreamViewer'
import { useLayoutStore } from '@/store/layout'

describe('StreamViewer', () => {
  describe('Empty State', () => {
    it('shows "Nothing is streaming!" when no devices are active', () => {
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: {},
        },
      })

      expect(screen.getByText('Nothing is streaming!')).toBeInTheDocument()
    })

    it('shows the "connect + enable" hint when no devices are active', () => {
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: {},
        },
      })

      expect(screen.getByText('Connect a device and enable any stream to start')).toBeInTheDocument()
    })

    it('shows the same empty state when device is active but no streams enabled', () => {
      const device = createMockDevice()
      const deviceState = createMockDeviceState(device, {
        // Use `enable` (singular) — the actual StreamConfig field name. The
        // mock factory defaults `enable: true`, so the wrong field name leaves
        // the stream enabled and the empty-state branch never renders.
        streamConfigs: [createMockStreamConfig({ enable: false })],
      })

      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: deviceState },
        },
      })

      expect(screen.getByText('Nothing is streaming!')).toBeInTheDocument()
    })
  })

  describe('Stream Rendering', () => {
    it('renders a stream tile for an enabled depth stream that is actually streaming', () => {
      const device = createMockDevice()
      const depthConfig = createMockStreamConfig({
        stream_type: 'depth',
        enable: true, // Component uses 'enable' property
      })
      const deviceState = createMockDeviceState(device, {
        streamConfigs: [depthConfig],
        isStreaming: true,
        sensorStreamingStatus: {
          'test-device-1-sensor-0': { sensor_id: 'test-device-1-sensor-0', name: '', is_streaming: true, stream_types: ['depth', 'color', 'infrared'] },
        },
      })
      
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: deviceState },
        },
      })
      
      expect(document.querySelectorAll('video.stream-video')).toHaveLength(1)
      expect(screen.getByText('DEPTH')).toBeInTheDocument()
    })

    it('shows multiple stream tiles for multiple enabled streams', () => {
      const device = createMockDevice()
      const depthConfig = createMockStreamConfig({
        stream_type: 'depth',
        enable: true,
      })
      const colorConfig = createMockStreamConfig({
        stream_type: 'color',
        enable: true,
        format: 'RGB8',
      })
      const deviceState = createMockDeviceState(device, {
        streamConfigs: [depthConfig, colorConfig],
        isStreaming: true,
        sensorStreamingStatus: {
          'test-device-1-sensor-0': { sensor_id: 'test-device-1-sensor-0', name: '', is_streaming: true, stream_types: ['depth', 'color'] },
        },
      })
      
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: deviceState },
        },
      })
      
      expect(document.querySelectorAll('video.stream-video')).toHaveLength(2)
      expect(screen.getByText('DEPTH')).toBeInTheDocument()
      expect(screen.getByText('COLOR')).toBeInTheDocument()
    })
  })

  describe('Multi-device Support', () => {
    it('shows streams from multiple active streaming devices', () => {
      const device1 = createMockDevice({ device_id: 'device-1', name: 'D435 Camera 1' })
      const device2 = createMockDevice({ device_id: 'device-2', name: 'D455 Camera 2' })
      
      const config1 = createMockStreamConfig({ stream_type: 'depth', enable: true })
      const config2 = createMockStreamConfig({ stream_type: 'depth', enable: true })
      
      const state1 = createMockDeviceState(device1, {
        streamConfigs: [config1],
        isStreaming: true,
        sensorStreamingStatus: {
          'test-device-1-sensor-0': { sensor_id: 'test-device-1-sensor-0', name: '', is_streaming: true, stream_types: ['depth', 'color', 'infrared'] },
        },
      })
      const state2 = createMockDeviceState(device2, {
        streamConfigs: [config2],
        isStreaming: true,
        sensorStreamingStatus: {
          'test-device-1-sensor-0': { sensor_id: 'test-device-1-sensor-0', name: '', is_streaming: true, stream_types: ['depth', 'color', 'infrared'] },
        },
      })
      
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: {
            [device1.device_id]: state1,
            [device2.device_id]: state2,
          },
        },
      })
      
      expect(document.querySelectorAll('video.stream-video')).toHaveLength(2)
    })
  })

  describe('Tile layout', () => {
    const twoStreams = () => {
      const device = createMockDevice()
      return createMockDeviceState(device, {
        streamConfigs: [
          createMockStreamConfig({ stream_type: 'color', format: 'RGB8', enable: true }),
          createMockStreamConfig({ stream_type: 'depth', enable: true }),
        ],
        isStreaming: true,
        sensorStreamingStatus: {
          'test-device-1-sensor-0': { sensor_id: 'test-device-1-sensor-0', name: '', is_streaming: true, stream_types: ['depth', 'color'] },
        },
      })
    }

    beforeEach(() => useLayoutStore.setState({ tileOrder: {}, maximized: null }))

    it('draws depth before color whatever order the configs came in', () => {
      const ds = twoStreams()
      render(<StreamViewer />, { initialStoreState: { deviceStates: { [ds.device.device_id]: ds } } })
      const labels = screen.getAllByTestId('stream-tile').map((t) => t.textContent?.includes('DEPTH') ? 'depth' : 'color')
      expect(labels).toEqual(['depth', 'color'])
    })

    it('follows the remembered arrangement', () => {
      const ds = twoStreams()
      useLayoutStore.setState({ tileOrder: { [ds.device.device_id]: [`${ds.device.device_id}:color`, `${ds.device.device_id}:depth`] } })
      render(<StreamViewer />, { initialStoreState: { deviceStates: { [ds.device.device_id]: ds } } })
      const labels = screen.getAllByTestId('stream-tile').map((t) => t.textContent?.includes('DEPTH') ? 'depth' : 'color')
      expect(labels).toEqual(['color', 'depth'])
    })

    it('maximizes one tile and restores the grid', async () => {
      const ds = twoStreams()
      render(<StreamViewer />, { initialStoreState: { deviceStates: { [ds.device.device_id]: ds } } })
      expect(screen.getAllByTestId('stream-tile')).toHaveLength(2)

      await userEvent.click(screen.getAllByRole('button', { name: 'Maximize tile' })[0])
      expect(screen.getAllByTestId('stream-tile')).toHaveLength(1)
      expect(screen.getByText('DEPTH')).toBeInTheDocument()

      await userEvent.click(screen.getByRole('button', { name: 'Restore tile' }))
      expect(screen.getAllByTestId('stream-tile')).toHaveLength(2)
    })
  })

  describe('Tile overlays', () => {
    const streamingState = (device: ReturnType<typeof createMockDevice>, over: Record<string, unknown> = {}, config: Record<string, unknown> = {}) =>
      createMockDeviceState(device, {
        streamConfigs: [createMockStreamConfig({ enable: true, ...config })],
        isStreaming: true,
        sensorStreamingStatus: {
          'test-device-1-sensor-0': { sensor_id: 'test-device-1-sensor-0', name: '', is_streaming: true, stream_types: ['depth'], ...over },
        },
      })

    it('shows the pause button and a Paused overlay while the sensor is paused', () => {
      const device = createMockDevice()
      render(<StreamViewer />, { initialStoreState: { deviceStates: { [device.device_id]: streamingState(device, { paused: true }) } } })
      expect(screen.getByText(/Paused/)).toBeInTheDocument()
      expect(screen.getByRole('button', { name: 'Resume sensor' })).toBeInTheDocument()
    })

    it('flags a stalled stream from the server clocks', () => {
      const device = createMockDevice()
      const ds = streamingState(device)
      ds.streamMetadata = { depth: { stream_type: 'depth', timestamp: 0, frame_number: 1, width: 640, height: 480, received_at: 100 } }
      ds.metadataServerTime = 105
      render(<StreamViewer />, { initialStoreState: { deviceStates: { [device.device_id]: ds } } })
      expect(screen.getByText('No frames received!')).toBeInTheDocument()
    })

    it('does not flag a fresh stream', () => {
      const device = createMockDevice()
      const ds = streamingState(device)
      ds.streamMetadata = { depth: { stream_type: 'depth', timestamp: 0, frame_number: 1, width: 640, height: 480, received_at: 104.5 } }
      ds.metadataServerTime = 105
      render(<StreamViewer />, { initialStoreState: { deviceStates: { [device.device_id]: ds } } })
      expect(screen.queryByText('No frames received!')).not.toBeInTheDocument()
    })

    it('offers a snapshot download for the stream', () => {
      const device = createMockDevice()
      render(<StreamViewer />, { initialStoreState: { deviceStates: { [device.device_id]: streamingState(device) } } })
      const link = screen.getByRole('link', { name: 'Save snapshot' })
      expect(link).toHaveAttribute('href', `/api/v1/devices/${device.device_id}/stream/snapshot?stream=depth`)
      expect(link).toHaveAttribute('download')
    })

    it('shows the max usable range while the depth sensor estimates it', async () => {
      server.use(http.get('/api/v1/devices/:deviceId/stream/max-usable-range', () =>
        HttpResponse.json({ supported: true, enabled: true, range_m: 4.5 })))
      const device = createMockDevice()
      render(<StreamViewer />, { initialStoreState: { deviceStates: { [device.device_id]: streamingState(device) } } })
      await waitFor(() => expect(screen.getByText('Max usable range: 4.500 m')).toBeInTheDocument())
    })

    it('says so for a format the viewer cannot render', () => {
      const device = createMockDevice()
      render(<StreamViewer />, { initialStoreState: { deviceStates: { [device.device_id]: streamingState(device, {}, { format: 'RAW16' }) } } })
      expect(screen.getByText(/Rendering not supported for RAW16/)).toBeInTheDocument()
    })
  })

  describe('Streaming Status', () => {
    it('hides the tile and shows the empty state when not streaming', () => {
      const device = createMockDevice()
      const config = createMockStreamConfig({ enable: true })

      const notStreamingState = createMockDeviceState(device, {
        isStreaming: false,
        streamConfigs: [config],
      })

      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: notStreamingState },
        },
      })

      expect(screen.getByText('Nothing is streaming!')).toBeInTheDocument()
      expect(document.querySelector('video.stream-video')).toBeNull()
    })
  })

  describe('Tile hiding until streaming', () => {
    it('renders tile when stream_type is in the active list (case-insensitive)', () => {
      const device = createMockDevice()
      const config = createMockStreamConfig({
        stream_type: 'INFRARED-1',
        sensor_id: 'sensor-0',
        enable: true,
      })
      const deviceState = createMockDeviceState(device, {
        streamConfigs: [config],
        sensorStreamingStatus: {
          'sensor-0': {
            sensor_id: 'sensor-0',
            name: 'Stereo Module',
            is_streaming: true,
            stream_types: ['depth', 'infrared-1'],
          },
        },
      })

      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: deviceState },
        },
      })

      expect(document.querySelectorAll('video.stream-video')).toHaveLength(1)
    })

    it('hides tile when stream_type is not in the active list', () => {
      const device = createMockDevice()
      const config = createMockStreamConfig({
        stream_type: 'color',
        sensor_id: 'sensor-0',
        enable: true,
      })
      const deviceState = createMockDeviceState(device, {
        streamConfigs: [config],
        sensorStreamingStatus: {
          'sensor-0': {
            sensor_id: 'sensor-0',
            name: 'Stereo Module',
            is_streaming: true,
            stream_types: ['depth'],
          },
        },
      })

      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: deviceState },
        },
      })

      expect(screen.getByText('Nothing is streaming!')).toBeInTheDocument()
      expect(document.querySelector('video.stream-video')).toBeNull()
    })
  })

  describe('Stream Types', () => {
    it('handles color stream type', () => {
      const device = createMockDevice()
      const config = createMockStreamConfig({ stream_type: 'color', format: 'RGB8', enable: true })
      const deviceState = createMockDeviceState(device, {
        streamConfigs: [config],
        isStreaming: true,
        sensorStreamingStatus: {
          'test-device-1-sensor-0': { sensor_id: 'test-device-1-sensor-0', name: '', is_streaming: true, stream_types: ['depth', 'color', 'infrared'] },
        },
      })
      
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: deviceState },
        },
      })
      
      expect(screen.getByText('COLOR')).toBeInTheDocument()
      expect(document.querySelectorAll('video.stream-video')).toHaveLength(1)
    })

    it('handles infrared stream type', () => {
      const device = createMockDevice()
      const config = createMockStreamConfig({ stream_type: 'infrared', format: 'Y8', enable: true })
      const deviceState = createMockDeviceState(device, {
        streamConfigs: [config],
        isStreaming: true,
        sensorStreamingStatus: {
          'test-device-1-sensor-0': { sensor_id: 'test-device-1-sensor-0', name: '', is_streaming: true, stream_types: ['depth', 'color', 'infrared'] },
        },
      })
      
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: deviceState },
        },
      })
      
      expect(screen.getByText('INFRARED')).toBeInTheDocument()
      expect(document.querySelectorAll('video.stream-video')).toHaveLength(1)
    })
  })
})
