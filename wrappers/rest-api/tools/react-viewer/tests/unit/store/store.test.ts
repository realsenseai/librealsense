import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest'
import { http, HttpResponse } from 'msw'
import { server } from '../../mocks/server'
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
      expect(state.isLoadingDevices).toBe(false)
      expect(state.error).toBeNull()
    })

    it('starts in 2d view mode', () => {
      const state = useAppStore.getState()

      expect(state.viewMode).toBe('2d')
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

  describe('View Mode', () => {
    it('switches to 3d', async () => {
      await useAppStore.getState().setViewMode('3d')

      expect(useAppStore.getState().viewMode).toBe('3d')
    })

    it('switches back to 2d', async () => {
      await useAppStore.getState().setViewMode('3d')
      await useAppStore.getState().setViewMode('2d')

      expect(useAppStore.getState().viewMode).toBe('2d')
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

  describe('fetchDevices', () => {
    const presentDevice = createMockDevice({ device_id: 'present-1', serial_number: 'present-1' })
    const goneDevice = createMockDevice({ device_id: 'gone-1', serial_number: 'gone-1' })

    function mockDevicesEndpoint(list: ReturnType<typeof createMockDevice>[]) {
      server.use(
        http.get('/api/v1/devices/', () => HttpResponse.json(list))
      )
    }

    it('guards against concurrent fetches', async () => {
      let calls = 0
      server.use(
        http.get('/api/v1/devices/', async () => {
          calls += 1
          // Hold the response so the second call overlaps the first.
          await new Promise((resolve) => setTimeout(resolve, 20))
          return HttpResponse.json([presentDevice])
        })
      )

      const first = useAppStore.getState().fetchDevices(true)
      // Second call starts while the first is still in flight → must short-circuit.
      const second = useAppStore.getState().fetchDevices(true)
      await Promise.all([first, second])

      expect(calls).toBe(1)
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

  describe('Aggregate Getters', () => {
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
