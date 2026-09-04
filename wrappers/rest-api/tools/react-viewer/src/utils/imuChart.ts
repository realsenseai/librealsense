// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

export interface IMUSample {
  timestamp: number
  x: number
  y: number
  z: number
}

export interface IMUChartPoint {
  t: number
  x: number
  y: number
  z: number
  n: number
}

export const imuMagnitude = (s: { x: number; y: number; z: number }) =>
  Math.sqrt(s.x ** 2 + s.y ** 2 + s.z ** 2)

// Plot against the sample timestamp rather than its array position: the history is
// a sliding window, so an index-based x re-labels every point on each redraw and
// the whole trace appears to jitter. On a time axis a sample keeps the same x, so
// old data stays put and only the right edge advances.
export function toIMUChartSeries(samples: IMUSample[]): IMUChartPoint[] {
  return samples.map((s) => ({ t: s.timestamp, x: s.x, y: s.y, z: s.z, n: imuMagnitude(s) }))
}

// The plotted series, matching the C++ viewer's graph (common/graph-model.cpp): the
// three axes in the colors used for the X/Y/Z bars elsewhere, plus the magnitude.
export const IMU_AXES = [
  { key: 'x', color: '#ef4444' },
  { key: 'y', color: '#22c55e' },
  { key: 'z', color: '#3b82f6' },
  { key: 'n', color: '#e5e7eb' },
] as const

export type IMUAxisKey = (typeof IMU_AXES)[number]['key']

// Chart geometry, shared by the LineChart/XAxis props and by the wheel handler's
// pixel-to-value mapping. Everything that shrinks the plot area lives here so the
// two cannot drift apart.
export const IMU_CHART_LAYOUT = {
  marginTop: 6,
  marginRight: 8,
  marginBottom: 0,
  axisHeight: 12,
} as const

// The plot redraws on a timer rather than on every new sample. Re-rendering a
// 300-point, four-series SVG at the sample cadence saturates the main thread, which
// delays the Socket.IO callbacks that deliver the samples — the stream then looks
// like it keeps stopping, because gaps beyond IMU_STALE_GAP_MS restart the window.
export const IMU_CHART_REDRAW_MS = 100

export const imuPlotHeight = (wrapperHeight: number) =>
  wrapperHeight -
  IMU_CHART_LAYOUT.marginTop -
  IMU_CHART_LAYOUT.marginBottom -
  IMU_CHART_LAYOUT.axisHeight

// Axis bounds snap to these so the scale settles on round numbers instead of
// tracking the peak exactly.
// 15 and 150 are on the ladder so a resting accelerometer, whose magnitude sits at
// ~9.8 and needs a bound just over 10, does not jump straight to 20 and leave half
// the plot empty.
const AXIS_STEPS = [0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 20, 50, 100, 150, 200, 500, 1000]

// Beyond the ladder, fall back to the value itself rather than the largest step: a
// bad frame or wrong-unit stream should still be shown in full, not clipped.
const snapUp = (value: number) => AXIS_STEPS.find((step) => step >= value) ?? value

// Symmetric Y bound that grows the moment the signal needs room but shrinks only
// once it fits well inside the current scale — without that hysteresis the axis
// rescales on nearly every redraw and the chart looks like it is jumping. `floor`
// is a dead zone: the smallest scale the axis will use, so a stationary camera
// does not look like it is shaking.
// `visible` keeps a hidden series out of the scale: the accel magnitude sits at ~9.8
// even at rest, so leaving it in would hold the axis at ±10 and flatten X/Y/Z long
// after the user switched that series off.
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
  // Headroom applies to the data, then the floor clamps. Scaling the floor itself
  // would push the resting scale a ladder step above the intended dead zone.
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
