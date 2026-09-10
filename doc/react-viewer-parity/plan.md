# React Viewer Parity — Program Plan

> **For agentic workers:** This is a program-level plan: phases → work packages (WP). Each WP
> is sized for its own detailed TDD implementation plan (`superpowers:writing-plans`) at
> execution time, executed via `superpowers:subagent-driven-development` or
> `superpowers:executing-plans`. Do not implement from this document alone. Checkboxes track
> WP completion.

**Goal:** Close every Tier A/B gap in [design.md §3](design.md#3-gap-matrix) so the React
viewer can replace `realsense-viewer`.

**Architecture:** Approach A from the design: keep FastAPI + pyrealsense2 + React; split
`rs_manager.py` into feature services; add a generic job/progress pattern and settings
persistence; deliver each gap cluster as backend service + endpoint + UI slice with mocked
tests and a live E2E.

**Tech stack:** Python 3.10+ / FastAPI / python-socketio / aiortc / pyrealsense2 (repo
build); React 18 / TypeScript / Vite / Zustand / three.js (`@react-three/fiber`) /
Tailwind; Vitest + MSW, Playwright, pytest; Tauri.

**Conventions for every WP** (from design §5.1):
- New endpoint module + service module + pydantic model; mutation returns applied state.
- Blocking SDK calls in `run_in_threadpool`; anything >1 s through `services/jobs.py`.
- HTTP-layer pytest with the pyrealsense2 mock + `tests/live/test_<wp>.py`.
- Vitest for the store slice and component; `real-device.spec.ts` scenario.
- Branch per WP off `development`, PR to `development`, `Tracked on` Jira line
  (`.github/skills/pr-create.md`). Flip the design gap-matrix row to Done in the same PR.

Effort is engineer-days, single engineer, excluding review. Hardware column = camera
needed for the live test.

---

## Phase 0 — Land in-flight work (prereq, ~3 d)

- [ ] **WP0.1 Finish PR #15402** (`origin/react-viewer-control-parity`). Live test
  advanced-mode toggle off→on (device restart, controls refetch). Address review from
  RSDSO-21748. Merge. HW: D455. 1 d.
- [ ] **WP0.2 Merge PR #15559** (theme + IMU history graphs + orientation wireframe).
  Resolve conflicts with WP0.1 in `DevicePanel.tsx`, `store/index.ts`. 1 d.
- [ ] **WP0.3 Merge PR #15662** (Windows Tauri build). 0.5 d.
- [ ] **WP0.4 Close stale Jira**: RSDEV-12682/12721/12698 are merged; update RSDSO-21750
  description with a link to `doc/react-viewer-parity/design.md`. 0.5 d.

## Phase 1 — Foundation (~14 d)

Enables everything after it. No user-visible parity except settings and multi-cam fixes.

- [ ] **WP1.1 Split `rs_manager.py`** (RSDEV-12009). Move code into
  `services/devices.py` (registry, hot-plug, info, reset, DFU wait helpers),
  `services/streaming.py` (per-sensor open/start/stop, frame queues, colorizer, filter
  chain, `wait_for_frame_after`), leave `rs_manager.py` as a façade that composes them so
  endpoints keep working. Pure move + tests green; no behaviour change. Delete the
  pipeline-based `/stream/*` code path only if WP1.5 confirms the viewer never uses it
  (it does not today; tests do). 4 d.
- [ ] **WP1.2 Jobs service.** `services/jobs.py`: `create(kind, device_id) -> Job`,
  `Job.progress(pct, msg)`, `Job.done(result)`, `Job.fail(err)`, `cancel()`; emits socket
  `job_<id>` `{state, progress, message, result}`; `GET /jobs/{id}`, `GET /jobs?device=`.
  Migrate firmware update to it (keep old event names as aliases for one release).
  Frontend `store/jobs.ts` + `components/jobs/JobProgressModal.tsx` generalising
  `FirmwareProgressModal.tsx`. 2 d.
- [ ] **WP1.3 Settings service.** `services/settings.py` reading/writing
  `~/.realsense/rest-api-settings.json` with defaults; `GET/PUT /settings` (partial merge);
  keys defined in a pydantic model (`record.*`, `update.sw_update_url`,
  `update.sw_update_official_server`, `console.max_entries`, `viewer.hwlogger_xml`,
  `viewer.commands_xml`, `context.dds.enabled|domain`, `calibration.enable_writing`,
  `post_processing.performance_mode`, `viewer.metric_system`). Frontend
  `store/settings.ts` + `components/settings/SettingsDialog.tsx` with tabs
  Playback&Record / General / Online (Performance tab dropped, Tier C). Units toggle
  wired into depth readouts. 2.5 d.
- [ ] **WP1.4 Store slices + persist.** Split `store/index.ts` into
  `store/{devices,streaming,controls,view,jobs,settings,chat}.ts`; add `persist` for
  `viewMode`, expanded sections, tile order per serial (used in WP2.2), snoozed
  notifications (WP7.4). Tests moved with the slices. 2 d.
- [ ] **WP1.5 Remove "Activate device"** (RSDEV-12007). All connected devices show
  sensors immediately as in legacy; auto-open first device; `LoadingSplash` per device.
  Requires WP1.1. 1.5 d.
- [ ] **WP1.6 Multi-camera hardening** (RSDEV-12011). Reproduce black second stream, fix
  WebRTC session/track lifecycle per device, metadata keyed by device; live test with two
  D4xx. HW: 2× D4xx. 1.5 d.
- [ ] **WP1.7 Default profiles** (RSDEV-14130). Server marks `is_default` on profiles
  (`stream_profile.is_default()`); client initial selection uses them; fall back to
  current heuristic. 0.5 d.
- [ ] **WP1.8 Single-port serve + trailing-slash cleanup.** Mount `static/` (from
  `scripts/bundle-for-prod.js`) with `StaticFiles(html=True)` when present; fix the nine
  client paths that rely on 307 redirects (`client.ts:199-275`). 0.5 d.
- [ ] **WP1.9 Live E2E harness on LibCI** (RSDEV-13683). Jenkins job runs
  `REAL_DEVICE=true npx playwright test --project=real-device` against a bench D455;
  `tests/live` pytest in the same job. Every later WP adds to these suites. 1.5 d.

## Phase 2 — 2D streaming and controls parity (~22 d)

- [ ] **WP2.1 Stream pause + overlays.** `POST /sensors/{s}/pause|resume` (server holds
  last frame, WebRTC keeps sending it); Space pauses all incl. playback; tile overlays for
  Paused, "No frames received" (server `last_frame_ts` stale >2 s), unsupported format.
  `utils/shortcuts.ts` created here. 2 d.
- [ ] **WP2.2 Tile layout parity.** Default order depth→color→IR→motion; drag-to-swap;
  maximize/restore single tile; F8 fullscreen; order persisted per serial (WP1.4). 2 d.
- [ ] **WP2.3 Snapshot.** `services/snapshot.py`, `POST /sensors/{s}/snapshot?stream=`
  returning a zip (PNG colorized via cached colorizer, `.raw`, `_metadata.csv`; motion/pose
  `.csv`) matching `stream-model.cpp:1927-2038` naming. Tile header button. 1.5 d.
- [ ] **WP2.4 Depth readouts.** Hover readout in m/mm/ft per units setting, mm under
  20 cm; batch hover via socket instead of REST per move; max-usable-range readout
  (`max_usable_range_sensor`) when option on; colormap ruler ticks (`DepthLegend`) sampled
  from colorizer min/max. 2 d.
- [ ] **WP2.5 Zoom/pan + grid overlay.** Wheel zoom about cursor, drag pan, preview inset;
  configurable crosshair/grid (lines, width, color) persisted. Client-only. 2 d.
- [ ] **WP2.6 Metadata table parity.** Per-attribute descriptions, hex for bitmask
  fields, DDS/safety decoders ported from `stream-model.cpp:1238-1470` into
  `utils/metadataDecoders.ts` with unit tests. 1.5 d.
- [ ] **WP2.7 Option UX parity.** Tooltips from `description`; click-to-type exact value
  on sliders; legacy control ordering (`device-model.cpp:2776-2832`) in `ControlSection`;
  drag coalescing (send latest value at most every 200 ms, last-wins) matching
  `option-model.h` dispatcher. 2 d.
- [ ] **WP2.8 Device-pushed option changes.** Server registers `sensor.on_options_changed`
  per sensor → socket `options_changed {device, sensor, options[]}`; store patches groups.
  HW: D455. 1 d.
- [ ] **WP2.9 AE ROI.** `GET/PUT /sensors/{s}/roi` via `roi_sensor`; drag rectangle on
  tile, reset to full frame, default centre 3/4; algo ROI overlay toggle. HW: D455. 1.5 d.
- [ ] **WP2.10 HDR tool.** `GET/PUT /devices/{d}/hdr` exposing sequence size, per-item
  exposure/gain, enable; UI dialog mirroring `hdr-model.*` (load/save JSON, load from
  device, apply). HW: D455 with HDR FW. 2.5 d.
- [ ] **WP2.11 Stream config parity.** Per-stream FPS when no common FPS; mixed
  depth/IR resolutions; client-side unsupported-combination guard using profile list;
  device-info panel shows all `RS2_CAMERA_INFO_*`. 2 d.
- [ ] **WP2.12 Sync toggle + align.** `PUT /devices/{d}/sync {bool}` builds `rs.syncer`
  across streaming sensors in `streaming.py`; `PUT /devices/{d}/align {stream|none}`
  applies `rs.align` in the per-sensor path. UI device toggle + 2D align selector. 2 d.
- [ ] **WP2.13 Filter defaults parity.** Match `subdevice-model.cpp:285-321` (default-on
  set, D405 threshold 0.05–4 m, HDR merge only with `SEQUENCE_ID`), honouring
  `post_processing.performance_mode`; persist filter state server-side per serial. 1 d.

## Phase 3 — Record & playback (~10 d, RSDEV-9242)

- [ ] **WP3.1 Record.** `services/record.py`; `POST /devices/{d}/record/start {path?}`,
  `pause`, `resume`, `stop`, `GET status`; auto-name vs ask + default folder + compression
  from settings; gated on streaming; REC overlay on tiles; device-card button. HW: D455.
  2.5 d.
- [ ] **WP3.2 Playback load/unload.** `services/playback.py`; `POST /playback/load`
  (multipart or `{path}`) → `context.load_device`; playback device listed with
  `is_playback`, `file_name`; `DELETE /playback/{d}`; drag-and-drop onto the app; Tauri
  native open dialog. HW: none (uses recorded file). 2.5 d.
- [ ] **WP3.3 Transport.** `play|pause|stop|seek|speed|step|repeat` + `GET status`;
  `set_status_changed_callback` → socket `playback_status`; server-side repeat like
  `realsense-viewer.cpp:59-115`; `components/playback/Transport.tsx` (step buttons, seek
  bar with hh:mm:ss.mmm, speed combo x0.25–x2, repeat, info). 3 d.
- [ ] **WP3.4 Playback E2E.** Record 5 s on D455 → load → play → seek → step → loop, in
  `real-device.spec.ts` and `tests/live/test_playback.py`. 1 d.
- [ ] **WP3.5 (stretch) Legacy .bag conversion.** Detect ROS1 bag, offer conversion via
  `rs-convert` subprocess if found on PATH. 1 d.

## Phase 4 — 3D parity (~16 d, RSDEV-11580)

- [ ] **WP4.1 Intrinsics + depth transport.** `GET /devices/{d}/intrinsics?stream=`;
  lossless Z16 delivery (spike: 16-bit-in-RGB over WebRTC vs binary WebSocket; pick by
  measured latency and fidelity on D455). 3 d.
- [ ] **WP4.2 GPU unprojection.** Fragment/vertex shader unprojects depth texture with
  intrinsics + depth units; removes base64 point cloud path (kept behind a flag one
  release). Target: 30 fps at 1280×720. 3 d.
- [ ] **WP4.3 Texture + depth source, sync lock.** `PUT /point_cloud/texture {stream}`;
  client maps selected WebRTC track as texture; auto-switch on new non-Y8 stream; depth
  source picker for multi-cam; lock/unlock. 2 d.
- [ ] **WP4.4 Scene furniture.** Ground grid (metric/imperial), axes, frustum from
  intrinsics at 1/3/5 m, reset viewport (R), WASD fly, camera model mesh optional. 2 d.
- [ ] **WP4.5 Shading modes + occlusion.** Points / flat mesh (index buffer from depth
  grid) / diffuse; `PUT /point_cloud/occlusion`. 2 d.
- [ ] **WP4.6 Measurement.** Raycast pick on the unprojected mesh; click-click distance;
  Shift chains polygon with area; Z undo; labels in units setting. 2.5 d.
- [ ] **WP4.7 Export PLY (server).** `POST /point_cloud/export {mesh, normals, binary}` →
  job → download via `rs.save_to_ply`; remove client ASCII exporter. 1.5 d.

## Phase 5 — Presets and device modes (~5 d)

- [ ] **WP5.1 JSON presets.** `services/presets.py`; `GET /presets` (folder + enum),
  `GET /presets/current` (download, includes `viewer` section), `POST /presets/load`
  (upload); 409 `advanced_mode_required` → UI toggle prompt; sets preset Custom and
  refetches controls. HW: D455. 2.5 d.
- [ ] **WP5.2 Sensor config mode switch.** Expose `SENSORS_CONFIG_MODE` as a device-menu
  action (dual-RGB / dedicated-RGB) that writes then hardware-resets and waits for
  re-enumeration (reuse DFU wait helpers). HW: D585 2C. 1.5 d.
- [ ] **WP5.3 Stream exclusion rules.** Port D401-GMSL dual-RGB/IR exclusivity
  (`subdevice-model.h:286-306`) into `StreamConfig` as data-driven rules keyed by PID. 1 d.

## Phase 6 — Calibration (~18 d, RSDEV-9262)

- [ ] **WP6.1 Calibration job core.** `services/calibration.py` with workspace
  save/restore (stop other sensors, force depth profile, laser/thermal off, restore);
  `POST /devices/{d}/calibration/occ {speed, accuracy, scan, host_assist, dry_run}` → job;
  result `{health[], new_table_id}`; `POST /calibration/apply {keep|new}`;
  `POST /calibration/cancel`. HW: D455. 4 d.
- [ ] **WP6.2 OCC + dry-run wizard UI.** `components/calibration/OccWizard.tsx`: params,
  progress, health with before/after and colour thresholds, Apply / Keep / Recalibrate;
  disclaimer notice with docs links. 3 d.
- [ ] **WP6.3 Tare + ground truth.** `POST /calibration/tare {ground_truth_mm, ...}`,
  `POST /calibration/ground_truth {target_w, target_h}` (`calculate_target_z`), UI with
  retry. HW: D455 + target. 2.5 d.
- [ ] **WP6.4 Focal length, UV mapping, FL-plus.** Three more flows on the same job core;
  UI forms per `on-chip-calib.cpp:2018-2452`; FL limitation notice. HW: D455 + target.
  3 d.
- [ ] **WP6.5 D500 OCC interactive.** `POST /calibration/d500 {action: run|dry_run|abort|
  try_new|try_old|commit}` with the `rect_health` screen. HW: D555/D585. 2.5 d.
- [ ] **WP6.6 Calibration tables.** `GET/PUT /calibration/table`, `POST
  /calibration/reset_factory`, write gate from settings; `CalibrationTableEditor.tsx`
  (intrinsics, 3×3, distortion, per-resolution). 2.5 d.
- [ ] **WP6.7 Recommend-calibration notice** with snooze (WP7.4 dependency). 0.5 d.

## Phase 7 — Firmware, updates, notifications (~9 d)

- [ ] **WP7.1 DFU-stuck recovery** (RSDEV-11404). List recovery-mode devices
  (`update_device`), offer FW upload directly; UI card state. HW: D4xx in recovery. 2 d.
- [ ] **WP7.2 Unsigned FW.** `POST /firmware/update_unsigned` for unlocked D400;
  menu item only when `updatable.check_firmware_compatibility` allows. HW: unlocked
  D400. 1 d.
- [ ] **WP7.3 Updates service.** `services/updates.py` parses SW + FW from versions DB,
  essential/recommended; custom URL / `file://` from settings; `GET /updates/{d}`;
  `components/updates/UpdatesDialog.tsx`; up-to-date + recommended notifications. 2.5 d.
- [ ] **WP7.4 Notification center.** Server `services/notifications.py` subscribes
  `set_notifications_callback` → socket `notification`; client
  `NotificationCenter.tsx` with expand/dismiss/snooze (once / N days / never) persisted;
  error dialog with "don't show again"; Report Issue prefilled GitHub link; RS Store link;
  About dialog with license. HW: D455 for a hardware event. 3.5 d.

## Phase 8 — Console, logs, terminal (~9 d)

- [ ] **WP8.1 SDK log stream.** `services/logs.py`: `rs.log_to_callback` at startup →
  bounded deque + socket `log`; `GET /logs?since=`; log-to-file/severity from settings.
  `components/console/OutputConsole.tsx`: bottom panel, severity counters as filters,
  search, copy line/all, save as, max entries, Esc closes. 3 d.
- [ ] **WP8.2 FW logs.** `POST /devices/{d}/fw_logs/start|stop` (thread:
  `start_collecting`, `get_firmware_log`, `parse_log` with XML from settings), `POST
  /fw_logs/flash`; console toggle + "Recover logs from flash" menu item. HW: D455. 2 d.
- [ ] **WP8.3 Terminal.** `services/terminal.py` (`terminal_parser` with `Commands.xml`
  path from settings) wrapping `/hwm`; `POST /devices/{d}/terminal {line}`, `GET
  /terminal/commands`; console command line with history (Up/Down), Tab completion,
  `clear`, raw hex. HW: D455. 2 d.
- [ ] **WP8.4 (stretch) Dashboards.** Frame-drops/s and processing-vs-camera rate charts
  from metadata stream (`recharts`, already a dependency). 1.5 d.
- [ ] **WP8.5 (stretch) IR reflectivity readout.** Port `common/reflectivity` estimator
  to `streaming.py`, expose in metadata payload. 1.5 d.

## Phase 9 — D500 / DDS features (~9 d, RSDEV-9262)

- [ ] **WP9.1 Embedded filters.** Feature-detect `rs.embedded_filter`; `GET
  /sensors/{s}/embedded_filters`, `PUT /embedded_filters/{type}` (whole composite struct,
  atomic); UI editors for DPP decimation / temporal / HDRD / close range with reset and
  arrow nudges; Temporal DPP sensor panel. HW: D555/D585. 3.5 d.
- [ ] **WP9.2 DDS context settings.** `context.dds.enabled|domain` from settings applied
  at server start; UI shows "restart required"; `--domain-id` parity for `main.py`. HW:
  D555 over Ethernet. 1 d.
- [ ] **WP9.3 Ethernet config dialog.** `GET/PUT /devices/{d}/eth_config`
  (`eth_config_device`); dialog per `dds-model.h:15-64` incl. reset-to-default. HW: D555.
  2 d.
- [ ] **WP9.4 Safety / labeled point cloud.** Occupancy polygons in 2D, labeled points
  with size selector and 3D safety zones (`labeled_points` binding); perception stream
  exclusion rules. HW: D585 depth-mapping. 2.5 d.

## Phase 10 — Packaging and platform (~8 d)

- [ ] **WP10.1 Tauri 2 migration** (unblocks Ubuntu 24.04/26.04). 2 d.
- [ ] **WP10.2 Installer + release flow** (RSDEV-14434, RSDEV-9390): Windows installer
  bundles REST API + viewer; Linux `.deb`/AppImage; Jenkins publish. 3 d.
- [ ] **WP10.3 Platform matrix** (RSDEV-13547): Python 3.9–3.14, Jetson/aarch64. 2 d.
- [ ] **WP10.4 CMake hook.** `BUILD_REST_API` option that builds the Python wheel + viewer
  bundle into `build/` and installs alongside `realsense-viewer`. 1 d.

---

## Totals and sequencing

| Phase | Days | Depends on |
|---|---|---|
| 0 In-flight | 3 | — |
| 1 Foundation | 14 | 0 |
| 2 2D + controls | 22 | 1 |
| 3 Record/playback | 10 | 1 (1.1, 1.2, 1.3) |
| 4 3D | 16 | 1 |
| 5 Presets/modes | 5 | 1 |
| 6 Calibration | 18 | 1, 2.1 (pause), 2.12 (streaming.py sync) |
| 7 FW/updates/notifications | 9 | 1 |
| 8 Console/logs/terminal | 9 | 1 |
| 9 D500/DDS | 9 | 1, 2 |
| 10 Packaging | 8 | any time after 1 |
| **Total** | **~123 d** (~6 months single engineer; ~3 months with two after Phase 1) | |

Phases 3, 4, 5, 7, 8 are independent after Phase 1 and can run in parallel. Recommended
order for one engineer: 0 → 1 → 2 → 3 → 5 → 7 → 8 → 4 → 6 → 9 → 10, so the most-used
legacy workflows (streaming, controls, record/playback, presets, logs) land first and the
long-tail hardware-specific flows (calibration, D500) last.

## Verification gate before Phase 2 (needs a camera on this machine)

Run with a D455 connected, server from `wrappers/rest-api` using the repo-built
`pyrealsense2` in `build/Release`:

1. PR #15402 branch: toggle advanced mode off→on via the UI, confirm reconnect and that
   `GET /advanced_mode/controls/` repopulates. Closes WP0.1.
2. Python REPL spot checks that fix estimates for Phases 2/3/8:
   `sensor.on_options_changed` fires on preset change; `sensor.set_notifications_callback`
   delivers a hardware event on laser toggle; `rs.recorder` → `context.load_device` →
   `sensor.open/start` on the playback device works with the per-sensor path;
   `firmware_logger.start_collecting` + `init_parser(xml)`; `terminal_parser` with
   `common/Commands.xml`-style file; `roi_sensor.get_region_of_interest`.
3. Two D4xx: reproduce RSDEV-12011 for WP1.6.

D555/D585 checks (Phase 9, WP5.2, WP6.5) can wait until those phases start.
