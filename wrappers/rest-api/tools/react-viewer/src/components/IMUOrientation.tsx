// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import { memo } from 'react'
import {
  AXES,
  ORIGIN_2D,
  VIEW_BOX,
  WIRE_CIRCLES,
  motionVector,
  type Vec3,
} from '../utils/imuOrientation'

interface IMUOrientationProps {
  sample: Vec3 | null
  unit: string
}

// The wireframe and the axes never move, so they are plain constants rendered once
// per mount; only the white vector and its label follow the samples.
function IMUOrientation({ sample, unit }: IMUOrientationProps) {
  const motion = sample ? motionVector(sample) : null

  return (
    <div className="h-44 max-h-full flex items-center justify-center">
      <svg
        viewBox={VIEW_BOX}
        className="h-full w-auto"
        role="img"
        aria-label={
          motion
            ? `Orientation, magnitude ${motion.norm.toFixed(3)} ${unit}`
            : 'Orientation, waiting for data'
        }
      >
        {WIRE_CIRCLES.map((d, i) => (
          <path key={i} d={d} fill="none" stroke="#808080" strokeWidth={0.012} />
        ))}

        {AXES.map(({ key, color, line, heads }) => (
          <g key={key} fill={color} stroke={color}>
            <path d={line} strokeWidth={0.024} fill="none" />
            {heads.map((d, i) => (
              <path key={i} d={d} stroke="none" />
            ))}
          </g>
        ))}

        {motion?.tip ? (
          <>
            <path
              d={`M${ORIGIN_2D[0]},${ORIGIN_2D[1]}L${motion.tip[0]},${motion.tip[1]}`}
              stroke="#ffffff"
              strokeWidth={0.03}
              strokeLinecap="round"
            />
            <text
              x={motion.label[0]}
              y={motion.label[1]}
              fill="#ffffff"
              fontSize={0.22}
              // Nudged off the line so the vector does not strike the digits through.
              dx={0.1}
              dominantBaseline="middle"
            >
              {motion.norm.toFixed(3)}
            </text>
          </>
        ) : (
          // Near-zero motion has no meaningful direction; the C++ viewer draws a dot.
          <circle cx={ORIGIN_2D[0]} cy={ORIGIN_2D[1]} r={0.05} fill="#ffffff" />
        )}
      </svg>
    </div>
  )
}

export default memo(IMUOrientation)
