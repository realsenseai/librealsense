import type { HdrControls, HdrItem, HdrPreset, OptionRangeInfo } from '../api/types'

export const MIN_ITEMS = 2
export const MAX_ITEMS = 6

/** The legacy default: two exposures, alternating every frame. */
export function defaultPreset(): HdrPreset {
  return {
    id: '0', iterations: 0, control_type_auto: false,
    items: [
      { iterations: 1, controls: { depth_gain: 16, depth_exp: 1, delta_gain: 0, delta_exp: 0 } },
      { iterations: 1, controls: { depth_gain: 16, depth_exp: 8500, delta_gain: 0, delta_exp: 0 } },
    ],
  }
}

export function newItem(gain: OptionRangeInfo | null, exposure: OptionRangeInfo | null): HdrItem {
  return { iterations: 1, controls: { depth_gain: gain?.default ?? 16, depth_exp: exposure?.default ?? 8500, delta_gain: 0, delta_exp: 0 } }
}

const clamp = (v: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, v))

/** Clamp one control the way the legacy editor does: deltas within +-max, values within the range. */
export function clampControl(c: HdrControls, key: keyof HdrControls, value: number, gain: OptionRangeInfo | null, exposure: OptionRangeInfo | null): HdrControls {
  const v = Math.trunc(Number.isFinite(value) ? value : 0)
  const out = { ...c }
  if (key === 'depth_gain') out.depth_gain = gain ? clamp(v, gain.min, gain.max) : v
  if (key === 'depth_exp') out.depth_exp = exposure ? clamp(v, exposure.min, exposure.max) : v
  if (key === 'delta_gain') out.delta_gain = gain ? clamp(v, -gain.max, gain.max) : v
  if (key === 'delta_exp') out.delta_exp = exposure ? clamp(v, -exposure.max, exposure.max) : v
  return out
}

/** Parse a device JSON's "hdr-preset" section (hdr_preset::from_json). */
export function fromJson(text: string): HdrPreset {
  const j = text ? JSON.parse(text) as Record<string, unknown> : {}
  const hp = j['hdr-preset'] as { id?: string; iterations?: string; items?: { iterations?: string; controls?: Record<string, string> }[] } | undefined
  const preset: HdrPreset = { id: '0', iterations: 0, control_type_auto: false, items: [] }
  if (!hp) return preset
  preset.id = String(hp.id ?? '0')
  preset.iterations = parseInt(String(hp.iterations ?? '0'), 10) || 0
  for (const item of hp.items ?? []) {
    const ctrl: HdrControls = { depth_gain: 0, depth_exp: 0, delta_gain: 0, delta_exp: 0 }
    let marker = false
    for (const [name, value] of Object.entries(item.controls ?? {})) {
      const n = parseInt(value, 10) || 0
      if (name === 'depth-ae') { preset.control_type_auto = true; marker = true }
      else if (name === 'depth-ae-gain') { preset.control_type_auto = true; ctrl.delta_gain = n }
      else if (name === 'depth-ae-exp') { preset.control_type_auto = true; ctrl.delta_exp = n }
      else if (name === 'depth-gain') ctrl.depth_gain = n
      else if (name === 'depth-exposure') ctrl.depth_exp = n
    }
    if (marker) continue
    preset.items.push({ iterations: parseInt(String(item.iterations ?? '1'), 10) || 1, controls: ctrl })
  }
  return preset
}

/** Serialize as the device JSON (hdr_preset::to_json). */
export function toJson(preset: HdrPreset): string {
  const items: { iterations: string; controls: Record<string, string> }[] = []
  if (preset.control_type_auto) items.push({ iterations: '1', controls: { 'depth-ae': '1' } })
  for (const item of preset.items) {
    const c = item.controls
    items.push({
      iterations: String(item.iterations),
      controls: preset.control_type_auto
        ? { 'depth-ae-gain': String(c.delta_gain), 'depth-ae-exp': String(c.delta_exp) }
        : { 'depth-gain': String(c.depth_gain), 'depth-exposure': String(c.depth_exp) },
    })
  }
  return JSON.stringify({ 'hdr-preset': { id: preset.id, iterations: String(preset.iterations), items } }, null, 4)
}

export function samePreset(a: HdrPreset | null, b: HdrPreset | null): boolean {
  return JSON.stringify(a) === JSON.stringify(b)
}
