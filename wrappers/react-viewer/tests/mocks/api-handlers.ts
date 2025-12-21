import { http, HttpResponse } from 'msw'
import { mockDeviceList, mockDevice } from './fixtures/devices'

const API_BASE = 'http://localhost:8000'

export const handlers = [
  // Get devices list
  http.get(`${API_BASE}/api/devices/`, () => {
    return HttpResponse.json(mockDeviceList)
  }),

  // Get single device
  http.get(`${API_BASE}/api/devices/:deviceId`, ({ params }) => {
    const device = mockDeviceList.find((d) => d.device_id === params.deviceId)
    if (!device) {
      return new HttpResponse(null, { status: 404 })
    }
    return HttpResponse.json(device)
  }),

  // Start streaming
  http.post(`${API_BASE}/api/devices/:deviceId/stream/start`, () => {
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

  // Stop streaming
  http.post(`${API_BASE}/api/devices/:deviceId/stream/stop`, () => {
    return HttpResponse.json({
      is_streaming: false,
      active_streams: [],
      stopping: false,
    })
  }),

  // Get stream status
  http.get(`${API_BASE}/api/devices/:deviceId/stream/status`, () => {
    return HttpResponse.json({
      is_streaming: true,
      active_streams: ['depth'],
      stopping: false,
    })
  }),

  // Get depth range
  http.get(`${API_BASE}/api/devices/:deviceId/stream/depth-range`, () => {
    return HttpResponse.json({
      min_depth: 0.3,
      max_depth: 3.5,
    })
  }),

  // Get depth at pixel
  http.get(`${API_BASE}/api/devices/:deviceId/stream/depth-at-pixel`, ({ request }) => {
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
  http.post(`${API_BASE}/api/devices/:deviceId/point_cloud/activate`, () => {
    return HttpResponse.json({
      device_id: mockDevice.device_id,
      is_active: true,
    })
  }),

  // Get sensors
  http.get(`${API_BASE}/api/devices/:deviceId/sensors/`, () => {
    return HttpResponse.json([
      {
        sensor_id: 0,
        name: 'Stereo Module',
        streams: [
          { stream_type: 'depth', formats: ['Z16'], resolutions: [[640, 480], [1280, 720]], fps_options: [30, 60] },
          { stream_type: 'infrared', formats: ['Y8', 'Y16'], resolutions: [[640, 480]], fps_options: [30] },
        ],
      },
    ])
  }),
]
