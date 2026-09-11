import { useEffect, useState } from 'react'
import { apiClient } from '../../api/client'
import type { CalibrationTable } from '../../api/types'

interface CalibrationTableEditorProps {
  deviceId: string
  deviceName: string
  onClose: () => void
}

const input = 'bg-gray-900 border border-gray-600 rounded px-1.5 py-0.5 text-xs text-white font-mono w-24'
const btn = 'px-4 py-1.5 rounded text-sm bg-gray-700 hover:bg-gray-600 text-white disabled:opacity-40 disabled:cursor-not-allowed'
const primary = 'px-4 py-1.5 rounded text-sm bg-rs-blue hover:bg-blue-600 text-white disabled:opacity-40 disabled:cursor-not-allowed'
const MATRICES: [keyof Pick<CalibrationTable, 'intrinsic_left' | 'intrinsic_right' | 'world2left_rot' | 'world2right_rot'>, string][] = [
  ['intrinsic_left', 'Left Intrinsics'],
  ['intrinsic_right', 'Right Intrinsics'],
  ['world2left_rot', 'World to Left Rotation'],
  ['world2right_rot', 'World to Right Rotation'],
]

function Matrix({ label, value, original, onChange }: { label: string; value: number[][]; original: number[][]; onChange: (m: number[][]) => void }) {
  return (
    <div>
      <div className="text-xs text-gray-400 mb-1">{label}</div>
      <div className="grid grid-cols-3 gap-1">
        {value.map((row, i) => row.map((v, j) => (
          <input key={`${i}${j}`} className={`${input} ${v !== original[i][j] ? 'border-yellow-500' : ''}`} type="number" step="any" value={v}
            aria-label={`${label} ${i},${j}`}
            onChange={(e) => { const m = value.map((r) => [...r]); m[i][j] = Number(e.target.value); onChange(m) }} />
        )))}
      </div>
    </div>
  )
}

/** The legacy calibration table editor (calibration-model.cpp): every field the D400
 * coefficients table exposes, changed cells highlighted, apply to the running device or write. */
