// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import { useEffect, useRef, useState } from 'react'
import { useAppStore } from '../../store'
import { useFilePicker } from '../../hooks/useFilePicker'
import type { DeviceState } from '../../api/types'

function elapsed(sinceMs: number): string {
  const total = Math.max(0, Math.floor((Date.now() - sinceMs) / 1000))
  return `${String(Math.floor(total / 60)).padStart(2, '0')}:${String(total % 60).padStart(2, '0')}`
}

const button = 'px-2 py-1 rounded text-xs font-medium disabled:cursor-not-allowed'
const fileName = (path?: string | null) => (path ? path.split(/[\\/]/).pop() : null)

/** One camera: whether it is recording right now, and the one button that changes that. */
function CameraRow({ deviceState }: { deviceState: DeviceState }) {
  const { startRecording, setRecordingPaused, stopRecording } = useAppStore()
  const deviceId = deviceState.device.device_id
  const record = deviceState.record
  const recording = !!record?.recording
  const streaming = deviceState.isStreaming
  const startedAt = useRef<number | null>(null)
  const [, tick] = useState(0)

  useEffect(() => {
    if (!recording) { startedAt.current = null; return }
    if (startedAt.current === null) startedAt.current = Date.now()
    const timer = setInterval(() => tick((n) => n + 1), 1000)
    return () => clearInterval(timer)
  }, [recording])

  const written = fileName(record?.file)
  return (
    <div className="rounded border border-gray-700 bg-gray-800/40 p-2 space-y-1" data-testid="recording-camera">
      <div className="flex items-center justify-between gap-2">
        <span className="text-xs text-gray-300 truncate" title={deviceState.device.name}>
          {deviceState.device.name} <span className="text-gray-500">{deviceState.device.serial_number}</span>
        </span>
        {!recording ? (
          <button
            onClick={() => void startRecording(deviceId)}
            disabled={!streaming}
            data-testid="record-start"
            aria-label="Record"
            title={streaming ? 'Record every active stream of this camera to a file' : 'Start streaming first, then record'}
            className={`${button} ${streaming ? 'bg-red-700 hover:bg-red-600 text-white' : 'bg-gray-700 text-gray-500'}`}
          >
            ● Record
          </button>
        ) : (
          <div className="flex items-center gap-1">
            <button
              onClick={() => void setRecordingPaused(deviceId, !record?.paused)}
              title={record?.paused ? 'Resume recording' : 'Pause recording'}
              aria-label={record?.paused ? 'Resume recording' : 'Pause recording'}
              className={`${button} bg-gray-700 hover:bg-gray-600 text-white`}
            >
              {record?.paused ? '▶' : '❚❚'}
            </button>
            <button
              onClick={() => void stopRecording(deviceId)}
              title="Stop recording and close the file"
              aria-label="Stop recording"
              data-testid="record-stop"
              className={`${button} bg-gray-700 hover:bg-gray-600 text-white`}
            >
              ■ Stop
            </button>
          </div>
        )}
      </div>

      {/* The state in words: a pane that only shows buttons leaves the user guessing */}
      <div className="flex items-center gap-1 text-[11px]" data-testid="record-indicator">
        {recording ? (
          <>
            <span className={`text-red-500 ${record?.paused ? '' : 'animate-pulse'}`}>●</span>
            <span className="text-red-300 font-semibold">
              {record?.paused ? 'Paused' : 'Recording'} {startedAt.current !== null && elapsed(startedAt.current)}
            </span>
          </>
        ) : (
          <>
            <span className="text-gray-600">○</span>
            <span className="text-gray-400">{streaming ? 'Not recording' : 'Not recording — start streaming to enable'}</span>
          </>
        )}
      </div>
      {written && (
        <div className="text-[11px] text-gray-500 truncate" title={record?.file ?? undefined}>
          {recording ? 'Writing to' : 'Last file'}: {written}
        </div>
      )}
    </div>
  )
}

