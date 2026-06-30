import { describe, it, expect, vi } from 'vitest'
import { render, screen, fireEvent } from '@testing-library/react'
import type { StreamMetadata } from '@/api/types'
import { MetadataItem, MetadataOverlay, MetadataPanel } from '@/components/StreamViewer'

const baseMetadata: StreamMetadata = {
  stream_type: 'depth',
  timestamp: 12345,
  frame_number: 42,
  width: 320,
  height: 240,
  hardware_width: 640,
  hardware_height: 480,
  hardware_fps: 29.97,
  clock_domain: 'global_time',
  pixel_format: 'Z16',
  frame_metadata: { actual_fps: 29970, frame_counter: 42 },
}

describe('MetadataItem', () => {
  it('renders nothing when value is undefined', () => {
    const { container } = render(<MetadataItem label="x" value={undefined} />)
    expect(container).toBeEmptyDOMElement()
  })

  it('renders nothing when value is null', () => {
    const { container } = render(<MetadataItem label="x" value={null} />)
    expect(container).toBeEmptyDOMElement()
  })

  it('renders label verbatim and value when both present', () => {
    render(<MetadataItem label="Hardware FPS" value={30} />)
    expect(screen.getByText('Hardware FPS')).toBeInTheDocument()
    expect(screen.getByText('30')).toBeInTheDocument()
  })

  it('renders falsy numeric value 0', () => {
    render(<MetadataItem label="Frame Counter" value={0} />)
    expect(screen.getByText('Frame Counter')).toBeInTheDocument()
    expect(screen.getByText('0')).toBeInTheDocument()
  })
})

describe('MetadataOverlay', () => {
  it('renders viewer info rows for video streams', () => {
    render(<MetadataOverlay streamType="depth" metadata={baseMetadata} fps={30} />)
    expect(screen.getByText('Frame Timestamp')).toBeInTheDocument()
    expect(screen.getByText('Frame Number')).toBeInTheDocument()
    expect(screen.getByText('Pixel Format')).toBeInTheDocument()
    expect(screen.getByText('Hardware Size')).toBeInTheDocument()
    expect(screen.getByText('640×480')).toBeInTheDocument()
    expect(screen.getByText('Display Size')).toBeInTheDocument()
    expect(screen.getByText('320×240')).toBeInTheDocument()
    expect(screen.getByText('Hardware FPS')).toBeInTheDocument()
    expect(screen.getByText('Viewer FPS')).toBeInTheDocument()
  })

  it('renders per-key frame_metadata rows', () => {
    render(<MetadataOverlay streamType="depth" metadata={baseMetadata} fps={30} />)
    expect(screen.getByText('Actual Fps')).toBeInTheDocument()
    expect(screen.getByText('29970')).toBeInTheDocument()
    expect(screen.getByText('Frame Counter')).toBeInTheDocument()
  })

  it('hides hardware_size and display_size for motion streams', () => {
    render(<MetadataOverlay streamType="gyro" metadata={baseMetadata} fps={400} />)
    expect(screen.queryByText('Hardware Size')).not.toBeInTheDocument()
    expect(screen.queryByText('Display Size')).not.toBeInTheDocument()
    expect(screen.getByText('Frame Timestamp')).toBeInTheDocument()
  })

  it('header reflects stream type', () => {
    render(<MetadataOverlay streamType="color" metadata={baseMetadata} fps={30} />)
    expect(screen.getByText(/Frame Metadata — COLOR/)).toBeInTheDocument()
  })

  it('shows OS-level warning when clock_domain is system_time', () => {
    const md: StreamMetadata = { ...baseMetadata, clock_domain: 'system_time' }
    render(<MetadataOverlay streamType="depth" metadata={md} fps={30} />)
    const alert = screen.getByRole('alert')
    expect(alert).toHaveTextContent('Per-frame metadata is not enabled at the OS level!')
    expect(alert).toHaveTextContent('Please follow the installation guide for the details.')
  })

  it('does not show OS-level warning when clock_domain is global_time', () => {
    render(<MetadataOverlay streamType="depth" metadata={baseMetadata} fps={30} />)
    expect(screen.queryByRole('alert')).not.toBeInTheDocument()
  })

  it('does not show OS-level warning when clock_domain is hardware_clock', () => {
    const md: StreamMetadata = { ...baseMetadata, clock_domain: 'hardware_clock' }
    render(<MetadataOverlay streamType="depth" metadata={md} fps={30} />)
    expect(screen.queryByRole('alert')).not.toBeInTheDocument()
  })
})

describe('MetadataPanel', () => {
  it('renders nothing when no metadata at all', () => {
    const { container } = render(
      <MetadataPanel
        metadata={undefined}
        streamType="depth"
        fps={30}
        show={false}
        onToggle={() => {}}
      />,
    )
    expect(container).toBeEmptyDOMElement()
  })

  it('renders toggle button with viewer info only (no frame_metadata keys)', () => {
    const md: StreamMetadata = {
      stream_type: 'depth',
      timestamp: 100,
      frame_number: 1,
      width: 640,
      height: 480,
    }
    render(
      <MetadataPanel
        metadata={md}
        streamType="depth"
        fps={30}
        show={false}
        onToggle={() => {}}
      />,
    )
    expect(screen.getByRole('button', { name: 'Metadata' })).toBeInTheDocument()
  })

  it('shows "Metadata" label when closed', () => {
    render(
      <MetadataPanel
        metadata={baseMetadata}
        streamType="depth"
        fps={30}
        show={false}
        onToggle={() => {}}
      />,
    )
    expect(screen.getByRole('button')).toHaveTextContent('Metadata')
  })

  it('shows "✕" label when open and renders overlay', () => {
    render(
      <MetadataPanel
        metadata={baseMetadata}
        streamType="depth"
        fps={30}
        show={true}
        onToggle={() => {}}
      />,
    )
    expect(screen.getByRole('button')).toHaveTextContent('✕')
    expect(screen.getByText(/Frame Metadata — DEPTH/)).toBeInTheDocument()
  })

  it('calls onToggle with !show when button clicked', () => {
    const onToggle = vi.fn()
    render(
      <MetadataPanel
        metadata={baseMetadata}
        streamType="depth"
        fps={30}
        show={false}
        onToggle={onToggle}
      />,
    )
    fireEvent.click(screen.getByRole('button'))
    expect(onToggle).toHaveBeenCalledWith(true)
  })
})
