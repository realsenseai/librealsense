import { useEffect } from 'react'

export type ToastType = 'success' | 'error' | 'info'

export interface ToastAction {
  label: string
  /** Receives the toast id so the handler can decide when to dismiss. */
  onClick: (toastId: string) => void
}

interface ToastProps {
  id: string
  type: ToastType
  message: string
  onClose: (id: string) => void
  duration?: number
  action?: ToastAction
}

export function Toast({ id, type, message, onClose, duration = 4000, action }: ToastProps) {
  useEffect(() => {
    // Sticky if there's an action — user must explicitly dismiss or take action.
    if (action) return
    const timer = setTimeout(() => onClose(id), duration)
    return () => clearTimeout(timer)
  }, [id, onClose, duration, action])

  const bgColor = {
    success: 'bg-green-900/80 border-green-700',
    error: 'bg-red-900/80 border-red-700',
    info: 'bg-blue-900/80 border-blue-700',
  }[type]

  const textColor = {
    success: 'text-green-200',
    error: 'text-red-200',
    info: 'text-blue-200',
  }[type]

  const icon = {
    success: '✓',
    error: '✕',
    info: 'ℹ',
  }[type]

  return (
    <div className={`border rounded-lg p-4 flex items-start gap-3 ${bgColor} ${textColor}`}>
      <div className="font-bold text-lg mt-0.5">{icon}</div>
      <div className="flex-1">{message}</div>
      {action && (
        <button
          onClick={() => action.onClick(id)}
          className="px-2 py-0.5 rounded bg-white/10 hover:bg-white/20 text-sm font-medium border border-white/20"
        >
          {action.label}
        </button>
      )}
      <button
        onClick={() => onClose(id)}
        className="text-lg leading-none hover:opacity-70 transition-opacity"
        aria-label="Close"
      >
        ×
      </button>
    </div>
  )
}

interface ToastContainerProps {
  toasts: Array<{ id: string; type: ToastType; message: string; action?: ToastAction }>
  onClose: (id: string) => void
}

export function ToastContainer({ toasts, onClose }: ToastContainerProps) {
  if (toasts.length === 0) return null

  return (
    <div className="fixed top-4 right-4 z-50 space-y-2 max-w-sm">
      {toasts.map((toast) => (
        <Toast key={toast.id} {...toast} onClose={onClose} />
      ))}
    </div>
  )
}
