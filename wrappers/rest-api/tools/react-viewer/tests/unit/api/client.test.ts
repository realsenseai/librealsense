import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest'
import { http, HttpResponse } from 'msw'
import { server } from '../../mocks/server'
import { mockDeviceList, mockDevice } from '../../mocks/fixtures/devices'
import { mockSensors, mockDepthOptions } from '../../mocks/fixtures/sensors'

// Helper: capture the query string the apiClient sends on /devices/
function captureDevicesQuery(): { current: string | null } {
  const seen = { current: null as string | null }
  server.use(
    http.get('/api/v1/devices/', ({ request }) => {
      seen.current = new URL(request.url).search
      return HttpResponse.json(mockDeviceList)
    })
  )
  return seen
}

// Note: apiClient is a singleton, so we need to import it fresh or reset state
// For these tests, we'll test the API endpoints through MSW handlers

describe('API Client', () => {
  // Import the client dynamically to ensure it uses the mocked fetch
  let apiClient: any

  beforeEach(async () => {
    // Dynamically import to get fresh instance with MSW active
    const module = await import('@/api/client')
    apiClient = module.apiClient
  })

  describe('getDevices', () => {
    it('fetches devices from the API', async () => {
      const devices = await apiClient.getDevices()
      
      expect(devices).toEqual(mockDeviceList)
      expect(devices.length).toBeGreaterThanOrEqual(1)
    })

    it('returns device properties correctly', async () => {
      const devices = await apiClient.getDevices()
      const device = devices.find((d: any) => d.device_id === mockDevice.device_id)

      expect(device).toBeDefined()
      expect(device.info.name).toBe('RealSense D435')
      expect(device.info.serial_number).toBe('123456789')
      expect(device.info.firmware_version).toBe('5.16.0.1')
    })
  })

  describe('getSensors', () => {
    it('fetches sensors for a device', async () => {
      const sensors = await apiClient.getSensors(mockDevice.device_id)
      
      expect(sensors).toEqual(mockSensors)
      expect(sensors).toHaveLength(3)
    })

    it('returns sensor profiles correctly', async () => {
      const sensors = await apiClient.getSensors(mockDevice.device_id)
      const depthSensor = sensors.find((s: any) => s.name === 'Stereo Module')
      
      expect(depthSensor).toBeDefined()
      if (depthSensor) {
        expect(depthSensor.name).toBe('Stereo Module')
        expect(depthSensor.stream_profiles).toBeDefined()
        expect(depthSensor.stream_profiles.length).toBeGreaterThan(0)
        // Check the first profile's stream_type
        const firstProfile = depthSensor.stream_profiles[0]
        expect(firstProfile.stream_type).toBeDefined()
      }
    })
  })

  describe('getSensorFilters', () => {
    it('answers the filter chain, keyed by filter name', async () => {
      const filters = await apiClient.getSensorFilters(mockDevice.device_id, 0)
      expect(Object.keys(filters).length).toBeGreaterThan(0)
      expect(filters['Decimation Filter'].options[0].option_id).toBe('filter_magnitude')
    })
  })

  describe('setControl', () => {
    it('writes the value to the control route', async () => {
      let written: unknown
      server.use(http.put('/api/v1/devices/:deviceId/sensors/0/options/exposure', async ({ request }) => {
        written = await request.json()
        return new HttpResponse(null)
      }))

      await apiClient.setControl(mockDevice.device_id, 'sensors/0/options', 'exposure', 10000)

      expect(written).toEqual({ value: 10000 })
    })
  })


  describe('Sensor Streaming', () => {
    it('opens the sensor with the listed profiles to start it', async () => {
      let body: unknown
      server.use(http.post('/api/v1/devices/:deviceId/sensors/:sensorIndex/open', async ({ request }) => {
        body = await request.json()
        return HttpResponse.json(null)
      }))
      const profile = mockSensors[0].stream_profiles[0]

      await apiClient.startSensor(mockDevice.device_id, 0, [profile])

      expect(body).toEqual({ profiles: [profile] })
    })

    it('closes a sensor that opened but would not start', async () => {
      const steps: string[] = []
      server.use(http.post('/api/v1/devices/:deviceId/sensors/:sensorIndex/:step', ({ params }) => {
        steps.push(params.step as string)
        return params.step === 'start' ? HttpResponse.json({ detail: 'failed' }, { status: 400 }) : HttpResponse.json(null)
      }))

      await expect(apiClient.startSensor(mockDevice.device_id, 0, [])).rejects.toThrow()

      expect(steps).toEqual(['open', 'start', 'close'])
    })

    it('stops sensor streaming', async () => {
      await expect(apiClient.stopSensor(mockDevice.device_id, 0)).resolves.toBeUndefined()
    })

    it('closes a sensor that was opened but not started', async () => {
      const steps: string[] = []
      server.use(http.post('/api/v1/devices/:deviceId/sensors/:sensorIndex/:step', ({ params }) => {
        steps.push(params.step as string)
        return params.step === 'stop' ? HttpResponse.json({ detail: 'not streaming' }, { status: 400 }) : HttpResponse.json(null)
      }))

      await apiClient.stopSensor(mockDevice.device_id, 0)

      expect(steps).toEqual(['stop', 'close'])
    })
  })

  describe('Firmware', () => {
    // A camera that goes to recovery mode on enter_update_state and back once written; the steps posted
    function camera(compatible: boolean, recovery = false) {
      const steps: string[] = []
      server.use(
        http.get('/api/v1/devices/:deviceId/is_update_device', () => HttpResponse.json(recovery)),
        http.post('/api/v1/devices/:deviceId/firmware/:step', ({ params }) => {
          steps.push(params.step as string)
          if (params.step === 'check_firmware_compatibility') return HttpResponse.json(compatible)
          recovery = params.step === 'enter_update_state'
          return HttpResponse.json(null)
        }),
      )
      return steps
    }

    it('updates through the SDK calls in turn: check, recovery mode, write, back', async () => {
      const steps = camera(true)
      await apiClient.updateFirmware(mockDevice.device_id, new Blob(['fw']))
      expect(steps).toEqual(['check_firmware_compatibility', 'enter_update_state', 'update'])
    })

    it('leaves a camera out of recovery mode when the image is refused', async () => {
      const steps = camera(false)
      await expect(apiClient.updateFirmware(mockDevice.device_id, new Blob(['fw'])))
        .rejects.toThrow('Firmware is not compatible with this device')
      expect(steps).toEqual(['check_firmware_compatibility'])
    })

    it('writes a camera already in recovery mode at once', async () => {
      const steps = camera(true, true)
      await apiClient.updateFirmware(mockDevice.device_id, new Blob(['fw']))
      expect(steps).toEqual(['update'])
    })
  })

  describe('getExtrinsics', () => {
    it('asks for one pair of streams, by query', async () => {
      let query = ''
      server.use(http.get('/api/v1/devices/:deviceId/extrinsics', ({ request }) => {
        query = new URL(request.url).search
        return HttpResponse.json({ rotation: [], translation: [0.015, 0, 0] })
      }))
      const depth = { stream_type: 'depth', stream_index: 0, format: 'z16', width: 848, height: 480, fps: 30, default: true }
      const color = { ...depth, stream_type: 'color', format: 'rgb8' }

      const e = await apiClient.getExtrinsics(mockDevice.device_id, depth, color)

      expect(query).toBe('?from_stream=depth&from_index=0&to_stream=color&to_index=0')
      expect(e.translation).toEqual([0.015, 0, 0])
    })
  })

  describe('resetDevice', () => {
    it('resets a device', async () => {
      await expect(apiClient.resetDevice(mockDevice.device_id)).resolves.not.toThrow()
    })
  })
})
