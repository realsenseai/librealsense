/**
 * Frame-metadata presentation, ported from the legacy viewer (common/stream-model.cpp):
 * per-attribute descriptions, hex display for bitmask fields, decoded bit meanings for the
 * depth-mapping (safety) and DDS attributes, and enum-like values spelled out.
 * Keys are the SDK's rs2_frame_metadata_value names as the API emits them ("frame_counter").
 */

const DESCRIPTIONS: Record<string, string> = {
  frame_counter: 'A sequential index managed per-stream. Integer value',
  frame_timestamp: 'Timestamp set by device clock when data readout and transmit commence. Units are device dependent',
  sensor_timestamp: "Timestamp of the middle of sensor's exposure calculated by device. usec",
  actual_exposure: "Sensor's exposure width. When Auto Exposure (AE) is on the value is controlled by firmware. usec",
  gain_level: "A relative value increasing which will increase the Sensor's gain factor. When AE is set On, the value is controlled by firmware. Integer value",
  auto_exposure: 'Auto Exposure Mode indicator. Zero corresponds to AE switched off.',
  white_balance: 'White Balance setting as a color temperature. Kelvin degrees',
  time_of_arrival: 'Time of arrival in system clock',
  temperature: 'Temperature of the device, measured at the time of the frame capture. Celsius degrees',
  backend_timestamp: 'Timestamp get from uvc driver. usec',
  actual_fps: 'Hardware FPS * 1000 = 1000000 * (frame-number - prev-frame-number) / (timestamp - prev-timestamp)',
  frame_laser_power_mode: 'Laser power mode. Zero corresponds to Laser power switched off and one for switched on.',
  exposure_priority: 'Exposure priority. When enabled Auto-exposure algorithm is allowed to reduce requested FPS to sufficiently increase exposure time (an get enough light)',
  power_line_frequency: 'Power Line Frequency for anti-flickering Off/50Hz/60Hz/Auto.',
  // depth-mapping (D5xx safety) attributes
  safety_depth_frame_counter: 'Counter of the depth frame upon which the stream was calculated',
  safety_level1: 'Designates the "Yellow" zone status: 1 - High, 0 - Low',
  safety_level1_origin: 'When l1 is low - equals to frame_counter in safety_header - For l1=0x1 : hold the Frame id on last transition to High state',
  safety_level2: 'Designates the "Red" zone status: 1 - High, 0 - Low',
  safety_level2_origin: 'When l2 is low - equals to frame_counter in safety_header - For l2=0x1 : hold the Frame id on last transition to High state',
  safety_level1_verdict: 'Current verdict for l1 Safety Signal - May differ from l1_signal due to additional logics applied',
  safety_level2_verdict: 'Current verdict for l2 Safety Signal - May differ from l2_signal due to additional logics applied',
  safety_operational_mode: 'Reflects the SC operational mode (XU control)',
  safety_preset_id_selected: 'Safety Preset index set via Adaptive Field selection GPIO',
  safety_preset_id_used: 'Safety Preset index used in the latest Vision Safety algo processing',
  safety_soc_fusa_events: 'SOC critical notification: L2/L3 that requires handling/troubleshooting in S.MCU 32-bit value, as supplied by STL mechanism',
  safety_mb_fusa_action: 'Bitmask, enumerated',
  safety_mb_status: 'Provision for future enhancements',
  safety_smcu_liveliness: 'Bitmask, enumerated',
  safety_smcu_state: 'Bitmask, enumerated',
  safety_preset_id: 'Designates the Safety Zone index in [0..63] range used in algo pipe',
  sensor_angle_roll: 'In millidegrees. Relative to X (forward) axis. Positive value is CCW',
  sensor_angle_pitch: 'In millidegrees. Relative to Y (left) axis. Positive value is CCW',
  diagnostic_zone_median_height: 'In millimeters. Relative to the leveled pointcloud CS',
  floor_detection: 'Percentage',
  diagnostic_zone_fill_rate: 'Percentage',
  depth_fill_rate: 'Unsigned value in range of [0..100]. Use [x = 0xFF] if not applicable',
  depth_stdev: 'Spatial accuracy in millimetric units',
  occupancy_grid_rows: 'Number of rows in the grid. Max value is 250 (corresponding to 5M width with 2cm tile)',
  occupancy_grid_columns: 'Number of columns in the grid. Max value is 320 (corresponding to ~6.5M depth with 2cm tile)',
  occupancy_cell_size: 'Edge size of each tile, measured in cm',
  number_of_3d_vertices: 'The max number of points is 640*360',
}

interface Bitmask {
  title: string
  meanings: string[]
  zero: string
}

const repeat = (s: string, n: number) => Array.from({ length: n }, () => s)

