import { create } from 'zustand'
import { persist } from 'zustand/middleware'

export type Severity = 'info' | 'warn' | 'error'

/** A notification card, as the legacy viewer stacks them (notifications.h). */
export interface Notice {
  id: string
  /** Stable key for snoozing ("fw-update:<serial>:<version>"); omitted cards cannot be snoozed. */
  kind?: string
  severity: Severity
  title: string
  message: string
  deviceId?: string
  timestamp: number
  action?: { label: string; run: () => void }
  link?: { label: string; href: string }
}

export type Dismissal = 'once' | 'later' | 'never'

const SNOOZE_DAYS = 7
const DAY_MS = 24 * 3600 * 1000
const MAX_VISIBLE = 6 // the legacy stack shows at most six cards

interface NotificationsState {
  notices: Notice[]
  /** kind -> when it may show again (ms epoch; Infinity for "don't show again") */
  snoozed: Record<string, number>
  push: (notice: Omit<Notice, 'id' | 'timestamp'> & { id?: string }) => void
  dismiss: (id: string, how?: Dismissal) => void
  clearAll: () => void
  isSnoozed: (kind: string) => boolean
}

let seq = 0

export const useNotificationsStore = create<NotificationsState>()(
  persist(
    (set, get) => ({
      notices: [],
      snoozed: {},

      push: (notice) => {
        if (notice.kind && get().isSnoozed(notice.kind)) return
        const id = notice.id ?? `n${Date.now()}-${seq++}`
        set((s) => {
          const others = s.notices.filter((n) => n.id !== id && (!notice.kind || n.kind !== notice.kind))
          return { notices: [...others, { ...notice, id, timestamp: Date.now() }].slice(-MAX_VISIBLE) }
        })
      },

      dismiss: (id, how = 'once') =>
        set((s) => {
          const notice = s.notices.find((n) => n.id === id)
          const snoozed = { ...s.snoozed }
          if (notice?.kind && how === 'later') snoozed[notice.kind] = Date.now() + SNOOZE_DAYS * DAY_MS
          if (notice?.kind && how === 'never') snoozed[notice.kind] = Number.POSITIVE_INFINITY
          return { notices: s.notices.filter((n) => n.id !== id), snoozed }
        }),

      clearAll: () => set({ notices: [] }),

      isSnoozed: (kind) => {
        const until = get().snoozed[kind]
        return until !== undefined && Date.now() < until
      },
    }),
    {
      name: 'rs-viewer-notifications',
      partialize: (s) => ({ snoozed: s.snoozed }),
      // Infinity does not survive JSON; store it as a far-future date.
      storage: {
        getItem: (name) => {
          try {
            const raw = localStorage.getItem(name)
            return raw ? JSON.parse(raw, (_k, v) => (v === 'never' ? Number.POSITIVE_INFINITY : v)) : null
          } catch { return null }
        },
        setItem: (name, value) => {
          try { localStorage.setItem(name, JSON.stringify(value, (_k, v) => (v === Number.POSITIVE_INFINITY ? 'never' : v))) } catch { /* private mode */ }
        },
        removeItem: (name) => { try { localStorage.removeItem(name) } catch { /* ignore */ } },
      },
    },
  ),
)

/** GitHub issue prefilled with the device details, like the legacy "Report Issue". */
export function reportIssueUrl(devices: { name: string; firmware_version?: string; serial_number: string }[], sdkVersion?: string): string {
  const lines = [
    `**librealsense**: ${sdkVersion ?? 'unknown'} (React viewer)`,
    `**Platform**: ${navigator.platform}`,
    ...devices.map((d) => `**Camera**: ${d.name} S/N ${d.serial_number} FW ${d.firmware_version ?? '?'}`),
    '', '**Issue description**:', '',
  ]
  return `https://github.com/realsenseai/librealsense/issues/new?body=${encodeURIComponent(lines.join('\n'))}`
}
