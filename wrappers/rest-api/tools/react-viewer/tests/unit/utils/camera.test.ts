import { describe, it, expect } from 'vitest'
import { deproject, depthAt, project, toScene, transform, unprojectImage } from '@/utils/camera'

const pinhole = { width: 640, height: 480, fx: 600, fy: 600, ppx: 320, ppy: 240, model: 'brown_conrady', coeffs: [0, 0, 0, 0, 0] }

describe('camera math', () => {
  it('deprojects the principal point straight ahead and projects it back', () => {
    expect(deproject(pinhole, 320, 240, 2)).toEqual([0, 0, 2])
    const p = deproject(pinhole, 620, 40, 1.5)
    expect(p[0]).toBeCloseTo(0.75)
    expect(p[1]).toBeCloseTo(-0.5)
    const [u, v] = project(pinhole, p)
    expect(u).toBeCloseTo(620)
    expect(v).toBeCloseTo(40)
  })

  it('applies Brown-Conrady distortion when projecting', () => {
    const distorted = { ...pinhole, model: 'inverse_brown_conrady', coeffs: [0.1, 0, 0, 0, 0] }
    const [u] = project(distorted, [0.5, 0, 1])
    expect(u).toBeGreaterThan(project(pinhole, [0.5, 0, 1])[0]) // k1 > 0 pushes the pixel outwards
  })

  it('transforms through column-major extrinsics', () => {
    const ext = { rotation: [0, 1, 0, -1, 0, 0, 0, 0, 1], translation: [0.1, 0, 0] } // 90 degrees about z
    const q = transform(ext, [1, 0, 0])
    expect(q[0]).toBeCloseTo(0.1)
    expect(q[1]).toBeCloseTo(1)
    expect(q[2]).toBeCloseTo(0)
  })

  it('flips into the scene frame and unprojects a whole image, skipping holes', () => {
    expect(toScene([1, 2, 3])).toEqual([1, -2, -3])
    const data = new Uint16Array(4)
    data[0] = 1000 // 1 m at (0,0)
    data[3] = 2000 // 2 m at (1,1)
    const frame = { width: 2, height: 2, units: 0.001, data }
    const pts = unprojectImage(frame, { ...pinhole, width: 2, height: 2, ppx: 0, ppy: 0, fx: 1, fy: 1 })
    expect(pts).toHaveLength(6)
    expect(Array.from(pts)).toEqual([0, -0, -1, 2, -2, -2])
    expect(depthAt(frame, 1, 0)).toBeNull()
    expect(depthAt(frame, 1, 1)).toBeCloseTo(2)
  })
})
