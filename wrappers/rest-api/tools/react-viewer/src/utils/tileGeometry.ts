/** Mapping between a letterboxed <video> tile and the frame's pixel grid. */
import { FULL_FRAME, type Zoom } from './zoom'

export interface DisplayRect {
  offsetX: number
  offsetY: number
  width: number
  height: number
}

/** Where the frame actually paints inside a tile that shows it with object-contain. */
export function displayRect(tileWidth: number, tileHeight: number, frameWidth: number, frameHeight: number): DisplayRect {
  const scale = Math.min(tileWidth / frameWidth, tileHeight / frameHeight)
  const width = frameWidth * scale
  const height = frameHeight * scale
  return { offsetX: (tileWidth - width) / 2, offsetY: (tileHeight - height) / 2, width, height }
}

/** Frame pixel under a tile-relative mouse position, or null over the letterbox bars. */
export function pixelFromMouse(
  mouseX: number, mouseY: number, tileWidth: number, tileHeight: number, frameWidth: number, frameHeight: number,
  zoom: Zoom = FULL_FRAME,
): { x: number; y: number } | null {
  const d = displayRect(tileWidth, tileHeight, frameWidth, frameHeight)
  const fx = (mouseX - d.offsetX) / d.width
  const fy = (mouseY - d.offsetY) / d.height
  if (fx < 0 || fx >= 1 || fy < 0 || fy >= 1) return null
  const x = Math.floor((zoom.x + fx * zoom.w) * frameWidth)
  const y = Math.floor((zoom.y + fy * zoom.h) * frameHeight)
  if (x < 0 || x >= frameWidth || y < 0 || y >= frameHeight) return null
  return { x, y }
}

/** Tile-relative CSS box of a frame-pixel rectangle, clipped to the displayed area. */
export function boxFromPixels(
  min: { x: number; y: number }, max: { x: number; y: number },
  tileWidth: number, tileHeight: number, frameWidth: number, frameHeight: number,
  zoom: Zoom = FULL_FRAME,
): { left: number; top: number; width: number; height: number } {
  const d = displayRect(tileWidth, tileHeight, frameWidth, frameHeight)
  const sx = d.width / (frameWidth * zoom.w)
  const sy = d.height / (frameHeight * zoom.h)
  const left = Math.max(d.offsetX, d.offsetX + (min.x - zoom.x * frameWidth) * sx)
  const top = Math.max(d.offsetY, d.offsetY + (min.y - zoom.y * frameHeight) * sy)
  const right = Math.min(d.offsetX + d.width, d.offsetX + (max.x - zoom.x * frameWidth) * sx)
  const bottom = Math.min(d.offsetY + d.height, d.offsetY + (max.y - zoom.y * frameHeight) * sy)
  return { left, top, width: Math.max(0, right - left), height: Math.max(0, bottom - top) }
}
