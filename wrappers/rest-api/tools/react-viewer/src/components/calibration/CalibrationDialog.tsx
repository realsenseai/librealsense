import { useEffect, useState } from 'react'
import { apiClient } from '../../api/client'
import type { CalibrationStatus, OccParams, TareParams } from '../../api/types'
import { useJobsStore } from '../../store/jobs'

interface CalibrationDialogProps {
  deviceId: string
  deviceName: string
  mode: 'occ' | 'tare'
  onClose: () => void
}

const SPEEDS = ['Very fast', 'Fast', 'Medium', 'Slow', 'White wall']
const ACCURACIES = ['Very high', 'High', 'Medium', 'Low']
const input = 'bg-gray-900 border border-gray-600 rounded px-2 py-1 text-sm text-white w-28'
const btn = 'px-4 py-1.5 rounded text-sm bg-gray-700 hover:bg-gray-600 text-white disabled:opacity-40 disabled:cursor-not-allowed'
const primary = 'px-4 py-1.5 rounded text-sm bg-rs-blue hover:bg-blue-600 text-white disabled:opacity-40 disabled:cursor-not-allowed'

const VERDICT: Record<string, [string, string]> = {
  good: ['Good', 'text-green-400'],
  ok: ['Could be better', 'text-yellow-300'],
  bad: ['Bad', 'text-red-400'],
  unknown: ['Unknown', 'text-gray-400'],
}

function Field({ label, children, title }: { label: string; children: React.ReactNode; title?: string }) {
  return (
    <label className="flex items-center justify-between gap-3 text-sm text-gray-300" title={title}>
      <span>{label}</span>
      {children}
    </label>
  )
}

/** The legacy on-chip / tare calibration flow (on-chip-calib.cpp): parameters, a progress
 * bar while the firmware works, then health with Keep / Use old / Recalibrate. */
