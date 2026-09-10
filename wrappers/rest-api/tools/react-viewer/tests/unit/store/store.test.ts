import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest'
import { http, HttpResponse } from 'msw'
import { server } from '../../mocks/server'
import { useAppStore } from '@/store'
import { firmwareStatus } from '@/api/types'
import { resetStore, createMockDevice, createMockDeviceState, createMockSensor, createMockOption } from '../../utils/test-utils'

describe('AppStore', () => {
  beforeEach(() => {
    resetStore()
  })

  describe('Firmware recommendation across enumeration', () => {
    // The device-list endpoint never consults the online versions DB, so it reports
    // neither a recommendation nor a status for any device.
    function enumerateWith(firmwareVersion: string) {
      const device = createMockDevice({
        device_id: 'fw-1',
        serial_number: 'fw-1',
        firmware_version: firmwareVersion,
      })
      server.use(http.get('/api/v1/devices/', () => HttpResponse.json([device])))
      return device
    }

    it('keeps a learned recommendation when the device list re-enumerates', async () => {
      const device = enumerateWith('5.17.0.10')
      useAppStore.setState({
        devices: [device],
        deviceStates: {
          'fw-1': createMockDeviceState(device, { firmware: { recommended: '5.17.3.10' } }),
        },
      })

      await useAppStore.getState().fetchDevices()

      expect(useAppStore.getState().deviceStates['fw-1'].firmware?.recommended).toBe('5.17.3.10')
    })

    it('the verdict follows the installed version without being recomputed', async () => {
      expect(firmwareStatus('5.17.0.10', '5.17.3.10')).toBe('outdated')
      expect(firmwareStatus('5.17.3.10', '5.17.3.10')).toBe('up_to_date')
      expect(firmwareStatus('5.17.3.25', '5.17.3.10')).toBe('up_to_date')
      expect(firmwareStatus(undefined, '5.17.3.10')).toBe('unknown')
      expect(firmwareStatus('5.17.3.10', undefined)).toBe('unknown')
    })
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

  describe('Device States', () => {
    it('stores device state by device_id', () => {
      const device = createMockDevice()
      const deviceState = createMockDeviceState(device)
      
      useAppStore.setState({
        devices: [device],
        deviceStates: { [device.device_id]: deviceState },
      })
      
      const state = useAppStore.getState()
      expect(state.deviceStates[device.device_id]).toEqual(deviceState)
    })

    it('getDeviceStates returns every connected device', () => {
      const device1 = createMockDevice({ device_id: 'device-1' })
      const device2 = createMockDevice({ device_id: 'device-2' })

      useAppStore.setState({
        devices: [device1, device2],
        deviceStates: {
          [device1.device_id]: createMockDeviceState(device1),
          [device2.device_id]: createMockDeviceState(device2),
        },
      })

      expect(useAppStore.getState().getDeviceStates().map((ds) => ds.device.device_id)).toEqual(['device-1', 'device-2'])
    })

    it('fetchDevices opens every newly connected device', async () => {
      await useAppStore.getState().fetchDevices()

      const states = useAppStore.getState().deviceStates
      expect(Object.keys(states)).toEqual(useAppStore.getState().devices.map((d) => d.device_id))
      expect(Object.values(states).every((ds) => ds.sensors.length > 0 && !ds.isLoading)).toBe(true)
    })

    it('isAnyDeviceStreaming returns true when a device is streaming', () => {
      const device = createMockDevice()
      const deviceState = createMockDeviceState(device, { isStreaming: true })
      
      useAppStore.setState({
        devices: [device],
        deviceStates: { [device.device_id]: deviceState },
      })
      
      expect(useAppStore.getState().isAnyDeviceStreaming()).toBe(true)
    })

    it('isAnyDeviceStreaming returns false when no devices are streaming', () => {
      const device = createMockDevice()
      const deviceState = createMockDeviceState(device, { isStreaming: false })
      
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

    it('never drops a caller: every fetch reaches the backend', async () => {
      const seen: (string | null)[] = []
      server.use(
        http.get('/api/v1/devices/', async ({ request }) => {
          seen.push(new URL(request.url).searchParams.get('force_refresh'))
          await new Promise((resolve) => setTimeout(resolve, 20))
          return HttpResponse.json([presentDevice])
        })
      )

      const cached = useAppStore.getState().fetchDevices(false)
      await useAppStore.getState().fetchDevices(true)
      await cached

      expect(seen).toEqual([null, 'true'])
      expect(useAppStore.getState().devices.map((d) => d.device_id)).toEqual(['present-1'])
    })

    it('ignores a response older than one already applied', async () => {
      let call = 0
      server.use(
        http.get('/api/v1/devices/', async () => {
          call += 1
          if (call === 1) {
            // First request is slow and returns a list the second one supersedes.
            await new Promise((resolve) => setTimeout(resolve, 40))
            return HttpResponse.json([goneDevice])
          }
          return HttpResponse.json([presentDevice])
        })
      )

      const stale = useAppStore.getState().fetchDevices()
      await useAppStore.getState().fetchDevices()
      await stale

      expect(useAppStore.getState().devices.map((d) => d.device_id)).toEqual(['present-1'])
    })
  })

  describe('Pause', () => {
    const streaming = (paused = false) => ({
      'test-device-1-sensor-0': { sensor_id: 'test-device-1-sensor-0', name: '', is_streaming: true, paused, stream_types: ['depth'] },
    })

    it('setSensorPaused stores the status the server answers', async () => {
      const device = createMockDevice()
      useAppStore.setState({ deviceStates: { [device.device_id]: createMockDeviceState(device, { sensorStreamingStatus: streaming() }) } })

      await useAppStore.getState().setSensorPaused(device.device_id, 'test-device-1-sensor-0', true)

      expect(useAppStore.getState().deviceStates[device.device_id].sensorStreamingStatus['test-device-1-sensor-0'].paused).toBe(true)
    })

    it('togglePauseAll pauses every running sensor, then resumes them all', async () => {
      const a = createMockDevice({ device_id: 'a' })
      const b = createMockDevice({ device_id: 'b' })
      useAppStore.setState({ deviceStates: {
        a: createMockDeviceState(a, { sensorStreamingStatus: streaming(false) }),
        b: createMockDeviceState(b, { sensorStreamingStatus: streaming(true) }),
      } })

      await useAppStore.getState().togglePauseAll()
      let states = useAppStore.getState().deviceStates
      expect([states.a, states.b].map((ds) => ds.sensorStreamingStatus['test-device-1-sensor-0'].paused)).toEqual([true, true])

      await useAppStore.getState().togglePauseAll()
      states = useAppStore.getState().deviceStates
      expect([states.a, states.b].map((ds) => ds.sensorStreamingStatus['test-device-1-sensor-0'].paused)).toEqual([false, false])
    })
  })

  describe('Stream Configuration', () => {
    it('starts a sensor with the profiles the SDK marks default', async () => {
      const device = createMockDevice({ device_id: '123456789' })
      useAppStore.setState({
        devices: [device],
        deviceStates: { [device.device_id]: createMockDeviceState(device) },
      })

      await useAppStore.getState().fetchSensors(device.device_id)

      const ds = useAppStore.getState().deviceStates[device.device_id]
      const depth = ds.streamConfigs.find((c) => c.stream_type === 'Depth')
      expect(depth).toMatchObject({ enable: true, format: 'Z16', resolution: { width: 848, height: 480 }, framerate: 30 })
      expect(ds.streamConfigs.find((c) => c.stream_type === 'Infrared')?.enable).toBe(false)
      expect(ds.sensorConfigs['123456789-sensor-0']).toMatchObject({ resolution: { width: 848, height: 480 }, framerate: 30 })
      expect(ds.streamConfigs.find((c) => c.stream_type === 'Accel')?.framerate).toBe(100)
    })

    it('falls back to depth/color/IMU at the first mode when no profile is default', async () => {
      const device = createMockDevice({ device_id: '123456789' })
      server.use(http.get('/api/v1/devices/:deviceId/sensors/', () => HttpResponse.json([
        createMockSensor({ sensor_id: '123456789-sensor-0', options: [], supported_stream_profiles: [
          { stream_type: 'Depth', resolutions: [[640, 480]], fps: [30], formats: ['Z16'] },
          { stream_type: 'Infrared', resolutions: [[640, 480]], fps: [30], formats: ['Y8'] },
        ] }),
      ])))
      useAppStore.setState({
        devices: [device],
        deviceStates: { [device.device_id]: createMockDeviceState(device) },
      })

      await useAppStore.getState().fetchSensors(device.device_id)

      const configs = useAppStore.getState().deviceStates[device.device_id].streamConfigs
      expect(configs.find((c) => c.stream_type === 'Depth')?.enable).toBe(true)
      expect(configs.find((c) => c.stream_type === 'Infrared')?.enable).toBe(false)
    })

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

    it('isAnyDeviceStreaming tracks deviceStates after a state update', () => {
      const device = createMockDevice()

      expect(useAppStore.getState().isAnyDeviceStreaming()).toBe(false)

      useAppStore.setState({
        devices: [device],
        deviceStates: {
          [device.device_id]: createMockDeviceState(device, { isStreaming: true }),
        },
      })

      expect(useAppStore.getState().isAnyDeviceStreaming()).toBe(true)
    })
  })
})
