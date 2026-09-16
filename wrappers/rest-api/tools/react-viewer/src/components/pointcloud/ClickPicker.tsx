import { useEffect } from 'react'
import { useThree } from '@react-three/fiber'
import * as THREE from 'three'
import type { Ray } from '../../utils/measurement'

interface ClickPickerProps {
  onClick: (ray: Ray, shift: boolean) => void
}

/** A click (not a drag) anywhere on the canvas, as a camera ray. The cloud's vertices live
 * on the GPU, so three.js raycasting cannot hit them; the caller picks against the depth image. */
export function ClickPicker({ onClick }: ClickPickerProps) {
  const { camera, gl } = useThree()
  useEffect(() => {
    const el = gl.domElement
    let start: { x: number; y: number } | null = null
    const down = (e: PointerEvent) => { if (e.button === 0) start = { x: e.clientX, y: e.clientY } }
    const up = (e: PointerEvent) => {
      const s = start
      start = null
      if (!s || e.button !== 0 || Math.hypot(e.clientX - s.x, e.clientY - s.y) > 4) return
      const rect = el.getBoundingClientRect()
      const ndc = new THREE.Vector2(((e.clientX - rect.left) / rect.width) * 2 - 1, -((e.clientY - rect.top) / rect.height) * 2 + 1)
      const raycaster = new THREE.Raycaster()
      raycaster.setFromCamera(ndc, camera)
      const { origin, direction } = raycaster.ray
      onClick({ origin: [origin.x, origin.y, origin.z], direction: [direction.x, direction.y, direction.z] }, e.shiftKey)
    }
    el.addEventListener('pointerdown', down)
    el.addEventListener('pointerup', up)
    return () => {
      el.removeEventListener('pointerdown', down)
      el.removeEventListener('pointerup', up)
    }
  }, [camera, gl, onClick])
  return null
}
