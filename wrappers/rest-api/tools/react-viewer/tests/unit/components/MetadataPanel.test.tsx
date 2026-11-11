import { describe, it, expect, vi } from 'vitest'
import { render, screen, fireEvent } from '@testing-library/react'
import type { FrameHeader } from '@/api/frames'
import type { SupportedStreamProfile } from '@/api/types'
import { MetadataItem, MetadataOverlay, MetadataPanel } from '@/components/StreamViewer'

// A decimated depth frame: the camera's 640x480 arrives as 320x240
const header: FrameHeader = {
  frame_number: 42,
  timestamp: 12345,
  timestamp_domain: 'global_time',
  format: 'z16',
  metadata: { actual_fps: 29970, frame_counter: 42 },
  width: 320,
  height: 240,
  stride: 640,
}
const profile: SupportedStreamProfile = {
  stream_type: 'depth', stream_index: 0, format: 'z16', width: 640, height: 480, fps: 30, default: true,
}
const motionHeader: FrameHeader = {
  frame_number: 7, timestamp: 12345, timestamp_domain: 'global_time', format: 'motion_xyz32f', metadata: {},
}
const motionProfile: SupportedStreamProfile = { ...profile, stream_type: 'gyro', format: 'motion_xyz32f', width: 0, height: 0, fps: 400 }

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
    render(<MetadataOverlay streamType="depth" header={header} profile={profile} fps={30} />)
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
    render(<MetadataOverlay streamType="depth" header={header} profile={profile} fps={30} />)
    expect(screen.getByText('Actual Fps')).toBeInTheDocument()
    expect(screen.getByText('29970')).toBeInTheDocument()
    expect(screen.getByText('Frame Counter')).toBeInTheDocument()
  })

  it('hides hardware_size and display_size for motion streams', () => {
    render(<MetadataOverlay streamType="gyro" header={motionHeader} profile={motionProfile} fps={400} />)
    expect(screen.queryByText('Hardware Size')).not.toBeInTheDocument()
    expect(screen.queryByText('Display Size')).not.toBeInTheDocument()
    expect(screen.getByText('Frame Timestamp')).toBeInTheDocument()
  })

  it('header reflects stream type', () => {
    render(<MetadataOverlay streamType="color" header={header} profile={profile} fps={30} />)
    expect(screen.getByText(/Frame Metadata — COLOR/)).toBeInTheDocument()
  })

  it('shows OS-level warning when clock_domain is system_time', () => {
    render(<MetadataOverlay streamType="depth" header={{ ...header, timestamp_domain: 'system_time' }} profile={profile} fps={30} />)
    const alert = screen.getByRole('alert')
    expect(alert).toHaveTextContent('Per-frame metadata is not enabled at the OS level!')
    expect(alert).toHaveTextContent('Please follow the installation guide for the details.')
  })

  it('does not show OS-level warning when clock_domain is global_time', () => {
    render(<MetadataOverlay streamType="depth" header={header} profile={profile} fps={30} />)
    expect(screen.queryByRole('alert')).not.toBeInTheDocument()
  })

  it('does not show OS-level warning when clock_domain is hardware_clock', () => {
    render(<MetadataOverlay streamType="depth" header={{ ...header, timestamp_domain: 'hardware_clock' }} profile={profile} fps={30} />)
    expect(screen.queryByRole('alert')).not.toBeInTheDocument()
  })
})

describe('MetadataPanel', () => {
  it('renders nothing until a frame arrives', () => {
    const { container } = render(
      <MetadataPanel
        header={null}
        streamType="depth"
        profile={profile}
        fps={30}
        show={false}
        onToggle={() => {}}
      />,
    )
    expect(container).toBeEmptyDOMElement()
  })

  it('renders toggle button for a frame without metadata', () => {
    render(
      <MetadataPanel
        header={{ ...header, metadata: {} }}
        streamType="depth"
        profile={profile}
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
        header={header}
        streamType="depth"
        profile={profile}
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
        header={header}
        streamType="depth"
        profile={profile}
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
        header={header}
        streamType="depth"
        profile={profile}
        fps={30}
        show={false}
        onToggle={onToggle}
      />,
    )
    fireEvent.click(screen.getByRole('button'))
    expect(onToggle).toHaveBeenCalledWith(true)
  })
})
