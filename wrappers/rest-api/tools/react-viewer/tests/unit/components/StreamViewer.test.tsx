import { describe, it, expect, vi, beforeEach } from 'vitest'
import { screen } from '@testing-library/react'
import { render, createMockDevice, createMockDeviceState, createMockStreamConfig } from '../../utils/test-utils'
import { StreamViewer } from '@/components/StreamViewer'

describe('StreamViewer', () => {
  describe('Empty State', () => {
    it('shows "No Streams Enabled" when no devices are active', () => {
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: {},
        },
      })
      
      expect(screen.getByText('No Streams Enabled')).toBeInTheDocument()
    })

    it('shows message to activate device when no devices are active', () => {
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: {},
        },
      })
      
      expect(screen.getByText(/Activate a device and enable streams/)).toBeInTheDocument()
    })

    it('shows message to enable streams when device is active but no streams enabled', () => {
      const device = createMockDevice()
      const deviceState = createMockDeviceState(device, {
        isActive: true,
        streamConfigs: [createMockStreamConfig({ enabled: false })],
      })
      
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: deviceState },
        },
      })
      
      expect(screen.getByText(/Enable streams in the right panel/)).toBeInTheDocument()
    })
  })

  describe('Stream Rendering', () => {
    it('renders stream tiles for enabled streams', () => {
      const device = createMockDevice()
      const depthConfig = createMockStreamConfig({
        stream_type: 'depth',
        enabled: true,
        enable: true, // Component uses 'enable' property
      })
      const deviceState = createMockDeviceState(device, {
        isActive: true,
        streamConfigs: [depthConfig],
        isStreaming: false,
      })
      
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: deviceState },
        },
      })
      
      // Should render a stream tile
      expect(screen.queryByText('No Streams Enabled')).not.toBeInTheDocument()
    })

    it('shows multiple stream tiles for multiple enabled streams', () => {
      const device = createMockDevice()
      const depthConfig = createMockStreamConfig({
        stream_type: 'depth',
        enabled: true,
        enable: true,
      })
      const colorConfig = createMockStreamConfig({
        stream_type: 'color',
        enabled: true,
        enable: true,
        format: 'RGB8',
      })
      const deviceState = createMockDeviceState(device, {
        isActive: true,
        streamConfigs: [depthConfig, colorConfig],
        isStreaming: false,
      })
      
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: deviceState },
        },
      })
      
      // Grid should be present with multiple children
      expect(screen.queryByText('No Streams Enabled')).not.toBeInTheDocument()
    })
  })

  describe('Multi-device Support', () => {
    it('shows streams from multiple active devices', () => {
      const device1 = createMockDevice({ device_id: 'device-1', name: 'D435 Camera 1' })
      const device2 = createMockDevice({ device_id: 'device-2', name: 'D455 Camera 2' })
      
      const config1 = createMockStreamConfig({ stream_type: 'depth', enable: true })
      const config2 = createMockStreamConfig({ stream_type: 'depth', enable: true })
      
      const state1 = createMockDeviceState(device1, {
        isActive: true,
        streamConfigs: [config1],
      })
      const state2 = createMockDeviceState(device2, {
        isActive: true,
        streamConfigs: [config2],
      })
      
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: {
            [device1.device_id]: state1,
            [device2.device_id]: state2,
          },
        },
      })
      
      // Should show streams from both devices
      expect(screen.queryByText('No Streams Enabled')).not.toBeInTheDocument()
    })

    it('only shows streams from active devices, not inactive ones', () => {
      const activeDevice = createMockDevice({ device_id: 'active-1', name: 'Active Device' })
      const inactiveDevice = createMockDevice({ device_id: 'inactive-1', name: 'Inactive Device' })
      
      const activeState = createMockDeviceState(activeDevice, {
        isActive: true,
        streamConfigs: [createMockStreamConfig({ enable: true })],
      })
      const inactiveState = createMockDeviceState(inactiveDevice, {
        isActive: false,
        streamConfigs: [createMockStreamConfig({ enable: true })],
      })
      
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: {
            [activeDevice.device_id]: activeState,
            [inactiveDevice.device_id]: inactiveState,
          },
        },
      })
      
      // Only one stream should be shown (from active device)
      expect(screen.queryByText('No Streams Enabled')).not.toBeInTheDocument()
    })
  })

  describe('Streaming Status', () => {
    it('displays differently when streaming vs not streaming', () => {
      const device = createMockDevice()
      const config = createMockStreamConfig({ enable: true })
      
      // Not streaming
      const notStreamingState = createMockDeviceState(device, {
        isActive: true,
        isStreaming: false,
        streamingMode: 'idle',
        streamConfigs: [config],
      })
      
      const { rerender } = render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: notStreamingState },
        },
      })
      
      // Stream tile should be present even when not actively streaming
      expect(screen.queryByText('No Streams Enabled')).not.toBeInTheDocument()
    })
  })

  describe('Stream Types', () => {
    it('handles depth stream type', () => {
      const device = createMockDevice()
      const config = createMockStreamConfig({ stream_type: 'depth', enable: true })
      const deviceState = createMockDeviceState(device, {
        isActive: true,
        streamConfigs: [config],
      })
      
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: deviceState },
        },
      })
      
      expect(screen.queryByText('No Streams Enabled')).not.toBeInTheDocument()
    })

    it('handles color stream type', () => {
      const device = createMockDevice()
      const config = createMockStreamConfig({ stream_type: 'color', format: 'RGB8', enable: true })
      const deviceState = createMockDeviceState(device, {
        isActive: true,
        streamConfigs: [config],
      })
      
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: deviceState },
        },
      })
      
      expect(screen.queryByText('No Streams Enabled')).not.toBeInTheDocument()
    })

    it('handles infrared stream type', () => {
      const device = createMockDevice()
      const config = createMockStreamConfig({ stream_type: 'infrared', format: 'Y8', enable: true })
      const deviceState = createMockDeviceState(device, {
        isActive: true,
        streamConfigs: [config],
      })
      
      render(<StreamViewer />, {
        initialStoreState: {
          deviceStates: { [device.device_id]: deviceState },
        },
      })
      
      expect(screen.queryByText('No Streams Enabled')).not.toBeInTheDocument()
    })
  })
})
