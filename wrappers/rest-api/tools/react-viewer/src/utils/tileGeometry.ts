/** Mapping between a letterboxed <video> tile and the frame's pixel grid. */

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
): { x: number; y: number } | null {
  const d = displayRect(tileWidth, tileHeight, frameWidth, frameHeight)
  const x = Math.floor((mouseX - d.offsetX) / d.width * frameWidth)
  const y = Math.floor((mouseY - d.offsetY) / d.height * frameHeight)
  if (x < 0 || x >= frameWidth || y < 0 || y >= frameHeight) return null
  return { x, y }
}

/** Tile-relative CSS box of a frame-pixel rectangle. */
export function boxFromPixels(
  min: { x: number; y: number }, max: { x: number; y: number },
  tileWidth: number, tileHeight: number, frameWidth: number, frameHeight: number,
): { left: number; top: number; width: number; height: number } {
  const d = displayRect(tileWidth, tileHeight, frameWidth, frameHeight)
  const sx = d.width / frameWidth
  const sy = d.height / frameHeight
  return { left: d.offsetX + min.x * sx, top: d.offsetY + min.y * sy, width: (max.x - min.x) * sx, height: (max.y - min.y) * sy }
}
