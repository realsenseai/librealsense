import { create } from 'zustand'
import { apiClient } from '../api/client'
import type { ViewerSettings, ViewerSettingsPatch } from '../api/types'

interface SettingsState {
  settings: ViewerSettings | null
  isLoading: boolean
  error: string | null
  fetchSettings: () => Promise<void>
  updateSettings: (patch: ViewerSettingsPatch) => Promise<void>
}

/** Server-side viewer settings (persisted by the backend, shared by every client). */
export const useSettingsStore = create<SettingsState>((set) => ({
  settings: null,
  isLoading: false,
  error: null,

  fetchSettings: async () => {
    set({ isLoading: true })
    try {
      set({ settings: await apiClient.getSettings(), isLoading: false, error: null })
    } catch (error) {
      set({ isLoading: false, error: error instanceof Error ? error.message : 'Failed to load settings' })
    }
  },

  updateSettings: async (patch) => {
    // The server answers with the merged result, so what is shown is what was stored.
    set({ settings: await apiClient.updateSettings(patch), error: null })
  },
}))

/** Whether distances are shown in metric units; metric until the settings arrive. */
export function useMetric(): boolean {
  return useSettingsStore((s) => s.settings?.viewer.metric_system ?? true)
}
