import { describe, it, expect, beforeEach } from 'vitest'
import { http, HttpResponse } from 'msw'
import { server } from '../../mocks/server'
import { useJobsStore } from '@/store/jobs'
import type { JobInfo } from '@/api/types'

const job = (over: Partial<JobInfo> = {}): JobInfo => ({
  id: 'j1', kind: 'firmware_update', device_id: 'dev', state: 'running', progress: 0.25,
  message: null, result: null, error: null, created_at: 1, updated_at: 1, ...over,
})

describe('jobs store', () => {
  beforeEach(() => useJobsStore.setState({ jobs: {} }))

  it('upserts snapshots by id', () => {
    useJobsStore.getState().upsert(job())
    useJobsStore.getState().upsert(job({ progress: 0.5 }))
    expect(useJobsStore.getState().jobs.j1.progress).toBe(0.5)
  })

  it('loads the job list from the server', async () => {
    server.use(http.get('/api/v1/jobs/', () => HttpResponse.json([job(), job({ id: 'j2', state: 'done' })])))
    await useJobsStore.getState().fetchJobs()
    expect(Object.keys(useJobsStore.getState().jobs).sort()).toEqual(['j1', 'j2'])
  })

  it('cancel posts to the job and dismiss drops it', async () => {
    let cancelled = false
    server.use(http.post('/api/v1/jobs/j1/cancel', () => { cancelled = true; return HttpResponse.json(job()) }))
    useJobsStore.getState().upsert(job())

    await useJobsStore.getState().cancelJob('j1')
    expect(cancelled).toBe(true)

    useJobsStore.getState().dismiss('j1')
    expect(useJobsStore.getState().jobs).toEqual({})
  })
})
