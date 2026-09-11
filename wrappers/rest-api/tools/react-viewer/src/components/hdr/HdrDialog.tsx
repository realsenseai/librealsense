import { useEffect, useRef, useState } from 'react'
import { apiClient } from '../../api/client'
import type { HdrControls, HdrPreset, HdrStatus } from '../../api/types'
import { MAX_ITEMS, MIN_ITEMS, clampControl, defaultPreset, fromJson, newItem, samePreset, toJson } from '../../utils/hdrPreset'

interface HdrDialogProps {
  deviceId: string
  deviceName: string
  onClose: () => void
}

const input = 'bg-gray-900 border border-gray-600 rounded px-2 py-1 text-sm text-white w-28'
const btn = 'px-4 py-1.5 rounded text-sm bg-gray-700 hover:bg-gray-600 text-white disabled:opacity-40 disabled:cursor-not-allowed'

function NumberField({ label, value, onChange, title }: { label: string; value: number; onChange: (v: number) => void; title?: string }) {
  return (
    <label className="flex items-center gap-2 text-sm text-gray-300" title={title}>
      <span className="w-32">{label}</span>
      <input className={input} type="number" value={value} aria-label={label} onChange={(e) => onChange(Number(e.target.value))} />
    </label>
  )
}

/** The legacy "HDR Configuration" window (hdr-model.cpp render_hdr_config_window). */
export function HdrDialog({ deviceId, deviceName, onClose }: HdrDialogProps) {
  const [status, setStatus] = useState<HdrStatus | null>(null)
  const [current, setCurrent] = useState<HdrPreset | null>(null)
  const [draft, setDraft] = useState<HdrPreset | null>(null)
  const [error, setError] = useState<string | null>(null)
  const [open, setOpen] = useState<Record<number, boolean>>({ 0: true })
  const fileRef = useRef<HTMLInputElement>(null)

  useEffect(() => {
    let cancelled = false
    apiClient.getHdr(deviceId)
      .then((s) => {
        if (cancelled) return
        setStatus(s)
        const p = s.preset ?? defaultPreset()
        setCurrent(p)
        setDraft(structuredClone(p))
      })
      .catch((e) => { if (!cancelled) setError(e instanceof Error ? e.message : 'Failed to read the HDR configuration') })
    return () => { cancelled = true }
  }, [deviceId])

  const gain = status?.gain_range ?? null
  const exposure = status?.exposure_range ?? null
  const auto = draft?.control_type_auto ?? false

  const update = (fn: (p: HdrPreset) => void) => setDraft((p) => { if (!p) return p; const next = structuredClone(p); fn(next); return next })
  const setControl = (index: number, key: keyof HdrControls, value: number) =>
    update((p) => { p.items[index].controls = clampControl(p.items[index].controls, key, value, gain, exposure) })

  const apply = async () => {
    if (!draft) return false
    try {
      const applied = await apiClient.applyHdr(deviceId, { ...draft, iterations: 0 })
      setStatus(applied)
      const p = applied.preset ?? draft
      setCurrent(p)
      setDraft(structuredClone(p))
      setError(null)
      return true
    } catch (e) {
      const detail = (e as { response?: { data?: { detail?: string } } })?.response?.data?.detail
      setError(detail ?? (e instanceof Error ? e.message : 'Failed to apply the HDR configuration'))
      return false
    }
  }

  const loadFile = async (file: File) => {
    try {
      const p = fromJson(await file.text())
      if (p.items.length === 0) throw new Error('No "hdr-preset" items in this file')
      setDraft(p)
      setError(null)
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Failed to read the file')
    }
  }

  const saveFile = () => {
    if (!draft) return
    const url = URL.createObjectURL(new Blob([toJson(draft)], { type: 'application/json' }))
    const a = document.createElement('a')
    a.href = url
    a.download = `hdr-preset-${draft.id}.json`
    a.click()
    URL.revokeObjectURL(url)
  }

  const changed = !samePreset(draft, current)

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm" role="dialog" aria-label="HDR Configuration">
      <div className="bg-rs-dark border border-gray-600 rounded-xl shadow-2xl w-full max-w-2xl mx-4 flex flex-col max-h-[90vh]">
        <div className="px-6 py-4 border-b border-gray-700">
          <h2 className="text-lg font-bold text-white">HDR Configuration</h2>
          <p className="text-sm text-gray-400">{deviceName}</p>
        </div>

        <div className="p-6 space-y-4 overflow-y-auto">
          {error && <p className="text-red-400 text-sm" role="alert">{error}</p>}
          {!status && !error && <p className="text-gray-400 text-sm">Reading the HDR configuration…</p>}
          {status && !status.supported && (
            <p className="text-yellow-300 text-sm">This device has no HDR preset support (no &quot;hdr-preset&quot; section in its firmware).</p>
          )}
          {status?.supported && draft && (
            <>
              <div className="flex flex-wrap items-center gap-6">
                <label className="flex items-center gap-2 text-sm text-gray-300">
                  Preset ID
                  <input className={input} value={draft.id} aria-label="Preset ID" onChange={(e) => update((p) => { p.id = e.target.value })} />
                </label>
                <label className="flex items-center gap-2 text-sm text-gray-300" title="Enable auto exposure for HDR, otherwise manual exposure and gain will be used">
                  <input type="checkbox" checked={auto} onChange={(e) => update((p) => { p.control_type_auto = e.target.checked })} />
                  Auto HDR
                </label>
              </div>

              <div className="flex items-center gap-2">
                <h3 className="text-yellow-200 font-semibold text-sm">Preset Items</h3>
                <button className={`${btn} px-2 py-0.5`} aria-label="Remove last item" disabled={draft.items.length <= MIN_ITEMS}
                  onClick={() => update((p) => { p.items.pop() })}>−</button>
                <button className={`${btn} px-2 py-0.5`} aria-label="Add item" disabled={draft.items.length >= MAX_ITEMS}
                  onClick={() => update((p) => { p.items.push(newItem(gain, exposure)) })}>+</button>
              </div>

              <div className="space-y-2">
                {draft.items.map((item, i) => (
                  <div key={i} className="border border-gray-700 rounded-lg">
                    <button className="w-full text-left px-3 py-2 text-sm text-white hover:bg-gray-700/50 flex justify-between"
                      onClick={() => setOpen((o) => ({ ...o, [i]: !o[i] }))} aria-expanded={!!open[i]}>
                      <span>Preset Item {i + 1}</span>
                      <span className="text-gray-400 text-xs">
                        {auto ? `Δgain ${item.controls.delta_gain}, Δexp ${item.controls.delta_exp}` : `gain ${item.controls.depth_gain}, exp ${item.controls.depth_exp}`} × {item.iterations}
                      </span>
                    </button>
                    {open[i] && (
                      <div className="px-3 pb-3 space-y-2" data-testid={`hdr-item-${i}`}>
                        <NumberField label="Iterations" value={item.iterations}
                          title="Number of consecutive frames to be received with the configuration in this preset item per global iteration"
                          onChange={(v) => update((p) => { p.items[i].iterations = Math.max(1, Math.trunc(v) || 1) })} />
                        <div className="border-t border-gray-700 pt-2 space-y-2">
                          <NumberField label={auto ? 'Gain Delta' : 'Gain Value'} value={auto ? item.controls.delta_gain : item.controls.depth_gain}
                            onChange={(v) => setControl(i, auto ? 'delta_gain' : 'depth_gain', v)} />
                          <NumberField label={auto ? 'Exposure Delta' : 'Exposure Value'} value={auto ? item.controls.delta_exp : item.controls.depth_exp}
                            onChange={(v) => setControl(i, auto ? 'delta_exp' : 'depth_exp', v)} />
                        </div>
                      </div>
                    )}
                  </div>
                ))}
              </div>

              <h3 className="text-yellow-200 font-semibold text-sm border-b border-gray-700 pb-1">Load/Save Configuration</h3>
              <div className="flex flex-wrap gap-2">
                <button className={btn} onClick={() => setDraft(defaultPreset())}>Load defaults</button>
                <button className={btn} onClick={() => fileRef.current?.click()}>Load from File</button>
                <button className={btn} onClick={saveFile}>Save to File</button>
                <input ref={fileRef} type="file" accept=".json,application/json" className="hidden" aria-label="HDR preset file"
                  onChange={(e) => { const f = e.target.files?.[0]; if (f) void loadFile(f); e.target.value = '' }} />
              </div>
            </>
          )}
        </div>

        <div className="px-6 py-4 bg-gray-800/50 flex justify-center gap-3">
          {status?.supported && (
            <>
              <button className="px-5 py-2 bg-rs-blue text-white rounded-lg hover:bg-blue-600 text-sm" onClick={async () => { if (await apply()) onClose() }}>OK</button>
              <button className={`px-5 py-2 rounded-lg text-sm ${changed ? 'bg-rs-blue text-white hover:bg-blue-600' : 'bg-gray-700 text-gray-300'}`} onClick={() => void apply()}>Apply</button>
            </>
          )}
          <button className={btn} onClick={onClose}>{status?.supported ? 'Cancel' : 'Close'}</button>
        </div>
      </div>
    </div>
  )
}
