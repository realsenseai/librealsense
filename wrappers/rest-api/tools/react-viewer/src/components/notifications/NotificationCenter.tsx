import { useState } from 'react'
import { useNotificationsStore, type Notice } from '../../store/notifications'

const STYLES: Record<Notice['severity'], string> = {
  info: 'border-rs-blue/60 bg-gray-900/95',
  warn: 'border-yellow-500/70 bg-yellow-950/90',
  error: 'border-red-500/70 bg-red-950/90',
}

function Card({ notice }: { notice: Notice }) {
  const dismiss = useNotificationsStore((s) => s.dismiss)
  const [expanded, setExpanded] = useState(false)
  const [menu, setMenu] = useState(false)
  const long = notice.message.length > 140

  return (
    <div className={`rounded-lg border shadow-xl text-xs text-gray-100 w-80 p-3 space-y-2 ${STYLES[notice.severity]}`} role="status" data-testid="notice">
      <div className="flex items-start gap-2">
        <div className="flex-1 min-w-0">
          <div className="font-semibold">{notice.title}</div>
          <div className={`text-gray-300 whitespace-pre-wrap ${expanded ? '' : 'line-clamp-3'}`}>{notice.message}</div>
          {long && (
            <button className="text-rs-blue hover:underline mt-1" onClick={() => setExpanded((e) => !e)}>
              {expanded ? 'Show less' : 'Show more'}
            </button>
          )}
        </div>
        <div className="relative shrink-0">
          {notice.kind ? (
            <button onClick={() => setMenu((m) => !m)} className="text-gray-400 hover:text-white px-1" aria-label="Dismiss options" title="Dismiss…">✕</button>
          ) : (
            <button onClick={() => dismiss(notice.id)} className="text-gray-400 hover:text-white px-1" aria-label="Dismiss" title="Dismiss">✕</button>
          )}
          {menu && (
            <div className="absolute right-0 mt-1 w-44 bg-gray-800 border border-gray-600 rounded shadow-xl z-50 py-1">
              {([['once', 'Just this time'], ['later', 'Remind me later'], ['never', "Don't show again"]] as const).map(([how, label]) => (
                <button key={how} onClick={() => dismiss(notice.id, how)} className="w-full px-3 py-1 text-left hover:bg-gray-700">{label}</button>
              ))}
            </div>
          )}
        </div>
      </div>
      <div className="flex items-center justify-between text-[10px] text-gray-500">
        <span>{new Date(notice.timestamp).toLocaleTimeString(undefined, { hour12: false })}</span>
        <span className="flex gap-2">
          {notice.link && <a href={notice.link.href} target="_blank" rel="noopener noreferrer" className="text-rs-blue hover:underline">{notice.link.label}</a>}
          {notice.action && <button onClick={notice.action.run} className="px-2 py-0.5 rounded bg-rs-blue text-white">{notice.action.label}</button>}
        </span>
      </div>
    </div>
  )
}

/** The legacy stacked notification cards (notifications.h): newest at the bottom, up to six. */
export function NotificationCenter() {
  const notices = useNotificationsStore((s) => s.notices)
  if (notices.length === 0) return null
  return (
    <div className="fixed right-4 bottom-4 z-40 flex flex-col gap-2" aria-label="Notifications">
      {notices.map((n) => <Card key={n.id} notice={n} />)}
    </div>
  )
}
