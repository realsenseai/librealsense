import { describe, it, expect, beforeEach } from 'vitest'
import { orderKeys, tileKey, useLayoutStore } from '@/store/layout'

describe('layout store', () => {
  beforeEach(() => {
    useLayoutStore.setState({ tileOrder: {}, maximized: null })
    localStorage.removeItem('rs-viewer-layout')
  })

  it('orders new tiles depth, color, infrared, motion like the legacy viewer', () => {
    const present = ['d:gyro', 'd:infrared-2', 'd:color', 'd:infrared-1', 'd:depth', 'd:accel']
    expect(orderKeys(present)).toEqual(['d:depth', 'd:color', 'd:infrared-1', 'd:infrared-2', 'd:accel', 'd:gyro'])
  })

  it('keeps the remembered order and appends tiles it has not seen', () => {
    expect(orderKeys(['d:depth', 'd:color', 'd:gyro'], ['d:color', 'd:depth', 'd:stale'])).toEqual(['d:color', 'd:depth', 'd:gyro'])
  })

  it('swaps two tiles and remembers the arrangement per device', () => {
    const present = [tileKey('d', 'Depth'), tileKey('d', 'Color'), tileKey('d', 'Infrared-1')]
    useLayoutStore.getState().swapTiles('d', 'd:depth', 'd:infrared-1', present)

    expect(useLayoutStore.getState().tileOrder.d).toEqual(['d:infrared-1', 'd:color', 'd:depth'])
    expect(JSON.parse(localStorage.getItem('rs-viewer-layout') ?? '{}').state.tileOrder.d[0]).toBe('d:infrared-1')
  })

  it('does not persist which tile is maximized', () => {
    useLayoutStore.getState().setMaximized('d:depth')
    expect(useLayoutStore.getState().maximized).toBe('d:depth')
    expect(JSON.parse(localStorage.getItem('rs-viewer-layout') ?? '{}').state.maximized).toBeUndefined()
  })
})
