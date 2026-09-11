import { describe, it, expect } from 'vitest'
import { chainLengths, pickPoint, polygonArea } from '@/utils/measurement'

const intr = { width: 4, height: 4, fx: 1, fy: 1, ppx: 0, ppy: 0, model: 'brown_conrady', coeffs: [0, 0, 0, 0, 0] }

describe('measurement', () => {
  it('picks the cloud point under the ray and ignores far ones', () => {
    // pixel (1,1) at 2 m -> camera (2,2,2) -> scene (2,-2,-2); pixel (3,3) at 1 m -> (3,-3,-1)
    const data = new Uint16Array(16)
    data[1 * 4 + 1] = 2000
    data[3 * 4 + 3] = 1000
    const frame = { width: 4, height: 4, units: 0.001, data }
    const hit = pickPoint(frame, intr, { origin: [0, 0, 0], direction: [1 / Math.sqrt(3), -1 / Math.sqrt(3), -1 / Math.sqrt(3)] }, 0.05, 1)
    expect(hit).toEqual([2, -2, -2])
    expect(pickPoint(frame, intr, { origin: [0, 0, 0], direction: [0, 0, -1] }, 0.05, 1)).toBeNull()
  })

  it('chains distances and computes a polygon area', () => {
    const { segments, total } = chainLengths([[0, 0, 0], [1, 0, 0], [1, 1, 0]])
    expect(segments).toEqual([1, 1])
    expect(total).toBe(2)
    expect(polygonArea([[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]])).toBeCloseTo(1)
    expect(polygonArea([[0, 0, 0], [1, 0, 0]])).toBe(0)
  })
})
