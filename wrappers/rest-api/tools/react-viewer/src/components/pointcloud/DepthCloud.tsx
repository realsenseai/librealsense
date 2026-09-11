import { useEffect, useMemo, useRef } from 'react'
import * as THREE from 'three'
import type { PointCloudGeometry } from '../../api/types'
import type { DepthFrame, Shading } from '../../store/pointcloud'
import { DISTORTION_BROWN, DISTORTION_NONE, fragmentShader, vertexShader } from './shaders'

interface DepthCloudProps {
  frame: DepthFrame
  geometry: PointCloudGeometry
  /** The texture stream's <video>, or null for the depth colormap. */
  video: HTMLVideoElement | null
  shading: Shading
  occlusionInvalidation: boolean
  depthRange: [number, number]
}

/** One vertex per depth pixel; the mesh index joins neighbours into two triangles per cell. */
function gridGeometry(width: number, height: number, indexed: boolean): THREE.BufferGeometry {
  const positions = new Float32Array(width * height * 3)
  for (let v = 0, i = 0; v < height; v++) {
    for (let u = 0; u < width; u++, i += 3) {
      positions[i] = u
      positions[i + 1] = v
    }
  }
  const geo = new THREE.BufferGeometry()
  geo.setAttribute('position', new THREE.BufferAttribute(positions, 3))
  if (indexed) {
    const index = new Uint32Array((width - 1) * (height - 1) * 6)
    for (let v = 0, k = 0; v < height - 1; v++) {
      for (let u = 0; u < width - 1; u++) {
        const i = v * width + u
        index[k++] = i; index[k++] = i + 1; index[k++] = i + width
        index[k++] = i + 1; index[k++] = i + width + 1; index[k++] = i + width
      }
    }
    geo.setIndex(new THREE.BufferAttribute(index, 1))
  }
  // The shader places vertices; keep three.js from culling on the pixel-space bounds.
  geo.boundingSphere = new THREE.Sphere(new THREE.Vector3(), 1e6)
  return geo
}

/** The point cloud of one device, unprojected on the GPU from its latest depth image. */
export function DepthCloud({ frame, geometry, video, shading, occlusionInvalidation, depthRange }: DepthCloudProps) {
  const depth = geometry.depth
  const texture = video && geometry.texture ? geometry.texture : null
  const meshMode = shading !== 'points'

  const depthTex = useMemo(() => {
    const t = new THREE.DataTexture(frame.data as unknown as BufferSource, frame.width, frame.height, THREE.RedIntegerFormat, THREE.UnsignedShortType)
    t.internalFormat = 'R16UI'
    t.magFilter = THREE.NearestFilter
    t.minFilter = THREE.NearestFilter
    t.generateMipmaps = false
    t.needsUpdate = true
    return t
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [frame.width, frame.height])

  useEffect(() => {
    const image = depthTex.image as unknown as { data: Uint16Array }
    image.data = frame.data
    depthTex.needsUpdate = true
  }, [frame, depthTex])
  useEffect(() => () => depthTex.dispose(), [depthTex])

  const videoTex = useMemo(() => (video ? new THREE.VideoTexture(video) : null), [video])
  useEffect(() => () => videoTex?.dispose(), [videoTex])

  const grid = useMemo(() => gridGeometry(frame.width, frame.height, meshMode), [frame.width, frame.height, meshMode])
  useEffect(() => () => grid.dispose(), [grid])

  const material = useRef<THREE.RawShaderMaterial>(null)
  const uniforms = useMemo(() => ({
    depthTex: { value: depthTex },
    imageSize: { value: new THREE.Vector2(frame.width, frame.height) },
    units: { value: frame.units },
    depthK: { value: new THREE.Vector4(1, 1, 0, 0) },
    hasTexture: { value: 0 },
    texK: { value: new THREE.Vector4(1, 1, 0, 0) },
    texSize: { value: new THREE.Vector2(1, 1) },
    texCoeffs: { value: [0, 0, 0, 0, 0] },
    texModel: { value: DISTORTION_NONE },
    extRot: { value: new THREE.Matrix3() },
    extTrans: { value: new THREE.Vector3() },
    minDeltaZ: { value: 0.05 },
    occlusion: { value: 1 },
    pointSize: { value: 2 },
    colorTex: { value: videoTex ?? depthTex },
    shaded: { value: 0 },
    depthRange: { value: new THREE.Vector2(0, 6) },
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }), [depthTex])

  // Keep uniforms current without rebuilding the material
  useEffect(() => {
    const u = uniforms
    u.imageSize.value.set(frame.width, frame.height)
    u.units.value = frame.units
    if (depth) u.depthK.value.set(depth.fx, depth.fy, depth.ppx, depth.ppy)
    u.hasTexture.value = texture && videoTex ? 1 : 0
    if (texture) {
      u.texK.value.set(texture.fx, texture.fy, texture.ppx, texture.ppy)
      u.texSize.value.set(texture.width, texture.height)
      u.texCoeffs.value = texture.coeffs.slice(0, 5)
      u.texModel.value = texture.model.endsWith('brown_conrady') ? DISTORTION_BROWN : DISTORTION_NONE
      const r = texture.extrinsics.rotation
      // rs2_extrinsics is column-major; Matrix3.set takes rows
      u.extRot.value.set(r[0], r[3], r[6], r[1], r[4], r[7], r[2], r[5], r[8])
      u.extTrans.value.set(...(texture.extrinsics.translation as [number, number, number]))
    }
    if (videoTex) u.colorTex.value = videoTex
    u.occlusion.value = occlusionInvalidation ? 1 : 0
    u.shaded.value = shading === 'diffuse' ? 1 : 0
    u.depthRange.value.set(depthRange[0], depthRange[1])
  }, [uniforms, frame.width, frame.height, frame.units, depth, texture, videoTex, occlusionInvalidation, shading, depthRange])

  if (!depth) return null

  const mat = (
    <rawShaderMaterial
      ref={material}
      glslVersion={THREE.GLSL3}
      vertexShader={vertexShader}
      fragmentShader={fragmentShader}
      uniforms={uniforms}
      side={THREE.DoubleSide}
    />
  )
  return meshMode
    ? <mesh geometry={grid} frustumCulled={false}>{mat}</mesh>
    : <points geometry={grid} frustumCulled={false}>{mat}</points>
}
