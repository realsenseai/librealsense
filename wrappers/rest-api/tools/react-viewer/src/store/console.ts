import { create } from 'zustand'
import { apiClient } from '../api/client'
import type { LogEntry } from '../api/types'

const MAX_ENTRIES = 5000
const HISTORY_MAX = 100

interface ConsoleState {
  entries: LogEntry[]
  isOpen: boolean
  /** Severity filters, as the legacy counters toggle them. */
  show: { error: boolean; warn: boolean; info: boolean }
  search: string
  fwLogs: Record<string, boolean> // device_id -> collecting
  commandHistory: string[]
  commandNames: string[]
  append: (entry: LogEntry) => void
  backfill: () => Promise<void>
  clear: () => Promise<void>
  setOpen: (open: boolean) => void
  toggleSeverity: (severity: keyof ConsoleState['show']) => void
  setSearch: (search: string) => void
  toggleFwLogs: (deviceId: string) => Promise<void>
  recoverFlashLogs: (deviceId: string) => Promise<number>
  runCommand: (deviceId: string, line: string) => Promise<void>
  fetchCommandNames: () => Promise<void>
}

const severityBucket = (severity: string): keyof ConsoleState['show'] =>
  severity === 'error' || severity === 'fatal' ? 'error' : severity === 'warn' || severity === 'warning' ? 'warn' : 'info'

/** The output console (legacy output-model): everything the server logs, live over the socket. */
export const useConsoleStore = create<ConsoleState>((set, get) => ({
  entries: [],
  isOpen: false,
  show: { error: true, warn: true, info: true },
  search: '',
  fwLogs: {},
  commandHistory: [],
  commandNames: [],

  append: (entry) => set((s) => {
    if (s.entries.some((e) => e.id === entry.id)) return s
    const entries = [...s.entries, entry]
    return { entries: entries.length > MAX_ENTRIES ? entries.slice(entries.length - MAX_ENTRIES) : entries }
  }),

  backfill: async () => {
    const last = get().entries[get().entries.length - 1]?.id ?? 0
    const fresh = await apiClient.getLogs(last)
    fresh.forEach((e) => get().append(e))
  },

  clear: async () => {
    await apiClient.clearLogs()
    set({ entries: [] })
  },

  setOpen: (isOpen) => set({ isOpen }),
  toggleSeverity: (severity) => set((s) => ({ show: { ...s.show, [severity]: !s.show[severity] } })),
  setSearch: (search) => set({ search }),

  toggleFwLogs: async (deviceId) => {
    const running = get().fwLogs[deviceId]
    const status = running ? await apiClient.stopFwLogs(deviceId) : await apiClient.startFwLogs(deviceId)
    set((s) => ({ fwLogs: { ...s.fwLogs, [deviceId]: status.running } }))
  },

  recoverFlashLogs: async (deviceId) => (await apiClient.recoverFlashLogs(deviceId)).messages,

  runCommand: async (deviceId, line) => {
    const trimmed = line.trim()
    if (!trimmed) return
    set((s) => ({ commandHistory: [...s.commandHistory.filter((c) => c !== trimmed), trimmed].slice(-HISTORY_MAX) }))
    if (trimmed.toLowerCase() === 'clear') {
      await get().clear()
      return
    }
    await apiClient.runTerminal(deviceId, trimmed) // the output arrives as a console entry
  },

  fetchCommandNames: async () => {
    try {
      set({ commandNames: await apiClient.getTerminalCommands() })
    } catch {
      set({ commandNames: [] })
    }
  },
}))

/** Entries after the severity filters and the search box. */
export function visibleEntries(state: Pick<ConsoleState, 'entries' | 'show' | 'search'>): LogEntry[] {
  const needle = state.search.trim().toLowerCase()
  return state.entries.filter((e) => state.show[severityBucket(e.severity)] &&
    (!needle || e.message.toLowerCase().includes(needle) || (e.file ?? '').toLowerCase().includes(needle)))
}

export function severityCounts(entries: LogEntry[]): { error: number; warn: number; info: number } {
  const counts = { error: 0, warn: 0, info: 0 }
  for (const e of entries) counts[severityBucket(e.severity)]++
  return counts
}

export { severityBucket }
