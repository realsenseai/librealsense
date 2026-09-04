import { describe, it, expect } from 'vitest'
import {
  imuMagnitude,
  nextIMUAxisBound,
  toIMUChartSeries,
  zoomIMUAxisRange,
  imuPlotHeight,
  IMU_AXES,
  IMU_CHART_LAYOUT,
  type IMUChartPoint,
} from '@/utils/imuChart'

const point = (t: number, x: number, y = 0, z = 0): IMUChartPoint =>
  ({ t, x, y, z, n: imuMagnitude({ x, y, z }) })

describe('toIMUChartSeries', () => {
  it('keys points on the sample timestamp, not the array index', () => {
    const series = toIMUChartSeries([
      { timestamp: 1_000, x: 1, y: 2, z: 3 },
      { timestamp: 1_050, x: 4, y: 5, z: 6 },
    ])
    expect(series).toEqual([
      { t: 1_000, x: 1, y: 2, z: 3, n: Math.sqrt(14) },
      { t: 1_050, x: 4, y: 5, z: 6, n: Math.sqrt(77) },
    ])
  })

  it('keeps a sample at the same x after the window slides', () => {
    const samples = [
      { timestamp: 1_000, x: 1, y: 0, z: 0 },
      { timestamp: 1_050, x: 2, y: 0, z: 0 },
    ]
    const before = toIMUChartSeries(samples)
    // Oldest sample drops off, as the ring buffer does.
    const after = toIMUChartSeries(samples.slice(1))
    expect(after[0].t).toBe(before[1].t)
  })
})

describe('nextIMUAxisBound', () => {
  it('never goes below the dead zone, so at-rest noise is not magnified', () => {
    const noise = [point(0, 0.004), point(1, -0.003)]
    expect(nextIMUAxisBound(noise, 0.5, 0.5)).toBe(0.5)
  })

  it('grows to fit a signal that needs more room', () => {
    expect(nextIMUAxisBound([point(0, 9.8)], 0.5, 0.5)).toBeGreaterThanOrEqual(9.8)
  })

  it('holds the current scale instead of rescaling on every redraw', () => {
    // Comfortably inside 20 but not below the shrink threshold (20 / 2.5 = 8).
    expect(nextIMUAxisBound([point(0, 9)], 20, 0.5)).toBe(20)
  })

  it('shrinks only once the signal fits well inside the current scale', () => {
    expect(nextIMUAxisBound([point(0, 0.01)], 20, 0.5)).toBeLessThan(20)
  })

  it('fits values beyond the snap ladder rather than clipping them', () => {
    // A wrong-unit or bad frame must still be fully visible.
    const bound = nextIMUAxisBound([point(0, 5_000)], 0.5, 0.5)
    expect(bound).toBeGreaterThanOrEqual(5_000)
  })
})

describe('magnitude series', () => {
  it('matches the N line in the C++ viewer: the norm of the three axes', () => {
    expect(imuMagnitude({ x: 3, y: 4, z: 0 })).toBe(5)
    expect(toIMUChartSeries([{ timestamp: 0, x: 0, y: -9.8, z: 0 }])[0].n).toBeCloseTo(9.8)
  })

  it('is plotted as a fourth series', () => {
    expect(IMU_AXES.map((a) => a.key)).toEqual(['x', 'y', 'z', 'n'])
  })

  it('does not hold the axis open once the user hides it', () => {
    // Accel at rest: the axes are small but the magnitude sits at ~1g, so leaving
    // N in the scale would keep the axis at 10 and flatten X/Y/Z.
    const resting = [point(0, 0.05, 0.05, 9.81)]
    expect(nextIMUAxisBound(resting, 0.1, 0.1)).toBe(15)
    expect(nextIMUAxisBound(resting, 0.1, 0.1, ['x', 'y'])).toBe(0.1)
  })
})

describe('zoomIMUAxisRange', () => {
  it('keeps the value under the pointer fixed while zooming in', () => {
    // Pointer a quarter down the plot of the automatic range [-12, 12] sits at 6.
    const ratio = 0.25
    const range = zoomIMUAxisRange(null, 12, ratio, 1 / 1.25)
    expect(range).not.toBeNull()
    const [min, max] = range!
    expect(max - ratio * (max - min)).toBeCloseTo(6, 6)
  })

  it('produces an asymmetric range when the pointer is off centre', () => {
    const [min, max] = zoomIMUAxisRange(null, 12, 0.9, 1 / 1.25)!
    expect(Math.abs(min)).not.toBeCloseTo(Math.abs(max), 3)
  })

  it('narrows the visible span when zooming in', () => {
    const [min, max] = zoomIMUAxisRange(null, 12, 0.5, 1 / 1.25)!
    expect(max - min).toBeLessThan(24)
  })

  it('hands the axis back to auto once zoomed out past the automatic scale', () => {
    expect(zoomIMUAxisRange([-11, 11], 12, 0.5, 1.25)).toBeNull()
  })

  it('refuses to zoom past a degenerate span', () => {
    const tiny: [number, number] = [-0.00002, 0.00002]
    expect(zoomIMUAxisRange(tiny, 12, 0.5, 1 / 1.25)).toBe(tiny)
  })
})

describe('imuPlotHeight', () => {
  it('subtracts every chart inset that shrinks the plot area', () => {
    const { marginTop, marginBottom, axisHeight } = IMU_CHART_LAYOUT
    expect(imuPlotHeight(200)).toBe(200 - marginTop - marginBottom - axisHeight)
  })
})
