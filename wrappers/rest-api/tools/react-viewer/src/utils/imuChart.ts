// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

export interface IMUChartPoint {
  t: number
  x: number
  y: number
  z: number
  n: number
}

// Graph cadence of the C++ viewer (common/graph-model.h): one sample per 50 ms,
// 300 kept, a 15 s window.
export const IMU_SAMPLE_INTERVAL_MS = 50
export const IMU_HISTORY_SIZE = 300

export const imuMagnitude = (s: { x: number; y: number; z: number }) =>
  Math.sqrt(s.x ** 2 + s.y ** 2 + s.z ** 2)

// Adds one sample to a tile's graph window, keeping one per IMU_SAMPLE_INTERVAL_MS
// (motion frames arrive at up to 400 Hz); returns `points` itself when throttled.
// The first sample fills the window with zeros back-dated at that cadence, as
// graph_model::clear() does, so the trace scrolls in from the right. Points are
// plotted against time, so a point keeps its x as the window slides.
export function appendIMUPoint(
  points: IMUChartPoint[],
  sample: { x: number; y: number; z: number },
  now: number,
): IMUChartPoint[] {
  const last = points[points.length - 1]
  if (last && now - last.t < IMU_SAMPLE_INTERVAL_MS) return points
  const point = { t: now, x: sample.x, y: sample.y, z: sample.z, n: imuMagnitude(sample) }
  if (!last) {
    const zeros = Array.from({ length: IMU_HISTORY_SIZE - 1 }, (_, i) => ({
      t: now - (IMU_HISTORY_SIZE - 1 - i) * IMU_SAMPLE_INTERVAL_MS, x: 0, y: 0, z: 0, n: 0,
    }))
    return [...zeros, point]
  }
  return [...points.slice(1), point]
}

// The plotted series, matching the C++ viewer's graph (common/graph-model.cpp): the
// three axes, plus the magnitude. The orientation view uses the same axis colors.
export const IMU_AXES = [
  { key: 'x', color: '#ef4444' },
  { key: 'y', color: '#22c55e' },
  { key: 'z', color: '#3b82f6' },
  { key: 'n', color: '#e5e7eb' },
] as const

export type IMUAxisKey = (typeof IMU_AXES)[number]['key']

// Chart geometry, shared by the chart props and the wheel handler's pixel mapping.
export const IMU_CHART_LAYOUT = {
  marginTop: 6,
  marginRight: 8,
  marginBottom: 0,
  axisHeight: 12,
} as const

// Axis bounds snap to these so the scale settles on round numbers instead of
// tracking the peak exactly.
// 15 and 150 are on the ladder so a resting accelerometer, whose magnitude sits at
// ~9.8 and needs a bound just over 10, does not jump straight to 20 and leave half
// the plot empty.
const AXIS_STEPS = [0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 20, 50, 100, 150, 200, 500, 1000]

// Beyond the ladder, fall back to the value itself rather than the largest step: a
// bad frame or wrong-unit stream should still be shown in full, not clipped.
const snapUp = (value: number) => AXIS_STEPS.find((step) => step >= value) ?? value

// Symmetric Y bound: grows at once, shrinks only when the data fits well inside
// (hysteresis), never below `floor`. Hidden series do not count toward it.
export function nextIMUAxisBound(
  points: IMUChartPoint[],
  current: number,
  floor: number,
  visible: readonly IMUAxisKey[] = IMU_AXES.map((a) => a.key),
): number {
  let peak = 0
  for (const p of points) {
    for (const key of visible) {
      peak = Math.max(peak, Math.abs(p[key]))
    }
  }
  // Headroom on the data, not on the floor.
  const needed = Math.max(floor, peak * 1.15)
  if (needed > current) return snapUp(needed)
  if (needed < current / 2.5) return snapUp(Math.max(needed, floor))
  return current
}

// Next manual Y range for one wheel notch. `ratio` is the pointer's position in the
// plot area (0 at the top, 1 at the bottom); the value under it stays put, the way
// ImPlot behaves, so a trace offset from zero does not walk off screen. Returns
// null once zoomed back out past the automatic scale, handing the axis back to it.
export function zoomIMUAxisRange(
  current: [number, number] | null,
  autoBound: number,
  ratio: number,
  factor: number,
): [number, number] | null {
  const [min, max] = current ?? [-autoBound, autoBound]
  const span = max - min
  const nextSpan = span * factor
  if (nextSpan >= autoBound * 2) return null
  if (nextSpan < 1e-4) return current
  const cursorValue = max - ratio * span
  const nextMax = cursorValue + ratio * nextSpan
  return [nextMax - nextSpan, nextMax]
}