const BITMASKS: Record<string, Bitmask> = {
  safety_vision_verdict: { title: 'Depth Visual Safety Verdict:', zero: 'Safe',
    meanings: ['Not Safe', 'Collison(s) in danger zone', 'Collision(s) in warning zone'] },
  safety_hara_events: { title: 'HaRa events:', zero: 'No HaRa events identified', meanings: [
    'HaRa triggers identified', 'Collison(s) in danger zone', 'Collision(s) in warning zone',
    'Depth fill rate in the diagnostic zone is lower than the require confidence level',
    'Depth fill rate in the floor area is lower than the require confidence level',
    'Cliff detection was triggered', 'Depth noise standard deviation is higher than permitted level',
    'Camera posture/floor position critical deviation is detected', 'Safety preset error',
    'Image depth fill Rate is lower than the require confidence level', 'Contious frame drops',
    'Sustained frame drops', 'Frozen depth image (CRC recurrence)', 'FTTI miss (data latency)',
    'Safety & Security check failure'] },
  safety_preset_integrity: { title: 'Preset integrity:', zero: 'Preset integrity identified', meanings: [
    'Preset inconsistency identified', 'Preset CRC check invalid',
    'Discrepancy between actual and expected Preset Id', 'Preset Selection with GPIO is invalidated.'] },
  safety_soc_fusa_action: { title: 'Bitmask, enumerated:', zero: 'No action taken', meanings: ['HKR Reset', 'HKR Shutdown'] },
  safety_mb_fusa_event: { title: 'MB Fusa events:', zero: 'Safe', meanings: [
    'Not safe', 'ADC1 over-voltage', 'ADC1 under-voltage', 'ADC2 over-voltage', 'ADC2 under-voltage', 'ADC3 over-voltage',
    'ADC3 under-voltage', 'ADC4 over-voltage', 'ADC4 under-voltage', 'ADC5 over-voltage', 'ADC5 under-voltage', 'ADC6 over-voltage',
    'ADC6 under-voltage', 'ADC7 over-voltage', 'ADC7 under-voltage', 'PVT0 over-voltage', 'PVT0 under-voltage', 'PVT1 over-voltage',
    'PVT1 under-voltage', 'PVT2 over-voltage', 'PVT2 under-voltage', 'PVT3 over-voltage', 'PVT3 under-voltage', 'PVT4 over-voltage',
    'PVT4 under-voltage', 'PVT5 over-voltage', 'PVT5 under-voltage', 'PVT6 over-voltage', 'PVT6 under-voltage', 'IR L over-voltage',
    'IR L under-voltage', 'Projector L over-voltage', 'Projector L under-voltage', 'IMU sensor over-voltage', 'IMU sensor under-voltage',
    'RGB sensor over-voltage', 'RGB sensor under-voltage', 'IR R over-voltage', 'IR R under-voltage', 'Projector R over-voltage',
    'Projector R under-voltage', 'APM PRIN L over-voltage', 'APM PRIN L under-voltage', 'APM PRIN R over-voltage', 'APM PRIN R under-voltage',
    'Thermal over-voltage', 'Thermal under-voltage', 'Humidity over-voltage', 'Humidity under-voltage', 'SMCU temprature over-voltage',
    'SMCU temprature under-voltage'] },
  safety_soc_gmt_status: { title: 'MB Fusa events:', zero: 'GMT Clock Ok',
    meanings: ['GMT Clock is outside safe threshold', 'GMT Clock is not avaialble'] },
  safety_preset_error_type: { title: 'Safety Preset Error Types:', zero: 'OK', meanings: [
    'ERROR_UNKNOWN', 'ERROR_GRID_CELL_SIZE_OUT_OF_RANGE', 'ERROR_DANGER_ZONE_OUT_OF_FOV', 'ERROR_DANGER_ZONE_INVALID_GEOMETRY',
    'ERROR_WARNING_ZONE_OUT_OF_FOV', 'ERROR_WARNING_ZONE_INVALID_GEOMETRY', 'ERROR_DIAGNOSTIC_ZONE_OUT_OF_FOV',
    'ERROR_DIAGNOSTIC_ZONE_INVALID_GEOMETRY', 'ERROR_MASK_INVALID_GEOMETRY', 'ERROR_MASK_MIN_DISTANCE_OUT_OF_RANGE', 'ERROR_MASK_OUT_OF_FOV',
    'ERROR_ROBOT_HEIGHT_OUT_OF_RANGE', 'ERROR_SURFACE_HEIGHT_OUT_OF_RANGE', 'ERROR_SURFACE_STEEPNESS_OUT_OF_RANGE', 'ERROR_TRANSFORMATION_INVALID'] },
  safety_non_fusa_gpio_out: { title: 'Non-FuSa GPIO Out:', zero: 'OK', meanings: [
    'OSSD2_A_present', 'OSSD2_A status : Raised / Idle', 'OSSD2_B_present', 'OSSD2_B status : Raised / Idle',
    'Device_Ready_present', 'Device_Ready on / off', 'Error signal present', 'Error signal on / off'] },
  safety_non_fusa_gpio_in: { title: 'Non-FuSa GPIO In:', zero: 'OK',
    meanings: ['Interlock_present', 'Interlock_status: Raised / Idle', 'HW_Reset_present', 'HW_Reset status: Raised / Idle'] },
  safety_soc_safety_and_security: { title: 'Soc Safety and Security:', zero: 'None', meanings: [
    'Unit is Locked', 'OHM Serial Numbers check is valid', 'APM Serial Numbers check is valid', 'TBD',
    'Depth calibration data is valid', 'Triggered calibration result is valid', 'Triggered calibration data is valid'] },
  safety_smcu_hw_monitor_status: { title: 'SMCU HW Monitor Status:', zero: 'None', meanings: [
    'Global Notification', ...repeat('SMCU Alarm', 10), 'SMCU Image CRC Check failed', 'LBIST Failure', 'MONBIST Failure',
    'SMU Alive Alarm Failure', 'RegMon CPU0 SRAM Failure', 'MBIST Config0 Failure', 'MBIST Config1 Failure', 'DTS Result Failure',
    'RegMon CPU1 SRAM Failure', 'RegMon CPU2 SRAM Failure', 'RegMon SMU and PLL Failure', 'MCU Startup SFR Test'] },
  safety_smcu_sw_monitor_status: { title: 'SMCU SW Monitor Status:', zero: 'None', meanings: [
    'Global Notification (SW Monitor Ok=0, Fail=1)', 'SW Monitor Status (Ok=0, fail =1)', ...repeat('Keep-Alive failure', 9),
    'SMCU Image CRC Check failed', ...repeat('SafeTpack Startup Tests Failure', 20)] },
  embedded_filters: { title: 'Embedded Filter', zero: 'None', meanings: ['Decimation', 'Spatial', 'Temporal', 'Holes Filling'] },
}