export function CalibrationDialog({ deviceId, deviceName, mode, onClose }: CalibrationDialogProps) {
  const [status, setStatus] = useState<CalibrationStatus | null>(null)
  const [error, setError] = useState<string | null>(null)
  const [jobId, setJobId] = useState<string | null>(null)
  const job = useJobsStore((s) => (jobId ? s.jobs[jobId] : undefined))
  const [occ, setOcc] = useState<OccParams>({ speed: 3, average_step_count: 20, step_count: 20, accuracy: 2, apply_preset: true, intrinsic_scan: true, host_assistance: false })
  const [tare, setTare] = useState<TareParams>({ ground_truth_mm: 1000, average_step_count: 20, step_count: 20, accuracy: 2, apply_preset: true, host_assistance: false })

  const refresh = () => apiClient.getCalibration(deviceId).then(setStatus).catch(() => undefined)
  useEffect(() => { void refresh() }, [deviceId]) // eslint-disable-line react-hooks/exhaustive-deps
  useEffect(() => { if (job && job.state !== 'running') void refresh() }, [job?.state]) // eslint-disable-line react-hooks/exhaustive-deps

  const running = status?.state === 'running' || job?.state === 'running'

  const start = async () => {
    setError(null)
    try {
      const j = mode === 'occ' ? await apiClient.startOnChipCalibration(deviceId, occ) : await apiClient.startTareCalibration(deviceId, tare)
      useJobsStore.getState().upsert(j)
      setJobId(j.id)
      setStatus((s) => (s ? { ...s, state: 'running', error: null } : s))
    } catch (e) {
      setError((e as { response?: { data?: { detail?: string } } })?.response?.data?.detail ?? (e instanceof Error ? e.message : 'Failed to start'))
    }
  }
  const act = (fn: () => Promise<CalibrationStatus>) => async () => {
    setError(null)
    try { setStatus(await fn()) } catch (e) {
      setError((e as { response?: { data?: { detail?: string } } })?.response?.data?.detail ?? (e instanceof Error ? e.message : 'Failed'))
    }
  }

  const title = mode === 'occ' ? 'On-Chip Calibration' : 'Tare Calibration'
  const verdict = status?.verdict ? VERDICT[status.verdict] : null

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm" role="dialog" aria-label={title}>
      <div className="bg-rs-dark border border-gray-600 rounded-xl shadow-2xl w-full max-w-lg mx-4 flex flex-col">
        <div className="px-6 py-4 border-b border-gray-700">
          <h2 className="text-lg font-bold text-white">{title}</h2>
          <p className="text-sm text-gray-400">{deviceName}</p>
        </div>

        <div className="p-6 space-y-4">
          {error && <p className="text-red-400 text-sm" role="alert">{error}</p>}

          {!running && status?.state !== 'done' && (
            <>
              <p className="text-xs text-gray-400">
                {mode === 'occ'
                  ? 'Point the camera at a flat, textured surface about a meter away and keep it still. The firmware improves depth noise (plane-fit RMS) and the camera keeps streaming afterwards.'
                  : 'Point the camera straight at a flat target at a known distance and enter that distance. Tare adjusts the absolute depth.'}
              </p>
              {mode === 'tare' && (
                <Field label="Ground truth (mm)">
                  <input className={input} type="number" min={1} value={tare.ground_truth_mm} aria-label="Ground truth"
                    onChange={(e) => setTare({ ...tare, ground_truth_mm: Number(e.target.value) })} />
                </Field>
              )}
              {mode === 'occ' && (
                <Field label="Speed" title="Very fast to White wall: slower scans converge on harder scenes">
                  <select className={input} value={occ.speed} aria-label="Speed" onChange={(e) => setOcc({ ...occ, speed: Number(e.target.value) })}>
                    {SPEEDS.map((s, i) => <option key={s} value={i}>{s}</option>)}
                  </select>
                </Field>
              )}
              <Field label="Accuracy" title="Subpixel accuracy level: Very high = 0.025%, High = 0.05%, Medium = 0.1%, Low = 0.2%">
                <select className={input} value={mode === 'occ' ? occ.accuracy : tare.accuracy} aria-label="Accuracy"
                  onChange={(e) => mode === 'occ' ? setOcc({ ...occ, accuracy: Number(e.target.value) }) : setTare({ ...tare, accuracy: Number(e.target.value) })}>
                  {ACCURACIES.map((s, i) => <option key={s} value={i}>{s}</option>)}
                </select>
              </Field>
              <Field label="Average step count" title="Number of frames to average, 1-30">
                <input className={input} type="number" min={1} max={30} aria-label="Average step count" value={mode === 'occ' ? occ.average_step_count : tare.average_step_count}
                  onChange={(e) => mode === 'occ' ? setOcc({ ...occ, average_step_count: Number(e.target.value) }) : setTare({ ...tare, average_step_count: Number(e.target.value) })} />
              </Field>
              <Field label="Step count" title="Max iteration steps, 1-30">
                <input className={input} type="number" min={1} max={30} aria-label="Step count" value={mode === 'occ' ? occ.step_count : tare.step_count}
                  onChange={(e) => mode === 'occ' ? setOcc({ ...occ, step_count: Number(e.target.value) }) : setTare({ ...tare, step_count: Number(e.target.value) })} />
              </Field>
              <label className="flex items-center gap-2 text-sm text-gray-300">
                <input type="checkbox" checked={mode === 'occ' ? occ.apply_preset : tare.apply_preset}
                  onChange={(e) => mode === 'occ' ? setOcc({ ...occ, apply_preset: e.target.checked }) : setTare({ ...tare, apply_preset: e.target.checked })} />
                Apply High-Accuracy preset during calibration
              </label>
              {status?.state === 'failed' && status.error && (
                <p className="text-sm text-red-300 whitespace-pre-wrap" data-testid="calibration-error">Last attempt failed: {status.error}</p>
              )}
            </>
          )}

          {running && (
            <div className="space-y-2" data-testid="calibration-progress">
              <p className="text-sm text-gray-300">{job?.message ?? 'Calibrating…'} Keep the camera still.</p>
              <div className="h-2 bg-gray-700 rounded overflow-hidden">
                <div className="h-full bg-rs-blue transition-all" style={{ width: `${Math.round((job?.progress ?? 0) * 100)}%` }} />
              </div>
            </div>
          )}

          {!running && status?.state === 'done' && (
            <div className="space-y-3" data-testid="calibration-result">
              <div className="flex items-center justify-between">
                <span className="text-sm text-gray-300">Health</span>
                <span className="font-mono text-white">
                  {status.health?.map((h) => (mode === 'tare' ? `${h.toFixed(2)}%` : h.toFixed(4))).join(' / ')}
                </span>
              </div>
              {verdict && <p className={`text-sm font-semibold ${verdict[1]}`}>{verdict[0]}</p>}
              <p className="text-xs text-gray-400">
                The new calibration is {status.active === 'new' ? 'active' : 'not active'}{status.written ? ' and written to the device' : ' but not written to the device'}.
                Compare the depth before and after, then Keep to write it to flash or go back to the old table.
              </p>
              <div className="flex flex-wrap gap-2">
                <button className={status.active === 'new' ? btn : primary} onClick={act(() => apiClient.applyCalibration(deviceId, true))} disabled={status.active === 'new'}>Use new</button>
                <button className={status.active === 'old' ? btn : primary} onClick={act(() => apiClient.applyCalibration(deviceId, false))} disabled={status.active === 'old'}>Use old</button>
                <button className={primary} onClick={act(() => apiClient.keepCalibration(deviceId))} disabled={status.written || status.active !== 'new'}
                  title="Write the new table to the device (Settings > Calibration must allow writing)">Keep</button>
                <button className={btn} onClick={() => setStatus((s) => (s ? { ...s, state: 'idle' } : s))}>Recalibrate</button>
              </div>
            </div>
          )}
        </div>

        <div className="px-6 py-4 bg-gray-800/50 flex justify-end gap-3">
          {!running && status?.state !== 'done' && (
            <button className={primary} onClick={() => void start()} data-testid="calibration-start">
              {status?.state === 'failed' ? 'Retry' : 'Start'}
            </button>
          )}
          <button className={btn} onClick={onClose} disabled={running}>{running ? 'Calibrating…' : 'Close'}</button>
        </div>
      </div>
    </div>
  )
}
