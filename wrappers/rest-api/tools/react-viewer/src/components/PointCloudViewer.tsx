import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { Canvas, type ThreeEvent } from '@react-three/fiber'
import { OrbitControls, PerspectiveCamera } from '@react-three/drei'
import type { OrbitControls as OrbitControlsImpl } from 'three-stdlib'
import { apiClient } from '../api/client'
import { useAppStore } from '../store'
import { useMetric } from '../store/settings'
import { pickTextureSource, usePointCloudStore, type Shading } from '../store/pointcloud'
import { DepthCloud } from './pointcloud/DepthCloud'
import { Axes, FloorGrid, Frustum } from './pointcloud/Furniture'
import { Measurement } from './pointcloud/Measurement'
import { TextureFeed } from './pointcloud/TextureFeed'
import { pickPoint } from '../utils/measurement'
import type { Vec3 } from '../utils/camera'

const SHADINGS: [Shading, string][] = [
  ['points', 'Raw Point-Cloud'],
  ['flat', 'Flat-Shaded Mesh'],
  ['diffuse', 'With Diffuse Lighting'],
]
const select = 'bg-gray-700 text-white rounded px-2 py-1 text-sm'
const button = 'control-button-secondary text-sm py-1 disabled:opacity-40 disabled:cursor-not-allowed'

/** Video streams of a device that are streaming right now (candidates for the texture). */
function streamingVideoStreams(deviceId: string): string[] {
  const ds = useAppStore.getState().deviceStates[deviceId]
  if (!ds) return []
  const types = Object.values(ds.sensorStreamingStatus).filter((s) => s.is_streaming).flatMap((s) => s.stream_types ?? [])
  return types.filter((t) => !['gyro', 'accel', 'pose'].includes(t.toLowerCase()))
}

