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

- [x] **WP0.1 Finish PR #15402** (`origin/react-viewer-control-parity`). Live test
  advanced-mode toggle off→on (device restart, controls refetch). Address review from
  RSDSO-21748. Merge. HW: D455. 1 d.
- [ ] **WP0.2 Merge PR #15559** (theme + IMU history graphs + orientation wireframe).
  Resolve conflicts with WP0.1 in `DevicePanel.tsx`, `store/index.ts`. 1 d.
- [ ] **WP0.3 Merge PR #15662** (Windows Tauri build). 0.5 d.
- [ ] **WP0.4 Close stale Jira**: RSDEV-12682/12721/12698 are merged; update RSDSO-21750
  description with a link to `doc/react-viewer-parity/design.md`. 0.5 d.

## Phase 1 — Foundation (~14 d)

Enables everything after it. No user-visible parity except settings and multi-cam fixes.

- [x] **WP1.1 Split `rs_manager.py`** (RSDEV-12009). Move code into
  `services/devices.py` (registry, hot-plug, info, reset, DFU wait helpers),
  `services/streaming.py` (per-sensor open/start/stop, frame queues, colorizer, filter
  chain, `wait_for_frame_after`), leave `rs_manager.py` as a façade that composes them so
  endpoints keep working. Pure move + tests green; no behaviour change. Delete the
  pipeline-based `/stream/*` code path only if WP1.5 confirms the viewer never uses it
  (it does not today; tests do). 4 d.
- [x] **WP1.2 Jobs service.** `services/jobs.py`: `create(kind, device_id) -> Job`,
  `Job.progress(pct, msg)`, `Job.done(result)`, `Job.fail(err)`, `cancel()`; emits socket
  `job_<id>` `{state, progress, message, result}`; `GET /jobs/{id}`, `GET /jobs?device=`.
  Migrate firmware update to it (keep old event names as aliases for one release).
  Frontend `store/jobs.ts` + `components/jobs/JobProgressModal.tsx` generalising
  `FirmwareProgressModal.tsx`. 2 d.
- [x] **WP1.3 Settings service.** `services/settings.py` reading/writing
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
- [x] **WP1.5 Remove "Activate device"** (RSDEV-12007). All connected devices show
  sensors immediately as in legacy; auto-open first device; `LoadingSplash` per device.
  Requires WP1.1. 1.5 d.
- [ ] **WP1.6 Multi-camera hardening** (RSDEV-12011). Reproduce black second stream, fix
  WebRTC session/track lifecycle per device, metadata keyed by device; live test with two
  D4xx. HW: 2× D4xx. 1.5 d.
- [x] **WP1.7 Default profiles** (RSDEV-14130). Server marks `is_default` on profiles
  (`stream_profile.is_default()`); client initial selection uses them; fall back to
  current heuristic. 0.5 d.
- [x] **WP1.8 Single-port serve + trailing-slash cleanup.** Mount `static/` (from
  `scripts/bundle-for-prod.js`) with `StaticFiles(html=True)` when present; fix the nine
  client paths that rely on 307 redirects (`client.ts:199-275`). 0.5 d.
- [ ] **WP1.9 Live E2E harness on LibCI** (RSDEV-13683). Jenkins job runs
  `REAL_DEVICE=true npx playwright test --project=real-device` against a bench D455;
  `tests/live` pytest in the same job. Every later WP adds to these suites. 1.5 d.

## Phase 2 — 2D streaming and controls parity (~22 d)

- [x] **WP2.1 Stream pause + overlays.** `POST /sensors/{s}/pause|resume` (server holds
  last frame, WebRTC keeps sending it); Space pauses all incl. playback; tile overlays for
  Paused, "No frames received" (server `last_frame_ts` stale >2 s), unsupported format.
  `utils/shortcuts.ts` created here. 2 d.
- [x] **WP2.2 Tile layout parity.** Default order depth→color→IR→motion; drag-to-swap;
  maximize/restore single tile; F8 fullscreen; order persisted per serial (WP1.4). 2 d.
