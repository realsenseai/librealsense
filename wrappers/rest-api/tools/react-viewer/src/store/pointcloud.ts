import { create } from 'zustand'
import { persist } from 'zustand/middleware'
import type { PointCloudGeometry } from '../api/types'
import type { DepthImage, Vec3 } from '../utils/camera'

/** The last depth image of a device, as the server ships it (z16, throttled). */
export interface DepthFrame extends DepthImage {
  frameNumber: number
}

/** viewer.cpp shader_type: raw points, flat-shaded mesh, mesh with diffuse lighting. */
export type Shading = 'points' | 'flat' | 'diffuse'

interface PointCloudState {
  frames: Record<string, DepthFrame>
  geometry: Record<string, PointCloudGeometry>
  /** Texture stream per device; null = depth colormap. Absent = pick automatically. */
  textureSource: Record<string, string | null>
  /** Which device's cloud is drawn (the legacy "depth source"); null = the first with frames. */
  depthSource: string | null
  shading: Shading
  occlusionInvalidation: boolean
  /** Measurement interest points in scene coordinates (measurement.cpp), with undo. */
  measurement: Vec3[]
  measurementHistory: Vec3[][]
  addMeasurementPoint: (p: Vec3, chain: boolean) => void
  undoMeasurement: () => void
  clearMeasurement: () => void
  pushFrame: (deviceId: string, frame: DepthFrame) => void
  setGeometry: (deviceId: string, geometry: PointCloudGeometry) => void
  setTextureSource: (deviceId: string, stream: string | null) => void
  setDepthSource: (deviceId: string | null) => void
  setShading: (shading: Shading) => void
  setOcclusionInvalidation: (on: boolean) => void
  clear: (deviceId?: string) => void
}

export const usePointCloudStore = create<PointCloudState>()(
  persist(
    (set) => ({
      frames: {},
      geometry: {},
      textureSource: {},
      depthSource: null,
      shading: 'diffuse', // configurations::viewer::shading_mode default
      occlusionInvalidation: true,
      measurement: [],
      measurementHistory: [],

      // Without Shift a third click starts a new measurement; with Shift the chain grows.
      addMeasurementPoint: (p, chain) => set((s) => {
        const next = !chain && s.measurement.length >= 2 ? [p] : [...s.measurement, p]
        return { measurement: next, measurementHistory: [...s.measurementHistory, s.measurement].slice(-20) }
      }),
      undoMeasurement: () => set((s) => {
        const history = [...s.measurementHistory]
        const previous = history.pop()
        return previous ? { measurement: previous, measurementHistory: history } : {}
      }),
      clearMeasurement: () => set({ measurement: [], measurementHistory: [] }),

      pushFrame: (deviceId, frame) => set((s) => ({ frames: { ...s.frames, [deviceId]: frame } })),
      setGeometry: (deviceId, geometry) => set((s) => ({ geometry: { ...s.geometry, [deviceId]: geometry } })),
      setTextureSource: (deviceId, stream) => set((s) => ({ textureSource: { ...s.textureSource, [deviceId]: stream } })),
      setDepthSource: (deviceId) => set({ depthSource: deviceId }),
      setShading: (shading) => set({ shading }),
      setOcclusionInvalidation: (on) => set({ occlusionInvalidation: on }),
      clear: (deviceId) =>
        set((s) => {
          if (!deviceId) return { frames: {}, geometry: {} }
          const frames = { ...s.frames }
          const geometry = { ...s.geometry }
          delete frames[deviceId]
          delete geometry[deviceId]
          return { frames, geometry }
        }),
    }),
    {
      name: 'rs-viewer-3d',
      partialize: (s) => ({ shading: s.shading, occlusionInvalidation: s.occlusionInvalidation }),
    },
  ),
)

/** The texture stream to use: the chosen one, else color when it streams, never IR by default
 * (viewer.cpp: "Don't auto-switch to IR stream"). */
export function pickTextureSource(chosen: string | null | undefined, streaming: string[]): string | null {
  if (chosen !== undefined) return chosen && streaming.includes(chosen) ? chosen : null
  if (streaming.includes('color')) return 'color'
  return null
}