/** One loaded recording: what it is doing, and how to drive or close it. */
function PlaybackRow({ deviceState }: { deviceState: DeviceState }) {
  const { playbackControl, unloadRecording } = useAppStore()
  const deviceId = deviceState.device.device_id
  const state = deviceState.playback?.state ?? 'stopped'
  const playing = state === 'playing'
  const name = fileName(deviceState.device.file_name) ?? deviceId.replace(/^playback-/, '')
  const colour = playing ? 'text-green-400' : state === 'paused' ? 'text-yellow-400' : 'text-gray-400'

  return (
    <div className="rounded border border-gray-700 bg-gray-800/40 p-2 space-y-1" data-testid="recording-playback">
      <div className="flex items-center justify-between gap-2">
        <span className="text-xs text-gray-300 truncate" title={deviceState.device.file_name ?? name}>{name}</span>
        <div className="flex items-center gap-1">
          <button className={`${button} bg-rs-blue hover:bg-blue-600 text-white`}
            onClick={() => void playbackControl(deviceId, playing ? 'pause' : 'play')}
            title={playing ? 'Pause playback' : 'Play the recording'} aria-label={playing ? 'Pause playback' : 'Play recording'}>
            {playing ? '❚❚' : '▶'}
          </button>
          <button className={`${button} bg-gray-700 hover:bg-gray-600 text-white`}
            onClick={() => void playbackControl(deviceId, 'stop')} title="Stop playback" aria-label="Stop playback">■</button>
          <button className={`${button} bg-gray-700 hover:bg-gray-600 text-white`}
            onClick={() => void unloadRecording(deviceId)} title="Close this recording" aria-label="Close recording">✕</button>
        </div>
      </div>
      <div className={`text-[11px] ${colour}`} data-testid="playback-state">
        {playing ? '▶ Playing' : state === 'paused' ? '❚❚ Paused' : '■ Stopped'}
      </div>
    </div>
  )
}

/**
 * Recording and playback in one place: which cameras are recording (and for how long, into
 * which file), which recordings are open and what they are doing, and the two buttons that
 * start those things. The legacy viewer scatters this across the device panel; a user should
 * not have to hover a red dot to find out whether it records or reports.
 */
export function RecordingPanel() {
  const deviceStates = useAppStore((s) => s.deviceStates)
  const uploadRecording = useAppStore((s) => s.uploadRecording)
  const picker = useFilePicker((file) => void uploadRecording(file), '.bag,.db3')

  const cameras = Object.values(deviceStates).filter((ds) => !ds.device.is_playback)
  const playbacks = Object.values(deviceStates).filter((ds) => ds.device.is_playback)

  return (
    // Sticky so it stays reachable however long the device list grows, and padded at the
    // bottom to clear the floating connection and console chips.
    <div className="sticky bottom-0 z-10 bg-rs-dark border-t border-gray-700 p-4 pb-14 space-y-2" data-testid="recording-panel">
      {picker.input}
      <h2 className="panel-header mb-0">Recording</h2>
      {cameras.length === 0
        ? <p className="text-xs text-gray-500">Connect a camera to record.</p>
        : cameras.map((ds) => <CameraRow key={ds.device.device_id} deviceState={ds} />)}

      <div className="flex items-center justify-between pt-2">
        <h2 className="panel-header mb-0">Playback</h2>
        <button
          onClick={() => picker.open()}
          data-testid="open-recording"
          aria-label="Load recorded sequence"
          title="Open a recorded sequence (.bag / .db3) and play it back as a device"
          className="flex items-center gap-1 px-2 py-1 rounded bg-gray-700 hover:bg-gray-600 text-xs text-gray-100"
        >
          <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M3 7a2 2 0 012-2h4l2 2h8a2 2 0 012 2v8a2 2 0 01-2 2H5a2 2 0 01-2-2V7z" />
          </svg>
          Open recording…
        </button>
      </div>
      {playbacks.length === 0
        ? <p className="text-xs text-gray-500" data-testid="playback-state">No recording open</p>
        : playbacks.map((ds) => <PlaybackRow key={ds.device.device_id} deviceState={ds} />)}
    </div>
  )
}
