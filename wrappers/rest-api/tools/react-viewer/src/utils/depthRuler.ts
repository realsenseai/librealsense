// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// The depth ruler as the legacy viewer draws it (common/viewer.cpp: calculate_ruler_bounds, draw_color_ruler):
// a range that holds still unless the scene really moves, round tick labels, and the colors the colorizer gives each depth,
// read from the frame's color table

interface Ruler {
  min: number // meters, on the snap grid
  max: number
}

// Snap grid for the bounds: coarse, so jitter under a cm can't cross its deadband
const SNAP = [0.05, 0.1, 0.25, 0.5, 1, 2, 5, 10, 20, 50, 100]
// Tick labels: finer, so a ruler of about a cm still gets a few
const LABEL = [0.001, 0.002, 0.005, 0.01, 0.02, ...SNAP]
const MIN_RANGE = 0.05

/** The coarsest step that keeps about 10 cells across the range */
const snapStep = (range: number) => SNAP.find(s => range <= s * 10) ?? SNAP[SNAP.length - 1]

const percentile = (sorted: number[], p: number) => sorted[Math.floor(p * (sorted.length - 1))]

/** Every 30th pixel's distance in meters, 0 where the pixel has no depth */
export function sampleDistances(values: Uint16Array, width: number, height: number, rowLength: number, units: number) {
  const distances: number[] = []
  for (let y = 0; y < height; y += 30)
    for (let x = 0; x < width; x += 30) distances.push(values[y * rowLength + x] * units)
  return distances
}

/**
 * The ruler for this frame's samples. Each frame stands on its own (nothing lags behind the scene), but there is no
 * ruler when under 40% of the frame has depth, like a covered camera, and the bounds move only past half a step
 * outward or one and a half inward, so a bound near a tick doesn't flip back and forth
 */
export function updateRuler(ruler: Ruler | null, samples: number[]): Ruler | null {
  const sorted = samples.filter(d => d > 0).sort((a, b) => a - b)
  if (!sorted.length || sorted.length / samples.length < 0.4) return null
  // 5th to 95th percentile, so stray pixels can't stretch it, with 5% headroom on each side
  let lo = percentile(sorted, 0.05)
  let hi = percentile(sorted, 0.95)
  const span = Math.max(hi - lo, MIN_RANGE)
  lo = Math.max(0, lo - 0.05 * span)
  hi = Math.max(hi + 0.05 * span, lo + MIN_RANGE)

  if (ruler) {
    const step = snapStep(ruler.max - ruler.min)
    const holds = ruler.max - 1.5 * step <= hi && hi <= ruler.max + 0.5 * step
      && ruler.min - 0.5 * step <= lo && lo <= ruler.min + 1.5 * step
    if (holds) return ruler
  }
  const step = snapStep(hi - lo)
  return { min: Math.floor(lo / step) * step, max: Math.ceil(hi / step) * step }
}

/** The labeled depths: multiples of the finest step that gives at most 6 inside the range, and the decimals the step has */
export function rulerTicks(min: number, max: number) {
  const ticks = (s: number) => {
    const values: number[] = []
    for (let k = Math.ceil(min / s - 1e-4); k * s <= max + 1e-4; k++) values.push(k * s + 0) // + 0: no -0
    return values
  }
  let i = LABEL.findIndex(s => ticks(s).length <= 6)
  if (i < 0) i = LABEL.length - 1
  if (i > 0 && ticks(LABEL[i]).length < 3) i-- // a range under the finest step still gets a few labels
  const step = LABEL[i]
  return { values: ticks(step), decimals: String(step).split('.')[1]?.length ?? 0 } // 0.25 → 2, not legacy's 1
}

/** A CSS gradient, min at the bottom, in the colors the colorizer gives each depth (table: RGB of every depth value) */
export function rulerGradient(table: Uint8Array, units: number, min: number, max: number) {
  const colors = Array.from({ length: 64 }, (_, i) => {
    const value = Math.max(1, Math.min(65535, Math.round((min + (max - min) * i / 63) / units))) // 0 is no depth: black
    return `rgb(${table[value * 3]}, ${table[value * 3 + 1]}, ${table[value * 3 + 2]})`
  })
  return `linear-gradient(to top, ${colors.join(', ')})`
}
