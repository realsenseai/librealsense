import { describe, it, expect, vi, beforeEach } from 'vitest'
import { screen } from '@testing-library/react'
import { render, createMockDevice, createMockDeviceState, createMockStreamConfig, startedProfiles } from '../../utils/test-utils'
import { StreamViewer } from '@/components/StreamViewer'

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
        sensorStreamingStatus: {
          0: { sensor_index: 0, name: '', is_streaming: true, profiles: startedProfiles('depth', 'color', 'infrared') },
        },
      })
      
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: deviceState },
        },
      })
      
      expect(document.querySelectorAll('canvas.stream-video')).toHaveLength(1)
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
        sensorStreamingStatus: {
          0: { sensor_index: 0, name: '', is_streaming: true, profiles: startedProfiles('depth', 'color') },
        },
      })
      
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: deviceState },
        },
      })
      
      expect(document.querySelectorAll('canvas.stream-video')).toHaveLength(2)
      expect(screen.getByText('DEPTH')).toBeInTheDocument()
      expect(screen.getByText('COLOR')).toBeInTheDocument()
    })
  })

  describe('Multi-device Support', () => {
    it('shows streams from multiple active streaming devices', () => {
      const device1 = createMockDevice({ device_id: 'device-1', info: { name: 'D435 Camera 1' } })
      const device2 = createMockDevice({ device_id: 'device-2', info: { name: 'D455 Camera 2' } })
      
      const config1 = createMockStreamConfig({ stream_type: 'depth', enable: true })
      const config2 = createMockStreamConfig({ stream_type: 'depth', enable: true })
      
      const state1 = createMockDeviceState(device1, {
        streamConfigs: [config1],
        sensorStreamingStatus: {
          0: { sensor_index: 0, name: '', is_streaming: true, profiles: startedProfiles('depth', 'color', 'infrared') },
        },
      })
      const state2 = createMockDeviceState(device2, {
        streamConfigs: [config2],
        sensorStreamingStatus: {
          0: { sensor_index: 0, name: '', is_streaming: true, profiles: startedProfiles('depth', 'color', 'infrared') },
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
      
      expect(document.querySelectorAll('canvas.stream-video')).toHaveLength(2)
    })
  })

  describe('Streaming Status', () => {
    it('hides the tile and shows the empty state when not streaming', () => {
      const device = createMockDevice()
      const config = createMockStreamConfig({ enable: true })

      const notStreamingState = createMockDeviceState(device, {
        streamConfigs: [config],
      })

      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: notStreamingState },
        },
      })

      expect(screen.getByText('Nothing is streaming!')).toBeInTheDocument()
      expect(document.querySelector('canvas.stream-video')).toBeNull()
    })
  })

  describe('Tile hiding until streaming', () => {
    it('renders tile when its stream is among the started profiles', () => {
      const device = createMockDevice()
      const config = createMockStreamConfig({
        stream_type: 'infrared-1',
        sensor_index: 0,
        enable: true,
      })
      const deviceState = createMockDeviceState(device, {
        streamConfigs: [config],
        sensorStreamingStatus: {
          0: {
            sensor_index: 0,
            name: 'Stereo Module',
            is_streaming: true,
            profiles: startedProfiles('depth', 'infrared-1'),
          },
        },
      })

      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: deviceState },
        },
      })

      expect(document.querySelectorAll('canvas.stream-video')).toHaveLength(1)
    })

    it('hides tile when stream_type is not in the active list', () => {
      const device = createMockDevice()
      const config = createMockStreamConfig({
        stream_type: 'color',
        sensor_index: 0,
        enable: true,
      })
      const deviceState = createMockDeviceState(device, {
        streamConfigs: [config],
        sensorStreamingStatus: {
          0: {
            sensor_index: 0,
            name: 'Stereo Module',
            is_streaming: true,
            profiles: startedProfiles('depth'),
          },
        },
      })

      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: deviceState },
        },
      })

      expect(screen.getByText('Nothing is streaming!')).toBeInTheDocument()
      expect(document.querySelector('canvas.stream-video')).toBeNull()
    })
  })

  describe('Stream Types', () => {
    it('handles color stream type', () => {
      const device = createMockDevice()
      const config = createMockStreamConfig({ stream_type: 'color', format: 'RGB8', enable: true })
      const deviceState = createMockDeviceState(device, {
        streamConfigs: [config],
        sensorStreamingStatus: {
          0: { sensor_index: 0, name: '', is_streaming: true, profiles: startedProfiles('depth', 'color', 'infrared') },
        },
      })
      
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: deviceState },
        },
      })
      
      expect(screen.getByText('COLOR')).toBeInTheDocument()
      expect(document.querySelectorAll('canvas.stream-video')).toHaveLength(1)
    })

    it('handles infrared stream type', () => {
      const device = createMockDevice()
      const config = createMockStreamConfig({ stream_type: 'infrared-1', format: 'Y8', enable: true })
      const deviceState = createMockDeviceState(device, {
        streamConfigs: [config],
        sensorStreamingStatus: {
          0: { sensor_index: 0, name: '', is_streaming: true, profiles: startedProfiles('depth', 'color', 'infrared-1') },
        },
      })
      
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: deviceState },
        },
      })
      
      expect(screen.getByText('INFRARED-1')).toBeInTheDocument()
      expect(document.querySelectorAll('canvas.stream-video')).toHaveLength(1)
    })
  })
})
