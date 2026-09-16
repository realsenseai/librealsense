import { useEffect, useState } from 'react'
import { useAppStore } from '../../store'
import type { PlaybackStatus } from '../../api/types'

const SPEEDS = [0.25, 0.5, 1, 1.5, 2]

/** hh:mm:ss.mmm like the legacy seek bar labels. */
export function formatNs(ns: number): string {
  const total = Math.max(0, Math.floor(ns / 1e6))
  const ms = total % 1000
  const s = Math.floor(total / 1000) % 60
  const m = Math.floor(total / 60000) % 60
  const h = Math.floor(total / 3600000)
  const two = (n: number) => String(n).padStart(2, '0')
  return `${two(h)}:${two(m)}:${two(s)}.${String(ms).padStart(3, '0')}`
}

interface TransportProps {
  deviceId: string
}

/**
 * The legacy playback panel (device-model.cpp draw_playback_controls): step back, stop,
 * play/pause, step forward, repeat, speed, and a seek bar with elapsed / total time.
 */
export function Transport({ deviceId }: TransportProps) {
  const status = useAppStore((s) => s.deviceStates[deviceId]?.playback)
  const { playbackControl, refreshPlayback, unloadRecording } = useAppStore()
  const [scrub, setScrub] = useState<number | null>(null)

  // Position moves on the server; poll it while playing (status events only carry state).
  useEffect(() => {
    void refreshPlayback(deviceId)
    const interval = setInterval(() => void refreshPlayback(deviceId), 500)
    return () => clearInterval(interval)
  }, [deviceId, refreshPlayback])

  if (!status) return null
  const playing = status.state === 'playing'
  const paused = status.state === 'paused'
  const position = scrub ?? status.position_ns
  const act = (action: Parameters<typeof playbackControl>[1], value?: number) => () => void playbackControl(deviceId, action, value)
  const btn = 'px-2 py-1 rounded text-sm bg-gray-700 hover:bg-gray-600 text-white disabled:opacity-40 disabled:cursor-not-allowed'

  return (
    <div className="border-t border-gray-700 p-3 space-y-2" data-testid="playback-transport">
      <div className="flex items-center gap-1">
        <button className={btn} onClick={act('step', -1)} disabled={!paused} title="Step back one frame" aria-label="Step back">⏮</button>
        <button className={btn} onClick={act('stop')} title="Stop" aria-label="Stop">■</button>
        <button className={`${btn} bg-rs-blue hover:bg-blue-600`} onClick={act(playing ? 'pause' : 'play')}
          title={playing ? 'Pause' : 'Play'} aria-label={playing ? 'Pause' : 'Play'}>
          {playing ? '❚❚' : '▶'}
        </button>
        <button className={btn} onClick={act('step', 1)} disabled={!paused} title="Step forward one frame" aria-label="Step forward">⏭</button>
        <button className={`${btn} ${status.repeat ? 'text-rs-blue' : ''}`} onClick={act('repeat', status.repeat ? 0 : 1)}
          title={status.repeat ? 'Repeat on' : 'Repeat off'} aria-label="Repeat" aria-pressed={status.repeat}>🔁</button>
        <select
          className="ml-auto bg-gray-700 text-white rounded px-1 py-1 text-xs"
          value={status.speed}
          onChange={(e) => void playbackControl(deviceId, 'speed', Number(e.target.value))}
          aria-label="Playback speed"
        >
          {SPEEDS.map((s) => <option key={s} value={s}>x{s}</option>)}
        </select>
        <button className={btn} onClick={() => void unloadRecording(deviceId)} title="Close recording" aria-label="Close recording">✕</button>
      </div>
      <div className="flex items-center gap-2 text-[11px] font-mono text-gray-300">
        <span>{formatNs(position)}</span>
        <input
          type="range"
          min={0}
          max={Math.max(1, status.duration_ns)}
          value={Math.min(position, status.duration_ns)}
          onChange={(e) => setScrub(Number(e.target.value))}
          onMouseUp={() => { if (scrub !== null) { void playbackControl(deviceId, 'seek', scrub); setScrub(null) } }}
          onTouchEnd={() => { if (scrub !== null) { void playbackControl(deviceId, 'seek', scrub); setScrub(null) } }}
          className="flex-1 h-1"
          aria-label="Seek"
        />
        <span>{formatNs(status.duration_ns)}</span>
      </div>
      <div className="text-[11px] text-gray-500 truncate" title={status.file_name}>{status.file_name}</div>
    </div>
  )
}

export type { PlaybackStatus }
