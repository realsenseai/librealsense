import type { JobInfo } from '../../api/types'
import { useJobsStore } from '../../store/jobs'

interface JobProgressModalProps {
  job: JobInfo | undefined
  title: string
  subtitle?: string
  /** Shown while running, under the title; falls back to the job's own message. */
  runningText?: string
  cancellable?: boolean
  onClose: () => void
}

/**
 * Progress dialog for any server job: progress bar, phase text, failure or success, and
 * Close once the job is terminal. Firmware, calibration and export all render through it.
 */
export function JobProgressModal({ job, title, subtitle, runningText, cancellable, onClose }: JobProgressModalProps) {
  const cancelJob = useJobsStore((s) => s.cancelJob)
  if (!job) return null

  const percent = Math.round(job.progress * 100)
  const running = job.state === 'running'
  const failed = job.state === 'failed'
  const barColor = failed ? 'bg-red-500' : job.state === 'done' ? 'bg-green-500' : 'bg-rs-blue'

  return (
    <>
      <div className="fixed inset-0 bg-black/60 z-40" />
      <div className="fixed inset-0 flex items-center justify-center z-50" role="dialog" aria-label={title}>
        <div className="bg-gray-900 border border-gray-700 rounded-lg shadow-2xl p-6 max-w-md w-full mx-4">
          <div className="mb-4">
            <h2 className="text-xl font-bold text-white">{title}</h2>
            {subtitle && <p className="text-sm text-gray-400 mt-1">{subtitle}</p>}
          </div>

          <div className="mb-6 text-sm">
            {failed ? (
              <div className="p-3 bg-red-900/50 border border-red-700 rounded text-red-200">
                <div className="font-semibold mb-1">Failed</div>
                <div>{job.error}</div>
              </div>
            ) : job.state === 'done' ? (
              <div className="p-3 bg-green-900/50 border border-green-700 rounded text-green-200">
                <div className="font-semibold">✓ Complete</div>
                {job.message && <div className="mt-1">{job.message}</div>}
              </div>
            ) : job.state === 'cancelled' ? (
              <div className="p-3 bg-gray-800 border border-gray-600 rounded text-gray-300">Cancelled</div>
            ) : (
              <div className="text-gray-300">
                <div className="font-semibold mb-1">{runningText ?? 'In progress…'}</div>
                {job.message && <div className="text-gray-400">{job.message}</div>}
              </div>
            )}
          </div>

          <div className="mb-6">
            <div className="flex items-center justify-between mb-2">
              <span className="text-sm text-gray-400">Progress</span>
              <span className="text-sm font-semibold text-white">{percent}%</span>
            </div>
            <div className="w-full bg-gray-700 rounded-full h-2 overflow-hidden">
              <div className={`h-full transition-all duration-300 ${barColor}`} style={{ width: `${percent}%` }} />
            </div>
          </div>

          <div className="flex gap-3">
            {running ? (
              <>
                <div className="flex-1 px-4 py-2 bg-gray-700 text-gray-300 rounded font-medium text-center">
                  <div className="inline-block w-4 h-4 border-2 border-rs-blue border-t-transparent rounded-full animate-spin mr-2" />
                  In Progress
                </div>
                {cancellable && (
                  <button onClick={() => void cancelJob(job.id)}
                    className="px-4 py-2 bg-gray-800 hover:bg-gray-700 text-gray-200 rounded font-medium transition-colors">
                    Cancel
                  </button>
                )}
              </>
            ) : (
              <button onClick={onClose}
                className={`flex-1 px-4 py-2 text-white rounded font-medium transition-colors ${failed ? 'bg-gray-700 hover:bg-gray-600' : 'bg-green-600 hover:bg-green-500'}`}>
                {failed ? 'Close' : 'Done'}
              </button>
            )}
          </div>
        </div>
      </div>
    </>
  )
}
