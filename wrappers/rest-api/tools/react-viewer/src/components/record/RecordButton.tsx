import { useAppStore } from '../../store'

interface RecordButtonProps {
  deviceId: string
  streaming: boolean
}

/** The legacy device-panel record button: REC while streaming, then pause/resume and stop. */
export function RecordButton({ deviceId, streaming }: RecordButtonProps) {
  const record = useAppStore((s) => s.deviceStates[deviceId]?.record)
  const { startRecording, setRecordingPaused, stopRecording } = useAppStore()
  const base = 'px-2 py-0.5 rounded text-xs font-semibold'

  if (!record?.recording) {
    return (
      <button
        onClick={(e) => { e.stopPropagation(); void startRecording(deviceId) }}
        disabled={!streaming}
        title={streaming ? 'Record to file' : 'Start streaming to record'}
        aria-label="Record"
        className={`${base} ${streaming ? 'bg-red-700 hover:bg-red-600 text-white' : 'bg-gray-700 text-gray-500 cursor-not-allowed'}`}
      >
        ● REC
      </button>
    )
  }
  return (
    <span className="flex items-center gap-1">
      <span className="text-red-500 text-xs font-semibold animate-pulse" title={record.file ?? undefined}>● REC</span>
      <button
        onClick={(e) => { e.stopPropagation(); void setRecordingPaused(deviceId, !record.paused) }}
        title={record.paused ? 'Resume recording' : 'Pause recording'}
        aria-label={record.paused ? 'Resume recording' : 'Pause recording'}
        className={`${base} bg-gray-700 hover:bg-gray-600 text-white`}
      >
        {record.paused ? '▶' : '❚❚'}
      </button>
      <button
        onClick={(e) => { e.stopPropagation(); void stopRecording(deviceId) }}
        title="Stop recording"
        aria-label="Stop recording"
        className={`${base} bg-gray-700 hover:bg-gray-600 text-white`}
      >
        ■
      </button>
    </span>
  )
}
