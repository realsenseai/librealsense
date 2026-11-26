import { describe, it, expect } from 'vitest'
import { http, HttpResponse } from 'msw'
import { server } from '../../mocks/server'
import { VERSIONS_DB_URL, recommendedFirmware } from '@/api/firmware'

// Cameras named before the wildcards they'd also match, as in the real versions list
const DB = [
  { device_name: 'Intel RealSense D455', policy_type: 'RECOMMENDED', component: 'FIRMWARE', version: '5.17.3.10', platform: '*', link: 'd455.bin' },
  { device_name: 'Intel RealSense D4*', policy_type: 'RECOMMENDED', component: 'FIRMWARE', version: '5.17.0.10', platform: '*', link: 'd4x.bin' },
  { device_name: 'Intel RealSense D4*', policy_type: 'RECOMMENDED', component: 'LIBREALSENSE', version: '2.58.2', platform: '*', link: 'sw' },
  { device_name: 'Intel RealSense D457', policy_type: 'ESSENTIAL', component: 'FIRMWARE', version: '9.9.9.9', platform: '*', link: 'x' },
  { device_name: 'Intel RealSense D415', policy_type: 'RECOMMENDED', component: 'FIRMWARE', version: '1.1.1.1', platform: 'Linux arm', link: 'arm.bin' },
]

describe('recommendedFirmware', () => {
  const pick = async (name: string) => {
    server.use(http.get(VERSIONS_DB_URL, () => HttpResponse.json({ versions: DB })))
    return (await recommendedFirmware(name))?.link
  }

  it('prefers the camera named over the wildcard', async () => expect(await pick('Intel RealSense D455')).toBe('d455.bin'))
  it('falls back to the wildcard', async () => expect(await pick('Intel RealSense D435')).toBe('d4x.bin'))
  it('takes only recommended firmware', async () => expect(await pick('Intel RealSense D457')).toBe('d4x.bin'))
  it('ignores the Intel prefix newer SDKs drop', async () => expect(await pick('RealSense D455')).toBe('d455.bin'))
  it('takes only entries for any platform', async () => expect(await pick('Intel RealSense D415')).toBe('d4x.bin'))
  it('has none for another camera', async () => expect(await pick('Some Other Camera')).toBeUndefined())
})
