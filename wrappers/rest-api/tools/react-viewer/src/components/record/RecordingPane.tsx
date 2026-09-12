// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import { useEffect, useRef, useState } from 'react'
import { useAppStore } from '../../store'

interface RecordingPaneProps {
  deviceId: string
  streaming: boolean
}

function elapsed(sinceMs: number): string {
  const total = Math.max(0, Math.floor((Date.now() - sinceMs) / 1000))
  return `${String(Math.floor(total / 60)).padStart(2, '0')}:${String(total % 60).padStart(2, '0')}`
}

/**
 * The recording controls as their own labelled pane. A single red dot cannot say whether it
 * starts a recording or reports one in progress, so the state is spelled out: an idle pane
 * offers "Record", a running one names the file and counts the seconds.
 */
export function RecordingPane({ deviceId, streaming }: RecordingPaneProps) {
  const record = useAppStore((s) => s.deviceStates[deviceId]?.record)
  const { startRecording, setRecordingPaused, stopRecording } = useAppStore()
  const recording = !!record?.recording
  const startedAt = useRef<number | null>(null)
  const [, tick] = useState(0)

  useEffect(() => {
    if (!recording) { startedAt.current = null; return }
    if (startedAt.current === null) startedAt.current = Date.now()
    const timer = setInterval(() => tick((n) => n + 1), 1000)
    return () => clearInterval(timer)
  }, [recording])

  const button = 'px-2 py-1 rounded text-xs font-medium'
  const fileName = record?.file ? record.file.split(/[\\/]/).pop() : null

  return (
    <div className="border-t border-gray-700 px-3 py-2" data-testid="recording-pane">
      <div className="flex items-center justify-between gap-2">
        <span className="text-[11px] uppercase tracking-wide text-gray-400">Recording</span>
        {!recording ? (
          <button
            onClick={(e) => { e.stopPropagation(); void startRecording(deviceId) }}
            disabled={!streaming}
            data-testid="record-start"
            aria-label="Record"
            title={streaming ? 'Record every active stream of this camera to a file' : 'Start streaming first, then record'}
            className={`${button} ${streaming ? 'bg-red-700 hover:bg-red-600 text-white' : 'bg-gray-700 text-gray-500 cursor-not-allowed'}`}
          >
            ● Record
          </button>
        ) : (
          <div className="flex items-center gap-1">
            <span className="flex items-center gap-1 text-red-400 text-xs font-semibold" data-testid="record-indicator">
              <span className={record?.paused ? '' : 'animate-pulse'}>●</span>
              {record?.paused ? 'Paused' : 'Recording'} {startedAt.current !== null && elapsed(startedAt.current)}
            </span>
            <button
              onClick={(e) => { e.stopPropagation(); void setRecordingPaused(deviceId, !record?.paused) }}
              title={record?.paused ? 'Resume recording' : 'Pause recording'}
              aria-label={record?.paused ? 'Resume recording' : 'Pause recording'}
              className={`${button} bg-gray-700 hover:bg-gray-600 text-white`}
            >
              {record?.paused ? '▶' : '❚❚'}
            </button>
            <button
              onClick={(e) => { e.stopPropagation(); void stopRecording(deviceId) }}
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
      {fileName && (
        <div className="mt-1 text-[11px] text-gray-400 truncate" title={record?.file ?? undefined}>
          {recording ? 'Writing to' : 'Last file'}: {fileName}
        </div>
      )}
    </div>
  )
}
