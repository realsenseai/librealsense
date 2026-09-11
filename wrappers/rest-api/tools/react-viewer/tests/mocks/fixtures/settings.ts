import type { ViewerSettings } from '@/api/types'

/** The backend's defaults (app/models/settings.py). */
export const mockSettings: ViewerSettings = {
  record: { file_save_mode: 'auto', default_path: '', compression: 'auto' },
  update: { sw_update_official_server: true, sw_update_url: '', recommend_calibration: true },
  console: { max_entries: 1000, log_to_file: false, log_filename: '', log_severity: 'info' },
  paths: { hwlogger_xml: '', commands_xml: '', presets_folder: '' },
  context: { dds_enabled: false, dds_domain: 0 },
  calibration: { enable_writing: true },
  post_processing: { performance_mode: false },
  viewer: { metric_system: true, grid_horizontal_lines: 1, grid_vertical_lines: 1, grid_line_width: 1, grid_line_color: '#ffffff' },
}
