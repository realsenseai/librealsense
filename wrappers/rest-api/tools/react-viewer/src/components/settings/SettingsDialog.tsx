import { useEffect, useState } from 'react'
import { useSettingsStore } from '../../store/settings'
import type { ViewerSettings } from '../../api/types'

const TABS = ['Playback & Record', 'General', 'Online'] as const
type Tab = typeof TABS[number]
const LAST_TAB_KEY = 'rs-settings-tab'

interface SettingsDialogProps {
  isOpen: boolean
  onClose: () => void
}

function readLastTab(): Tab {
  try {
    const saved = localStorage.getItem(LAST_TAB_KEY)
    return (TABS as readonly string[]).includes(saved ?? '') ? (saved as Tab) : TABS[0]
  } catch {
    return TABS[0]
  }
}

function Field({ label, children, hint }: { label: string; children: React.ReactNode; hint?: string }) {
  return (
    <label className="flex flex-col gap-1 text-sm">
      <span className="text-gray-300">{label}</span>
      {children}
      {hint && <span className="text-xs text-gray-500">{hint}</span>}
    </label>
  )
}

function Check({ label, checked, onChange, disabled }: {
  label: string; checked: boolean; onChange: (v: boolean) => void; disabled?: boolean
}) {
  return (
    <label className={`flex items-center gap-2 text-sm ${disabled ? 'text-gray-500' : 'text-gray-300'}`}>
      <input type="checkbox" checked={checked} disabled={disabled} onChange={(e) => onChange(e.target.checked)} />
      {label}
    </label>
  )
}

const input = 'bg-gray-800 border border-gray-600 rounded px-2 py-1 text-white text-sm w-full'

/**
 * The legacy viewer's settings window: three tabs, Apply / Save & Close / Cancel. Edits are
 * kept in a draft until applied, and the whole draft is sent as one partial update.
 */
