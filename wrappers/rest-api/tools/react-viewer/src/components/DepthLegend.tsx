import { useState, useRef } from 'react'
import { rulerTicks } from '../utils/depthRuler'

interface DepthLegendProps {
  minDepth: number // in meters, at the bottom
  maxDepth: number
  gradient: string // CSS background, min at the bottom (utils/depthRuler.ts)
}

export function DepthLegend({ minDepth, maxDepth, gradient }: DepthLegendProps) {
  const [hoverY, setHoverY] = useState<number | null>(null)
  const [hoverDepth, setHoverDepth] = useState<number | null>(null)
  const containerRef = useRef<HTMLDivElement>(null)

  const handleMouseMove = (e: React.MouseEvent<HTMLDivElement>) => {
    if (!containerRef.current) return
    const rect = containerRef.current.getBoundingClientRect()
    const y = e.clientY - rect.top
    const relativeY = y / rect.height
    // Invert: top = max depth, bottom = min depth
    const depth = maxDepth - relativeY * (maxDepth - minDepth)
    setHoverY(y)
    setHoverDepth(depth)
  }

  const handleMouseLeave = () => {
    setHoverY(null)
    setHoverDepth(null)
  }

  const depthRange = maxDepth - minDepth
  const ticks = rulerTicks(minDepth, maxDepth)

  return (
    <div className="flex items-stretch h-full">
      {/* Color gradient bar */}
      <div
        ref={containerRef}
        className="relative w-5 cursor-crosshair"
        style={{
          background: gradient,
          border: '1px solid rgba(0, 0, 0, 0.8)',
        }}
        onMouseMove={handleMouseMove}
        onMouseLeave={handleMouseLeave}
      >
        {/* Hover indicator line */}
        {hoverY !== null && (
          <div
            className="absolute left-0 right-0 h-px bg-white opacity-80"
            style={{ top: `${hoverY}px` }}
          />
        )}
      </div>

      {/* Numeric scale */}
      <div className="relative w-12 text-xs text-white font-mono ml-1">
        {ticks.values.map((depth) => {
          const y = ((maxDepth - depth) / depthRange) * 100
          return (
            <div
              key={depth}
              className="absolute right-0 transform -translate-y-1/2"
              style={{ top: `${y}%` }}
            >
              {depth.toFixed(ticks.decimals)}
            </div>
          )
        })}
      </div>

      {/* Hover tooltip */}
      {hoverDepth !== null && hoverY !== null && (
        <div
          className="absolute left-16 bg-black/90 text-white text-xs px-2 py-1 rounded shadow-lg pointer-events-none whitespace-nowrap z-10"
          style={{ top: `${hoverY}px`, transform: 'translateY(-50%)' }}
        >
          {hoverDepth.toFixed(3)} m
        </div>
      )}
    </div>
  )
}
