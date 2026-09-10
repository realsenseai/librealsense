import type { OptionInfo } from '../api/types'

/**
 * The legacy viewer's control order (device-model.cpp): the preset and the exposure switches
 * first, then the rest as the SDK lists them, with the color-tuning controls moved to the end.
 */
const FIRST = ['visual_preset', 'emitter_enabled', 'enable_auto_exposure', 'depth_auto_exposure_mode']
const LAST = [
  'backlight_compensation', 'brightness', 'contrast', 'gamma', 'hue', 'saturation', 'sharpness',
  'enable_auto_white_balance', 'white_balance',
]

export function orderOptions(options: OptionInfo[]): OptionInfo[] {
  const byId = new Map(options.map((o) => [o.option_id.toLowerCase(), o]))
  const pick = (ids: string[]) => ids.map((id) => byId.get(id)).filter((o): o is OptionInfo => !!o)
  const placed = new Set([...FIRST, ...LAST])
  return [
    ...pick(FIRST),
    ...options.filter((o) => !placed.has(o.option_id.toLowerCase())),
    ...pick(LAST),
  ]
}
