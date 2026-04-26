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
  http.post(`${API_BASE}/devices/:deviceId/reset`, () => {
    return HttpResponse.json({ success: true })
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

  // Start streaming (pipeline mode)
  http.post(`${API_BASE}/devices/:deviceId/stream/start`, () => {
    return HttpResponse.json({
      is_streaming: true,
      active_streams: ['depth'],
      timings: {
        refresh_devices: 0.0,
        device_lookup: 0.001,
        pipeline_config_init: 0.2,
        stream_enable: 0.001,
        pipeline_start: 0.5,
        post_start_setup: 0.001,
        thread_start: 0.001,
        total: 0.7,
      },
    })
  }),

  // Stop streaming (pipeline mode)
  http.post(`${API_BASE}/devices/:deviceId/stream/stop`, () => {
    return HttpResponse.json({
      is_streaming: false,
      active_streams: [],
      stopping: false,
    })
  }),

  // Get stream status
  http.get(`${API_BASE}/devices/:deviceId/stream/status`, () => {
    return HttpResponse.json({
      is_streaming: false,
      active_streams: [],
      stopping: false,
    })
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

  // Batch sensor start
  http.post(`${API_BASE}/devices/:deviceId/sensors/batch/start`, async ({ params }) => {
    const deviceId = params.deviceId as string
    return HttpResponse.json({
      device_id: deviceId,
      mode: 'sensor',
      sensors: [],
      errors: [],
    })
  }),

  // Batch sensor stop
  http.post(`${API_BASE}/devices/:deviceId/sensors/batch/stop`, async ({ params }) => {
    const deviceId = params.deviceId as string
    return HttpResponse.json({
      device_id: deviceId,
      mode: 'idle',
      sensors: [],
      errors: [],
    })
  }),

  // Batch sensor status
  http.get(`${API_BASE}/devices/:deviceId/sensors/batch/status`, async ({ params }) => {
    const deviceId = params.deviceId as string
    return HttpResponse.json({
      device_id: deviceId,
      mode: 'idle',
      sensors: [],
      errors: [],
    })
  }),

  // Firmware status
  http.get(`${API_BASE}/devices/:deviceId/firmware`, ({ params }) => {
    const device = mockDeviceList.find((d) => d.device_id === params.deviceId)
    return HttpResponse.json({
      device_id: params.deviceId,
      current: device?.firmware_version || '5.16.0.1',
      recommended: device?.recommended_firmware_version || '5.16.0.1',
      status: device?.firmware_status || 'up_to_date',
      file_available: device?.firmware_file_available ?? true,
    })
  }),

  // Firmware update
  http.post(`${API_BASE}/devices/:deviceId/firmware/update`, () => {
    return HttpResponse.json({
      device_id: mockDevice.device_id,
      progress: 0,
      status: 'started',
    })
  }),
]

