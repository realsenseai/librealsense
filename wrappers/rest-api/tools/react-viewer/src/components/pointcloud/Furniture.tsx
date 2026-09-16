import { useMemo } from 'react'
import * as THREE from 'three'
import type { CameraIntrinsics } from '../../api/types'
import { deproject, toScene } from '../../utils/camera'

const FEET_TO_METER = 0.3048

/** The legacy floor grid (viewer.cpp render_3d_view): 24 tiles of a meter (or a foot), one
 * meter below the camera, a meter ahead; the middle lines a little brighter. */
export function FloorGrid({ metric }: { metric: boolean }) {
  const { dim, bright } = useMemo(() => {
    const unit = metric ? 1 : FEET_TO_METER
    const tiles = metric ? 24 : Math.ceil(24 / FEET_TO_METER)
    const half = (tiles * unit) / 2
    const dim: number[] = []
    const bright: number[] = []
    for (let i = 0; i <= tiles; i++) {
      const p = i * unit - half
      const target = i === tiles / 2 ? bright : dim
      target.push(p, -1, -half - 1, p, -1, half - 1) // along z (scene -z is ahead)
      target.push(-half, -1, p - 1, half, -1, p - 1) // along x
    }
    return { dim: Float32Array.from(dim), bright: Float32Array.from(bright) }
  }, [metric])
  return (
    <group>
      <lineSegments>
        <bufferGeometry><bufferAttribute attach="attributes-position" args={[dim, 3]} /></bufferGeometry>
        <lineBasicMaterial color={new THREE.Color(0.4, 0.4, 0.4)} />
      </lineSegments>
      <lineSegments>
        <bufferGeometry><bufferAttribute attach="attributes-position" args={[bright, 3]} /></bufferGeometry>
        <lineBasicMaterial color={new THREE.Color(0.7, 0.7, 0.7)} />
      </lineSegments>
    </group>
  )
}

const AXIS_LEN = 0.4
const AXES: [Float32Array, string][] = [
  [new Float32Array([0, 0, 0, AXIS_LEN, 0, 0]), 'red'],
  [new Float32Array([0, 0, 0, 0, AXIS_LEN, 0]), 'green'],
  [new Float32Array([0, 0, 0, 0, 0, -AXIS_LEN]), 'blue'], // the camera's +Z, ahead
]

/** World axes at the camera origin (texture_buffer::draw_axes, 0.4 m). */
export function Axes() {
  return (
    <group>
      {AXES.map(([pts, color]) => (
        <line key={color}>
          <bufferGeometry><bufferAttribute attach="attributes-position" args={[pts, 3]} /></bufferGeometry>
          <lineBasicMaterial color={color} />
        </line>
      ))}
    </group>
  )
}

/** The depth camera's frustum at 1, 3 and 5 m, from its intrinsics (viewer.cpp draw_frustrum). */
export function Frustum({ intrinsics }: { intrinsics: CameraIntrinsics }) {
  const pts = useMemo(() => {
    const out: number[] = []
    for (let d = 1; d < 6; d += 2) {
      const corners = [[0, 0], [intrinsics.width, 0], [intrinsics.width, intrinsics.height], [0, intrinsics.height]]
        .map(([x, y]) => toScene(deproject(intrinsics, x, y, d)))
      for (let i = 0; i < 4; i++) {
        out.push(0, 0, 0, ...corners[i])
        out.push(...corners[i], ...corners[(i + 1) % 4])
      }
    }
    return Float32Array.from(out)
  }, [intrinsics])
  return (
    <lineSegments>
      <bufferGeometry><bufferAttribute attach="attributes-position" args={[pts, 3]} /></bufferGeometry>
      <lineBasicMaterial color={new THREE.Color(0.2, 0.25, 0.3)} transparent opacity={0.6} />
    </lineSegments>
  )
}
