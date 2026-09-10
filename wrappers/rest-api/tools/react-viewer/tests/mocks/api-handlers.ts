import { http, HttpResponse } from 'msw'
import { mockDeviceList, mockDevice } from './fixtures/devices'
import { mockSensors, mockDepthOptions, mockColorOptions, mockMotionOptions, mockFilters } from './fixtures/sensors'
import { mockSettings } from './fixtures/settings'

const API_BASE = '/api/v1'

// Map of sensor options by sensor_id suffix
const sensorOptionsMap: Record<string, any[]> = {
  'sensor-0': mockDepthOptions,
  'sensor-1': mockColorOptions,
  'sensor-2': mockMotionOptions,
}

export const handlers = [
  http.get(`${API_BASE}/settings/`, () => HttpResponse.json(mockSettings)),
  http.put(`${API_BASE}/settings/`, async ({ request }) => {
    const patch = (await request.json()) as Record<string, Record<string, unknown>>
    const merged = structuredClone(mockSettings) as unknown as Record<string, Record<string, unknown>>
    for (const [group, values] of Object.entries(patch)) merged[group] = { ...merged[group], ...values }
    return HttpResponse.json(merged)
  }),
  // Health check
  http.get(`${API_BASE}/health`, () => {
    return HttpResponse.json({ status: 'ok', service: 'realsense-api' })
  }),

  // Get devices list
  http.get(`${API_BASE}/devices/`, () => {
    return HttpResponse.json(mockDeviceList)
  }),

  // Get single device
  http.get(`${API_BASE}/devices/:deviceId`, ({ params }) => {
    const device = mockDeviceList.find((d) => d.device_id === params.deviceId)
    if (!device) {
      return new HttpResponse(null, { status: 404 })
    }
    return HttpResponse.json(device)
  }),

  // Reset device
  http.post(`${API_BASE}/devices/:deviceId/hw_reset/`, () => {
    return HttpResponse.json(true)
  }),

  // Get sensors
  http.get(`${API_BASE}/devices/:deviceId/sensors/`, () => {
    return HttpResponse.json(mockSensors)
  }),

  // Get sensor options
  http.get(`${API_BASE}/devices/:deviceId/sensors/:sensorId/options/`, ({ params }) => {
    const sensorId = params.sensorId as string
    const sensorSuffix = sensorId.split('-').slice(-2).join('-') // e.g., 'sensor-0'
    const options = sensorOptionsMap[sensorSuffix] || []
    return HttpResponse.json(options)
  }),

  // Set sensor option; answers it as the device now holds it, as every control write does
  http.put(`${API_BASE}/devices/:deviceId/sensors/:sensorId/options/:optionId`, async ({ params, request }) => {
    const body = await request.json() as { value: number }
    return HttpResponse.json({ option_id: params.optionId, current_value: body.value, read_only: false })
  }),

  // Post-processing filters, keyed by name; only the depth sensor has any
  http.get(`${API_BASE}/devices/:deviceId/sensors/:sensorId/filters/`, ({ params }) => {
    const sensorId = params.sensorId as string
    return HttpResponse.json(sensorId.endsWith('sensor-0') ? mockFilters : {})
  }),

  // Bypass or apply one filter; answers the state it now holds
  http.put(
    `${API_BASE}/devices/:deviceId/sensors/:sensorId/filters/:filterName/enabled/`,
    async ({ params, request }) => {
      const body = await request.json() as { value: boolean }
      return HttpResponse.json({ name: params.filterName, enabled: body.value })
    }
  ),

  // Device-level controls: a list to read, one option at a time to write
  http.get(`${API_BASE}/devices/:deviceId/colorizer/`, () => HttpResponse.json([])),
  http.put(`${API_BASE}/devices/:deviceId/colorizer/:field/`, async ({ params, request }) => {
    const body = await request.json() as { value: number }
    return HttpResponse.json({
      option_id: params.field, current_value: body.value,
      default_value: body.value, min_value: 0, max_value: 100, read_only: false,
    })
  }),
  http.get(`${API_BASE}/devices/:deviceId/advanced_mode/controls/`, () => HttpResponse.json({})),
  http.put(
    `${API_BASE}/devices/:deviceId/advanced_mode/controls/:group/:field/`,
    async ({ params, request }) => {
      const body = await request.json() as { value: number }
      return HttpResponse.json({
        option_id: params.field, current_value: body.value,
        default_value: body.value, min_value: 0, max_value: 100, read_only: false,
      })
    }
  ),

  // Get depth range
  http.get(`${API_BASE}/devices/:deviceId/stream/depth-range`, () => {
    return HttpResponse.json({
      min_depth: 0.3,
      max_depth: 3.5,
    })
  }),

  // Get depth at pixel
  http.get(`${API_BASE}/devices/:deviceId/stream/depth-at-pixel`, ({ request }) => {
    const url = new URL(request.url)
    const x = url.searchParams.get('x')
    const y = url.searchParams.get('y')
    
    return HttpResponse.json({
      x: parseInt(x || '0'),
      y: parseInt(y || '0'),
      depth: 1.5,
    })
  }),

  // Activate point cloud
  http.post(`${API_BASE}/devices/:deviceId/point_cloud/activate`, () => {
    return HttpResponse.json({
      device_id: mockDevice.device_id,
      is_active: true,
    })
  }),

  // Deactivate point cloud
  http.post(`${API_BASE}/devices/:deviceId/point_cloud/deactivate`, () => {
    return HttpResponse.json({
      device_id: mockDevice.device_id,
      is_active: false,
    })
  }),

  // Per-sensor streaming: start sensor
  http.post(`${API_BASE}/devices/:deviceId/sensors/:sensorId/start`, async ({ params }) => {
    const sensorId = params.sensorId as string
    return HttpResponse.json({
      sensor_id: sensorId,
      name: 'Stereo Module',
      is_streaming: true,
      stream_type: 'depth',
      stream_types: ['depth'],
      resolution: { width: 640, height: 480 },
      framerate: 30,
      format: 'Z16',
      started_at: new Date().toISOString(),
    })
  }),

  // Per-sensor streaming: stop sensor
  http.post(`${API_BASE}/devices/:deviceId/sensors/:sensorId/stop`, async ({ params }) => {
    const sensorId = params.sensorId as string
    return HttpResponse.json({
      sensor_id: sensorId,
      name: 'Stereo Module',
      is_streaming: false,
    })
  }),

  // Per-sensor streaming: get sensor status
  http.get(`${API_BASE}/devices/:deviceId/sensors/:sensorId/status`, async ({ params }) => {
    const sensorId = params.sensorId as string
    return HttpResponse.json({
      sensor_id: sensorId,
      name: 'Stereo Module',
      is_streaming: false,
    })
  }),

  // Recommended firmware (none, unless a test says otherwise)
  http.get(`${API_BASE}/devices/:deviceId/firmware/`, () => HttpResponse.json({ recommended: null })),
]

