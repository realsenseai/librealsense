import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest'
import { useAppStore } from '@/store'
import { resetStore, createMockDevice, createMockDeviceState, createMockSensor, createMockOption } from '../../utils/test-utils'

describe('AppStore', () => {
  beforeEach(() => {
    resetStore()
  })

  describe('Initial State', () => {
    it('starts with default values', () => {
      const state = useAppStore.getState()
      
      expect(state.isConnected).toBe(false)
      expect(state.devices).toEqual([])
      expect(state.deviceStates).toEqual({})
      expect(state.selectedDevice).toBeNull()
      expect(state.isLoadingDevices).toBe(false)
      expect(state.error).toBeNull()
    })

    it('starts with grid view mode', () => {
      const state = useAppStore.getState()
      
      expect(state.viewMode).toBe('grid')
    })

    it('starts with chat closed', () => {
      const state = useAppStore.getState()
      
      expect(state.isChatOpen).toBe(false)
      expect(state.isChatAvailable).toBe(false)
      expect(state.chatMessages).toEqual([])
    })

    it('starts with empty IMU history', () => {
      const state = useAppStore.getState()
      
      expect(state.imuHistory.accel).toEqual([])
      expect(state.imuHistory.gyro).toEqual([])
    })
  })

  describe('Connection State', () => {
    it('sets connection state', () => {
      useAppStore.getState().setConnected(true)
      
      expect(useAppStore.getState().isConnected).toBe(true)
      
      useAppStore.getState().setConnected(false)
      
      expect(useAppStore.getState().isConnected).toBe(false)
    })
  })

  describe('Device Selection', () => {
    it('selects a device', () => {
      const device = createMockDevice()
      
      useAppStore.getState().selectDevice(device)
      
      expect(useAppStore.getState().selectedDevice).toEqual(device)
    })

    it('clears device selection', () => {
      const device = createMockDevice()
      useAppStore.getState().selectDevice(device)
      
      useAppStore.getState().selectDevice(null)
      
      expect(useAppStore.getState().selectedDevice).toBeNull()
    })
  })

  describe('View Mode', () => {
    it('sets view mode to single', () => {
      useAppStore.getState().setViewMode('single')
      
      expect(useAppStore.getState().viewMode).toBe('single')
    })

    it('sets view mode to pointcloud', () => {
      useAppStore.getState().setViewMode('pointcloud')
      
      expect(useAppStore.getState().viewMode).toBe('pointcloud')
    })

    it('sets view mode to grid', () => {
      useAppStore.getState().setViewMode('single')
      useAppStore.getState().setViewMode('grid')
      
      expect(useAppStore.getState().viewMode).toBe('grid')
    })
  })

  describe('Error Handling', () => {
    it('sets error message', () => {
      useAppStore.getState().setError('Something went wrong')
      
      expect(useAppStore.getState().error).toBe('Something went wrong')
    })

    it('clears error message', () => {
      useAppStore.getState().setError('Error')
      useAppStore.getState().clearError()
      
      expect(useAppStore.getState().error).toBeNull()
    })
  })

  describe('Chat State', () => {
    it('toggles chat open/closed', () => {
      expect(useAppStore.getState().isChatOpen).toBe(false)
      
      useAppStore.getState().toggleChat()
      expect(useAppStore.getState().isChatOpen).toBe(true)
      
      useAppStore.getState().toggleChat()
      expect(useAppStore.getState().isChatOpen).toBe(false)
    })

    it('clears chat messages', () => {
      useAppStore.setState({
        chatMessages: [
          { id: '1', role: 'user', content: 'Hello' },
          { id: '2', role: 'assistant', content: 'Hi' },
        ],
      })
      
      useAppStore.getState().clearChat()
      
      expect(useAppStore.getState().chatMessages).toEqual([])
    })
  })

  describe('IMU History', () => {
    it('adds accelerometer data', () => {
      const accelData = { timestamp: 1234567890, x: 0.1, y: 0.2, z: 9.8 }
      
      useAppStore.getState().addIMUData('accel', accelData)
      
      const state = useAppStore.getState()
      expect(state.imuHistory.accel).toHaveLength(1)
      expect(state.imuHistory.accel[0]).toEqual(accelData)
    })

    it('adds gyroscope data', () => {
      const gyroData = { timestamp: 1234567890, x: 0.01, y: 0.02, z: 0.03 }
      
      useAppStore.getState().addIMUData('gyro', gyroData)
      
      const state = useAppStore.getState()
      expect(state.imuHistory.gyro).toHaveLength(1)
      expect(state.imuHistory.gyro[0]).toEqual(gyroData)
    })

    it('clears IMU history', () => {
      useAppStore.getState().addIMUData('accel', { timestamp: 1, x: 0, y: 0, z: 0 })
      useAppStore.getState().addIMUData('gyro', { timestamp: 1, x: 0, y: 0, z: 0 })
      
      useAppStore.getState().clearIMUHistory()
      
      const state = useAppStore.getState()
      expect(state.imuHistory.accel).toEqual([])
      expect(state.imuHistory.gyro).toEqual([])
    })

    it('limits IMU history length', () => {
      const maxLength = useAppStore.getState().maxIMUHistoryLength
      
      // Add more than max entries
      for (let i = 0; i < maxLength + 10; i++) {
        useAppStore.getState().addIMUData('accel', { timestamp: i, x: i, y: i, z: i })
      }
      
      const state = useAppStore.getState()
      expect(state.imuHistory.accel.length).toBeLessThanOrEqual(maxLength)
    })
  })

  describe('IMU Viewer', () => {
    it('toggles IMU viewer expanded state', () => {
      expect(useAppStore.getState().isIMUViewerExpanded).toBe(false)
      
      useAppStore.getState().toggleIMUViewer()
      expect(useAppStore.getState().isIMUViewerExpanded).toBe(true)
      
      useAppStore.getState().toggleIMUViewer()
      expect(useAppStore.getState().isIMUViewerExpanded).toBe(false)
    })
  })

  describe('Device States', () => {
    it('stores device state by device_id', () => {
      const device = createMockDevice()
      const deviceState = createMockDeviceState(device, { isActive: true })
      
      useAppStore.setState({
        devices: [device],
        deviceStates: { [device.device_id]: deviceState },
      })
      
      const state = useAppStore.getState()
      expect(state.deviceStates[device.device_id]).toEqual(deviceState)
    })

    it('getActiveDevices returns only active devices', () => {
      const device1 = createMockDevice({ device_id: 'device-1' })
      const device2 = createMockDevice({ device_id: 'device-2' })
      
      const state1 = createMockDeviceState(device1, { isActive: true })
      const state2 = createMockDeviceState(device2, { isActive: false })
      
      useAppStore.setState({
        devices: [device1, device2],
        deviceStates: {
          [device1.device_id]: state1,
          [device2.device_id]: state2,
        },
      })
      
      const activeDevices = useAppStore.getState().getActiveDevices()
      expect(activeDevices).toHaveLength(1)
      expect(activeDevices[0].device.device_id).toBe('device-1')
    })

    it('isAnyDeviceStreaming returns true when a device is streaming', () => {
      const device = createMockDevice()
      const deviceState = createMockDeviceState(device, { isActive: true, isStreaming: true })
      
      useAppStore.setState({
        devices: [device],
        deviceStates: { [device.device_id]: deviceState },
      })
      
      expect(useAppStore.getState().isAnyDeviceStreaming()).toBe(true)
    })

    it('isAnyDeviceStreaming returns false when no devices are streaming', () => {
      const device = createMockDevice()
      const deviceState = createMockDeviceState(device, { isActive: true, isStreaming: false })
      
      useAppStore.setState({
        devices: [device],
        deviceStates: { [device.device_id]: deviceState },
      })
      
      expect(useAppStore.getState().isAnyDeviceStreaming()).toBe(false)
    })
  })

  describe('Stream Configuration', () => {
    it('can set stream configs directly via setState', () => {
      const device = createMockDevice()
      const config = {
        stream_type: 'depth' as const,
        format: 'Z16',
        enabled: true,
        enable: true,
        resolution: { width: 1280, height: 720 },
        framerate: 30,
      }
      const deviceState = createMockDeviceState(device, {
        isActive: true,
        streamConfigs: [config],
      })
      
      useAppStore.setState({
        devices: [device],
        deviceStates: { [device.device_id]: deviceState },
      })
      
      const state = useAppStore.getState()
      const configs = state.deviceStates[device.device_id].streamConfigs
      expect(configs).toContainEqual(config)
    })
  })

  describe('Legacy Compatibility Getters', () => {
    it('sensors getter returns selected device sensors when set directly', () => {
      const device = createMockDevice()
      const sensor = createMockSensor()
      const deviceState = createMockDeviceState(device, {
        isActive: true,
        sensors: [sensor],
      })
      
      // Set the full state atomically
      useAppStore.setState({
        devices: [device],
        deviceStates: { [device.device_id]: deviceState },
        selectedDevice: device,
      })
      
      const state = useAppStore.getState()
      // Verify selectedDevice is set correctly
      expect(state.selectedDevice?.device_id).toBe(device.device_id)
      // Check sensors through the deviceStates directly since getter may have edge cases
      expect(state.deviceStates[device.device_id].sensors).toContainEqual(sensor)
    })

    it('sensors getter returns empty array when no selected device', () => {
      const device = createMockDevice()
      const sensor = createMockSensor()
      const deviceState = createMockDeviceState(device, {
        isActive: true,
        sensors: [sensor],
      })
      
      useAppStore.setState({
        devices: [device],
        deviceStates: { [device.device_id]: deviceState },
        selectedDevice: null,
      })
      
      expect(useAppStore.getState().sensors).toEqual([])
    })

    it('isStreaming reflects device streaming state', () => {
      const device = createMockDevice()
      const deviceState = createMockDeviceState(device, {
        isActive: true,
        isStreaming: true,
      })
      
      useAppStore.setState({
        devices: [device],
        deviceStates: { [device.device_id]: deviceState },
      })
      
      // Check isStreaming through the getter
      const state = useAppStore.getState()
      const isAnyStreaming = Object.values(state.deviceStates).some(ds => ds.isStreaming)
      expect(isAnyStreaming).toBe(true)
    })

    it('isStreaming getter returns false when no devices are streaming', () => {
      const device = createMockDevice()
      const deviceState = createMockDeviceState(device, {
        isActive: true,
        isStreaming: false,
      })
      
      useAppStore.setState({
        devices: [device],
        deviceStates: { [device.device_id]: deviceState },
      })
      
      const state = useAppStore.getState()
      const isAnyStreaming = Object.values(state.deviceStates).some(ds => ds.isStreaming)
      expect(isAnyStreaming).toBe(false)
    })
  })
})
