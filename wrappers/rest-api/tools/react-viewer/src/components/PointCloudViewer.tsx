import { useRef, useEffect, useLayoutEffect, memo } from 'react'
import { Canvas } from '@react-three/fiber'
import { OrbitControls, PerspectiveCamera } from '@react-three/drei'
import * as THREE from 'three'
import { useAppStore } from '../store'

// Round point sprite (built once). Default square points read as a coarse pixel grid;
// a circular alpha mask + alphaTest makes points blend into a smoother surface.
const circleSprite = (() => {
  const s = 64
  const cv = document.createElement('canvas')
  cv.width = cv.height = s
  const ctx = cv.getContext('2d')!
  ctx.beginPath()
  ctx.arc(s / 2, s / 2, s / 2 - 2, 0, Math.PI * 2)
  ctx.fillStyle = '#fff'
  ctx.fill()
  return new THREE.CanvasTexture(cv)
})()

export function PointCloudViewer() {
  const { pointCloudVertices, pointCloudColors, isStreaming, viewMode } = useAppStore()

  return (
    <div className="h-full flex flex-col">
      {/* Controls */}
      <div className="flex items-center justify-end gap-4 p-2 bg-gray-800 rounded-t-lg">
        <button
          onClick={() => {
            if (pointCloudVertices) {
              exportToPLY(pointCloudVertices)
            }
          }}
          disabled={!pointCloudVertices}
          className="control-button-secondary text-sm py-1"
        >
          Export PLY
        </button>
      </div>

      {/* 3D Canvas */}
      <div className="flex-1 bg-black rounded-b-lg overflow-hidden">
        {pointCloudVertices ? (
          <Canvas frameloop={viewMode === '3d' ? 'always' : 'never'}>
            {/* Head-on, on the depth optical axis (sensor POV) — matches the C++ viewer's
                default 3D camera and the 2D framing, so raw-cloud edge artifacts stay
                behind surfaces instead of reading as floating noise from an oblique angle. */}
            <PerspectiveCamera makeDefault position={[0, 0, 1]} fov={45} />
            <OrbitControls enablePan enableZoom enableRotate target={[0, 0, -1]} />
            <ambientLight intensity={0.5} />
            <PointCloud vertices={pointCloudVertices} sampledColors={pointCloudColors} />
            <SceneDecor />
          </Canvas>
        ) : (
          <div className="h-full flex items-center justify-center text-gray-500">
            <div className="text-center">
              <svg
                className="w-16 h-16 mx-auto mb-4 opacity-50"
                fill="none"
                stroke="currentColor"
                viewBox="0 0 24 24"
              >
                <path
                  strokeLinecap="round"
                  strokeLinejoin="round"
                  strokeWidth={1}
                  d="M14 10l-2 1m0 0l-2-1m2 1v2.5M20 7l-2 1m2-1l-2-1m2 1v2.5M14 4l-2-1-2 1M4 7l2-1M4 7l2 1M4 7v2.5M12 21l-2-1m2 1l2-1m-2 1v-2.5M6 18l-2-1v-2.5M18 18l2-1v-2.5"
                />
              </svg>
              <p className="text-lg">3D Point Cloud View</p>
              <p className="text-sm mt-1">
                {isStreaming
                  ? 'Waiting for point cloud data…'
                  : 'Start streaming depth to see points'}
              </p>
            </div>
          </div>
        )}
      </div>
    </div>
  )
}

interface PointCloudProps {
  vertices: Float32Array
  // Optional 1:1 per-vertex RGB sampled from the live color frame on the server
  // (3 bytes per vertex). When present the cloud is texture-mapped (cpp viewer
  // parity); when null the per-vertex color falls back to the depth colormap.
  sampledColors: Uint8Array | null
}

