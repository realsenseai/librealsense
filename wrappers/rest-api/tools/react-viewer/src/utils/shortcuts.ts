/**
 * Global keyboard shortcuts, the legacy viewer's map (Space pauses every stream, R resets
 * the 3D view, ...). Keys typed into a form control never trigger a shortcut.
 */

export type ShortcutHandlers = Record<string, (event: KeyboardEvent) => void>

const FORM_TAGS = new Set(['INPUT', 'TEXTAREA', 'SELECT', 'BUTTON'])

export function isTypingTarget(target: EventTarget | null): boolean {
  const el = target as HTMLElement | null
  return !!el && (FORM_TAGS.has(el.tagName) || el.isContentEditable)
}

/** Key name for a keydown event: "Space", "R", "Shift+Z", "F8". */
export function shortcutName(event: KeyboardEvent): string {
  const key = event.key === ' ' ? 'Space' : event.key.length === 1 ? event.key.toUpperCase() : event.key
  const mods = [event.ctrlKey && 'Ctrl', event.shiftKey && 'Shift', event.altKey && 'Alt'].filter(Boolean)
  return [...mods, key].join('+')
}

/** Install `handlers` keyed by `shortcutName`; returns the uninstall function. */
export function installShortcuts(handlers: ShortcutHandlers, target: Window = window): () => void {
  const onKeyDown = (event: KeyboardEvent) => {
    if (event.repeat || isTypingTarget(event.target)) return
    const handler = handlers[shortcutName(event)]
    if (!handler) return
    event.preventDefault()
    handler(event)
  }
  target.addEventListener('keydown', onKeyDown)
  return () => target.removeEventListener('keydown', onKeyDown)
}