- [x] **WP2.3 Snapshot.** `services/snapshot.py`, `POST /sensors/{s}/snapshot?stream=`
  returning a zip (PNG colorized via cached colorizer, `.raw`, `_metadata.csv`; motion/pose
  `.csv`) matching `stream-model.cpp:1927-2038` naming. Tile header button. 1.5 d.
- [x] **WP2.4 Depth readouts.** Hover readout in m/mm/ft per units setting, mm under
  20 cm; batch hover via socket instead of REST per move; max-usable-range readout
  (`max_usable_range_sensor`) when option on; colormap ruler ticks (`DepthLegend`) sampled
  from colorizer min/max. 2 d.
- [x] **WP2.5 Zoom/pan + grid overlay.** Wheel zoom about cursor, drag pan, preview inset;
  configurable crosshair/grid (lines, width, color) persisted. Client-only. 2 d.
- [x] **WP2.6 Metadata table parity.** Per-attribute descriptions, hex for bitmask
  fields, DDS/safety decoders ported from `stream-model.cpp:1238-1470` into
  `utils/metadataDecoders.ts` with unit tests. 1.5 d.
- [x] **WP2.7 Option UX parity.** Tooltips from `description`; click-to-type exact value
  on sliders; legacy control ordering (`device-model.cpp:2776-2832`) in `ControlSection`;
  drag coalescing (send latest value at most every 200 ms, last-wins) matching
  `option-model.h` dispatcher. 2 d.
- [x] **WP2.8 Device-pushed option changes.** Server registers `sensor.on_options_changed`
  per sensor → socket `options_changed {device, sensor, options[]}`; store patches groups.
  HW: D455. 1 d.
- [x] **WP2.9 AE ROI.** `GET/PUT /sensors/{s}/roi` via `roi_sensor`; drag rectangle on
  tile, reset to full frame, default centre 3/4; algo ROI overlay toggle. HW: D455. 1.5 d.
- [x] **WP2.10 HDR tool.** `GET/PUT /devices/{d}/hdr` exposing sequence size, per-item
  exposure/gain, enable; UI dialog mirroring `hdr-model.*` (load/save JSON, load from
  device, apply). HW: D455 with HDR FW. 2.5 d.
- [x] **WP2.11 Stream config parity.** Per-stream FPS when no common FPS; mixed
  depth/IR resolutions; client-side unsupported-combination guard using profile list;
  device-info panel shows all `RS2_CAMERA_INFO_*`. 2 d.
- [ ] **WP2.12 Sync toggle + align.** `PUT /devices/{d}/sync {bool}` builds `rs.syncer`
  across streaming sensors in `streaming.py`; `PUT /devices/{d}/align {stream|none}`
  applies `rs.align` in the per-sensor path. UI device toggle + 2D align selector. 2 d.
- [x] **WP2.13 Filter defaults parity.** Match `subdevice-model.cpp:285-321` (default-on
  set, D405 threshold 0.05–4 m, HDR merge only with `SEQUENCE_ID`), honouring
  `post_processing.performance_mode`; persist filter state server-side per serial. 1 d.

## Phase 3 — Record & playback (~10 d, RSDEV-9242)

- [x] **WP3.1 Record.** `services/record.py`; `POST /devices/{d}/record/start {path?}`,
  `pause`, `resume`, `stop`, `GET status`; auto-name vs ask + default folder + compression
  from settings; gated on streaming; REC overlay on tiles; device-card button. HW: D455.
  2.5 d.
- [x] **WP3.2 Playback load/unload.** `services/playback.py`; `POST /playback/load`
  (multipart or `{path}`) → `context.load_device`; playback device listed with
  `is_playback`, `file_name`; `DELETE /playback/{d}`; drag-and-drop onto the app; Tauri
  native open dialog. HW: none (uses recorded file). 2.5 d.