export function SettingsDialog({ isOpen, onClose }: SettingsDialogProps) {
  const { settings, updateSettings, fetchSettings } = useSettingsStore()
  const [tab, setTab] = useState<Tab>(readLastTab)
  const [draft, setDraft] = useState<ViewerSettings | null>(null)
  const [saving, setSaving] = useState(false)
  const [error, setError] = useState<string | null>(null)

  useEffect(() => {
    if (!isOpen) return
    if (!settings) void fetchSettings()
    setDraft(settings ? structuredClone(settings) : null)
    setError(null)
  }, [isOpen, settings, fetchSettings])

  useEffect(() => {
    try { localStorage.setItem(LAST_TAB_KEY, tab) } catch { /* private mode */ }
  }, [tab])

  if (!isOpen) return null

  const patch = <G extends keyof ViewerSettings>(group: G, values: Partial<ViewerSettings[G]>) =>
    setDraft((d) => d && { ...d, [group]: { ...d[group], ...values } })

  const apply = async (): Promise<boolean> => {
    if (!draft) return false
    setSaving(true)
    setError(null)
    try {
      await updateSettings(draft)
      return true
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Failed to save settings')
      return false
    } finally {
      setSaving(false)
    }
  }

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm" role="dialog" aria-label="Settings">
      <div className="bg-rs-dark border border-gray-600 rounded-xl shadow-2xl w-full max-w-xl mx-4 flex flex-col max-h-[90vh]">
        <div className="px-6 py-4 border-b border-gray-700 flex items-center justify-between">
          <h2 className="text-lg font-bold text-white">Settings</h2>
          <div className="flex bg-gray-700 rounded-lg p-1" role="tablist">
            {TABS.map((t) => (
              <button key={t} role="tab" aria-selected={tab === t} onClick={() => setTab(t)}
                className={`px-3 py-1 rounded-md text-sm ${tab === t ? 'bg-rs-blue text-white' : 'text-gray-300 hover:text-white'}`}>
                {t}
              </button>
            ))}
          </div>
        </div>

        <div className="p-6 space-y-4 overflow-y-auto">
          {!draft && <p className="text-gray-400 text-sm">Loading settings…</p>}

          {draft && tab === 'Playback & Record' && (
            <>
              <Field label="Recording file name">
                <select className={input} value={draft.record.file_save_mode}
                  onChange={(e) => patch('record', { file_save_mode: e.target.value as ViewerSettings['record']['file_save_mode'] })}>
                  <option value="auto">Select filename automatically</option>
                  <option value="ask">Ask me every time</option>
                </select>
              </Field>
              <Field label="Default recording folder" hint="Empty: the server's recordings folder">
                <input className={input} value={draft.record.default_path}
                  onChange={(e) => patch('record', { default_path: e.target.value })} />
              </Field>
              <Field label="ROS-bag compression">
                <select className={input} value={draft.record.compression}
                  onChange={(e) => patch('record', { compression: e.target.value as ViewerSettings['record']['compression'] })}>
                  <option value="auto">Device default</option>
                  <option value="always">Always</option>
                  <option value="never">Never</option>
                </select>
              </Field>
            </>
          )}

          {draft && tab === 'General' && (
            <>
              <Field label="Units of measurement">
                <select className={input} value={draft.viewer.metric_system ? 'metric' : 'imperial'}
                  onChange={(e) => patch('viewer', { metric_system: e.target.value === 'metric' })}>
                  <option value="metric">Metric</option>
                  <option value="imperial">Imperial</option>
                </select>
              </Field>
              <Field label="Output console max entries">
                <input className={input} type="number" min={10} max={100000} value={draft.console.max_entries}
                  onChange={(e) => patch('console', { max_entries: Number(e.target.value) })} />
              </Field>
              <Field label="Minimal log severity">
                <select className={input} value={draft.console.log_severity}
                  onChange={(e) => patch('console', { log_severity: e.target.value as ViewerSettings['console']['log_severity'] })}>
                  <option value="debug">Debug</option>
                  <option value="info">Info</option>
                  <option value="warn">Warning</option>
                  <option value="error">Error</option>
                </select>
              </Field>
              <Check label="Log to file" checked={draft.console.log_to_file}
                onChange={(v) => patch('console', { log_to_file: v })} />
              {draft.console.log_to_file && (
                <Field label="Log file name">
                  <input className={input} value={draft.console.log_filename}
                    onChange={(e) => patch('console', { log_filename: e.target.value })} />
                </Field>
              )}
              <Field label="Firmware logs XML file" hint="Parser definitions for the firmware-log console">
                <input className={input} value={draft.paths.hwlogger_xml}
                  onChange={(e) => patch('paths', { hwlogger_xml: e.target.value })} />
              </Field>
              <Field label="Commands XML file" hint="Terminal command definitions">
                <input className={input} value={draft.paths.commands_xml}
                  onChange={(e) => patch('paths', { commands_xml: e.target.value })} />
              </Field>
              <Field label="Presets folder" hint="Empty: ~/librealsense2/presets (under Documents on Windows)">
                <input className={input} value={draft.paths.presets_folder}
                  onChange={(e) => patch('paths', { presets_folder: e.target.value })} />
              </Field>
              <Check label="Enable DDS (Ethernet cameras) — takes effect after the server restarts"
                checked={draft.context.dds_enabled} onChange={(v) => patch('context', { dds_enabled: v })} />
              <Field label="DDS domain ID">
                <input className={input} type="number" min={0} max={232} value={draft.context.dds_domain}
                  disabled={!draft.context.dds_enabled}
                  onChange={(e) => patch('context', { dds_domain: Number(e.target.value) })} />
              </Field>
              <Check label="Allow writing calibration to the device" checked={draft.calibration.enable_writing}
                onChange={(v) => patch('calibration', { enable_writing: v })} />
              <Check label="Performance mode: start every post-processing filter disabled"
                checked={draft.post_processing.performance_mode}
                onChange={(v) => patch('post_processing', { performance_mode: v })} />
            </>
          )}

          {draft && tab === 'Online' && (
            <>
              <Check label="Use the official RealSense update server" checked={draft.update.sw_update_official_server}
                onChange={(v) => patch('update', { sw_update_official_server: v })} />
              <Field label="Custom versions database URL" hint="http(s):// or file://">
                <input className={input} value={draft.update.sw_update_url} disabled={draft.update.sw_update_official_server}
                  onChange={(e) => patch('update', { sw_update_url: e.target.value })} />
              </Field>
              <Check label="Recommend calibration when the camera reports it is needed"
                checked={draft.update.recommend_calibration}
                onChange={(v) => patch('update', { recommend_calibration: v })} />
            </>
          )}

          {error && <p className="text-red-400 text-sm">{error}</p>}
        </div>

        <div className="px-6 py-4 bg-gray-800/50 flex justify-end gap-2 border-t border-gray-700">
          <button onClick={onClose} className="px-4 py-2 text-gray-300 hover:text-white text-sm">Cancel</button>
          <button onClick={() => void apply()} disabled={!draft || saving}
            className="px-4 py-2 bg-gray-700 text-white rounded-lg hover:bg-gray-600 text-sm disabled:opacity-50">
            Apply
          </button>
          <button onClick={async () => { if (await apply()) onClose() }} disabled={!draft || saving}
            className="px-4 py-2 bg-rs-blue text-white rounded-lg hover:bg-blue-600 text-sm font-medium disabled:opacity-50">
            Save &amp; Close
          </button>
        </div>
      </div>
    </div>
  )
}