export function CalibrationTableEditor({ deviceId, deviceName, onClose }: CalibrationTableEditorProps) {
  const [original, setOriginal] = useState<CalibrationTable | null>(null)
  const [table, setTable] = useState<CalibrationTable | null>(null)
  const [resolution, setResolution] = useState(3) // 848x480
  const [error, setError] = useState<string | null>(null)
  const [busy, setBusy] = useState(false)

  const load = () => apiClient.getCalibrationTable(deviceId)
    .then((t) => { setOriginal(t); setTable(structuredClone(t)); setError(null) })
    .catch((e) => setError((e as { response?: { data?: { detail?: string } } })?.response?.data?.detail ?? (e instanceof Error ? e.message : 'Failed to read the table')))
  useEffect(() => { void load() }, [deviceId]) // eslint-disable-line react-hooks/exhaustive-deps

  const changed = !!table && !!original && JSON.stringify(table) !== JSON.stringify(original)

  const submit = async (write: boolean) => {
    if (!table || !original) return
    setBusy(true)
    setError(null)
    try {
      const updated = await apiClient.setCalibrationTable(deviceId, {
        baseline: table.baseline,
        intrinsic_left: table.intrinsic_left, intrinsic_right: table.intrinsic_right,
        world2left_rot: table.world2left_rot, world2right_rot: table.world2right_rot,
        rect_params: table.rect_params.map((r, index) => ({ index, fx: r.fx, fy: r.fy, ppx: r.ppx, ppy: r.ppy })),
        write,
      })
      setOriginal(updated)
      setTable(structuredClone(updated))
    } catch (e) {
      setError((e as { response?: { data?: { detail?: string } } })?.response?.data?.detail ?? (e instanceof Error ? e.message : 'Failed to update the table'))
    } finally {
      setBusy(false)
    }
  }

  const resetFactory = async () => {
    if (!window.confirm('Reset the device to its factory calibration? The current table is lost.')) return
    setBusy(true)
    try { await apiClient.resetFactoryCalibration(deviceId); await load() } catch (e) {
      setError((e as { response?: { data?: { detail?: string } } })?.response?.data?.detail ?? (e instanceof Error ? e.message : 'Reset failed'))
    } finally { setBusy(false) }
  }

  const rect = table?.rect_params[resolution]

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm" role="dialog" aria-label="Calibration Table">
      <div className="bg-rs-dark border border-gray-600 rounded-xl shadow-2xl w-full max-w-2xl mx-4 flex flex-col max-h-[90vh]">
        <div className="px-6 py-4 border-b border-gray-700">
          <h2 className="text-lg font-bold text-white">Calibration Table</h2>
          <p className="text-sm text-gray-400">{deviceName}{table ? ` · table v${table.version} · ${table.crc_valid ? 'CRC ok' : 'CRC invalid'}` : ''}</p>
        </div>
        <div className="p-6 space-y-4 overflow-y-auto">
          {error && <p className="text-red-400 text-sm" role="alert">{error}</p>}
          {!table && !error && <p className="text-gray-400 text-sm">Reading the calibration table…</p>}
          {table && original && (
            <>
              <label className="flex items-center gap-3 text-sm text-gray-300">
                Baseline (mm)
                <input className={`${input} ${table.baseline !== original.baseline ? 'border-yellow-500' : ''}`} type="number" step="any" value={table.baseline} aria-label="Baseline"
                  onChange={(e) => setTable({ ...table, baseline: Number(e.target.value) })} />
                <span className="text-xs text-gray-500">model: {table.brown_model ? 'Brown' : 'DS'}</span>
              </label>
              <div className="grid grid-cols-2 gap-4">
                {MATRICES.map(([key, label]) => (
                  <Matrix key={key} label={label} value={table[key]} original={original[key]} onChange={(m) => setTable({ ...table, [key]: m })} />
                ))}
              </div>
              <div className="space-y-2">
                <label className="flex items-center gap-3 text-sm text-gray-300">
                  Rectified resolution
                  <select className="bg-gray-900 border border-gray-600 rounded px-2 py-1 text-sm text-white" value={resolution} aria-label="Rectified resolution"
                    onChange={(e) => setResolution(Number(e.target.value))}>
                    {table.rect_params.map((r, i) => <option key={i} value={i}>{r.resolution}</option>)}
                  </select>
                </label>
                {rect && (
                  <div className="grid grid-cols-4 gap-2">
                    {(['fx', 'fy', 'ppx', 'ppy'] as const).map((k) => (
                      <label key={k} className="text-xs text-gray-400">
                        {k === 'fx' ? 'FocalX' : k === 'fy' ? 'FocalY' : k.toUpperCase()}
                        <input className={`${input} w-full ${rect[k] !== original.rect_params[resolution][k] ? 'border-yellow-500' : ''}`} type="number" step="any" value={rect[k]} aria-label={`Rectified ${k}`}
                          onChange={(e) => {
                            const params = table.rect_params.map((r) => ({ ...r }))
                            params[resolution] = { ...params[resolution], [k]: Number(e.target.value) }
                            setTable({ ...table, rect_params: params })
                          }} />
                      </label>
                    ))}
                  </div>
                )}
              </div>
              <p className="text-xs text-gray-500">
                Apply makes the edited table active on the running device only; Write stores it in the device&apos;s flash
                (Settings › Calibration must allow writing). Changed cells are outlined.
              </p>
            </>
          )}
        </div>
        <div className="px-6 py-4 bg-gray-800/50 flex flex-wrap justify-end gap-2">
          <button className={btn} onClick={() => void resetFactory()} disabled={busy || !table}>Reset to factory</button>
          <button className={btn} onClick={() => original && setTable(structuredClone(original))} disabled={!changed || busy}>Revert</button>
          <button className={primary} onClick={() => void submit(false)} disabled={!changed || busy}>Apply</button>
          <button className={primary} onClick={() => void submit(true)} disabled={busy || !table} title="Write the table to the device">Write</button>
          <button className={btn} onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  )
}
