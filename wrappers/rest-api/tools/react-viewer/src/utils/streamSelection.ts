// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import type { SensorConfig, StreamConfig, SupportedStreamProfile } from '../api/types'
import { streamKey } from '../api/types'

/** What one sensor is set to; changes snap like the C++ viewer's subdevice_model::get_supported_profiles. */
export type Selection = SensorConfig & { streams: StreamConfig[] }

type Res = { width: number; height: number }
type Profile = SupportedStreamProfile

const sameRes = (a: Res, b: Res) => a.width === b.width && a.height === b.height
const sameMode = (a: Profile, b: Profile) => sameRes(a, b) && a.fps === b.fps

export const resolutionOptions = (profiles: Profile[]): Res[] =>
  [...new Map(profiles.map(p => [`${p.width}x${p.height}`, { width: p.width, height: p.height }])).values()]

/** The sensor's fps, or one stream's when `key` is given (motion sensors pick fps per stream). */
export const fpsOptions = (profiles: Profile[], key?: string): number[] =>
  [...new Set(profiles.filter(p => !key || streamKey(p) === key).map(p => p.fps))].sort((a, b) => a - b)

export const formatOptions = (profiles: Profile[], key: string): string[] =>
  [...new Set(profiles.filter(p => streamKey(p) === key).map(p => p.format))]

/** One resolution/fps for the whole sensor, unless it has no resolutions or its streams share no fps (C++ show_single_fps_list). */
export function sharedFps(profiles: Profile[]): boolean {
  const keys = new Set(profiles.map(streamKey))
  return profiles.some(p => p.width)
    && fpsOptions(profiles).some(f => [...keys].every(k => profiles.some(p => streamKey(p) === k && p.fps === f)))
}

/** First group (by what the change leaves free) covering all enabled streams, else all but one, else one matching profile. */
function snap(profiles: Profile[], wanted: Selection, matches: (p: Profile) => boolean, group: (p: Profile) => string): Selection {
  if (!sharedFps(profiles)) return wanted
  const enabled = wanted.streams.filter(c => c.enable).map(c => c.stream_type)
  const format = (p: Profile) => wanted.streams.find(c => c.stream_type === streamKey(p))!.format
  const misses = (p: Profile) =>
    Number(p.fps !== wanted.framerate) + Number(p.format !== format(p)) + Number(!sameRes(p, wanted.resolution))

  const groups = new Map<string, Map<string, Profile>>()
  let partial: Profile[] | undefined
  let single: Profile | undefined
  let chosen: Profile[] | undefined
  for (const p of [...profiles].sort((a, b) => misses(a) - misses(b))) {
    if (!matches(p)) continue
    single ??= p
    if (!enabled.includes(streamKey(p))) continue
    const g = groups.get(group(p)) ?? groups.set(group(p), new Map()).get(group(p))!
    if (!g.has(streamKey(p))) g.set(streamKey(p), p)
    if (g.size === enabled.length) { chosen = [...g.values()]; break }
    if (g.size === enabled.length - 1) partial ??= [...g.values()]
  }
  chosen ??= partial ?? (single && [single])
  if (!chosen) return wanted
  return {
    ...wanted,
    resolution: { width: chosen[0].width, height: chosen[0].height },
    framerate: chosen[0].fps,
    streams: wanted.streams.map(c => {
      const p = chosen!.find(p => streamKey(p) === c.stream_type)
      return p ? { ...c, enable: true, format: p.format } : { ...c, enable: false }
    }),
  }
}

export const withStream = (sel: Selection, key: string, patch: Partial<StreamConfig>): Selection =>
  ({ ...sel, streams: sel.streams.map(c => (c.stream_type === key ? { ...c, ...patch } : c)) })

export const applyResolution = (profiles: Profile[], sel: Selection, resolution: Res) =>
  snap(profiles, { ...sel, resolution }, p => sameRes(p, resolution), p => `${p.fps}`)

export const applyFps = (profiles: Profile[], sel: Selection, framerate: number) =>
  snap(profiles, { ...sel, framerate }, p => p.fps === framerate, p => `${p.width}x${p.height}`)

export const applyFormat = (profiles: Profile[], sel: Selection, key: string, format: string) => {
  const picked = profiles.filter(p => streamKey(p) === key && p.format === format)
  return snap(profiles, withStream(sel, key, { format }),
    p => (streamKey(p) === key ? p.format === format : picked.some(m => sameMode(m, p))),
    p => `${p.width}x${p.height}@${p.fps}`)
}

// Unchecking needs no snap: the other streams already sit on real profiles
export const applyToggle = (profiles: Profile[], sel: Selection, key: string, enable: boolean) =>
  enable
    ? snap(profiles, withStream(sel, key, { enable }), () => true, p => `${p.width}x${p.height}@${p.fps}`)
    : withStream(sel, key, { enable })
