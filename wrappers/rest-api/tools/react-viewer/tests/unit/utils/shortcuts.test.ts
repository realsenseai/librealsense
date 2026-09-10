import { describe, it, expect, vi } from 'vitest'
import { installShortcuts, shortcutName } from '@/utils/shortcuts'

const press = (key: string, target: EventTarget = document.body, init: KeyboardEventInit = {}) => {
  const event = new KeyboardEvent('keydown', { key, bubbles: true, cancelable: true, ...init })
  Object.defineProperty(event, 'target', { value: target })
  window.dispatchEvent(event)
  return event
}

describe('shortcuts', () => {
  it('names keys the way the handler map does', () => {
    expect(shortcutName(new KeyboardEvent('keydown', { key: ' ' }))).toBe('Space')
    expect(shortcutName(new KeyboardEvent('keydown', { key: 'r' }))).toBe('R')
    expect(shortcutName(new KeyboardEvent('keydown', { key: 'z', shiftKey: true }))).toBe('Shift+Z')
    expect(shortcutName(new KeyboardEvent('keydown', { key: 'F8' }))).toBe('F8')
  })

  it('runs the handler and swallows the key', () => {
    const space = vi.fn()
    const uninstall = installShortcuts({ Space: space })
    const event = press(' ')
    expect(space).toHaveBeenCalledTimes(1)
    expect(event.defaultPrevented).toBe(true)
    uninstall()
    press(' ')
    expect(space).toHaveBeenCalledTimes(1)
  })

  it('ignores keys typed into form controls and auto-repeat', () => {
    const space = vi.fn()
    const uninstall = installShortcuts({ Space: space })
    press(' ', document.createElement('input'))
    press(' ', document.createElement('select'))
    press(' ', document.body, { repeat: true })
    expect(space).not.toHaveBeenCalled()
    uninstall()
  })
})
