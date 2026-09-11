import { useEffect, useMemo, useRef, useState } from 'react'
import { useAppStore } from '../../store'
import { severityCounts, useConsoleStore, visibleEntries } from '../../store/console'
import type { LogEntry } from '../../api/types'

const SEVERITY_CLASS: Record<string, string> = {
  error: 'text-red-400', fatal: 'text-red-400', warn: 'text-yellow-300', warning: 'text-yellow-300',
  info: 'text-gray-200', debug: 'text-gray-500',
}

function formatTime(ts: number): string {
  return new Date(ts * 1000).toLocaleTimeString(undefined, { hour12: false })
}

export function formatEntry(e: LogEntry): string {
  const where = e.file ? ` ${e.file}${e.line ? `:${e.line}` : ''}` : ''
  return `${formatTime(e.ts)} [${e.severity.toUpperCase()}] [${e.source}]${where} ${e.message}`
}

/**
 * The legacy output console (output-model.cpp): the bottom panel with severity counters as
 * filters, a search box, copy/save, firmware-log toggle, and a command line with history.
 */
export function OutputConsole() {
  const { entries, isOpen, show, search, fwLogs, commandHistory, commandNames,
    setOpen, toggleSeverity, setSearch, clear, toggleFwLogs, recoverFlashLogs, runCommand, backfill, fetchCommandNames } = useConsoleStore()
  const deviceStates = useAppStore((s) => s.deviceStates)
  const devices = Object.values(deviceStates).map((ds) => ds.device).filter((d) => !d.is_playback)
  const [deviceId, setDeviceId] = useState<string>('')
  const [command, setCommand] = useState('')
  const [historyIndex, setHistoryIndex] = useState<number | null>(null)
  const [flashMessage, setFlashMessage] = useState<string | null>(null)
  const listRef = useRef<HTMLDivElement>(null)

  const target = deviceId || devices[0]?.device_id || ''
  const visible = useMemo(() => visibleEntries({ entries, show, search }), [entries, show, search])
  const counts = useMemo(() => severityCounts(entries), [entries])

  useEffect(() => {
    if (!isOpen) return
    void backfill()
    void fetchCommandNames()
  }, [isOpen, backfill, fetchCommandNames])

  // Follow the newest line, as a terminal does.
  useEffect(() => {
    if (listRef.current) listRef.current.scrollTop = listRef.current.scrollHeight
  }, [visible.length])

  useEffect(() => {
    if (!isOpen) return
    const onKey = (e: KeyboardEvent) => { if (e.key === 'Escape') setOpen(false) }
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [isOpen, setOpen])

  const copy = (text: string) => void navigator.clipboard?.writeText(text)
  const saveAs = () => {
    const blob = new Blob([visible.map(formatEntry).join('\n')], { type: 'text/plain' })
    const url = URL.createObjectURL(blob)
    const a = document.createElement('a')
    a.href = url
    a.download = `realsense-console-${new Date().toISOString().replace(/[:.]/g, '-')}.log`
    a.click()
    URL.revokeObjectURL(url)
  }

  const submit = async () => {
    if (!target || !command.trim()) return
    const line = command
    setCommand('')
    setHistoryIndex(null)
    await runCommand(target, line)
  }

  const onCommandKey = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === 'Enter') { e.preventDefault(); void submit() }
    else if (e.key === 'ArrowUp' && commandHistory.length) {
      e.preventDefault()
      const idx = historyIndex === null ? commandHistory.length - 1 : Math.max(0, historyIndex - 1)
      setHistoryIndex(idx); setCommand(commandHistory[idx])
    } else if (e.key === 'ArrowDown' && historyIndex !== null) {
      e.preventDefault()
      const idx = historyIndex + 1
      if (idx >= commandHistory.length) { setHistoryIndex(null); setCommand('') } else { setHistoryIndex(idx); setCommand(commandHistory[idx]) }
    } else if (e.key === 'Tab' && command) {
      const match = commandNames.find((n) => n.toLowerCase().startsWith(command.toLowerCase()))
      if (match) { e.preventDefault(); setCommand(match) }
    }
  }

  if (!isOpen) {
    return (
      <button
        onClick={() => setOpen(true)}
        className="fixed bottom-4 left-40 px-3 py-1 rounded-full text-xs bg-gray-800 border border-gray-600 text-gray-300 hover:text-white"
        title="Open the output console"
        aria-label="Open output console"
      >
        Console {counts.error > 0 && <span className="text-red-400 ml-1">{counts.error}⚠</span>}
      </button>
    )
  }

  const counter = (key: 'error' | 'warn' | 'info', label: string, color: string) => (
    <button key={key} onClick={() => toggleSeverity(key)} aria-pressed={show[key]}
      title={show[key] ? `Hide ${label}` : `Show ${label}`}
      className={`px-2 py-0.5 rounded text-xs ${show[key] ? color : 'text-gray-500 line-through'}`}>
      {counts[key]} {label}
    </button>
  )

  return (
    <div className="border-t border-gray-700 bg-rs-dark text-xs flex flex-col h-64" data-testid="output-console" role="region" aria-label="Output console">
      <div className="flex items-center gap-2 px-3 py-1 border-b border-gray-700 bg-gray-800/60">
        <span className="font-semibold text-gray-200">Output</span>
        {counter('error', 'errors', 'text-red-400')}
        {counter('warn', 'warnings', 'text-yellow-300')}
        {counter('info', 'info', 'text-gray-200')}
        <input value={search} onChange={(e) => setSearch(e.target.value)} placeholder="Search…" aria-label="Search console"
          className="ml-2 bg-gray-700 text-white rounded px-2 py-0.5 w-40" />
        <div className="ml-auto flex items-center gap-1">
          {devices.length > 1 && (
            <select value={target} onChange={(e) => setDeviceId(e.target.value)} aria-label="Console device"
              className="bg-gray-700 text-white rounded px-1 py-0.5">
              {devices.map((d) => <option key={d.device_id} value={d.device_id}>{d.name} ({d.serial_number})</option>)}
            </select>
          )}
          {target && (
            <>
              <button onClick={() => void toggleFwLogs(target)} aria-pressed={!!fwLogs[target]}
                className={`px-2 py-0.5 rounded ${fwLogs[target] ? 'bg-rs-blue text-white' : 'bg-gray-700 text-gray-200 hover:bg-gray-600'}`}
                title={fwLogs[target] ? 'Disable firmware logs' : 'Enable firmware logs'}>
                FW logs
              </button>
              <button onClick={async () => { const n = await recoverFlashLogs(target); setFlashMessage(`${n} messages recovered from flash`) }}
                className="px-2 py-0.5 rounded bg-gray-700 text-gray-200 hover:bg-gray-600" title="Recover logs from flash">
                Flash
              </button>
            </>
          )}
          <button onClick={() => copy(visible.map(formatEntry).join('\n'))} className="px-2 py-0.5 rounded bg-gray-700 text-gray-200 hover:bg-gray-600" title="Copy all">Copy</button>
          <button onClick={saveAs} className="px-2 py-0.5 rounded bg-gray-700 text-gray-200 hover:bg-gray-600" title="Save as…">Save</button>
          <button onClick={() => void clear()} className="px-2 py-0.5 rounded bg-gray-700 text-gray-200 hover:bg-gray-600" title="Clear">Clear</button>
          <button onClick={() => setOpen(false)} className="px-2 py-0.5 rounded text-gray-400 hover:text-white" title="Close (Esc)" aria-label="Close output console">✕</button>
        </div>
      </div>
      {flashMessage && <div className="px-3 py-0.5 text-gray-400 bg-gray-800/40">{flashMessage}</div>}
      <div ref={listRef} className="flex-1 overflow-y-auto font-mono px-3 py-1 space-y-px" data-testid="console-lines">
        {visible.length === 0 && <div className="text-gray-500">No output</div>}
        {visible.map((e) => (
          <div key={e.id} className={`group flex gap-2 whitespace-pre-wrap break-all ${SEVERITY_CLASS[e.severity] ?? 'text-gray-200'}`}>
            <span className="text-gray-500 shrink-0">{formatTime(e.ts)}</span>
            <span className="text-gray-500 shrink-0">[{e.source}]</span>
            <span className="flex-1">{e.message}</span>
            <button onClick={() => copy(formatEntry(e))} className="opacity-0 group-hover:opacity-100 text-gray-500 hover:text-white" title="Copy line" aria-label="Copy line">⧉</button>
          </div>
        ))}
      </div>
      <div className="flex items-center gap-2 px-3 py-1 border-t border-gray-700">
        <span className="text-gray-500 font-mono">&gt;</span>
        <input
          value={command}
          onChange={(e) => { setCommand(e.target.value); setHistoryIndex(null) }}
          onKeyDown={onCommandKey}
          placeholder={target ? 'Command: clear, hex bytes (10 00 00 00), or a Commands.xml name (Tab completes)' : 'Connect a camera to send commands'}
          disabled={!target}
          aria-label="Console command"
          className="flex-1 bg-gray-700 text-white rounded px-2 py-0.5 font-mono"
        />
      </div>
    </div>
  )
}
