import { useState, useRef } from 'react'

interface DepthLegendProps {
  minDepth: number // in meters
  maxDepth: number // in meters
  colorScheme?: 'jet' | 'classic' | 'white_to_black' | 'black_to_white'
  show?: boolean
}

export function DepthLegend({
  minDepth = 0,
  maxDepth = 6,
  colorScheme = 'jet',
  show = true,
}: DepthLegendProps) {
  const [hoverY, setHoverY] = useState<number | null>(null)
  const [hoverDepth, setHoverDepth] = useState<number | null>(null)
  const containerRef = useRef<HTMLDivElement>(null)

  if (!show) return null

  // Color schemes matching the old viewer colorizer
  const getColorScheme = (scheme: string): string[] => {
    switch (scheme) {
      case 'jet':
        return [
          'rgb(50, 0, 0)',    // dark red (far)
          'rgb(255, 0, 0)',   // red
          'rgb(255, 255, 0)', // yellow
          'rgb(0, 255, 255)', // cyan
          'rgb(0, 0, 255)',   // blue (near)
        ]
      case 'classic':
        return [
          'rgb(198, 33, 24)',  // red (far)
          'rgb(196, 57, 178)', // pink
          'rgb(204, 108, 191)',
          'rgb(45, 117, 220)',
          'rgb(25, 60, 192)',
          'rgb(30, 77, 203)',  // blue (near)
        ]
      case 'white_to_black':
        return ['rgb(0, 0, 0)', 'rgb(255, 255, 255)']
      case 'black_to_white':
        return ['rgb(255, 255, 255)', 'rgb(0, 0, 0)']
      default:
        return [
          'rgb(50, 0, 0)',
          'rgb(255, 0, 0)',
          'rgb(255, 255, 0)',
          'rgb(0, 255, 255)',
          'rgb(0, 0, 255)',
        ]
    }
  }

  const colors = getColorScheme(colorScheme)
  const gradientStops = colors
    .map((color, i) => `${color} ${(i / (colors.length - 1)) * 100}%`)
    .join(', ')

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

  // Generate scale labels
  const depthRange = maxDepth - minDepth
  // Match legacy viewer ticks: label every 1m from max down to min (integers only)
  const labels: number[] = []
  for (let v = Math.floor(maxDepth); v >= Math.ceil(minDepth); v -= 1) {
    labels.push(v)
    if (labels.length > 20) break
  }

  return (
    <div className="flex items-stretch h-full">
      {/* Color gradient bar */}
      <div
        ref={containerRef}
        className="relative w-5 cursor-crosshair"
        style={{
          background: `linear-gradient(to bottom, ${gradientStops})`,
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
        {labels.map((depth, i) => {
          const y = ((maxDepth - depth) / depthRange) * 100
          return (
            <div
              key={i}
              className="absolute right-0 transform -translate-y-1/2"
              style={{ top: `${y}%` }}
            >
              {depth.toFixed(1)}
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
