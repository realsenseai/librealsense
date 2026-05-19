import { useRef, useMemo, useEffect } from 'react'
import { Canvas, useFrame, useThree } from '@react-three/fiber'
import { OrbitControls, PerspectiveCamera } from '@react-three/drei'
import * as THREE from 'three'
import { useAppStore } from '../store'

export function PointCloudViewer() {
  const { pointCloudVertices, isPointCloudEnabled, togglePointCloud, isStreaming } = useAppStore()

  return (
    <div className="h-full flex flex-col">
      {/* Controls */}
      <div className="flex items-center gap-4 p-2 bg-gray-800 rounded-t-lg">
        <label className="flex items-center gap-2 cursor-pointer">
          <input
            type="checkbox"
            checked={isPointCloudEnabled}
            onChange={() => togglePointCloud()}
            disabled={!isStreaming}
            className="control-checkbox"
          />
          <span className="text-sm">Enable Point Cloud</span>
        </label>
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
        {isPointCloudEnabled && pointCloudVertices ? (
          <Canvas>
            <PerspectiveCamera makeDefault position={[0, 0, 2]} />
            <OrbitControls enablePan enableZoom enableRotate />
            <ambientLight intensity={0.5} />
            <PointCloud vertices={pointCloudVertices} />
            <Axes />
            <gridHelper args={[10, 10, '#444', '#333']} />
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
                  ? 'Enable point cloud in controls above'
                  : 'Start streaming and enable point cloud'}
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
}

function PointCloud({ vertices }: PointCloudProps) {
  const pointsRef = useRef<THREE.Points>(null)
  const { camera } = useThree()

  // Create geometry from vertices
  const geometry = useMemo(() => {
    const geo = new THREE.BufferGeometry()
    
    // Vertices array is [x1, y1, z1, x2, y2, z2, ...]
    const positions = new Float32Array(vertices.length)
    const colors = new Float32Array(vertices.length)

    for (let i = 0; i < vertices.length; i += 3) {
      // Copy positions
      positions[i] = vertices[i]
      positions[i + 1] = vertices[i + 1]
      positions[i + 2] = vertices[i + 2]

      // Color based on depth (Z value)
      const z = vertices[i + 2]
      const normalizedZ = Math.min(Math.max((z + 1) / 4, 0), 1) // Normalize depth to 0-1
      
      // Create a gradient from blue (near) to red (far)
      colors[i] = normalizedZ // R
      colors[i + 1] = 0.2 // G
      colors[i + 2] = 1 - normalizedZ // B
    }

    geo.setAttribute('position', new THREE.BufferAttribute(positions, 3))
    geo.setAttribute('color', new THREE.BufferAttribute(colors, 3))
    geo.computeBoundingSphere()

    return geo
  }, [vertices])

  // Center camera on point cloud
  useEffect(() => {
    if (geometry.boundingSphere) {
      const center = geometry.boundingSphere.center
      camera.lookAt(center)
    }
  }, [geometry, camera])

  // Rotate slowly for effect
  useFrame(() => {
    if (pointsRef.current) {
      // Optional: Add subtle rotation
      // pointsRef.current.rotation.y += 0.001
    }
  })

  return (
    <points ref={pointsRef} geometry={geometry}>
      <pointsMaterial size={0.005} vertexColors sizeAttenuation />
    </points>
  )
}

function Axes() {
  return (
    <group>
      {/* X axis - Red */}
      <line>
        <bufferGeometry>
          <bufferAttribute
            attach="attributes-position"
            args={[new Float32Array([0, 0, 0, 1, 0, 0]), 3]}
          />
        </bufferGeometry>
        <lineBasicMaterial color="red" />
      </line>
      {/* Y axis - Green */}
      <line>
        <bufferGeometry>
          <bufferAttribute
            attach="attributes-position"
            args={[new Float32Array([0, 0, 0, 0, 1, 0]), 3]}
          />
        </bufferGeometry>
        <lineBasicMaterial color="green" />
      </line>
      {/* Z axis - Blue */}
      <line>
        <bufferGeometry>
          <bufferAttribute
            attach="attributes-position"
            args={[new Float32Array([0, 0, 0, 0, 0, 1]), 3]}
          />
        </bufferGeometry>
        <lineBasicMaterial color="blue" />
      </line>
    </group>
  )
}

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
