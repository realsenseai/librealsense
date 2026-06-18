import { describe, it, expect, vi, beforeEach } from 'vitest'
import { screen } from '@testing-library/react'
import { render, createMockDevice, createMockDeviceState, createMockStreamConfig } from '../../utils/test-utils'
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
        isActive: true,
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
        isActive: true,
        streamConfigs: [depthConfig],
        isStreaming: true,
        streamingMode: 'pipeline',
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
        isActive: true,
        streamConfigs: [depthConfig, colorConfig],
        isStreaming: true,
        streamingMode: 'pipeline',
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
        isActive: true,
        streamConfigs: [config1],
        isStreaming: true,
        streamingMode: 'pipeline',
      })
      const state2 = createMockDeviceState(device2, {
        isActive: true,
        streamConfigs: [config2],
        isStreaming: true,
        streamingMode: 'pipeline',
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

    it('only shows streams from active devices, not inactive ones', () => {
      const activeDevice = createMockDevice({ device_id: 'active-1', name: 'Active Device' })
      const inactiveDevice = createMockDevice({ device_id: 'inactive-1', name: 'Inactive Device' })
      
      const activeState = createMockDeviceState(activeDevice, {
        isActive: true,
        streamConfigs: [createMockStreamConfig({ enable: true })],
        isStreaming: true,
        streamingMode: 'pipeline',
      })
      const inactiveState = createMockDeviceState(inactiveDevice, {
        isActive: false,
        streamConfigs: [createMockStreamConfig({ enable: true })],
        isStreaming: true,
        streamingMode: 'pipeline',
      })
      
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: {
            [activeDevice.device_id]: activeState,
            [inactiveDevice.device_id]: inactiveState,
          },
        },
      })
      
      expect(document.querySelectorAll('video.stream-video')).toHaveLength(1)
    })
  })

  describe('Streaming Status', () => {
    it('hides the tile and shows the empty state when not streaming', () => {
      const device = createMockDevice()
      const config = createMockStreamConfig({ enable: true })

      const notStreamingState = createMockDeviceState(device, {
        isActive: true,
        isStreaming: false,
        streamingMode: 'idle',
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
    it('hides tile when stream is enabled but not actively streaming', () => {
      const device = createMockDevice()
      const config = createMockStreamConfig({ stream_type: 'depth', enable: true })
      const deviceState = createMockDeviceState(device, {
        isActive: true,
        isStreaming: false,
        streamingMode: 'pipeline',
        streamConfigs: [config],
      })

      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: deviceState },
        },
      })

      expect(screen.getByText('Nothing is streaming!')).toBeInTheDocument()
    })

    it('sensor mode: renders tile when stream_type is in active list (case-insensitive)', () => {
      const device = createMockDevice()
      const config = createMockStreamConfig({
        stream_type: 'INFRARED-1',
        sensor_id: 'sensor-0',
        enable: true,
      })
      const deviceState = createMockDeviceState(device, {
        isActive: true,
        streamingMode: 'sensor',
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

    it('sensor mode: hides tile when stream_type is not in active list', () => {
      const device = createMockDevice()
      const config = createMockStreamConfig({
        stream_type: 'color',
        sensor_id: 'sensor-0',
        enable: true,
      })
      const deviceState = createMockDeviceState(device, {
        isActive: true,
        streamingMode: 'sensor',
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
    it('handles depth stream type', () => {
      const device = createMockDevice()
      const config = createMockStreamConfig({ stream_type: 'depth', enable: true })
      const deviceState = createMockDeviceState(device, {
        isActive: true,
        streamConfigs: [config],
        isStreaming: true,
        streamingMode: 'pipeline',
      })
      
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: deviceState },
        },
      })
      
      expect(screen.getByText('DEPTH')).toBeInTheDocument()
      expect(document.querySelectorAll('video.stream-video')).toHaveLength(1)
    })

    it('handles color stream type', () => {
      const device = createMockDevice()
      const config = createMockStreamConfig({ stream_type: 'color', format: 'RGB8', enable: true })
      const deviceState = createMockDeviceState(device, {
        isActive: true,
        streamConfigs: [config],
        isStreaming: true,
        streamingMode: 'pipeline',
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
        isActive: true,
        streamConfigs: [config],
        isStreaming: true,
        streamingMode: 'pipeline',
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
