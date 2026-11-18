import { describe, it, expect } from 'vitest'
import { searchGroup } from '@/utils/optionSearch'
import type { ControlGroup } from '@/api/types'
import { createMockOption } from '../../utils/test-utils'

const exposure = createMockOption({ option_id: 'Exposure', name: 'Exposure', category: 'Basic Controls' })
const gain = createMockOption({ option_id: 'Gain', name: 'Gain', category: 'Basic Controls' })
const laser = createMockOption({ option_id: 'Laser_Power', name: 'Laser Power', category: 'Basic Controls' })
const ppSpatial = createMockOption({
  option_id: 'PP_Spatial_Filter_Magnitude',
  name: 'Filter Magnitude',
  category: 'Post-Processing',
  filter_name: 'Spatial Filter',
})

// Real-world trap: this control's description contains the word "laser" but it
// is NOT a laser control. Searching "laser" must not surface it.
const syncMode = createMockOption({
  option_id: 'inter_cam_sync_mode',
  name: 'Inter Cam Sync Mode',
  category: 'Basic Controls',
  description:
    'Inter-camera synchronization mode: ... 259 and 260 for two frames per trigger with laser ON-OFF and OFF-ON.',
})

const all = [exposure, gain, laser, ppSpatial]

const groupOf = (options = all, name = ''): ControlGroup =>
  ({ section: 'Controls', sensorIndex: 0, name, options })

describe('searchGroup', () => {
  it('returns all options unchanged for an empty query', () => {
    expect(searchGroup(groupOf(), '')).toEqual(all)
    expect(searchGroup(groupOf(), '   ')).toEqual(all)
  })

  it('matches by substring of the name, case-insensitively', () => {
    const r = searchGroup(groupOf(), 'GAI')
    expect(r).toContain(gain)
    expect(r).not.toContain(exposure)
  })

  it('does not match a control merely because its description mentions the term', () => {
    const r = searchGroup(groupOf([laser, syncMode]), 'laser')
    expect(r).toContain(laser)
    expect(r).not.toContain(syncMode)
  })

  it('never returns a control without the typed term in its labels', () => {
    // "option" appears in option ids but in no name/category/filter name, so it
    // must return nothing rather than loose fuzzy hits.
    expect(searchGroup(groupOf(), 'option')).toBeNull()
    expect(searchGroup(groupOf(), 'zzzqqq')).toBeNull()
  })

  it('does not tolerate typos (only what the user can see matches)', () => {
    expect(searchGroup(groupOf(), 'expsure')).toBeNull()
  })

  it('surfaces post-processing params via their filter name (spatial)', () => {
    const r = searchGroup(groupOf(), 'spatial')
    expect(r).toContain(ppSpatial)
    expect(r).not.toContain(gain)
  })

  it('preserves original array order among matches', () => {
    const r = searchGroup(groupOf(), 'a')! // Gain, Laser Power, Filter Magnitude
    const idx = r.map(o => all.indexOf(o))
    expect(idx).toEqual([...idx].sort((a, b) => a - b))
  })

  it('keeps every option of a group its own name matched', () => {
    expect(searchGroup(groupOf(all, 'depth_table'), 'depth table')).toEqual(all)
  })
})
