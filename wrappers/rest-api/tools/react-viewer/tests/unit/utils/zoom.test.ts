import { describe, it, expect } from 'vitest'
import { FULL_FRAME, isZoomed, pan, zoomAt, zoomTransform } from '@/utils/zoom'
import { boxFromPixels, pixelFromMouse } from '@/utils/tileGeometry'

describe('tile zoom', () => {
  it('zooms in about the cursor and never past the frame edge', () => {
    const z = zoomAt(FULL_FRAME, 0.5, 0.5, 0.5)
    expect(z).toEqual({ x: 0.25, y: 0.25, w: 0.5, h: 0.5 })
    const corner = zoomAt(FULL_FRAME, 0, 0, 0.5)
    expect(corner).toEqual({ x: 0, y: 0, w: 0.5, h: 0.5 })
    expect(isZoomed(z)).toBe(true)
    expect(isZoomed(FULL_FRAME)).toBe(false)
  })

  it('zooming out returns to the whole frame', () => {
    let z = FULL_FRAME
    for (let i = 0; i < 5; i++) z = zoomAt(z, 0.5, 0.5, 1 / 1.1)
    for (let i = 0; i < 5; i++) z = zoomAt(z, 0.2, 0.8, 1.1)
    expect(z).toBe(FULL_FRAME)
  })

  it('pans with the cursor and stops at the edges', () => {
    const z = { x: 0.25, y: 0.25, w: 0.5, h: 0.5 }
    expect(pan(z, 0.1, 0)).toEqual({ x: 0.2, y: 0.25, w: 0.5, h: 0.5 })
    expect(pan(z, -2, -2)).toEqual({ x: 0.5, y: 0.5, w: 0.5, h: 0.5 })
    expect(pan(FULL_FRAME, 0.3, 0.3)).toBe(FULL_FRAME)
  })

  it('produces a CSS transform that shows the zoomed region', () => {
    expect(zoomTransform(FULL_FRAME, 400, 300)).toBeUndefined()
    expect(zoomTransform({ x: 0.25, y: 0.25, w: 0.5, h: 0.5 }, 400, 300)).toBe('translate(-200px, -150px) scale(2)')
    // Letterboxed 100px on each side: the frame's zoomed origin still lands on the bar edge
    expect(zoomTransform({ x: 0.25, y: 0.25, w: 0.5, h: 0.5 }, 400, 300, 100, 0)).toBe('translate(-300px, -150px) scale(2)')
  })

  it('maps mouse and pixels through the zoom', () => {
    const z = { x: 0.5, y: 0.5, w: 0.5, h: 0.5 }
    expect(pixelFromMouse(0, 0, 640, 480, 640, 480, z)).toEqual({ x: 320, y: 240 })
    expect(pixelFromMouse(639, 479, 640, 480, 640, 480, z)).toEqual({ x: 639, y: 479 })
    // A full-frame ROI shows as the visible quarter only
    expect(boxFromPixels({ x: 0, y: 0 }, { x: 639, y: 479 }, 640, 480, 640, 480, z)).toEqual({ left: 0, top: 0, width: 638, height: 478 })
  })
})
