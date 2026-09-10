import { describe, it, expect } from 'vitest'
import { orderOptions } from '@/utils/optionOrder'
import { createMockOption } from '../../utils/test-utils'

const ids = (list: { option_id: string }[]) => list.map((o) => o.option_id)

describe('orderOptions', () => {
  it('puts the preset and exposure switches first and the color tuning last', () => {
    const options = ['brightness', 'exposure', 'emitter_enabled', 'gain', 'visual_preset', 'white_balance', 'enable_auto_exposure']
      .map((option_id) => createMockOption({ option_id }))

    expect(ids(orderOptions(options))).toEqual([
      'visual_preset', 'emitter_enabled', 'enable_auto_exposure', 'exposure', 'gain', 'brightness', 'white_balance',
    ])
  })

  it('keeps the SDK order for everything else and tolerates missing anchors', () => {
    const options = ['gain', 'exposure'].map((option_id) => createMockOption({ option_id }))
    expect(ids(orderOptions(options))).toEqual(['gain', 'exposure'])
  })

  it('matches option ids case-insensitively', () => {
    const options = ['Gain', 'Visual_Preset'].map((option_id) => createMockOption({ option_id }))
    expect(ids(orderOptions(options))).toEqual(['Visual_Preset', 'Gain'])
  })
})
