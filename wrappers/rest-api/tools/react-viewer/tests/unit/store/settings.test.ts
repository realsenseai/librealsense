import { describe, it, expect, beforeEach } from 'vitest'
import { http, HttpResponse } from 'msw'
import { server } from '../../mocks/server'
import { useSettingsStore } from '@/store/settings'
import { mockSettings } from '../../mocks/fixtures/settings'

describe('settings store', () => {
  beforeEach(() => {
    useSettingsStore.setState({ settings: null, isLoading: false, error: null })
  })

  it('loads the server settings', async () => {
    await useSettingsStore.getState().fetchSettings()
    expect(useSettingsStore.getState().settings).toEqual(mockSettings)
  })

  it('keeps what the server answers after an update', async () => {
    let sent: unknown = null
    server.use(
      http.put('/api/v1/settings/', async ({ request }) => {
        sent = await request.json()
        return HttpResponse.json({ ...mockSettings, viewer: { metric_system: false } })
      })
    )

    await useSettingsStore.getState().updateSettings({ viewer: { metric_system: false } })

    expect(sent).toEqual({ viewer: { metric_system: false } })
    expect(useSettingsStore.getState().settings?.viewer.metric_system).toBe(false)
  })

  it('records a load failure instead of throwing', async () => {
    server.use(http.get('/api/v1/settings/', () => HttpResponse.json({ detail: 'boom' }, { status: 500 })))
    await useSettingsStore.getState().fetchSettings()
    expect(useSettingsStore.getState().settings).toBeNull()
    expect(useSettingsStore.getState().error).toBeTruthy()
  })
})
