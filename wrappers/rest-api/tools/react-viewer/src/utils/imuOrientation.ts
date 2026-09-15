// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// Port of the C++ viewer's motion visualisation (common/rendering.h
// draw_motion_data / draw_axes / draw_circle): a fixed wireframe of three great
// circles with the X/Y/Z axes, and a unit-length vector showing which way the
// sensor is pointing. The vector is normalised, exactly as in the C++ code, so its
// direction carries the orientation and the magnitude is printed as text.

export interface Vec3 {
  x: number
  y: number
  z: number
}

// The C++ projection is glOrtho(-2.8, 2.8, -2.4, 2.4) with glRotatef(25, X),
// glTranslatef(0, -0.33, -1) and glRotatef(-135, Y) composed onto it, which applies
// to a vertex as Ortho · Rx(25) · T · Ry(-135) · v.
export const ORTHO_HALF_WIDTH = 2.8
export const ORTHO_HALF_HEIGHT = 2.4
const ROTATE_X_DEG = 25
const ROTATE_Y_DEG = -135
const TRANSLATE: Vec3 = { x: 0, y: -0.33, z: -1 }

// Radius of the three great circles, and the length of the axis arrows.
export const WIRE_RADIUS = 1.1
export const AXIS_LENGTH = 1
// Arrowhead reaches 10% past the axis and is 5% wide, as in draw_axes().
const ARROW_TIP = 1.1
const ARROW_HALF_WIDTH = 0.05

// Below this magnitude the C++ viewer draws a dot instead of a vector, because the
// direction of near-zero motion is just noise.
export const VECTOR_THRESHOLD = 0.2

const rad = (deg: number) => (deg * Math.PI) / 180

/**
 * Projects a point in sensor space to 2D plot coordinates.
 *
 * The returned y grows downward, so it can be used directly in an SVG viewBox.
 * That matches what the C++ tile shows on screen: it copies the GL buffer into a
 * texture that is drawn flipped, which is why +Y (green) points down there.
 */
export function projectIMU(v: Vec3): [number, number] {
  const cy = Math.cos(rad(ROTATE_Y_DEG))
  const sy = Math.sin(rad(ROTATE_Y_DEG))
  const x1 = cy * v.x + sy * v.z
  const z1 = -sy * v.x + cy * v.z

  const x2 = x1 + TRANSLATE.x
  const y2 = v.y + TRANSLATE.y
  const z2 = z1 + TRANSLATE.z

  const cx = Math.cos(rad(ROTATE_X_DEG))
  const sx = Math.sin(rad(ROTATE_X_DEG))
  return [x2, y2 * cx - z2 * sx]
}

// `+ 0` collapses -0 to 0, so a point reached from either side of the circle is
// written the same way and the path closes exactly on its starting coordinates.
const coord = (n: number) => (n + 0).toFixed(4).replace('-0.0000', '0.0000')

const toPath = (points: [number, number][]) =>
  points.map(([x, y], i) => `${i === 0 ? 'M' : 'L'}${coord(x)},${coord(y)}`).join('')

// One great circle, spanned by two orthogonal unit vectors, as draw_circle() does.
function circlePath(a: Vec3, b: Vec3, segments = 50): string {
  const points: [number, number][] = []
  for (let i = 0; i <= segments; i++) {
    const theta = ((2 * Math.PI) / segments) * i
    const cos = Math.cos(theta)
    const sin = Math.sin(theta)
    points.push(
      projectIMU({
        x: WIRE_RADIUS * (a.x * cos + b.x * sin),
        y: WIRE_RADIUS * (a.y * cos + b.y * sin),
        z: WIRE_RADIUS * (a.z * cos + b.z * sin),
      }),
    )
  }
  return toPath(points)
}

const X_AXIS: Vec3 = { x: 1, y: 0, z: 0 }
const Y_AXIS: Vec3 = { x: 0, y: 1, z: 0 }
const Z_AXIS: Vec3 = { x: 0, y: 0, z: 1 }

const scale = (v: Vec3, k: number): Vec3 => ({ x: v.x * k, y: v.y * k, z: v.z * k })
const add = (a: Vec3, b: Vec3): Vec3 => ({ x: a.x + b.x, y: a.y + b.y, z: a.z + b.z })

// The three circles of the wireframe: the XY, YZ and XZ planes.
export const WIRE_CIRCLES = [
  circlePath(X_AXIS, Y_AXIS),
  circlePath(Y_AXIS, Z_AXIS),
  circlePath(X_AXIS, Z_AXIS),
]

export interface AxisGeometry {
  key: 'x' | 'y' | 'z'
  color: string
  line: string
  // Two arrowhead triangles per axis, one in each perpendicular plane, so the head
  // stays visible whichever way the axis is pointing.
  heads: string[]
}

function axisGeometry(key: 'x' | 'y' | 'z', axis: Vec3, color: string, perpendiculars: Vec3[]): AxisGeometry {
  const tip = scale(axis, ARROW_TIP)
  const base = scale(axis, AXIS_LENGTH)
  return {
    key,
    color,
    line: toPath([projectIMU({ x: 0, y: 0, z: 0 }), projectIMU(base)]),
    heads: perpendiculars.map((perp) =>
      `${toPath([
        projectIMU(tip),
        projectIMU(add(base, scale(perp, ARROW_HALF_WIDTH))),
        projectIMU(add(base, scale(perp, -ARROW_HALF_WIDTH))),
      ])}Z`,
    ),
  }
}

// Pure red / green / blue, as in draw_axes().
export const AXES: AxisGeometry[] = [
  axisGeometry('x', X_AXIS, '#ff0000', [Y_AXIS, Z_AXIS]),
  axisGeometry('y', Y_AXIS, '#00ff00', [X_AXIS, Z_AXIS]),
  axisGeometry('z', Z_AXIS, '#0000ff', [X_AXIS, Y_AXIS]),
]

export const ORIGIN_2D = projectIMU({ x: 0, y: 0, z: 0 })

// The C++ ortho box is far wider than the wireframe, and its tile crops the 768x768
// texture it renders into; fitting the box to the geometry instead reproduces what
// the C++ tile shows without carrying the empty margin around it.
export const VIEW_BOX = (() => {
  const coords = [...WIRE_CIRCLES, ...AXES.flatMap((a) => [a.line, ...a.heads])]
    .flatMap((path) => path.replace(/Z$/, '').split(/[ML]/).filter(Boolean))
    .map((pair) => pair.split(',').map(Number) as [number, number])
  const xs = coords.map(([x]) => x)
  const ys = coords.map(([, y]) => y)
  // Room for the magnitude label, which sits beside the vector.
  const pad = 0.22
  const minX = Math.min(...xs) - pad
  const minY = Math.min(...ys) - pad
  return `${minX} ${minY} ${Math.max(...xs) + pad - minX} ${Math.max(...ys) + pad - minY}`
})()

export interface MotionVector {
  norm: number
  // Null below VECTOR_THRESHOLD, where the C++ viewer shows a dot instead.
  tip: [number, number] | null
  // Midpoint of the drawn vector, where the magnitude is labelled.
  label: [number, number]
}

export function motionVector(v: Vec3): MotionVector {
  const norm = Math.sqrt(v.x ** 2 + v.y ** 2 + v.z ** 2)
  if (norm < VECTOR_THRESHOLD) {
    return { norm, tip: null, label: ORIGIN_2D }
  }
  const unit = scale(v, 1 / norm)
  return {
    norm,
    tip: projectIMU(unit),
    label: projectIMU(scale(unit, 0.5)),
  }
}
