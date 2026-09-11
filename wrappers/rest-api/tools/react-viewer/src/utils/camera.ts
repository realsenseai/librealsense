/** Camera geometry on the CPU: the SDK's rsutil deproject / project / transform, used for
 * picking and export. The GPU path (components/pointcloud/shaders.ts) does the same math. */
import type { CameraIntrinsics, Extrinsics } from '../api/types'

export type Vec3 = [number, number, number]

/** rs2_deproject_pixel_to_point (inverse Brown-Conrady undistorts; other models are treated as pinhole). */
export function deproject(intr: CameraIntrinsics, px: number, py: number, depth: number): Vec3 {
  let x = (px - intr.ppx) / intr.fx
  let y = (py - intr.ppy) / intr.fy
  if (intr.model === 'inverse_brown_conrady') {
    const [k1, k2, p1, p2, k3] = intr.coeffs
    const r2 = x * x + y * y
    const f = 1 + k1 * r2 + k2 * r2 * r2 + k3 * r2 * r2 * r2
    const ux = x * f + 2 * p1 * x * y + p2 * (r2 + 2 * x * x)
    const uy = y * f + 2 * p2 * x * y + p1 * (r2 + 2 * y * y)
    x = ux
    y = uy
  }
  return [depth * x, depth * y, depth]
}

/** rs2_project_point_to_pixel for the Brown-Conrady family (the D400 color and depth models). */
export function project(intr: CameraIntrinsics, p: Vec3): [number, number] {
  let x = p[0] / p[2]
  let y = p[1] / p[2]
  if (intr.model === 'modified_brown_conrady' || intr.model === 'inverse_brown_conrady' || intr.model === 'brown_conrady') {
    const [k1, k2, p1, p2, k3] = intr.coeffs
    const r2 = x * x + y * y
    const f = 1 + k1 * r2 + k2 * r2 * r2 + k3 * r2 * r2 * r2
    const xf = x * f
    const yf = y * f
    const dx = xf + 2 * p1 * x * y + p2 * (r2 + 2 * x * x)
    const dy = yf + 2 * p2 * x * y + p1 * (r2 + 2 * y * y)
    x = dx
    y = dy
  }
  return [x * intr.fx + intr.ppx, y * intr.fy + intr.ppy]
}

/** rs2_transform_point_to_point: column-major 3x3 rotation then translation. */
export function transform(extr: Extrinsics, p: Vec3): Vec3 {
  const r = extr.rotation
  const t = extr.translation
  return [
    r[0] * p[0] + r[3] * p[1] + r[6] * p[2] + t[0],
    r[1] * p[0] + r[4] * p[1] + r[7] * p[2] + t[1],
    r[2] * p[0] + r[5] * p[1] + r[8] * p[2] + t[2],
  ]
}

/** Camera-frame point to scene coordinates: the SDK looks down +Z with Y down, three.js
 * looks down -Z with Y up, so the cloud sits in front of the default camera. */
export function toScene(p: Vec3): Vec3 {
  return [p[0], -p[1], -p[2]]
}

export interface DepthImage {
  width: number
  height: number
  units: number
  data: Uint16Array
}

/** Every valid pixel as a scene-space point (x,y,z triplets); `step` decimates. */
export function unprojectImage(frame: DepthImage, intr: CameraIntrinsics, step = 1): Float32Array {
  const out: number[] = []
  for (let v = 0; v < frame.height; v += step) {
    for (let u = 0; u < frame.width; u += step) {
      const d = frame.data[v * frame.width + u] * frame.units
      if (d <= 0) continue
      const p = toScene(deproject(intr, u, v, d))
      out.push(p[0], p[1], p[2])
    }
  }
  return Float32Array.from(out)
}

/** Depth at a pixel in meters, or null for a hole. */
export function depthAt(frame: DepthImage, u: number, v: number): number | null {
  if (u < 0 || v < 0 || u >= frame.width || v >= frame.height) return null
  const d = frame.data[v * frame.width + u] * frame.units
  return d > 0 ? d : null
}
