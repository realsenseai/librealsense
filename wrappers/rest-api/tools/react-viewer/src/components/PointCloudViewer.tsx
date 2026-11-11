import { useRef, useEffect, useState, useMemo, useCallback, memo } from 'react'
import { Canvas, useThree } from '@react-three/fiber'
import { OrbitControls, PerspectiveCamera } from '@react-three/drei'
import * as THREE from 'three'
import { useAppStore } from '../store'
import type { DeviceState, Extrinsics, SupportedStreamProfile } from '../api/types'
import { buildCloud, deprojectionRays, gridIndex, MESH, POINT } from '../utils/pointCloud'
import type { Frame } from '../api/frames'
import { useFrameStream } from '../api/useFrameStream'

// Legacy pc-shader style: a vertex is drawn only as what `valid` allows; in the mesh, a triangle
// is drawn only where all its corners are mesh corners (the interpolated value stays at MESH)
const VERTEX = `
in vec3 position;
in vec3 color;
in float valid;
uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform float pointSize;
out vec3 vColor;
out float vValid;
void main() {
  vColor = color;
  vValid = valid;
  gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
  gl_PointSize = pointSize;
}`
const FRAGMENT = `
precision highp float;
in vec3 vColor;
in float vValid;
uniform float minValid;
out vec4 outColor;
void main() {
  if (vValid < minValid) discard;
  outColor = vec4(vColor, 1.0);
}`

type Shading = 'mesh' | 'points'

/** What the cloud is built from: the first device streaming depth, and its color stream if it runs. */
interface CloudSource {
  deviceId: string
  depth: SupportedStreamProfile
  color?: SupportedStreamProfile
  depthToColor?: Extrinsics
}

function cloudSource(deviceStates: Record<string, DeviceState>): CloudSource | null {
  for (const ds of Object.values(deviceStates)) {
    const started = Object.values(ds.sensorStreamingStatus).filter(s => s.is_streaming)
    const profiles = started.flatMap(s => s.profiles ?? [])
    const depth = profiles.find(p => p.stream_type === 'depth')
    if (!depth) continue
    const color = profiles.find(p => p.stream_type === 'color')
    const fromDepth = ds.extrinsics?.find(e => e.stream_type === 'depth' && e.stream_index === depth.stream_index)
    const depthToColor = color && fromDepth?.to.find(t => t.stream_type === 'color' && t.stream_index === color.stream_index)
    return { deviceId: ds.device.device_id, depth, color, depthToColor }
  }
  return null
}

/** The cloud's current points, for Export PLY: the pixels whose `valid` is at least POINT */
interface Points { positions: Float32Array; valid: Float32Array }

export function PointCloudViewer() {
  const viewMode = useAppStore(s => s.viewMode)
  const deviceStates = useAppStore(s => s.deviceStates)
  const source = useMemo(() => cloudSource(deviceStates), [deviceStates])
  const [points, setPoints] = useState<Points | null>(null)
  const [shading, setShading] = useState<Shading>('mesh') // the legacy viewer's 3D default is a mesh too

  return (
    <div className="h-full flex flex-col">
      {/* Controls */}
      <div className="flex items-center justify-end gap-4 p-2 bg-gray-800 rounded-t-lg">
        <select
          value={shading}
          onChange={e => setShading(e.target.value as Shading)}
          className="bg-gray-700 text-white text-sm rounded px-2 py-1"
          aria-label="Shading"
        >
          <option value="mesh">Mesh</option>
          <option value="points">Points</option>
        </select>
        <button
          onClick={() => points && exportToPLY(points)}
          disabled={!points}
          className="control-button-secondary text-sm py-1"
        >
          Export PLY
        </button>
      </div>

      {/* 3D Canvas */}
      <div className="flex-1 bg-black rounded-b-lg overflow-hidden">
        {source ? (
          // flat + linear: no tone mapping or sRGB re-encoding of the axes and grid, as the cloud's shader has none
          <Canvas flat linear frameloop={viewMode === '3d' ? 'always' : 'never'}>
            {/* Head-on, on the depth optical axis (sensor POV) — matches the C++ viewer's
                default 3D camera and the 2D framing, so raw-cloud edge artifacts stay
                behind surfaces instead of reading as floating noise from an oblique angle. */}
            <PerspectiveCamera makeDefault position={[0, 0, 1]} fov={45} />
            <OrbitControls enablePan enableZoom enableRotate target={[0, 0, -1]} />
            <PointCloud {...source} shading={shading} active={viewMode === '3d'} onPoints={setPoints} />
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
              <p className="text-sm mt-1">Start streaming depth to see points</p>
            </div>
          </div>
        )}
      </div>
    </div>
  )
}

interface PointCloudProps extends CloudSource {
  shading: Shading
  active: boolean // the frame sockets are open only while the 3D view shows
  onPoints: (points: Points | null) => void // the same object while the resolution stays
}