function PointCloud({ vertices, sampledColors }: PointCloudProps) {
  // One persistent geometry with grow-only fixed-capacity attributes. Inspired by the
  // C++ viewer's upload_points: the GPU buffers are allocated once (regrown only when a
  // frame needs more room) and rewritten in place every frame — no per-frame BufferGeometry
  // or BufferAttribute allocation, so no GPU-buffer leak. setDrawRange limits rendering to
  // the live vertex count, since the point count varies frame to frame.
  const geometryRef = useRef<THREE.BufferGeometry>()
  const capacityRef = useRef(0)
  // EMA-smoothed depth range, derived from robust percentiles of the cloud so the
  // gradient uses the full blue→red span (farthest points reach red) while staying
  // temporally stable — and ignoring sparse far outliers that would wash everything blue.
  const rangeRef = useRef<{ near: number; far: number }>({ near: NaN, far: NaN })
  const histRef = useRef<Uint32Array>(new Uint32Array(128))
  if (!geometryRef.current) geometryRef.current = new THREE.BufferGeometry()
  const geometry = geometryRef.current

  useEffect(() => {
    const geo = geometry
    return () => geo.dispose()
  }, [geometry])

  // useLayoutEffect rather than useMemo: this writes side effects (typed-array
  // mutations, EMA range advance) which useMemo's contract doesn't guarantee
  // will fire — the runtime is allowed to drop a memoized value and re-run.
  useLayoutEffect(() => {
    const geo = geometry
    const n = vertices.length
    const count = (n / 3) | 0

    // Empty frame: nothing to upload. Skip before the attribute access — the
    // first frame may carry zero valid points (all depth pixels < 0.03m), and
    // posAttr would be undefined here.
    if (count === 0) {
      geo.setDrawRange(0, 0)
      return
    }

    // Grow-only: reallocate buffers only when a frame exceeds current capacity (in vertices).
    // After the first few frames this stops firing entirely (steady state = no allocation).
    // Capacity is tracked in vertices so the float arrays stay a multiple of 3 (a fractional
    // BufferAttribute.count would read past the array on writes).
    if (capacityRef.current < count) {
      const vcap = count + (count >> 2) + 1 // +25% headroom to avoid frequent regrows
      geo.setAttribute('position', new THREE.BufferAttribute(new Float32Array(vcap * 3), 3))
      geo.setAttribute('color', new THREE.BufferAttribute(new Float32Array(vcap * 3), 3))
      capacityRef.current = vcap
    }
    const posAttr = geo.getAttribute('position') as THREE.BufferAttribute
    const colAttr = geo.getAttribute('color') as THREE.BufferAttribute
    const positions = posAttr.array as Float32Array
    const colors = colAttr.array as Float32Array

    // Use server-sampled RGB (textured PC) when the array length matches the
    // current vertex count. The server sends 3 uint8 bytes per vertex, aligned
    // 1:1 with `vertices`. Length mismatch can happen for one tick when a
    // stale frame arrives during decimation-step change → fall back to the
    // depth colormap that frame.
    const useSampled = !!sampledColors && sampledColors.length === count * 3
    if (useSampled) {
      const src = sampledColors as Uint8Array
      const INV_255 = 1 / 255
      for (let i = 0; i < n; i += 3) {
        // RealSense: x-right, y-down, z-forward (m).  Three.js: x-right, y-up, z-toward-camera.
        positions[i] = vertices[i]
        positions[i + 1] = -vertices[i + 1]
        positions[i + 2] = -vertices[i + 2]
        colors[i] = src[i] * INV_255
        colors[i + 1] = src[i + 1] * INV_255
        colors[i + 2] = src[i + 2] * INV_255
      }
    } else {
      // Robust depth range via a coarse histogram → 2nd/98th percentile, then EMA-smoothed.
      // Percentiles (not raw min/max) reject sparse far/near outliers that would otherwise
      // stretch the range and wash the whole scene to one color; EMA keeps it stable.
      const HMAX = 10 // meters; histogram window
      const NB = histRef.current.length
      const hist = histRef.current
      hist.fill(0)
      const sc = NB / HMAX
      let total = 0
      for (let i = 2; i < n; i += 3) {
        const z = vertices[i]
        if (z <= 0) continue
        let b = (z * sc) | 0
        if (b >= NB) b = NB - 1
        hist[b]++
        total++
      }
      let zlo = NaN
      let zhi = NaN
      if (total > 0) {
        const loT = total * 0.02
        const hiT = total * 0.98
        let acc = 0
        let loB = -1
        let hiB = NB - 1
        for (let b = 0; b < NB; b++) {
          acc += hist[b]
          if (loB < 0 && acc >= loT) loB = b
          if (acc >= hiT) { hiB = b; break }
        }
        if (loB < 0) loB = 0
        zlo = loB / sc
        zhi = (hiB + 1) / sc
      }
      const r = rangeRef.current
      if (!isFinite(r.near) || !isFinite(r.far)) {
        r.near = isFinite(zlo) ? zlo : 0
        r.far = isFinite(zhi) ? zhi : HMAX
      } else if (isFinite(zlo) && isFinite(zhi) && zhi > zlo) {
        const A = 0.1 // ~10-frame time constant
        r.near += (zlo - r.near) * A
        r.far += (zhi - r.far) * A
      }
      const near = r.near
      const far = r.far
      const span = far > near ? far - near : 1
      const invSpan = 1 / span

      for (let i = 0; i < n; i += 3) {
        const z = vertices[i + 2]
        positions[i] = vertices[i]
        positions[i + 1] = -vertices[i + 1]
        positions[i + 2] = -z

        let t = (z - near) * invSpan
        if (t < 0) t = 0
        else if (t > 1) t = 1
        const c = jetColor(t)
        colors[i] = c[0]
        colors[i + 1] = c[1]
        colors[i + 2] = c[2]
      }
    }

    // Only the first `count` vertices are valid this frame; render exactly those.
    // No computeBoundingSphere: it reads posAttr.count (= full grown capacity),
    // which includes stale/zero trailing positions and inflates the sphere.
    // frustumCulled={false} on the <points> below makes the missing sphere safe.
    posAttr.needsUpdate = true
    colAttr.needsUpdate = true
    geo.setDrawRange(0, count)
  }, [vertices, sampledColors])

  return (
    <points geometry={geometry} frustumCulled={false}>
      <pointsMaterial
        size={3}
        sizeAttenuation={false}
        vertexColors
        map={circleSprite}
        alphaTest={0.5}
        transparent={false}
      />
    </points>
  )
}

