import { describe, it, expect, beforeEach } from 'vitest'
import { http, HttpResponse } from 'msw'
import { server } from '../../mocks/server'
import { severityCounts, useConsoleStore, visibleEntries } from '@/store/console'
import type { LogEntry } from '@/api/types'

const entry = (id: number, severity: string, message: string, source = 'sdk'): LogEntry =>
  ({ id, ts: 1, severity, message, source })

describe('console store', () => {
  beforeEach(() => useConsoleStore.setState({ entries: [], show: { error: true, warn: true, info: true }, search: '', commandHistory: [], fwLogs: {} }))

  it('appends batches without duplicates and caps the buffer', () => {
    const { append } = useConsoleStore.getState()
    append(entry(1, 'info', 'a')); append(entry(1, 'info', 'a')); append(entry(2, 'warn', 'b'))
    expect(useConsoleStore.getState().entries.map((e) => e.id)).toEqual([1, 2])
  })

  it('counts severities and filters by toggles and search', () => {
    const entries = [entry(1, 'error', 'boom'), entry(2, 'warn', 'careful'), entry(3, 'info', 'hello'), entry(4, 'fatal', 'dead')]
    expect(severityCounts(entries)).toEqual({ error: 2, warn: 1, info: 1 })
    expect(visibleEntries({ entries, show: { error: true, warn: false, info: true }, search: '' }).map((e) => e.id)).toEqual([1, 3, 4])
    expect(visibleEntries({ entries, show: { error: true, warn: true, info: true }, search: 'CARE' }).map((e) => e.id)).toEqual([2])
  })

  it('backfills from the server after the newest known id', async () => {
    let asked: string | null = null
    server.use(http.get('/api/v1/logs/', ({ request }) => { asked = new URL(request.url).searchParams.get('after'); return HttpResponse.json([entry(8, 'info', 'late')]) }))
    useConsoleStore.getState().append(entry(7, 'info', 'x'))
    await useConsoleStore.getState().backfill()
    expect(asked).toBe('7')
    expect(useConsoleStore.getState().entries.map((e) => e.id)).toEqual([7, 8])
  })

  it('runs commands, keeps history, and treats clear locally', async () => {
    let sent: unknown = null
    server.use(http.post('/api/v1/devices/:deviceId/terminal', async ({ request }) => { sent = await request.json(); return HttpResponse.json({ output: 'ok' }) }))
    await useConsoleStore.getState().runCommand('dev', ' 10 00 00 00 ')
    expect(sent).toEqual({ line: '10 00 00 00' })
    useConsoleStore.getState().append(entry(1, 'info', 'x'))
    await useConsoleStore.getState().runCommand('dev', 'clear')
    expect(useConsoleStore.getState().entries).toEqual([])
    expect(useConsoleStore.getState().commandHistory).toEqual(['10 00 00 00', 'clear'])
  })

  it('toggles firmware logs per device', async () => {
    await useConsoleStore.getState().toggleFwLogs('dev')
    expect(useConsoleStore.getState().fwLogs.dev).toBe(true)
    await useConsoleStore.getState().toggleFwLogs('dev')
    expect(useConsoleStore.getState().fwLogs.dev).toBe(false)
  })
})
