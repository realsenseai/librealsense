// The 3D view's points, computed from the raw depth frame as the SDK's rs2::pointcloud does:
// each depth pixel is deprojected with the depth intrinsics and, to texture it, moved into the
// color camera with the extrinsics and projected with the color intrinsics. The math follows
// rs2_deproject_pixel_to_point, rs2_transform_point_to_point and rs2_project_point_to_pixel
// (src/rs.cpp) for the distortion models RealSense depth and color streams use.
import type { Frame } from '../api/frames'
import type { Extrinsics, Intrinsics } from '../api/types'

const EPSILON = 1.1920929e-7 // FLT_EPSILON

/** Each depth pixel's ray at 1 m, as (x, y) pairs: rs2_deproject_pixel_to_point with depth 1. */
export function deprojectionRays(intr: Intrinsics): Float32Array {
  const { width, height, ppx, ppy, fx, fy, model } = intr
  const [k1, k2, p1, p2, k3] = intr.coeffs
  const undistort = (model === 'brown_conrady' || model === 'inverse_brown_conrady')
    && intr.coeffs.some(c => Math.abs(c) >= EPSILON)
  const inverse = model === 'inverse_brown_conrady'
  const rays = new Float32Array(width * height * 2)
  for (let v = 0, o = 0; v < height; v++) {
    for (let u = 0; u < width; u++, o += 2) {
      const xo = (u - ppx) / fx
      const yo = (v - ppy) / fy
      let x = xo
      let y = yo
      for (let i = 0; undistort && i < 10; i++) { // 10 iterations, as the SDK
        const r2 = x * x + y * y
        const icdist = 1 / (1 + ((k3 * r2 + k2) * r2 + k1) * r2)
        const xq = inverse ? x / icdist : x
        const yq = inverse ? y / icdist : y
        x = (xo - (2 * p1 * xq * yq + p2 * (r2 + 2 * xq * xq))) * icdist
        y = (yo - (2 * p2 * xq * yq + p1 * (r2 + 2 * yq * yq))) * icdist
      }
      rays[o] = x
      rays[o + 1] = y
    }
  }
  return rays
}

/** The color frame a cloud is textured with, and how depth points reach its pixels. */
export interface Texture {
  frame: Frame
  intrinsics: Intrinsics
  depthToColor: Extrinsics
}

// Color formats a point can take its color from; BGR in true colors, as the 2D view draws it
const TEXTURE_FORMATS = new Set(['rgb8', 'bgr8', 'rgba8', 'bgra8'])

/** The triangles of a mesh over the depth pixels: two per square of 4 neighbouring pixels, as the legacy viewer's make_grid */
export function gridIndex(width: number, height: number): Uint32Array {
  const index = new Uint32Array((width - 1) * (height - 1) * 6)
  for (let v = 0, o = 0; v < height - 1; v++) {
    for (let u = 0; u < width - 1; u++, o += 6) {
      const a = v * width + u
      index.set([a, a + width, a + 1, a + 1, a + width, a + width + 1], o)
    }
  }
  return index
}

// A pixel is no mesh corner when a neighbour's depth differs by more than this fraction of its own,
// as the legacy viewer's pc-shader (minDeltaZ): the mesh is not stretched over the edges of objects
const MESH_MAX_DELTA = 0.05

/** What each pixel is in `valid`: no point, a point only, or a point and a mesh corner */
export const NO_POINT = 0
export const POINT = 1
export const MESH = 2

/**
 * Writes each depth pixel's point, in three.js axes (y up, z toward the viewer), its color (0..1) and
 * what it is in `valid`. The color is the texture's where the point lands in it, else the depth color
 * table's, as the 2D view paints it. Returns the number of points.
 */
export function buildCloud(depth: Frame, rays: Float32Array, positions: Float32Array, colors: Float32Array, valid: Float32Array, texture?: Texture): number {
  const { width = 0, height = 0, stride = 0, bpp = 1, units = 0.001 } = depth.header
  const d = new Uint16Array(depth.pixels)
  const table = depth.colorTable
  const tex = texture && TEXTURE_FORMATS.has(texture.frame.header.format) ? texture : undefined
  const rgb = tex && new Uint8Array(tex.frame.pixels)
  const ci = tex?.intrinsics
  const { stride: cStride = 0, bpp: cBpp = 1 } = tex?.frame.header ?? {}
  const [red, blue] = tex?.frame.header.format.startsWith('bgr') ? [2, 0] : [0, 2]
  const [r0, r1, r2, r3, r4, r5, r6, r7, r8] = tex?.depthToColor.rotation ?? []
  const [t0, t1, t2] = tex?.depthToColor.translation ?? []
  const [k1, k2, p1, p2, k3] = ci?.coeffs ?? []
  const modified = ci?.model === 'modified_brown_conrady' || ci?.model === 'inverse_brown_conrady'
  const brown = ci?.model === 'brown_conrady'
  const pitch = stride / bpp
  let n = 0
  for (let v = 0; v < height; v++) {
    const row = v * pitch
    for (let u = 0; u < width; u++) {
      const i = v * width + u
      const value = d[row + u]
      if (!value) {
        valid[i] = NO_POINT
        continue
      }
      const limit = value * MESH_MAX_DELTA // neighbours past the frame's border count as the pixel itself
      valid[i] = Math.abs((u > 0 ? d[row + u - 1] : value) - value) > limit
        || Math.abs((u < width - 1 ? d[row + u + 1] : value) - value) > limit
        || Math.abs((v > 0 ? d[row - pitch + u] : value) - value) > limit
        || Math.abs((v < height - 1 ? d[row + pitch + u] : value) - value) > limit ? POINT : MESH
      const ray = i * 2
      const z = value * units
      const x = rays[ray] * z
      const y = rays[ray + 1] * z
      const o = i * 3
      positions[o] = x
      positions[o + 1] = -y
      positions[o + 2] = -z
      let c = -1 // offset of the point's color in rgb, when it lands in the color frame
      if (ci) {
        // rs2_transform_point_to_point: rotation is column-major
        const cx = r0 * x + r3 * y + r6 * z + t0
        const cy = r1 * x + r4 * y + r7 * z + t1
        const cz = r2 * x + r5 * y + r8 * z + t2
        let px = cx / cz
        let py = cy / cz
        if (modified || brown) { // rs2_project_point_to_pixel
          const rr = px * px + py * py
          const f = 1 + k1 * rr + k2 * rr * rr + k3 * rr * rr * rr
          const xf = px * f
          const yf = py * f
          const tx = modified ? xf : px
          const ty = modified ? yf : py
          px = xf + 2 * p1 * tx * ty + p2 * (rr + 2 * tx * tx)
          py = yf + 2 * p2 * tx * ty + p1 * (rr + 2 * ty * ty)
        }
        const cu = Math.round(px * ci.fx + ci.ppx)
        const cv = Math.round(py * ci.fy + ci.ppy)
        if (cz > 0 && cu >= 0 && cv >= 0 && cu < ci.width && cv < ci.height) c = cv * cStride + cu * cBpp
      }
      if (c >= 0) {
        colors[o] = rgb![c + red] / 255
        colors[o + 1] = rgb![c + 1] / 255
        colors[o + 2] = rgb![c + blue] / 255
      } else if (table) {
        colors[o] = table[value * 3] / 255
        colors[o + 1] = table[value * 3 + 1] / 255
        colors[o + 2] = table[value * 3 + 2] / 255
      } else {
        colors[o] = colors[o + 1] = colors[o + 2] = 1
      }
      n++
    }
  }
  return n
}
