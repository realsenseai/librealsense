import { create } from 'zustand'
import { persist } from 'zustand/middleware'

/** Tile key: one per stream per device. */
export const tileKey = (deviceId: string, streamType: string) => `${deviceId}:${streamType.toLowerCase()}`

// The legacy default (viewer.cpp): depth, color, infrared, then everything else, motion last.
function rank(streamType: string): number {
  const t = streamType.toLowerCase()
  if (t === 'depth') return 0
  if (t === 'color') return 1
  if (t.startsWith('infrared')) return 2
  if (t === 'gyro' || t === 'accel') return 9
  return 5
}

interface LayoutState {
  /** Per device serial: the order the user arranged its tiles in. */
  tileOrder: Record<string, string[]>
  /** Tile shown alone, if any. */
  maximized: string | null
  swapTiles: (deviceId: string, a: string, b: string, present: string[]) => void
  setMaximized: (key: string | null) => void
}

/**
 * 2D tile arrangement, remembered per camera the way the legacy viewer remembers
 * `_stream_arrangement_by_serial`.
 */
export const useLayoutStore = create<LayoutState>()(
  persist(
    (set) => ({
      tileOrder: {},
      maximized: null,

      swapTiles: (deviceId, a, b, present) =>
        set((s) => {
          const order = orderKeys(present, s.tileOrder[deviceId])
          const [ia, ib] = [order.indexOf(a), order.indexOf(b)]
          if (ia < 0 || ib < 0 || ia === ib) return s
          ;[order[ia], order[ib]] = [order[ib], order[ia]]
          return { tileOrder: { ...s.tileOrder, [deviceId]: order } }
        }),

      setMaximized: (key) => set({ maximized: key }),
    }),
    { name: 'rs-viewer-layout', partialize: (s) => ({ tileOrder: s.tileOrder }) },
  ),
)

/** `present` tile keys in the remembered order; new ones slot in by the legacy default. */
export function orderKeys(present: string[], remembered: string[] = []): string[] {
  const known = remembered.filter((k) => present.includes(k))
  const rest = present
    .filter((k) => !known.includes(k))
    .sort((x, y) => rank(x.split(':')[1]) - rank(y.split(':')[1]) || x.localeCompare(y))
  return [...known, ...rest]
}