/** The legacy 3D view: toolbar (viewer.cpp render_3d_view header) over a three.js scene. */
export function PointCloudViewer() {
  const viewMode = useAppStore((s) => s.viewMode)
  const deviceStates = useAppStore((s) => s.deviceStates)
  const togglePauseAll = useAppStore((s) => s.togglePauseAll)
  const metric = useMetric()
  const { frames, geometry, textureSource, depthSource, shading, occlusionInvalidation,
    setGeometry, setTextureSource, setDepthSource, setShading,
    measurement, addMeasurementPoint, undoMeasurement, clearMeasurement } = usePointCloudStore()
  const controls = useRef<OrbitControlsImpl>(null)
  const [video, setVideo] = useState<HTMLVideoElement | null>(null)
  const [exportMenu, setExportMenu] = useState(false)
  const [exportOptions, setExportOptions] = useState({ mesh: true, normals: false, binary: true })
  const [exporting, setExporting] = useState(false)

  // Depth source: the chosen device, else the first one delivering frames
  const sources = Object.keys(frames).filter((id) => deviceStates[id]?.isStreaming)
  const source = depthSource && sources.includes(depthSource) ? depthSource : sources[0] ?? null
  const frame = source ? frames[source] : null

  // Texture candidates are the source device's streaming video streams
  const streaming = source ? streamingVideoStreams(source) : []
  const streamingKey = streaming.join(',')
  const texture = source ? pickTextureSource(textureSource[source], streaming) : null

  // Camera geometry follows the source and its texture stream
  useEffect(() => {
    if (!source || viewMode !== '3d') return
    let cancelled = false
    apiClient.getPointCloudGeometry(source, texture)
      .then((g) => { if (!cancelled) setGeometry(source, g) })
      .catch((error) => console.error('Failed to read point cloud geometry:', error))
    return () => { cancelled = true }
  }, [source, texture, streamingKey, viewMode, setGeometry])

  const onVideo = useCallback((v: HTMLVideoElement | null) => setVideo(v), [])
  const anyPaused = Object.values(deviceStates).some((ds) => Object.values(ds.sensorStreamingStatus).some((s) => s.paused))
  const geo = source ? geometry[source] : undefined
  const depthRange = useMemo<[number, number]>(() => [0, 6], [])

  // R resets the viewport, Z undoes the last measurement point (viewer.cpp / measurement.cpp)
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (viewMode !== '3d' || e.ctrlKey || e.metaKey) return
      if ((e.target as HTMLElement)?.tagName === 'INPUT') return
      if (e.key.toLowerCase() === 'r') controls.current?.reset()
      if (e.key.toLowerCase() === 'z') undoMeasurement()
    }
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [viewMode, undoMeasurement])

  // A click (not a drag) on the cloud adds an interest point; Shift chains it.
  const pointerDown = useRef<{ x: number; y: number } | null>(null)
  const onPointerDown = (e: ThreeEvent<PointerEvent>) => { pointerDown.current = { x: e.clientX, y: e.clientY } }
  const onPointerUp = (e: ThreeEvent<PointerEvent>) => {
    const start = pointerDown.current
    pointerDown.current = null
    if (!start || Math.hypot(e.clientX - start.x, e.clientY - start.y) > 4 || e.button !== 0) return
    if (!frame || !geo?.depth) return
    const origin = e.ray.origin
    const dir = e.ray.direction
    const picked = pickPoint(frame, geo.depth, { origin: [origin.x, origin.y, origin.z], direction: [dir.x, dir.y, dir.z] })
    if (picked) addMeasurementPoint(picked as Vec3, e.nativeEvent.shiftKey)
  }

  // The legacy export dialog: mesh / normals / binary, written by the server's rs.save_to_ply
  const exportPly = async () => {
    if (!source) return
    setExporting(true)
    try {
      const blob = await apiClient.exportPointCloud(source, exportOptions)
      const url = URL.createObjectURL(blob)
      const a = document.createElement('a')
      a.href = url
      a.download = `${deviceStates[source]?.device.serial_number ?? source}-${Date.now()}.ply`
      a.click()
      URL.revokeObjectURL(url)
      setExportMenu(false)
    } catch (error) {
      console.error('PLY export failed:', error)
    } finally {
      setExporting(false)
    }
  }

  return (
    <div className="h-full flex flex-col">
      <div className="flex items-center gap-3 p-2 bg-gray-800 rounded-t-lg text-sm" data-testid="3d-toolbar">
        <button className={button} onClick={() => void togglePauseAll()} disabled={!source} title={anyPaused ? 'Resume streaming' : 'Pause streaming'}>
          {anyPaused ? 'Resume' : 'Pause'}
        </button>
        <button className={button} onClick={() => controls.current?.reset()} title="Reset 3D viewport to initial state (R)">Reset</button>
        <label className="flex items-center gap-1 text-gray-300">Source
          <select className={select} value={source ?? ''} onChange={(e) => setDepthSource(e.target.value || null)} aria-label="Depth source" disabled={sources.length === 0}>
            {sources.map((id) => <option key={id} value={id}>{deviceStates[id]?.device.name ?? id} ({deviceStates[id]?.device.serial_number ?? id})</option>)}
          </select>
        </label>
        <label className="flex items-center gap-1 text-gray-300">Texture
          <select className={select} value={texture ?? ''} onChange={(e) => source && setTextureSource(source, e.target.value || null)} aria-label="Texture source" disabled={!source}>
            <option value="">Depth colormap</option>
            {streaming.filter((s) => s !== 'depth').map((s) => <option key={s} value={s}>{s}</option>)}
          </select>
        </label>
        <label className="flex items-center gap-1 text-gray-300">Shading
          <select className={select} value={shading} onChange={(e) => setShading(e.target.value as Shading)} aria-label="Shading">
            {SHADINGS.map(([v, label]) => <option key={v} value={v}>{label}</option>)}
          </select>
        </label>
        {measurement.length > 0 && (
          <button className={button} onClick={clearMeasurement} title="Clear the measurement (Z undoes one point)">Clear ruler</button>
        )}
        <div className="relative ml-auto">
          <button className={button} onClick={() => setExportMenu((m) => !m)} disabled={!frame || !geo?.depth} aria-haspopup="menu" aria-expanded={exportMenu}>Export PLY</button>
          {exportMenu && (
            <div className="absolute right-0 mt-1 w-56 bg-gray-800 border border-gray-600 rounded-lg shadow-xl z-30 p-3 space-y-2 text-sm" role="menu" data-testid="ply-export-menu">
              <label className="flex items-center gap-2"><input type="checkbox" checked={exportOptions.mesh} onChange={(e) => setExportOptions({ ...exportOptions, mesh: e.target.checked })} /> Mesh (triangles)</label>
              <label className="flex items-center gap-2"><input type="checkbox" checked={exportOptions.normals} onChange={(e) => setExportOptions({ ...exportOptions, normals: e.target.checked })} /> Normals</label>
              <label className="flex items-center gap-2"><input type="checkbox" checked={exportOptions.binary} onChange={(e) => setExportOptions({ ...exportOptions, binary: e.target.checked })} /> Binary encoding</label>
              <button className="w-full px-3 py-1 rounded bg-rs-blue text-white hover:bg-blue-600 disabled:opacity-50" onClick={() => void exportPly()} disabled={exporting}>
                {exporting ? 'Exporting…' : 'Export'}
              </button>
            </div>
          )}
        </div>
      </div>

      {source && texture && <TextureFeed key={`${source}:${texture}`} deviceId={source} stream={texture} onVideo={onVideo} />}

      <div className="flex-1 bg-black rounded-b-lg overflow-hidden">
        {frame && geo?.depth ? (
          <Canvas frameloop={viewMode === '3d' ? 'always' : 'never'} gl={{ antialias: false }}>
            <PerspectiveCamera makeDefault position={[0, 0, 1]} fov={45} />
            <OrbitControls ref={controls} enablePan enableZoom enableRotate target={[0, 0, -1]} />
            <group onPointerDown={onPointerDown} onPointerUp={onPointerUp}>
              <DepthCloud frame={frame} geometry={geo} video={texture ? video : null} shading={shading}
                occlusionInvalidation={occlusionInvalidation} depthRange={depthRange} />
            </group>
            <Measurement points={measurement} metric={metric} />
            <FloorGrid metric={metric} />
            <Axes />
            <Frustum intrinsics={geo.depth} />
          </Canvas>
        ) : (
          <div className="h-full flex items-center justify-center text-gray-500">
            <div className="text-center">
              <svg className="w-16 h-16 mx-auto mb-4 opacity-50" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1}
                  d="M14 10l-2 1m0 0l-2-1m2 1v2.5M20 7l-2 1m2-1l-2-1m2 1v2.5M14 4l-2-1-2 1M4 7l2-1M4 7l2 1M4 7v2.5M12 21l-2-1m2 1l2-1m-2 1v-2.5M6 18l-2-1v-2.5M18 18l2-1v-2.5" />
              </svg>
              <p className="text-lg">3D Point Cloud View</p>
              <p className="text-sm mt-1">Start streaming depth to see points</p>
            </div>
          </div>
        )}
      </div>
    </div>
  )
}
