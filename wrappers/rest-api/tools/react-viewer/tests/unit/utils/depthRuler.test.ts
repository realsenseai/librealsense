import { describe, it, expect } from 'vitest'
import { rulerGradient, rulerTicks, sampleDistances, updateRuler } from '@/utils/depthRuler'

// A scene mostly between 1 and 2 m, with a few stray pixels
const scene = (lo = 1, hi = 2, stray: number[] = []) => [
  ...Array.from({ length: 200 }, (_, i) => lo + (hi - lo) * i / 199), ...stray]

describe('depthRuler', () => {
  it('samples every 30th pixel, 0 where there is no depth', () => {
    const values = new Uint16Array(64 * 31) // rows of 64, the frame 60 wide
    values[0] = 1000
    values[30 * 64 + 30] = 2000
    expect(sampleDistances(values, 60, 31, 64, 0.001)).toEqual([1, 0, 0, 2])
  })

  it('starts on round values around the scene, and stray pixels do not stretch it', () => {
    expect(updateRuler(null, scene(1, 2, [0.1, 9, 9.5]))).toEqual({ min: 0.75, max: 2.25 }) // 5% headroom, 0.25 m steps
  })

  it('has no ruler when under 40% of the frame has depth, like a covered camera', () => {
    const depth = (share: number) => [...Array(100 - share).fill(0), ...Array.from({ length: share }, (_, i) => 1 + i / 100)]
    expect(updateRuler(updateRuler(null, scene()), depth(39))).toBeNull()
    expect(updateRuler(null, depth(40))).not.toBeNull()
    expect(updateRuler(null, [])).toBeNull()
  })

  it('follows the scene at once, with no lag', () => {
    const ruler = updateRuler(null, scene(1, 2))
    expect(updateRuler(ruler, scene(1, 4))!.max).toBeGreaterThanOrEqual(4)
    expect(updateRuler(updateRuler(ruler, scene(1, 4)), scene(1, 2))).toEqual(ruler)
  })

  it('does not move for jitter around a tick', () => {
    const ruler = updateRuler(null, scene(1, 2))
    for (let i = 0; i < 50; i++) expect(updateRuler(ruler, scene(1 + (i % 2) * 0.05, 2 + (i % 2) * 0.05))).toBe(ruler)
  })

  it('labels 3 to 6 round depths with the decimals they need', () => {
    expect(rulerTicks(0, 4)).toEqual({ values: [0, 1, 2, 3, 4], decimals: 0 })
    expect(rulerTicks(0.75, 2)).toEqual({ values: [0.75, 1, 1.25, 1.5, 1.75, 2], decimals: 2 })
    const narrow = rulerTicks(1, 1.01)
    expect(narrow.values.length).toBeGreaterThanOrEqual(3)
    expect(narrow.decimals).toBe(3)
  })

  it('colors the bar with the color table, min at the bottom', () => {
    const table = new Uint8Array(65536 * 3)
    table.set([255, 0, 0], 1000 * 3) // 1 m red
    table.set([0, 0, 255], 2000 * 3) // 2 m blue
    expect(rulerGradient(table, 0.001, 1, 2)).toMatch(/^linear-gradient\(to top, rgb\(255, 0, 0\), .*rgb\(0, 0, 255\)\)$/)
  })
})