- [x] **WP3.3 Transport.** `play|pause|stop|seek|speed|step|repeat` + `GET status`;
  `set_status_changed_callback` → socket `playback_status`; server-side repeat like
  `realsense-viewer.cpp:59-115`; `components/playback/Transport.tsx` (step buttons, seek
  bar with hh:mm:ss.mmm, speed combo x0.25–x2, repeat, info). 3 d.
- [x] **WP3.4 Playback E2E.** Record 5 s on D455 → load → play → seek → step → loop, in
  `real-device.spec.ts` and `tests/live/test_playback.py`. 1 d.
- [ ] **WP3.5 (stretch) Legacy .bag conversion.** Detect ROS1 bag, offer conversion via
  `rs-convert` subprocess if found on PATH. 1 d.

## Phase 4 — 3D parity (~16 d, RSDEV-11580)

- [x] **WP4.1 Intrinsics + depth transport.** `GET /devices/{d}/intrinsics?stream=`;
  lossless Z16 delivery (spike: 16-bit-in-RGB over WebRTC vs binary WebSocket; pick by
  measured latency and fidelity on D455). 3 d.
- [x] **WP4.2 GPU unprojection.** Fragment/vertex shader unprojects depth texture with
  intrinsics + depth units; removes base64 point cloud path (kept behind a flag one
  release). Target: 30 fps at 1280×720. 3 d.
- [ ] **WP4.3 Texture + depth source, sync lock.** `PUT /point_cloud/texture {stream}`;
  client maps selected WebRTC track as texture; auto-switch on new non-Y8 stream; depth
  source picker for multi-cam; lock/unlock. 2 d.
- [x] **WP4.4 Scene furniture.** Ground grid (metric/imperial), axes, frustum from
  intrinsics at 1/3/5 m, reset viewport (R), WASD fly, camera model mesh optional. 2 d.
- [x] **WP4.5 Shading modes + occlusion.** Points / flat mesh (index buffer from depth
  grid) / diffuse; `PUT /point_cloud/occlusion`. 2 d.
- [x] **WP4.6 Measurement.** Raycast pick on the unprojected mesh; click-click distance;
  Shift chains polygon with area; Z undo; labels in units setting. 2.5 d.
- [x] **WP4.7 Export PLY (server).** `POST /point_cloud/export {mesh, normals, binary}` →
  job → download via `rs.save_to_ply`; remove client ASCII exporter. 1.5 d.

## Phase 5 — Presets and device modes (~5 d)

- [x] **WP5.1 JSON presets.** `services/presets.py`; `GET /presets` (folder + enum),
  `GET /presets/current` (download, includes `viewer` section), `POST /presets/load`
  (upload); 409 `advanced_mode_required` → UI toggle prompt; sets preset Custom and
  refetches controls. HW: D455. 2.5 d.
- [ ] **WP5.2 Sensor config mode switch.** Expose `SENSORS_CONFIG_MODE` as a device-menu
  action (dual-RGB / dedicated-RGB) that writes then hardware-resets and waits for
  re-enumeration (reuse DFU wait helpers). HW: D585 2C. 1.5 d.
- [ ] **WP5.3 Stream exclusion rules.** Port D401-GMSL dual-RGB/IR exclusivity
  (`subdevice-model.h:286-306`) into `StreamConfig` as data-driven rules keyed by PID. 1 d.

## Phase 6 — Calibration (~18 d, RSDEV-9262)

- [x] **WP6.1 Calibration job core.** `services/calibration.py` with workspace
  save/restore (stop other sensors, force depth profile, laser/thermal off, restore);
  `POST /devices/{d}/calibration/occ {speed, accuracy, scan, host_assist, dry_run}` → job;
  result `{health[], new_table_id}`; `POST /calibration/apply {keep|new}`;
  `POST /calibration/cancel`. HW: D455. 4 d.
- [x] **WP6.2 OCC + dry-run wizard UI.** `components/calibration/OccWizard.tsx`: params,
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
- [x] **WP6.6 Calibration tables.** `GET/PUT /calibration/table`, `POST
  /calibration/reset_factory`, write gate from settings; `CalibrationTableEditor.tsx`
  (intrinsics, 3×3, distortion, per-resolution). 2.5 d.