function PointCloud({ deviceId, depth, color, depthToColor, shading, active, onPoints }: PointCloudProps) {
  // Per-pixel attributes rewritten in place every frame, like the C++ viewer's upload_points, shared by
  // the mesh (with the pixel grid's triangles, built once per resolution) and the points
  const geometries = useMemo(() => ({ mesh: new THREE.BufferGeometry(), points: new THREE.BufferGeometry() }), [])
  useEffect(() => () => { geometries.mesh.dispose(); geometries.points.dispose() }, [geometries])
  // Drawn as the frame's bytes, unlit, as the legacy viewer's flat-shaded mesh
  const material = useMemo(() => new THREE.RawShaderMaterial({
    glslVersion: THREE.GLSL3, vertexShader: VERTEX, fragmentShader: FRAGMENT, side: THREE.DoubleSide,
    uniforms: { pointSize: { value: 1 }, minValid: { value: MESH - 0.001 } },
  }), [])
  useEffect(() => () => material.dispose(), [material])
  // The legacy viewer's point size: grows as the view is wider than the depth frame
  const { size, viewport } = useThree()
  material.uniforms.pointSize.value = Math.max(1, Math.sqrt(size.width / depth.width)) * viewport.dpr
  material.uniforms.minValid.value = shading === 'mesh' ? MESH - 0.001 : POINT - 0.5

  const points = useRef<Points | null>(null)
  useEffect(() => () => onPoints(null), [onPoints])
  const rays = useRef<{ key: string; rays: Float32Array } | null>(null)
  const latestColor = useRef<Frame | null>(null)
  useEffect(() => { latestColor.current = null }, [color, active])

  const draw = useCallback((frame: Frame) => {
    const intrinsics = frame.header.intrinsics
    if (!intrinsics) return
    const key = JSON.stringify(intrinsics) // a filter such as decimation changes them
    if (rays.current?.key !== key) rays.current = { key, rays: deprojectionRays(intrinsics) }
    const { width, height } = intrinsics
    let pos = geometries.points.getAttribute('position') as THREE.BufferAttribute | undefined
    if (!pos || pos.count !== width * height) {
      pos = new THREE.BufferAttribute(new Float32Array(width * height * 3), 3)
      const attributes = {
        position: pos,
        color: new THREE.BufferAttribute(new Float32Array(width * height * 3), 3),
        valid: new THREE.BufferAttribute(new Float32Array(width * height), 1),
      }
      for (const g of [geometries.mesh, geometries.points]) {
        for (const [name, attribute] of Object.entries(attributes)) g.setAttribute(name, attribute)
      }
      geometries.mesh.setIndex(new THREE.BufferAttribute(gridIndex(width, height), 1))
      points.current = { positions: pos.array as Float32Array, valid: attributes.valid.array as Float32Array }
    }
    const col = geometries.points.getAttribute('color') as THREE.BufferAttribute
    const val = geometries.points.getAttribute('valid') as THREE.BufferAttribute
    const c = latestColor.current
    const texture = c?.header.intrinsics && depthToColor
      ? { frame: c, intrinsics: c.header.intrinsics, depthToColor } : undefined
    const { positions, valid } = points.current!
    const count = buildCloud(frame, rays.current.rays, positions, col.array as Float32Array, valid, texture)
    pos.needsUpdate = col.needsUpdate = val.needsUpdate = true
    onPoints(count ? points.current : null)
  }, [geometries, depthToColor, onPoints])

  return (
    <>
      {active && <FrameSource deviceId={deviceId} profile={depth} onFrame={draw} />}
      {active && color && <FrameSource deviceId={deviceId} profile={color} onFrame={(f) => { latestColor.current = f }} />}
      {/* No bounding sphere: pixels without depth keep stale positions, hidden by `valid` */}
      {shading === 'mesh'
        ? <mesh geometry={geometries.mesh} material={material} frustumCulled={false} />
        : <points geometry={geometries.points} material={material} frustumCulled={false} />}
    </>
  )
}

function FrameSource({ deviceId, profile, onFrame }: { deviceId: string; profile: SupportedStreamProfile; onFrame: (frame: Frame) => void }) {
  useFrameStream(deviceId, profile, false, onFrame)
  return null
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

function exportToPLY({ positions, valid }: Points) {
  let count = 0
  let vertices = ''
  // Back from three.js axes to the SDK's: y down, z forward
  for (let i = 0; i < valid.length; i++) {
    if (valid[i] < POINT) continue
    vertices += `${positions[i * 3]} ${-positions[i * 3 + 1]} ${-positions[i * 3 + 2]}\n`
    count++
  }
  const plyContent = `ply
format ascii 1.0
element vertex ${count}
property float x
property float y
property float z
end_header
${vertices}`

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
