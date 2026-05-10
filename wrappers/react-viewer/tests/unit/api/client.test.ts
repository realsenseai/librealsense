import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest'
import { http, HttpResponse } from 'msw'
import { server } from '../../mocks/server'
import { mockDeviceList, mockDevice } from '../../mocks/fixtures/devices'
import { mockSensors, mockDepthOptions } from '../../mocks/fixtures/sensors'

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
      expect(device.name).toBe('Intel RealSense D435')
      expect(device.serial_number).toBe('123456789')
      expect(device.firmware_version).toBe('5.16.0.1')
    })
  })

  describe('getDevice', () => {
    it('fetches a single device by ID', async () => {
      const device = await apiClient.getDevice(mockDevice.device_id)
      
      expect(device).toEqual(mockDevice)
    })

    it('throws error for non-existent device', async () => {
      await expect(apiClient.getDevice('non-existent-id')).rejects.toThrow()
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
        expect(depthSensor.supported_stream_profiles).toBeDefined()
        expect(depthSensor.supported_stream_profiles.length).toBeGreaterThan(0)
        // Check the first profile's stream_type
        const firstProfile = depthSensor.supported_stream_profiles[0]
        expect(firstProfile.stream_type).toBeDefined()
      }
    })
  })

  describe('getOptions', () => {
    it('fetches options for a sensor', async () => {
      const options = await apiClient.getOptions(mockDevice.device_id, mockDevice.device_id + '-sensor-0')
      
      expect(options).toHaveLength(mockDepthOptions.length)
    })

    it('returns option properties correctly', async () => {
      const options = await apiClient.getOptions(mockDevice.device_id, mockDevice.device_id + '-sensor-0')
      const exposureOption = options.find((o: any) => o.option_id === 'exposure')
      
      expect(exposureOption).toBeDefined()
      expect(exposureOption.name).toBe('Exposure')
      expect(exposureOption.current_value).toBe(8500)
      expect(exposureOption.min_value).toBe(1)
      expect(exposureOption.max_value).toBe(165000)
    })

    it('returns post-processing filter options with category', async () => {
      const options = await apiClient.getOptions(mockDevice.device_id, mockDevice.device_id + '-sensor-0')
      const ppOptions = options.filter((o: any) => o.category === 'Post-Processing')
      
      expect(ppOptions.length).toBeGreaterThan(0)
      expect(ppOptions[0].filter_name).toBeDefined()
    })
  })

  describe('setOption', () => {
    it('sets an option value', async () => {
      const result = await apiClient.setOption(mockDevice.device_id, 'sensor-0', 'exposure', 10000)
      
      expect(result.success).toBe(true)
    })
  })

  describe('startStreaming', () => {
    it('starts streaming for a device', async () => {
      await expect(apiClient.startStreaming(mockDevice.device_id, {
        streams: [{ stream_type: 'depth', width: 640, height: 480, format: 'Z16', fps: 30 }],
      })).resolves.not.toThrow()
    })
  })

  describe('stopStreaming', () => {
    it('stops streaming for a device', async () => {
      const status = await apiClient.stopStreaming(mockDevice.device_id)
      
      expect(status.is_streaming).toBe(false)
      expect(status.active_streams).toEqual([])
    })
  })

  describe('getStreamStatus', () => {
    it('returns stream status', async () => {
      const status = await apiClient.getStreamStatus(mockDevice.device_id)
      
      expect(status).toHaveProperty('is_streaming')
      expect(status).toHaveProperty('active_streams')
    })
  })

  describe('getDepthRange', () => {
    it('returns depth range for a device', async () => {
      const range = await apiClient.getDepthRange(mockDevice.device_id)
      
      expect(range.min_depth).toBe(0.3)
      expect(range.max_depth).toBe(3.5)
    })
  })

  describe('getDepthAtPixel', () => {
    it('returns depth at specified pixel', async () => {
      const result = await apiClient.getDepthAtPixel(mockDevice.device_id, 320, 240)
      
      expect(result.x).toBe(320)
      expect(result.y).toBe(240)
      expect(result.depth).toBe(1.5)
    })
  })

  describe('Point Cloud', () => {
    it('enables point cloud', async () => {
      // enablePointCloud returns void, just check it doesn't throw
      await expect(apiClient.enablePointCloud(mockDevice.device_id)).resolves.not.toThrow()
    })

    it('disables point cloud', async () => {
      await expect(apiClient.disablePointCloud(mockDevice.device_id)).resolves.not.toThrow()
    })
  })

  describe('Sensor Streaming', () => {
    it('starts sensor streaming', async () => {
      const result = await apiClient.startSensor(
        mockDevice.device_id,
        'sensor-0',
        [{ stream_type: 'depth', width: 640, height: 480, format: 'Z16', fps: 30 }]
      )
      
      expect(result.is_streaming).toBe(true)
    })

    it('stops sensor streaming', async () => {
      const result = await apiClient.stopSensor(mockDevice.device_id, 'sensor-0')
      
      expect(result.is_streaming).toBe(false)
    })
  })

  describe('Firmware', () => {
    it('fetches firmware status', async () => {
      server.use(
        http.get('/api/v1/devices/:deviceId/status/', () => {
          return HttpResponse.json({
            device_id: mockDevice.device_id,
            current: '5.16.0.1',
            recommended: '5.16.0.1',
            status: 'up_to_date',
            file_available: true,
          })
        })
      )

      const status = await apiClient.getFirmwareStatus(mockDevice.device_id)
      
      expect(status.status).toBe('up_to_date')
      expect(status.current).toBe('5.16.0.1')
    })
  })

  describe('resetDevice', () => {
    it('resets a device', async () => {
      await expect(apiClient.resetDevice(mockDevice.device_id)).resolves.not.toThrow()
    })
  })
})
