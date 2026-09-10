import { create } from 'zustand'
import { apiClient } from '../api/client'
import type { JobInfo } from '../api/types'

interface JobsState {
  jobs: Record<string, JobInfo>
  /** Apply a snapshot from the `job` Socket.IO event or a REST fetch. */
  upsert: (job: JobInfo) => void
  fetchJobs: () => Promise<void>
  cancelJob: (id: string) => Promise<void>
  /** Drop a finished job from the UI. */
  dismiss: (id: string) => void
}

/** Long server operations (firmware, calibration, export...) keyed by job id. */
export const useJobsStore = create<JobsState>((set) => ({
  jobs: {},

  upsert: (job) => set((s) => ({ jobs: { ...s.jobs, [job.id]: job } })),

  fetchJobs: async () => {
    const jobs = await apiClient.getJobs()
    set({ jobs: Object.fromEntries(jobs.map((j) => [j.id, j])) })
  },

  cancelJob: async (id) => {
    await apiClient.cancelJob(id)
  },

  dismiss: (id) =>
    set((s) => ({ jobs: Object.fromEntries(Object.entries(s.jobs).filter(([jobId]) => jobId !== id)) })),
}))

export function useRunningJobs(kind?: string): JobInfo[] {
  return useJobsStore((s) =>
    Object.values(s.jobs).filter((j) => j.state === 'running' && (!kind || j.kind === kind)))
}