- [x] **WP6.7 Recommend-calibration notice** with snooze (WP7.4 dependency). 0.5 d. Legacy has
  the notice compiled out (device-model.cpp: "do not pre-emptively suggest auto-calibration");
  parity is the inert `update.recommend_calibration` setting, which the Settings dialog keeps.

## Phase 7 — Firmware, updates, notifications (~9 d)

- [ ] **WP7.1 DFU-stuck recovery** (RSDEV-11404). List recovery-mode devices
  (`update_device`), offer FW upload directly; UI card state. HW: D4xx in recovery. 2 d.
- [ ] **WP7.2 Unsigned FW.** `POST /firmware/update_unsigned` for unlocked D400;
  menu item only when `updatable.check_firmware_compatibility` allows. HW: unlocked
  D400. 1 d.
- [x] **WP7.3 Updates service.** `services/updates.py` parses SW + FW from versions DB,
  essential/recommended; custom URL / `file://` from settings; `GET /updates/{d}`;
  `components/updates/UpdatesDialog.tsx`; up-to-date + recommended notifications. 2.5 d.
- [x] **WP7.4 Notification center.** Server `services/notifications.py` subscribes
  `set_notifications_callback` → socket `notification`; client
  `NotificationCenter.tsx` with expand/dismiss/snooze (once / N days / never) persisted;
  error dialog with "don't show again"; Report Issue prefilled GitHub link; RS Store link;
  About dialog with license. HW: D455 for a hardware event. 3.5 d.

## Phase 8 — Console, logs, terminal (~9 d)

- [x] **WP8.1 SDK log stream.** `services/logs.py`: `rs.log_to_callback` at startup →
  bounded deque + socket `log`; `GET /logs?since=`; log-to-file/severity from settings.
  `components/console/OutputConsole.tsx`: bottom panel, severity counters as filters,
  search, copy line/all, save as, max entries, Esc closes. 3 d.
- [x] **WP8.2 FW logs.** `POST /devices/{d}/fw_logs/start|stop` (thread:
  `start_collecting`, `get_firmware_log`, `parse_log` with XML from settings), `POST
  /fw_logs/flash`; console toggle + "Recover logs from flash" menu item. HW: D455. 2 d.
- [x] **WP8.3 Terminal.** `services/terminal.py` (`terminal_parser` with `Commands.xml`
  path from settings) wrapping `/hwm`; `POST /devices/{d}/terminal {line}`, `GET
  /terminal/commands`; console command line with history (Up/Down), Tab completion,
  `clear`, raw hex. HW: D455. 2 d.
- [x] **WP8.4 (stretch) Dashboards.** Frame-drops/s and processing-vs-camera rate charts
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
- [x] **WP10.4 CMake hook.** `BUILD_REST_API` option that builds the Python wheel + viewer
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

## Progress log

