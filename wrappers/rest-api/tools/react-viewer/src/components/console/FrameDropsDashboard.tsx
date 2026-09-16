import { useDashboardsStore } from '../../store/dashboards'

const BARS = 30

/** The legacy output console dashboard (output-model.cpp): frame drops per second over the
 * last half minute for one stream, with the delivered frame rate. */
export function FrameDropsDashboard() {
  const { streams, selected, select } = useDashboardsStore()
  const keys = Object.keys(streams)
  const key = selected && streams[selected] ? selected : keys[0]
  const history = key ? streams[key] : undefined
  const drops = history?.drops.slice(-BARS) ?? []
  const frames = history?.frames.slice(-BARS) ?? []
  const max = Math.max(1, ...drops)
  const lastFrames = frames[frames.length - 1] ?? 0
  const lastDrops = drops[drops.length - 1] ?? 0

  return (
    <div className="w-72 border-l border-gray-700 p-2 text-xs text-gray-300 flex flex-col gap-2" data-testid="frame-drops-dashboard">
      <div className="flex items-center justify-between gap-2">
        <span className="font-semibold text-gray-200">Frame Drops per Second</span>
        <select className="bg-gray-800 border border-gray-600 rounded px-1 py-0.5 text-xs" value={key ?? ''} onChange={(e) => select(e.target.value || null)} aria-label="Dashboard stream" disabled={keys.length === 0}>
          {keys.length === 0 && <option value="">no streams</option>}
          {keys.map((k) => <option key={k} value={k}>{streams[k].stream} ({streams[k].deviceId.slice(-4)})</option>)}
        </select>
      </div>
      <svg viewBox={`0 0 ${BARS * 8} 60`} className="w-full h-16 bg-gray-900 rounded" role="img" aria-label="drops per second, last 30 seconds">
        {drops.map((d, i) => (
          <rect key={i} x={(BARS - drops.length + i) * 8 + 1} y={60 - (d / max) * 56} width={6} height={(d / max) * 56}
            fill={d === 0 ? '#3b82f6' : d < 3 ? '#eab308' : '#ef4444'} />
        ))}
      </svg>
      <div className="flex justify-between font-mono">
        <span title="Frames delivered in the last second">{lastFrames} fps{history ? ` / ${Math.round(history.expectedFps)}` : ''}</span>
        <span title="Frames dropped in the last second" className={lastDrops ? 'text-red-400' : ''}>{lastDrops} drops/s</span>
      </div>
    </div>
  )
}