// Depth colormap matching the 2D view's DepthLegend 'jet' (see DepthLegend.tsx):
// near = blue → cyan → yellow → red → dark red = far. Linear interpolation between
// the same 5 stops the legend uses, so 2D and 3D agree.
const JET_STOPS: [number, number, number][] = [
  [0, 0, 1],        // near  - blue
  [0, 1, 1],        //         cyan
  [1, 1, 0],        //         yellow
  [1, 0, 0],        //         red
  [50 / 255, 0, 0], // far   - dark red
]
function jetColor(t: number): [number, number, number] {
  const v = Math.min(Math.max(t, 0), 1)
  const seg = v * (JET_STOPS.length - 1)
  const i = Math.min(Math.floor(seg), JET_STOPS.length - 2)
  const f = seg - i
  const a = JET_STOPS[i]
  const b = JET_STOPS[i + 1]
  return [a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f, a[2] + (b[2] - a[2]) * f]
}

// Module-level constants: stable array identity so r3f never rebuilds these
// buffers. Previously these were `new Float32Array([...])` inline, which created
// fresh arrays every render → r3f reconstructed the GPU buffers each frame (leak).
const AXIS_X = new Float32Array([0, 0, 0, 1, 0, 0])
const AXIS_Y = new Float32Array([0, 0, 0, 0, 1, 0])
const AXIS_Z = new Float32Array([0, 0, 0, 0, 0, 1])

// Static scene helpers (axes + grid). memo + no props => rendered exactly once,
// so their buffers are allocated a single time regardless of parent re-renders.
const SceneDecor = memo(function SceneDecor() {
  return (
    <group>
      {/* X axis - Red */}
      <line>
        <bufferGeometry>
          <bufferAttribute attach="attributes-position" args={[AXIS_X, 3]} />
        </bufferGeometry>
        <lineBasicMaterial color="red" />
      </line>
      {/* Y axis - Green */}
      <line>
        <bufferGeometry>
          <bufferAttribute attach="attributes-position" args={[AXIS_Y, 3]} />
        </bufferGeometry>
        <lineBasicMaterial color="green" />
      </line>
      {/* Z axis - Blue */}
      <line>
        <bufferGeometry>
          <bufferAttribute attach="attributes-position" args={[AXIS_Z, 3]} />
        </bufferGeometry>
        <lineBasicMaterial color="blue" />
      </line>
      <gridHelper args={[10, 10, '#444', '#333']} />
    </group>
  )
})

function exportToPLY(vertices: Float32Array) {
  const numPoints = vertices.length / 3
  
  let plyContent = `ply
format ascii 1.0
element vertex ${numPoints}
property float x
property float y
property float z
end_header
`

  for (let i = 0; i < vertices.length; i += 3) {
    plyContent += `${vertices[i]} ${vertices[i + 1]} ${vertices[i + 2]}\n`
  }

  // Create and download file
  const blob = new Blob([plyContent], { type: 'text/plain' })
  const url = URL.createObjectURL(blob)
  const link = document.createElement('a')
  link.href = url
  link.download = `pointcloud_${Date.now()}.ply`
  document.body.appendChild(link)
  link.click()
  document.body.removeChild(link)
  URL.revokeObjectURL(url)
}