- 2026-09-11: Phase 0 (WP0.1 live-verified; #15559/#15662 are other people's PRs), Phase 1
  except WP1.4 (store slices deferred: new features already land as separate stores under
  `src/store/*.ts`; the legacy `store/index.ts` split waits for PR #15559 to merge), WP1.6
  (needs a second camera) and WP1.9 (Jenkins/LibCI infrastructure). Phase 2 done except
  WP2.5 (zoom/pan/grid), WP2.10 (HDR tool: the D455 firmware here has no `hdr-preset`
  section, so it can only be mock-tested), WP2.12 (sync/align needs the frame pump to
  route through `rs.syncer`; deferred). Phase 3 WP3.1-3.3 done and live-verified on the
  D455 (WP3.4 E2E scenario pending; WP3.5 stretch not started).
- Finding: the SDK's `on_options_changed` watcher wedges the D455 on Windows when more than
  one thread touches options; the server polls options itself (see design §7).
- Finding: recording in SDK 2.59 writes ROS2 `.db3`; `.bag` files play back but cannot be
  written.
- 2026-09-11 (later): WP5.1 presets (live-verified; `load_json` once raised a transient WMF
  `MFCreateDeviceSource` error, fine on retry), WP2.10 HDR backend (mock-tested only, the
  D455 lacks `hdr-preset`; UI pending), WP8.1-8.3 console/firmware logs/terminal
  (live-verified: ~670 raw FW lines/s on the D455, hence batched socket emits; 49 flash
  messages recovered). Playwright real-device suite green after each phase.
- 2026-09-11 (later): WP7.3 updates (live: official DB reachable, D455 FW 5.17.3.10 and
  SDK 2.59.0.0 both `up_to_date`) and WP7.4 notifications (SDK notifications forwarded from
  `manager/devices.py` as the `notification` socket event; snooze persisted in
  localStorage; Report Issue / Store / release-notes help menu; license text in About).
  WP7.1/7.2 need a recovery-mode or unlocked device and stay open.
- Finding: after a long Playwright run the server froze with the event loop inside
  `ctx.devices` and the options poller inside `get_option`, both in SDK native code; the
  camera itself was fine once the process died (a fresh process enumerated and read all
  options at once). A standalone enumerate/read/stream stress run did not reproduce it in
  45 s. Hardening: enumeration now pauses the poller, the poller takes the device lock per
  option rather than per sweep, device/sensor endpoints run the SDK off the event loop, and
  `RS_REST_LOG_FILE` mirrors the server log to a file. Note the legacy viewer never polls
  options (its 6 s read-only refresh is compiled out); the SDK's own watcher does, at 1 s.
- Finding (root cause of the freezes above): the `rs.context.query_devices` / `.devices` /
  `load_device` / `unload_device` bindings did not release the GIL. Enumeration blocks on the
  same SDK lock the device-watcher thread holds while it waits for the GIL to run the
  Python devices-changed callback, so every Python thread in the process stalls (the event
  loop included, which is why `/health` stopped answering). Fixed in
  `wrappers/python/pyrs_context.cpp` (`py::call_guard<py::gil_scoped_release>()`); the server
  additionally enumerates outside its own lock and never while a camera streams.
- Finding: the same GIL inversion exists for every SDK->Python callback. `rs.log_to_callback`
  runs on whichever SDK thread logs, often while that thread holds an SDK lock; a Python
  thread blocked inside a binding that keeps the GIL (get_supported_options,
  get_option_range, get_stream_profiles, filter process, frame_queue enqueue) then deadlocks
  the process. Those bindings now release the GIL, and the server reads the SDK log through
  `rs.log_to_file` plus a tailing thread instead of a callback. The notifications and
  devices-changed callbacks stay (rare events); the console flushes from one long-lived
  thread instead of a Timer started under its lock.
- 2026-09-11 (later): Phase 4 core. The server ships the filtered z16 depth image as a
  binary `depth_frame` socket event (at most 15 Hz) plus `GET /point_cloud/geometry`
  (depth intrinsics and units, texture intrinsics and depth->texture extrinsics); the
  browser unprojects on the GPU (a port of src/gl/pc-shader.cpp: occlusion invalidation,
  per-vertex normals, Brown-Conrady texture mapping, three-light diffuse), textures from
  its own WebRTC session of the chosen stream, and draws the legacy floor grid, axes and
  frustum. Live on the D455 in `tests/e2e/pointcloud.spec.ts`. WP4.3 texture source
  selection is in (sync lock not). WP4.7: `POST /point_cloud/export` writes the newest
  depth frame through `rs.save_to_ply` (mesh / normals / binary), depth only - the SDK
  block textures only from a frameset, which Python cannot assemble. WP4.6: click picks
  the nearest cloud point to the camera ray (CPU, from the depth image), Shift chains,
  Z undoes, labels per segment plus total and polygon area. Finding: starting a sensor that
  already streams used to stop/close the SDK sensor under the manager lock ("stale state
  recovery") and left the SDK unable to reopen it; start is now a no-op for the same
  configuration and a proper stop_sensor + start for a different one.
- 2026-09-12: WP6.1 calibration job core (`services/calibration.py`): OCC and tare as
  jobs inside a workspace that stops the device's streams, streams depth 256x144@90 with
  the emitter on and thermal loop off, runs the firmware calibration with the legacy JSON,
  activates the new table, restores streams and options; apply old/new, keep (write,
  gated by Settings > calibration) and factory reset. Live on the D455: the firmware
  answered "Not enough depth pixels! - low fill factor" for the desk scene, which the
  legacy tool would show the same way; the SDK's progress callback reported nothing until
  the end. Tare backend is in (WP6.3) but has not been run against a target. WP6.2:
  `components/calibration/CalibrationDialog.tsx` (parameters, job progress, health with the
  legacy colour bands, Use new / Use old / Keep / Recalibrate) from the device menu, live in
  `tests/e2e/calibration.spec.ts`. Dry-run (scan only) is exposed as host assistance but
  the host-assisted frame loop is not implemented.
- WP6.6 calibration table: `services/calibration_table.py` parses the D400 coefficients
  table (header, four 3x3 matrices, baseline, per-resolution rectified fx/fy/ppx/ppy) and
  rewrites edited fields with a fresh CRC-32; `GET/PUT /calibration/table` plus the editor
  dialog from the device menu. Finding: the D455 returns 512 bytes (no trailing reserved
  block), version 3.1, baseline -94.69 mm; `set_calibration_table` takes a list of ints.
- WP2.12 stays deferred on purpose: the legacy 2D view has no align selector, and its sync
  lock only ties the 3D texture to the point cloud; the React viewer textures the cloud from
  an independent WebRTC track, so a server-side syncer would not change what the user sees.
- WP8.4: the server counts frames and drops per stream in one-second windows (a gap over
  1.5 frame periods is a drop, as output-model.cpp does) and ships them in the metadata
  `stats`; the output console gets a "Dashboard" panel with the last 30 seconds of drops per
  second and the delivered frame rate per stream.
- WP10.4: `BUILD_REST_API` (CMake/lrs_options.cmake) adds `wrappers/rest-api/CMakeLists.txt`
  with targets `rest-api-viewer` (npm ci, tsc + vite build, bundle into static/, part of
  ALL), `rest-api-venv` (venv + requirements) and `rest-api` (both, after pyrealsense2 when
  BUILD_PYTHON_BINDINGS is on), plus an install rule under share/realsense2/rest-api.
  Configured and built on Windows with npm 10 and Python 3.14.
- 2026-09-12 (end of the autonomous run): 51 commits on `react-viewer-parity` on top of
  `origin/react-viewer-control-parity`, not pushed. Verification: backend `pytest tests`
  192 passed (mocked API suite, unit tests, live tests on the D455); frontend Vitest 262
  passed, tsc clean; Playwright real-device suite 9 passed / 1 skipped (multi-camera needs a
  second device). Open work packages need hardware or infrastructure that was not
  available: WP1.6 (second camera), WP1.9 (LibCI), WP5.2/5.3, WP6.5, Phase 9 (D555/D585 or
  D401-GMSL), WP7.1/7.2 (recovery-mode / unlocked device), WP6.3 ground truth and WP6.4
  (calibration target), WP10.1-10.3 (Tauri 2 migration, installer, platform matrix), plus
  the deliberate deferrals WP1.4 (store split waits for PR #15559), WP2.12, WP3.5 and
  WP4.3. Two E2E flakes remain: the first test after a long idle sometimes finds the camera
  slow to deliver frames, and the machine going to sleep mid-run stretches or aborts runs.
- 2026-09-12 (after a hands-on session by the user): streams, OCC, ROI and metadata "did not
  work" and then nothing streamed. Causes found: (1) after the machine slept, the SDK
  reported the D455 as "no longer present" while the server still held its handle, so every
  start succeeded without frames and the firmware-log poller flooded the console; the server
  now detects this (collectors, poller, advanced-controls read) and re-registers the camera
  from a fresh enumeration (`device_lost`). (2) The ROI drag was swallowed by the tile's
  drag-to-swap (no mouseup), mapped in decimated-frame pixels instead of sensor pixels, and
  "reset" asked for a full-frame region the firmware refuses; fixed to the legacy centre-3/4
  default. (3) The E2E suite only checked that elements existed. It now asserts outcomes:
  `tests/e2e/helpers.ts` (frames flow per the server, camera idle after, device menu),
  `tests/e2e/session.spec.ts` (one page: stream, metadata values change, ROI round trip,
  depth readout, OCC then frames, 3D then 2D, two stop/start cycles, table and updates
  dialogs load real data), a pixel oracle for the 3D canvas, and `tests/live/test_streams.py`
  (frames across stop/start cycles, ROI/readout/controls while streaming, streaming healthy
  after OCC). `GET /stream/metadata` exposes the newest frame metadata for tests and clients.
  (4) Two UI races the new tests exposed: a second click on a module's Start while the first
  request was in flight landed on Stop and undid it (the button is now disabled while a start
  is pending), and a sensor stopped by anyone but this page (another client, a calibration
  job, a lost device) kept its Stop button because the server never said so; the server now
  emits `sensor_status` on start/pause/stop and the store applies it.
- 2026-09-13 (second hands-on session): motion tiles showed no data and squashed the video
  tiles, the grid and metadata buttons "did nothing", drawing an ROI moved the tile, zoom was
  invisible, the record and load-recording buttons were unreadable, and the 3D view drew
  nothing. Causes: (1) `rs.recorder` held the GIL while wrapping a streaming device, so the
  recording request never returned and every later request on that server queued behind it -
  the binding now releases it (pyrs_record_playback.cpp) and `tests/live/test_streams.py`
  asserts recording returns and frames keep flowing. (2) The whole tile was draggable, so any
  press started a rearrange and swallowed the click; only the stream label drags now, and an
  accidental rearrange no longer pins motion tiles ahead of depth (`orderKeys` slots a stream
  started later into its legacy place; `resetTileOrder` forgets an arrangement). (3) Grid rows
  are `minmax(0, 1fr)` and every tile fills its cell, so a motion readout cannot stretch a
  row; a motion tile falls back to the sample its own frame carries. (4) A page that opened
  mid-stream showed an idle camera: `GET /sensors/{id}/status` now names the running streams
  and the store adopts them, and the 3D view re-enables depth-frame emission while it is on
  screen. (5) Zoom has buttons and a percentage readout, the metadata button stays (disabled
  until a frame carries metadata), a failed ROI probe is retried, and recording lives in a
  labelled pane with elapsed time and the file name. `tests/e2e/viewer-ui.spec.ts` covers all
  of it against the camera.
- 2026-09-13 (third hands-on session): most of what still looked broken was a stale bundle -
  the server serves `wrappers/rest-api/static/` and `npm run build` only wrote `dist/`, so the
  page at :8000 kept running the previous day's code. `npm run build` now publishes into
  static/ and the CMake target drops its extra `npm run bundle` step. The real remainders:
  (1) the stream label still dragged a tile while an ROI rectangle was being drawn - in ROI
  mode nothing in the tile arms a drag; (2) recording moved out of the camera card into a
  Recording pane under the device list that names the state ("Not recording" / "Recording
  00:12" / the file being written) and carries the Playback section (open, play, pause, stop,
  close, and whether a recording is open at all); (3) a long-lived server crawled because
  WebRTC sessions were only reaped after an hour and nothing closed them when a browser
  reloaded - the peer connection now closes on failed/closed/disconnected, a sweep every 15 s
  drops unanswered offers, and the page releases its sessions on unload. That last one is why
  earlier full-suite runs degraded from 3 minutes to 10 with unrelated-looking failures.
- WP3.4: `tests/e2e/playback.spec.ts` (record 6 s, load, transport) and
  `tests/live/test_playback.py`. Finding: a recording that ran to its end only plays again
  once its sensors are reopened; `play` now does that (the legacy play button does too).
