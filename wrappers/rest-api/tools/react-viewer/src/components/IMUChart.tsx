// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import { memo, useEffect, useMemo, useRef, useState } from 'react'
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ResponsiveContainer,
} from 'recharts'
import {
  IMU_AXES,
  IMU_CHART_LAYOUT,
  IMU_CHART_REDRAW_MS,
  imuPlotHeight,
  nextIMUAxisBound,
  zoomIMUAxisRange,
  type IMUAxisKey,
  type IMUChartPoint,
} from '../utils/imuChart'

interface IMUChartProps {
  data: IMUChartPoint[]
  // Smallest Y scale to use, so at-rest sensor noise is not magnified.
  axisFloor: number
}

// Memoised: the tile above re-renders on every metadata frame — 200 Hz per motion
// stream — while `data` only changes at the 50 ms sample cadence. Without this the
// charts redraw a few hundred times a second and block the main thread long enough
// for Socket.IO to drop the connection on a ping timeout.
function IMUChart({ data: live, axisFloor }: IMUChartProps) {
  const [hiddenAxes, setHiddenAxes] = useState<Record<string, boolean>>({})
  const [zoomRange, setZoomRange] = useState<[number, number] | null>(null)
  const [autoBound, setAutoBound] = useState(axisFloor)
  const plotRef = useRef<HTMLDivElement>(null)

  // Redraw on a timer, not on every sample — see IMU_CHART_REDRAW_MS.
  const liveRef = useRef(live)
  liveRef.current = live
  const [data, setData] = useState(live)
  useEffect(() => {
    const id = setInterval(() => setData(liveRef.current), IMU_CHART_REDRAW_MS)
    return () => clearInterval(id)
  }, [])

  const hasData = data.length > 0

  const visibleAxes = useMemo(
    () => IMU_AXES.filter(({ key }) => !hiddenAxes[key]).map(({ key }) => key as IMUAxisKey),
    [hiddenAxes],
  )

  useEffect(() => {
    setAutoBound((current) => nextIMUAxisBound(data, current, axisFloor, visibleAxes))
  }, [data, axisFloor, visibleAxes])

  const autoBoundRef = useRef(autoBound)
  autoBoundRef.current = autoBound
  useEffect(() => {
    const el = plotRef.current
    if (!el) return
    const onWheel = (e: WheelEvent) => {
      e.preventDefault()
      const rect = el.getBoundingClientRect()
      const plotHeight = imuPlotHeight(rect.height)
      if (plotHeight <= 0) return
      const ratio = Math.min(
        1,
        Math.max(0, (e.clientY - rect.top - IMU_CHART_LAYOUT.marginTop) / plotHeight),
      )
      const factor = e.deltaY > 0 ? 1.25 : 1 / 1.25
      setZoomRange((current) => zoomIMUAxisRange(current, autoBoundRef.current, ratio, factor))
    }
    // Non-passive: React's own onWheel cannot preventDefault the page scroll.
    el.addEventListener('wheel', onWheel, { passive: false })
    return () => el.removeEventListener('wheel', onWheel)
    // The plot is only rendered once there is data, so re-run when that flips.
  }, [hasData])

  if (!hasData) {
    return (
      <div className="h-44 max-h-full flex items-center justify-center text-xs text-rs-dim">
        Waiting for data…
      </div>
    )
  }

  const newest = data[data.length - 1].t
  const [yMin, yMax] = zoomRange ?? [-autoBound, autoBound]
  const ySpan = yMax - yMin
  const yDigits = ySpan >= 2 ? 1 : ySpan >= 0.2 ? 2 : ySpan >= 0.02 ? 3 : 4

  return (
    // Fixed height rather than filling the tile: the graph should occupy the same
    // footprint as the numeric readout it replaces, not stretch to whatever room a
    // sparsely populated stream grid happens to give the tile.
    <div className="h-44 max-h-full flex flex-col">
      <div className="flex items-center justify-end gap-1 text-xs mb-1">
        {IMU_AXES.map(({ key, color }) => {
          const hidden = !!hiddenAxes[key]
          return (
            <button
              key={key}
              onClick={() => setHiddenAxes((prev) => ({ ...prev, [key]: !prev[key] }))}
              aria-pressed={!hidden}
              title={`${hidden ? 'Show' : 'Hide'} ${key.toUpperCase()}`}
              className="px-1.5 py-0.5 rounded border font-bold uppercase transition-colors"
              style={{ borderColor: hidden ? '#29314a' : color, color: hidden ? '#626c82' : color }}
            >
              {key}
            </button>
          )
        })}
        {zoomRange !== null && (
          <button
            onClick={() => setZoomRange(null)}
            title="Reset Y axis to auto"
            className="ml-1 px-1.5 py-0.5 rounded border border-rs-accent/40 text-rs-accent nums hover:bg-rs-accent/10 transition-colors"
          >
            {yMin.toFixed(yDigits)} … {yMax.toFixed(yDigits)}
          </button>
        )}
      </div>
      <div ref={plotRef} className="flex-1 min-h-0" title="Scroll to scale the Y axis">
        <ResponsiveContainer width="100%" height="100%">
          <LineChart
            data={data}
            margin={{
              top: IMU_CHART_LAYOUT.marginTop,
              right: IMU_CHART_LAYOUT.marginRight,
              bottom: IMU_CHART_LAYOUT.marginBottom,
              left: 0,
            }}
          >
            <CartesianGrid strokeDasharray="3 3" stroke="#29314a" />
            <XAxis
              dataKey="t"
              type="number"
              domain={['dataMin', 'dataMax']}
              tick={false}
              height={IMU_CHART_LAYOUT.axisHeight}
              stroke="#29314a"
            />
            <YAxis
              stroke="#8e97ab"
              fontSize={10}
              tickLine={false}
              width={52}
              domain={[yMin, yMax]}
              allowDataOverflow
              tickFormatter={(v: number) => v.toFixed(yDigits)}
            />
            <Tooltip
              contentStyle={{ backgroundColor: '#151b2b', border: '1px solid #29314a', borderRadius: 6 }}
              labelStyle={{ color: '#8e97ab' }}
              labelFormatter={(t: number) => `${((t - newest) / 1000).toFixed(2)} s`}
              isAnimationActive={false}
            />
            {IMU_AXES.map(({ key, color }) => (
              <Line
                key={key}
                // Straight segments, like ImPlot's PlotLine in the C++ viewer. A
                // monotone spline over a 300-sample window costs far more to
                // recompute, and at the sample cadence it saturates the main thread.
                type="linear"
                dataKey={key}
                stroke={color}
                hide={!!hiddenAxes[key]}
                dot={false}
                strokeWidth={1}
                isAnimationActive={false}
              />
            ))}
          </LineChart>
        </ResponsiveContainer>
      </div>
    </div>
  )
}

export default memo(IMUChart)
