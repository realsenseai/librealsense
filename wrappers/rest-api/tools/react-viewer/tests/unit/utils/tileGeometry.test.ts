import { describe, it, expect } from 'vitest'
import { boxFromPixels, displayRect, pixelFromMouse } from '@/utils/tileGeometry'

describe('tile geometry', () => {
  it('letterboxes a 4:3 frame inside a wide tile', () => {
    expect(displayRect(1000, 300, 640, 480)).toEqual({ offsetX: 300, offsetY: 0, width: 400, height: 300 })
  })

  it('maps mouse positions to frame pixels and rejects the bars', () => {
    expect(pixelFromMouse(500, 150, 1000, 300, 640, 480)).toEqual({ x: 320, y: 240 })
    expect(pixelFromMouse(100, 150, 1000, 300, 640, 480)).toBeNull()
    expect(pixelFromMouse(699.9, 299.9, 1000, 300, 640, 480)).toEqual({ x: 639, y: 479 })
  })

  it('places a pixel rectangle back onto the tile', () => {
    expect(boxFromPixels({ x: 0, y: 0 }, { x: 320, y: 240 }, 1000, 300, 640, 480)).toEqual({ left: 300, top: 0, width: 200, height: 150 })
  })
})
