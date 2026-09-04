import { describe, it, expect } from 'vitest'
import {
  AXES,
  ORIGIN_2D,
  VECTOR_THRESHOLD,
  WIRE_CIRCLES,
  motionVector,
  projectIMU,
} from '@/utils/imuOrientation'

// The reference is the C++ viewer's on-screen layout (common/rendering.h
// draw_motion_data): +X points up and left, +Z down and left, +Y straight down, and
// gravity — accel at rest, so mostly -Y — points up.
describe('projectIMU', () => {
  const axisTip = (key: 'x' | 'y' | 'z') =>
    projectIMU({ x: key === 'x' ? 1 : 0, y: key === 'y' ? 1 : 0, z: key === 'z' ? 1 : 0 })

  it('places +X up and to the left of the origin', () => {
    const [x, y] = axisTip('x')
    expect(x).toBeLessThan(ORIGIN_2D[0])
    expect(y).toBeLessThan(ORIGIN_2D[1])
  })

  it('places +Y straight down', () => {
    const [x, y] = axisTip('y')
    expect(x).toBeCloseTo(ORIGIN_2D[0], 6)
    expect(y).toBeGreaterThan(ORIGIN_2D[1])
  })

  it('places +Z down and to the left', () => {
    const [x, y] = axisTip('z')
    expect(x).toBeLessThan(ORIGIN_2D[0])
    expect(y).toBeGreaterThan(ORIGIN_2D[1])
  })

  it('puts gravity opposite the +Y axis', () => {
    const [, gravityY] = projectIMU({ x: 0, y: -1, z: 0 })
    const [, upY] = axisTip('y')
    expect(gravityY).toBeLessThan(ORIGIN_2D[1])
    expect(upY).toBeGreaterThan(ORIGIN_2D[1])
  })

  it('keeps X and Z symmetric about the vertical, as the -135° yaw implies', () => {
    const [xx] = axisTip('x')
    const [zx] = axisTip('z')
    expect(xx).toBeCloseTo(zx, 6)
  })
})

describe('static wireframe', () => {
  it('draws the three great circles as closed paths', () => {
    expect(WIRE_CIRCLES).toHaveLength(3)
    for (const path of WIRE_CIRCLES) {
      const points = path.split('L')
      expect(points.length).toBe(51)
      // A great circle comes back to where it started.
      expect(points[0].replace('M', '')).toBe(points[points.length - 1])
    }
  })

  it('gives every axis a line and two arrowhead triangles', () => {
    expect(AXES.map((a) => a.key)).toEqual(['x', 'y', 'z'])
    expect(AXES.map((a) => a.color)).toEqual(['#ff0000', '#00ff00', '#0000ff'])
    for (const axis of AXES) {
      expect(axis.heads).toHaveLength(2)
      for (const head of axis.heads) expect(head.endsWith('Z')).toBe(true)
    }
  })
})

describe('motionVector', () => {
  it('reports the magnitude and draws a unit-length vector', () => {
    const far = motionVector({ x: 0, y: -9.81, z: 0 })
    const near = motionVector({ x: 0, y: -1, z: 0 })

    expect(far.norm).toBeCloseTo(9.81, 3)
    // Normalised, exactly as the C++ code does, so a 9.81 and a 1.0 reading in the
    // same direction draw the same arrow and differ only in the printed number.
    expect(far.tip).toEqual(near.tip)
  })

  it('labels the magnitude at the midpoint of the vector', () => {
    const { tip, label } = motionVector({ x: 0, y: -9.81, z: 0 })
    expect(label[0]).toBeCloseTo((ORIGIN_2D[0] + tip![0]) / 2, 6)
    expect(label[1]).toBeCloseTo((ORIGIN_2D[1] + tip![1]) / 2, 6)
  })

  it('drops the vector below the threshold, where direction is only noise', () => {
    const still = motionVector({ x: 0.01, y: -0.02, z: 0.01 })
    expect(still.norm).toBeLessThan(VECTOR_THRESHOLD)
    expect(still.tip).toBeNull()
  })
})
