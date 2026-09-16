import { useMemo } from 'react'
import { Html, Line } from '@react-three/drei'
import type { Vec3 } from '../../utils/camera'
import { chainLengths, polygonArea } from '../../utils/measurement'
import { formatDistance } from '../../utils/units'

interface MeasurementProps {
  points: Vec3[]
  metric: boolean
}

function formatArea(m2: number, metric: boolean): string {
  return metric ? `${m2.toFixed(3)} m²` : `${(m2 * 10.7639).toFixed(2)} ft²`
}

/** Rulers between interest points with a distance label per segment (measurement::draw). */
export function Measurement({ points, metric }: MeasurementProps) {
  const { segments, total } = useMemo(() => chainLengths(points), [points])
  const area = useMemo(() => polygonArea(points), [points])
  if (points.length === 0) return null
  return (
    <group>
      {points.map((p, i) => (
        <mesh key={`p${i}`} position={p}>
          <sphereGeometry args={[0.008, 12, 12]} />
          <meshBasicMaterial color="#ffd166" />
        </mesh>
      ))}
      {points.length > 1 && <Line points={points} color="#ffd166" lineWidth={2} />}
      {points.length > 2 && <Line points={[points[points.length - 1], points[0]]} color="#ffd166" lineWidth={1} dashed dashSize={0.02} gapSize={0.02} />}
      {segments.map((d, i) => {
        const a = points[i]
        const b = points[i + 1]
        const mid: Vec3 = [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2]
        return (
          <Html key={`l${i}`} position={mid} center zIndexRange={[10, 0]}>
            <div className="px-1.5 py-0.5 rounded bg-black/80 text-yellow-200 text-xs font-mono whitespace-nowrap pointer-events-none" data-testid="ruler-label">
              {formatDistance(d, metric)}
            </div>
          </Html>
        )
      })}
      {points.length > 2 && (
        <Html position={points[0]} zIndexRange={[10, 0]}>
          <div className="ml-3 px-1.5 py-0.5 rounded bg-black/80 text-yellow-200 text-xs font-mono whitespace-nowrap pointer-events-none">
            total {formatDistance(total, metric)} · area {formatArea(area, metric)}
          </div>
        </Html>
      )}
    </group>
  )
}
