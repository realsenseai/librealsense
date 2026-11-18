import { http, HttpResponse } from 'msw'
import { mockDeviceList, mockDevice } from './fixtures/devices'
import { mockSensors, mockDepthOptions, mockColorOptions, mockMotionOptions, mockFilters } from './fixtures/sensors'

const API_BASE = '/api/v1'

// Sensor options by sensor_index
const sensorOptionsMap: Record<string, any[]> = {
  0: mockDepthOptions,
  1: mockColorOptions,
  2: mockMotionOptions,
}

export const handlers = [
  // Health check
  http.get(`${API_BASE}/health`, () => {
    return HttpResponse.json({ sdk_version: '2.58.0', warnings: [] })
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
  http.get(`${API_BASE}/devices/:deviceId/sensors/:sensorIndex/options/`, ({ params }) => {
    return HttpResponse.json(sensorOptionsMap[params.sensorIndex as string] || [])
  }),

  // Set sensor option; answers it as the device now holds it, as every control write does
  http.put(`${API_BASE}/devices/:deviceId/sensors/:sensorIndex/options/:optionId`, async ({ params, request }) => {
    const body = await request.json() as { value: number }
    return HttpResponse.json({ option_id: params.optionId, current_value: body.value, read_only: false })
  }),

  // Post-processing filters, keyed by name; only the depth sensor has any
  http.get(`${API_BASE}/devices/:deviceId/sensors/:sensorIndex/filters/`, ({ params }) => {
    return HttpResponse.json(params.sensorIndex === '0' ? mockFilters : {})
  }),

  // Bypass or apply one filter; answers the state it now holds
  http.put(
    `${API_BASE}/devices/:deviceId/sensors/:sensorIndex/filters/:filterName/enabled/`,
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

  // Per-sensor streaming: open / start / stop / close return no body
  http.post(`${API_BASE}/devices/:deviceId/sensors/:sensorIndex/:step`, () => HttpResponse.json(null)),
  http.get(`${API_BASE}/devices/:deviceId/extrinsics`, () => HttpResponse.json([])),

  // Recommended firmware (none, unless a test says otherwise)
  http.get(`${API_BASE}/devices/:deviceId/firmware/`, () => HttpResponse.json({ recommended: null })),

]

