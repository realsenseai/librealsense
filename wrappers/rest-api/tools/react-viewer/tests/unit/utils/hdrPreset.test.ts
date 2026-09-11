import { describe, it, expect } from 'vitest'
import { clampControl, defaultPreset, fromJson, toJson } from '@/utils/hdrPreset'

describe('HDR preset JSON', () => {
  it('round-trips a manual sequence through the device JSON', () => {
    const p = defaultPreset()
    const j = JSON.parse(toJson(p))
    expect(j['hdr-preset'].items).toEqual([
      { iterations: '1', controls: { 'depth-gain': '16', 'depth-exposure': '1' } },
      { iterations: '1', controls: { 'depth-gain': '16', 'depth-exposure': '8500' } },
    ])
    expect(fromJson(toJson(p))).toEqual(p)
  })

  it('writes the auto marker item and reads it back as the auto flag', () => {
    const p = { ...defaultPreset(), control_type_auto: true }
    p.items[0].controls.delta_exp = -200
    const j = JSON.parse(toJson(p))
    expect(j['hdr-preset'].items[0]).toEqual({ iterations: '1', controls: { 'depth-ae': '1' } })
    expect(j['hdr-preset'].items[1].controls).toEqual({ 'depth-ae-gain': '0', 'depth-ae-exp': '-200' })
    const back = fromJson(toJson(p))
    expect(back.control_type_auto).toBe(true)
    expect(back.items).toHaveLength(2)
    expect(back.items[0].controls.delta_exp).toBe(-200)
  })

  it('yields an empty preset for JSON without the section', () => {
    expect(fromJson('{"parameters": {}}')).toEqual({ id: '0', iterations: 0, control_type_auto: false, items: [] })
  })

  it('clamps values to the range and deltas to +-max', () => {
    const gain = { min: 16, max: 248, step: 1, default: 16 }
    const exp = { min: 1, max: 165000, step: 1, default: 8500 }
    const c = defaultPreset().items[0].controls
    expect(clampControl(c, 'depth_gain', 1000, gain, exp).depth_gain).toBe(248)
    expect(clampControl(c, 'depth_exp', 0, gain, exp).depth_exp).toBe(1)
    expect(clampControl(c, 'delta_gain', -999, gain, exp).delta_gain).toBe(-248)
    expect(clampControl(c, 'delta_exp', 3.7, gain, exp).delta_exp).toBe(3)
  })
})
