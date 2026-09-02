import { http, HttpResponse } from 'msw'
import { mockDeviceList, mockDevice } from './fixtures/devices'
import { mockSensors, mockDepthOptions, mockColorOptions, mockMotionOptions } from './fixtures/sensors'

const API_BASE = '/api/v1'

// Map of sensor options by sensor_id suffix
const sensorOptionsMap: Record<string, any[]> = {
  'sensor-0': mockDepthOptions,
  'sensor-1': mockColorOptions,
  'sensor-2': mockMotionOptions,
}

export const handlers = [
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

  // Set sensor option
  http.put(`${API_BASE}/devices/:deviceId/sensors/:sensorId/options/:optionId`, async ({ request }) => {
    const body = await request.json() as any
    return HttpResponse.json({ success: true, value: body.value })
  }),

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

