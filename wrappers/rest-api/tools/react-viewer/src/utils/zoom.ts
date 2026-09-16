/** Zoom/pan state of a video tile: the normalized sub-rectangle of the frame being shown
 * (stream-model.cpp `normalized_zoom`). {0,0,1,1} is the whole frame. */
export interface Zoom {
  x: number
  y: number
  w: number
  h: number
}

export const FULL_FRAME: Zoom = { x: 0, y: 0, w: 1, h: 1 }
export const WHEEL_STEP = 0.1 // the legacy viewer zooms 10% per wheel notch
const MIN_SIZE = 0.05

export function isZoomed(z: Zoom): boolean {
  return z.w < 1 || z.h < 1
}

/** Keep the rectangle inside the frame, shrinking it if it is larger. */
function encloseInFrame(z: Zoom): Zoom {
  const w = Math.min(z.w, 1)
  const h = Math.min(z.h, 1)
  return { x: Math.min(Math.max(z.x, 0), 1 - w), y: Math.min(Math.max(z.y, 0), 1 - h), w, h }
}

/** Scale the rectangle by `factor` (< 1 zooms in) keeping the point under the cursor fixed.
 * `cx`/`cy` are the cursor as fractions of the displayed area. */
export function zoomAt(z: Zoom, cx: number, cy: number, factor: number): Zoom {
  let size = Math.min(1, Math.max(MIN_SIZE, z.w * factor))
  if (size > 1 - 1e-6) size = 1 // 1/1.1 then 1.1 must land back on the whole frame
  if (size === z.w) return z
  if (size === 1) return FULL_FRAME
  const px = z.x + cx * z.w
  const py = z.y + cy * z.h
  return encloseInFrame({ x: px - cx * size, y: py - cy * size, w: size, h: size })
}

/** Drag the view: `dx`/`dy` are the mouse movement as fractions of the displayed area,
 * and the content follows the cursor. */
export function pan(z: Zoom, dx: number, dy: number): Zoom {
  if (!isZoomed(z)) return z
  return encloseInFrame({ ...z, x: z.x - dx * z.w, y: z.y - dy * z.h })
}

/** CSS transform (origin top-left) that shows only `z` of the frame, which paints
 * `width` x `height` at (`offsetX`, `offsetY`) inside the element (its letterbox). */
export function zoomTransform(z: Zoom, width: number, height: number, offsetX = 0, offsetY = 0): string | undefined {
  if (!isZoomed(z)) return undefined
  const s = 1 / z.w
  const tx = offsetX * (1 - s) - s * z.x * width
  const ty = offsetY * (1 - s) - s * z.y * height
  return `translate(${tx}px, ${ty}px) scale(${s})`
}
