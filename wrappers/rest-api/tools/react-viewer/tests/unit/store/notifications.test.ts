import { describe, it, expect, beforeEach } from 'vitest'
import { reportIssueUrl, useNotificationsStore } from '@/store/notifications'

describe('notifications store', () => {
  beforeEach(() => {
    useNotificationsStore.setState({ notices: [], snoozed: {} })
    localStorage.removeItem('rs-viewer-notifications')
  })

  it('stacks at most six cards, newest last', () => {
    for (let i = 0; i < 8; i++) useNotificationsStore.getState().push({ severity: 'info', title: `t${i}`, message: '' })
    const titles = useNotificationsStore.getState().notices.map((n) => n.title)
    expect(titles).toEqual(['t2', 't3', 't4', 't5', 't6', 't7'])
  })

  it('replaces a card of the same kind instead of stacking duplicates', () => {
    useNotificationsStore.getState().push({ kind: 'fw:1', severity: 'warn', title: 'a', message: '' })
    useNotificationsStore.getState().push({ kind: 'fw:1', severity: 'warn', title: 'b', message: '' })
    expect(useNotificationsStore.getState().notices.map((n) => n.title)).toEqual(['b'])
  })

  it('remind-me-later and never suppress the kind, and persist', () => {
    const store = useNotificationsStore.getState()
    store.push({ kind: 'fw:1', severity: 'warn', title: 'a', message: '' })
    store.dismiss(useNotificationsStore.getState().notices[0].id, 'later')
    expect(useNotificationsStore.getState().isSnoozed('fw:1')).toBe(true)
    store.push({ kind: 'fw:1', severity: 'warn', title: 'again', message: '' })
    expect(useNotificationsStore.getState().notices).toEqual([])

    store.push({ kind: 'cal:1', severity: 'info', title: 'c', message: '' })
    store.dismiss(useNotificationsStore.getState().notices[0].id, 'never')
    const saved = JSON.parse(localStorage.getItem('rs-viewer-notifications') ?? '{}')
    expect(saved.state.snoozed['cal:1']).toBe('never')
  })

  it('dismissing once keeps the kind eligible', () => {
    const store = useNotificationsStore.getState()
    store.push({ kind: 'k', severity: 'info', title: 'x', message: '' })
    store.dismiss(useNotificationsStore.getState().notices[0].id)
    expect(useNotificationsStore.getState().isSnoozed('k')).toBe(false)
  })

  it('prefills a GitHub issue with the cameras', () => {
    const url = reportIssueUrl([{ name: 'RealSense D455', serial_number: '123', firmware_version: '5.17.3.10' }], '2.59.0.0')
    expect(url.startsWith('https://github.com/realsenseai/librealsense/issues/new?body=')).toBe(true)
    expect(decodeURIComponent(url)).toContain('RealSense D455 S/N 123 FW 5.17.3.10')
  })
})
