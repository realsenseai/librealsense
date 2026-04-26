import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest'
import { render, screen, waitFor, act } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { Toast, ToastContainer } from '@/components/Toast'

describe('Toast', () => {
  beforeEach(() => {
    vi.useFakeTimers()
  })

  afterEach(() => {
    vi.useRealTimers()
  })

  describe('Rendering', () => {
    it('renders the message', () => {
      const onClose = vi.fn()
      render(<Toast id="1" type="info" message="Test message" onClose={onClose} />)
      
      expect(screen.getByText('Test message')).toBeInTheDocument()
    })

    it('renders success icon for success type', () => {
      const onClose = vi.fn()
      render(<Toast id="1" type="success" message="Success!" onClose={onClose} />)
      
      expect(screen.getByText('✓')).toBeInTheDocument()
    })

    it('renders error icon for error type', () => {
      const onClose = vi.fn()
      render(<Toast id="1" type="error" message="Error!" onClose={onClose} />)
      
      expect(screen.getByText('✕')).toBeInTheDocument()
    })

    it('renders info icon for info type', () => {
      const onClose = vi.fn()
      render(<Toast id="1" type="info" message="Info" onClose={onClose} />)
      
      expect(screen.getByText('ℹ')).toBeInTheDocument()
    })
  })

  describe('Styling', () => {
    it('applies success styling for success type', () => {
      const onClose = vi.fn()
      const { container } = render(<Toast id="1" type="success" message="Success!" onClose={onClose} />)
      
      // The root div should have the background color class
      const toastRoot = container.firstChild as HTMLElement
      expect(toastRoot.className).toContain('bg-green')
    })

    it('applies error styling for error type', () => {
      const onClose = vi.fn()
      const { container } = render(<Toast id="1" type="error" message="Error!" onClose={onClose} />)
      
      const toastRoot = container.firstChild as HTMLElement
      expect(toastRoot.className).toContain('bg-red')
    })

    it('applies info styling for info type', () => {
      const onClose = vi.fn()
      const { container } = render(<Toast id="1" type="info" message="Info" onClose={onClose} />)
      
      const toastRoot = container.firstChild as HTMLElement
      expect(toastRoot.className).toContain('bg-blue')
    })
  })

  describe('Auto-dismiss', () => {
    it('calls onClose after default duration (4000ms)', async () => {
      const onClose = vi.fn()
      render(<Toast id="1" type="info" message="Test" onClose={onClose} />)
      
      expect(onClose).not.toHaveBeenCalled()
      
      await act(async () => {
        vi.advanceTimersByTime(4000)
      })
      
      expect(onClose).toHaveBeenCalledWith('1')
    })

    it('calls onClose after custom duration', async () => {
      const onClose = vi.fn()
      render(<Toast id="1" type="info" message="Test" onClose={onClose} duration={2000} />)
      
      await act(async () => {
        vi.advanceTimersByTime(2000)
      })
      
      expect(onClose).toHaveBeenCalledWith('1')
    })

    it('does not call onClose before duration expires', async () => {
      const onClose = vi.fn()
      render(<Toast id="1" type="info" message="Test" onClose={onClose} duration={5000} />)
      
      await act(async () => {
        vi.advanceTimersByTime(3000)
      })
      
      expect(onClose).not.toHaveBeenCalled()
    })
  })

  describe('Manual Close', () => {
    it('calls onClose when close button is clicked', async () => {
      vi.useRealTimers() // Use real timers for user events
      const onClose = vi.fn()
      render(<Toast id="test-id" type="info" message="Test" onClose={onClose} />)
      
      const closeButton = screen.getByText('×')
      await userEvent.click(closeButton)
      
      expect(onClose).toHaveBeenCalledWith('test-id')
    })
  })
})

describe('ToastContainer', () => {
  it('renders nothing when toasts array is empty', () => {
    const { container } = render(<ToastContainer toasts={[]} onClose={vi.fn()} />)
    
    expect(container.firstChild).toBeNull()
  })

  it('renders all toasts in the array', () => {
    const toasts = [
      { id: '1', type: 'success' as const, message: 'Success message' },
      { id: '2', type: 'error' as const, message: 'Error message' },
      { id: '3', type: 'info' as const, message: 'Info message' },
    ]
    
    render(<ToastContainer toasts={toasts} onClose={vi.fn()} />)
    
    expect(screen.getByText('Success message')).toBeInTheDocument()
    expect(screen.getByText('Error message')).toBeInTheDocument()
    expect(screen.getByText('Info message')).toBeInTheDocument()
  })

  it('passes onClose callback to each toast', async () => {
    const onClose = vi.fn()
    const toasts = [
      { id: 'toast-1', type: 'info' as const, message: 'Test' },
    ]
    
    render(<ToastContainer toasts={toasts} onClose={onClose} />)
    
    const closeButton = screen.getByText('×')
    await userEvent.click(closeButton)
    
    expect(onClose).toHaveBeenCalledWith('toast-1')
  })

  it('is positioned at bottom-right of screen', () => {
    const toasts = [{ id: '1', type: 'info' as const, message: 'Test' }]
    const { container } = render(<ToastContainer toasts={toasts} onClose={vi.fn()} />)
    
    const containerEl = container.firstChild as HTMLElement
    expect(containerEl.className).toContain('fixed')
    expect(containerEl.className).toContain('bottom-4')
    expect(containerEl.className).toContain('right-4')
  })
})