const HEX_KEYS = new Set([
  'safety_vision_verdict', 'safety_hara_events', 'safety_preset_integrity', 'safety_mb_fusa_event', 'safety_mb_fusa_action',
  'safety_soc_fusa_action', 'safety_soc_monitor_l2_error_type', 'safety_soc_monitor_l3_error_type', 'safety_soc_fusa_events',
  'safety_smcu_liveliness', 'safety_smcu_state', 'safety_smcu_debug_status_bitmask', 'safety_smcu_debug_info_bist_status',
  'safety_non_fusa_gpio_out', 'safety_soc_safety_and_security', 'safety_non_fusa_gpio_in', 'safety_smcu_hw_monitor_status',
  'safety_smcu_sw_monitor_status',
])

const SAFETY_MODES = ['Run', 'Standby', 'Service']
const SMCU_STATES = [
  'INIT_STATE', 'TRANSITION_STATE', 'RUN_SAFE_STATE', 'SERVICE_STATE', 'TESTER_STATE', 'DFU_HKR_STATE', 'PAUSE_STATE',
  'WARNING_STATE', 'DANGER_STATE', 'DANGER_ERROR_STATE', 'INTERLOCK_DANGER_STATE', 'NON_CRITICAL_ERROR_STATE',
  'IRRECOVERABLE_LOCK_ERROR_STATE',
]

/** The value as the legacy viewer prints it: hex for bitmasks, names for enum-like fields. */
export function formatMetadataValue(key: string, value: number): string {
  const k = key.toLowerCase()
  if (k === 'safety_operational_mode') return SAFETY_MODES[value] ?? String(value)
  if (k === 'safety_smcu_debug_info_internal_state') return SMCU_STATES[value] ?? String(value)
  if (HEX_KEYS.has(k)) return '0x' + (value >>> 0).toString(16)
  return String(value)
}

/** Set bits of a bitmask attribute spelled out, one line each, or the meaning of zero. */
export function decodeBits(key: string, value: number): string | undefined {
  const mask = BITMASKS[key.toLowerCase()]
  if (!mask) return undefined
  const set = mask.meanings.flatMap((m, i) => (BigInt(Math.trunc(value)) >> BigInt(i)) & 1n ? [`${m} (${i})`] : [])
  return `${mask.title} ${set.length ? set.join(', ') : mask.zero}`
}

/** Tooltip text for an attribute: its description, with bitmask bits decoded. */
export function describeMetadata(key: string, value: number): string | undefined {
  return decodeBits(key, value) ?? DESCRIPTIONS[key.toLowerCase()]
}

// Mirrors the SDK's rsutils::string::make_less_screamy: "ACTUAL_FPS" -> "Actual Fps".
export function lessScreamy(key: string): string {
  return key.split('_').map((w) => w.charAt(0).toUpperCase() + w.slice(1).toLowerCase()).join(' ')
}

/**
 * Display name for an attribute. Depth-mapping cameras report auto white balance in the
 * "manual" field too, so the legacy viewer drops the qualifier for them.
 */
export function metadataLabel(key: string, depthMapping = false): string {
  const label = lessScreamy(key)
  return depthMapping && label === 'Manual White Balance' ? 'White Balance' : label
}

/** The depth-mapping family (D5xx) gets the safety decoders and the white-balance rename. */
export function isDepthMappingDevice(deviceName: string | undefined): boolean {
  return /\bD5\d\d/.test(deviceName ?? '')
}
