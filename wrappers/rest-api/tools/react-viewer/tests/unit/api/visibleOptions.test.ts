import { describe, it, expect } from 'vitest'
import { visibleOptions, type WireOption } from '@/api/types'

const wire = (o: Partial<WireOption>): WireOption => ({
  description: '', current_value: 1, default_value: 0, min_value: 0, max_value: 10, step: 1, read_only: false,
  value_descriptions: {}, ...o,
})

describe('visibleOptions', () => {
  it('draws, as the legacy viewer, only options it can read now and has a range for', () => {
    const shown = visibleOptions({
      exposure: wire({}),
      asic_temperature: wire({ current_value: null }), // not streaming
      ambient_light: wire({ min_value: null, max_value: null, step: null, default_value: null }), // range unreadable
    })
    expect(shown.map(o => o.option_id)).toEqual(['exposure'])
  })
})
