/** The legacy measurement tool's math (common/measurement.cpp): pick a cloud point under the
 * mouse ray, chain distances, polygon area. Scene coordinates throughout (see camera.ts). */
import type { CameraIntrinsics } from '../api/types'
import { deproject, toScene, type DepthImage, type Vec3 } from './camera'

export interface Ray {
  origin: Vec3
  direction: Vec3 // unit length
}

const sub = (a: Vec3, b: Vec3): Vec3 => [a[0] - b[0], a[1] - b[1], a[2] - b[2]]
const dot = (a: Vec3, b: Vec3) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
const cross = (a: Vec3, b: Vec3): Vec3 => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
const len = (a: Vec3) => Math.sqrt(dot(a, a))

export function distance(a: Vec3, b: Vec3): number {
  return len(sub(a, b))
}

/** The cloud point closest to the ray, or null when nothing lies within `tolerance` meters
 * of it. Every `step`th pixel is tested (a 848x480 frame at step 2 is ~100k points). */
export function pickPoint(frame: DepthImage, intr: CameraIntrinsics, ray: Ray, tolerance = 0.03, step = 2): Vec3 | null {
  let best: Vec3 | null = null
  let bestDist = tolerance
  for (let v = 0; v < frame.height; v += step) {
    for (let u = 0; u < frame.width; u += step) {
      const d = frame.data[v * frame.width + u] * frame.units
      if (d <= 0) continue
      const p = toScene(deproject(intr, u, v, d))
      const rel = sub(p, ray.origin)
      const t = dot(rel, ray.direction)
      if (t <= 0) continue // behind the camera
      const perp = len(sub(rel, [ray.direction[0] * t, ray.direction[1] * t, ray.direction[2] * t]))
      if (perp < bestDist) {
        bestDist = perp
        best = p
      }
    }
  }
  return best
}

/** Area of the polygon through the points (measurement::calculate_area: fan triangulation). */
export function polygonArea(points: Vec3[]): number {
  if (points.length < 3) return 0
  let area = 0
  for (let i = 1; i + 1 < points.length; i++) {
    const a = sub(points[i], points[0])
    const b = sub(points[i + 1], points[0])
    area += len(cross(a, b)) / 2
  }
  return area
}

/** Distances along the chain and the total. */
export function chainLengths(points: Vec3[]): { segments: number[]; total: number } {
  const segments: number[] = []
  for (let i = 1; i < points.length; i++) segments.push(distance(points[i - 1], points[i]))
  return { segments, total: segments.reduce((s, d) => s + d, 0) }
}
