# React Viewer Feature Parity with the Legacy C++ Viewer — Design

Status: draft for review (2026-09-10). Companion plan: [plan.md](plan.md).

## 1. Goal and definition of "parity"

Bring `wrappers/rest-api` (FastAPI + pyrealsense2) and `wrappers/rest-api/tools/react-viewer`
(React 18 / Vite / Zustand / three.js, Tauri desktop shell) to functional parity with
`tools/realsense-viewer` + `common/` so the React viewer can replace it for day-to-day
camera work: streaming, control, record/playback, 3D, calibration, firmware, diagnostics.

Parity is defined per feature in three tiers:

| Tier | Meaning | Examples |
|---|---|---|
| **A — Must match** | Same capability, same SDK call, same user outcome | Record/playback, presets JSON, OCC/Tare, snapshot, FW logs, terminal |
| **B — Adapt** | Same outcome, browser-native shape | Settings persistence (JSON on server + localStorage), file dialogs (upload/download), notifications (toasts + center), keyboard shortcuts |
| **C — Drop** | Desktop-renderer or process-local concerns with no web equivalent or no user value | GLSL/MSAA/VSync toggles, font size, window geometry, UI-alignment self-check, udev/CUDA hints, OpenVINO face detection, RUM telemetry (decision pending, see §9) |

"Fully compliant" = every Tier A and B row in the gap matrix (§3) is Done and covered by
a mocked test plus a live E2E check on LibCI.

## 2. Current state

### 2.1 Legacy viewer
~24 k lines across `common/viewer.cpp`, `device-model.cpp`, `subdevice-model.cpp`,
`stream-model.cpp`, `on-chip-calib.cpp`, `output-model.cpp`, etc. `viewer_model`
(`common/viewer.h:64-339`) is the god object. Settings schema is the `configurations::*`
namespace in `common/model-views.h:97-207`. Full inventory was extracted from source and is
folded into the gap matrix below.

### 2.2 React viewer on `development`
Device list + activation, per-sensor stream config and start/stop, native option controls,
control search, FW update (file + recommended), HW reset, Windows metadata enable, 2D WebRTC
tiles with metadata overlay and depth hover, 3D point cloud (socket.io base64 float32,
~10 Hz) with client-side ASCII PLY export, IMU bar gauges, AI chat assistant, What's-New,
Tauri packaging. No record/playback, presets, calibration, logs, snapshot, settings
persistence, keyboard shortcuts.

### 2.3 In-flight branch `origin/react-viewer-control-parity` (PR #15402, RSDSO-21750)
9 commits, +1596/−1124, 27 files. Establishes the **control architecture the rest of this
design builds on**:

- Four control sources, each its own REST resource returning the *applied* `OptionInfo`
  on write: `sensors/{s}/options`, `advanced_mode/controls/{group}/{field}`,
  `colorizer/{field}`, `sensors/{s}/filters/{name}/{field}` (+ `/enabled`).
- Enum `value_descriptions` harvested from `get_option_value_description` → dropdowns.
- Per-device cached `rs.colorizer` shared by both frame paths.
- RS400 advanced mode: status/toggle (device restart) + 13 groups flattened.
- Frontend: one `SensorPanel` per sensor with four `ControlSection`s (Controls, Advanced
  Controls, Depth Visualization, Post-Processing) in legacy draw order; generic
  `Collapsible`/`ToggleSwitch`; store `controls: Record<endpointKey, ControlGroup>`;
  display names + hidden-option list moved to client (`types.ts:17-36`, mirrors
  `viewer_model::hide_common_options`).
- Remaining: manual live test of advanced-mode toggle; follow-up #15569 (bulk advanced read).

Other open PRs this design assumes land first: **#15559** (theme rework, IMU history
graphs + orientation wireframe ported from `graph-model.cpp` / `draw_motion_data`),
**#15662** (Windows Tauri build).

### 2.4 Related Jira (no consolidated gap list exists anywhere today)
RSDEV-9238 (Phase 2 umbrella: refresh, 3D, metadata, PP filters, record/playback),
RSDEV-9262 (Phase 3: D555 defaults/IMU/domain ID, Tare, OCC), RSDEV-9241 (PP filters),
RSDEV-9242 (record & playback), RSDEV-11580 (GPU unprojection 3D), RSDEV-12007 (remove
"Activate device"), RSDEV-12009 (split `rs_manager.py`), RSDEV-12011 (multi-camera),
RSDEV-11404 (DFU-stuck recovery), RSDEV-14130 (default profiles), RSDEV-13683 (live E2E on
LibCI), RSDEV-13547 (platform matrix), RSDEV-14434 (installer), RSDSO-21750 (controls).

## 3. Gap matrix

Status legend: **Done** on development or parity branch · **Partial** · **Missing** ·
**Adapt** (Tier B) · **Drop** (Tier C). Legacy anchors are `file:line` under `common/`
unless noted. Phase column refers to [plan.md](plan.md).

> **Status as of 2026-09-12 (branch `react-viewer-parity`).** The tables below record the
> gap as it was when this design was written. The checked work packages in
> [plan.md](plan.md) are authoritative for what has since landed: every Phase 1, 2, 3, 5,
> 7 and 8 row except multi-camera hardening, the store split, the LibCI harness, sync/align,
> `.bag` conversion, D500-only items, recovery/unsigned firmware and the reflectivity
> readout is Done; Phase 4 (3D) and Phase 6 (calibration) are Done except the sync lock,
> focal-length / UV-mapping calibration, ground-truth measurement and D500 OCC; Phase 10 has
> the CMake hook only. The plan's progress log lists the findings that changed the design
> (options polling instead of the SDK watcher, GIL release in the Python bindings, SDK log
> tailing, no device enumeration while streaming).

### 3.1 Devices, discovery, playback/record

| Feature | Legacy anchor | React status | Tier | Phase |
|---|---|---|---|---|
| Async hot-plug add/remove, per-event notification | `tools/realsense-viewer/realsense-viewer.cpp:137` | Done (socket `devices_changed`) | A | — |
| Auto-select first device | `realsense-viewer.cpp:203` | Done (single device only) | A | — |
| Device list w/ connection type, USB descriptor, S/N | `realsense-viewer.cpp:431` | Done | A | — |
| Multi-device simultaneous streaming | `model-views.h:71` | Partial (RSDEV-12011: black 2nd cam, WebRTC lifecycle) | A | 1 |
| "Activate device" concept removal (legacy has no such gate) | — | Missing (RSDEV-12007) | B | 1 |
| Device info panel (all `RS2_CAMERA_INFO_*`) | `device-model.cpp:929` | Partial (name/S/N/FW/USB only) | A | 2 |
| Load recorded file (`.bag`/`.db3`) via dialog, drag-drop, argv | `realsense-viewer.cpp:353-382,524` | Missing | A (upload / server path) | 3 |
| Playback transport: play/pause/stop/step/seek/speed/loop | `device-model.cpp:514-799` | Missing | A | 3 |
| Record to bag, pause/resume, REC overlay | `device-model.cpp:1541`, `viewer.cpp:1265` | Missing | A | 3 |
| Record settings (auto-name / ask, default folder, compression) | `viewer.cpp:2994-3032` | Missing | B | 3 |
| Legacy `.bag`→`.db3` conversion prompt | `bag-conversion-helper.*` | Missing; no python binding → `rs-convert` subprocess or drop | B | 3 (stretch) |
| Hardware-event notifications (`RS2_NOTIFICATION_CATEGORY_HARDWARE_EVENT`) | `realsense-viewer.cpp:246`, `device-model.h:338` | Missing | A | 7 |
| DDS / Ethernet devices, domain ID, eth config dialog | `dds-model.*`, `viewer.cpp:3356` | Missing (bindings exist: `wrappers/python/pyrs_eth_config.cpp`) | A | 9 |
| Partial device init setting | `viewer.cpp:3346` | Missing | B | 9 |
| HW reset | `device-model.cpp:1336` | Done | A | — |
| Dual-RGB / dedicated-RGB mode switch (D585 2C) | `device-model.cpp:1353-1412` | Missing (`SENSORS_CONFIG_MODE` hidden client-side) | A | 5 |

### 3.2 Sensor / stream configuration

| Feature | Legacy anchor | React status | Tier | Phase |
|---|---|---|---|---|
| Res / FPS / format / stream checkboxes per sensor | `subdevice-model.cpp:749-910` | Done | A | — |
| Shared vs per-stream FPS, mixed resolutions (depth vs IR) | `subdevice-model.h:76-93,172` | Partial (shared only) | A | 2 |
| Default profiles at start-up | `device-model.h:398` | Missing (RSDEV-14130: picks 1280x800 calib profile) | A | 1 |
| Unsupported-combination guard before start | `device-model.cpp:2630` | Partial (server error only) | A | 2 |
| Change res/fps while streaming gating | `subdevice-model.h:202` | Done | A | — |
| D401-GMSL dual-RGB / IR exclusion rules | `subdevice-model.h:286-306` | Missing | A | 5 |
| Perception / depth-mapping exclusions (D500) | `device-model.h:382` | Missing | A | 9 |
| Visual preset combo | `device-model.cpp:2012` | Done (enum dropdown on parity branch) | A | — |
| Load / save JSON preset (+ viewer section), presets folder | `device-model.cpp:1940-2219,350` | Missing (`serialize_json`/`load_json` bound) | A | 5 |
| "Requires Advanced Mode" prompt | `device-model.cpp:2000` | Partial | A | 5 |
| Per-device stream sync toggle (syncer) | `device-model.cpp:1274`, `device-model.h:214` | Missing | A | 2 |
| Selection persistence across reload | `subdevice-model.h:74-82` | Missing | B | 1 |

### 3.3 Controls / options

| Feature | Legacy anchor | React status | Tier | Phase |
|---|---|---|---|---|
| Slider / checkbox / enum / read-only dispatch | `option-model.cpp:93-105` | Done (parity branch) | A | — |
| Click-to-type exact value on slider | `option-model.cpp:429-551` | Partial (fallback text only) | A | 2 |
| Option description tooltips | `option-model.cpp:62` | Missing (`description` already in `OptionInfo`) | A | 2 |
| Async write dispatcher with FW echo reconciliation | `option-model.h:50-159` | Partial (optimistic + applied echo; no drag coalescing) | A | 2 |
| Device-pushed option changes (`on_options_changed`) | `subdevice-model.cpp:129` | Missing → socket `options_changed` | A | 2 |
| Control search | `device-model.cpp:2792` | Done | A | — |
| Control ordering (emitter/AE first, color controls last) | `device-model.cpp:2776-2832` | Missing | A | 2 |
| Hidden options list | `viewer.cpp:927` | Done (client) | A | — |
| Advanced mode toggle + 13 groups | `realsense-ui-advanced-mode.h` | Done (parity branch; live test pending) | A | 0 |
| Auto-exposure ROI drag on tile | `stream-model.cpp:343-441` | Missing (`roi_sensor` bound) | A | 2 |
| HDR configuration tool | `hdr-model.*` | Missing | A | 2 |
| Embedded (on-camera) filters + composite editors (DPP decimation/temporal, HDRD, close range) | `embedded-filter-model.*`, `subdevice-model.cpp:326` | Missing (bound in repo build, not in PyPI 2.56.4) | A | 9 |
| Temporal Filter DPP sensor panel (atomic apply) | `device-model.cpp:2887` | Missing | A | 9 |

### 3.4 2D view

| Feature | Legacy anchor | React status | Tier | Phase |
|---|---|---|---|---|
| Tile grid, stream order (depth→color→IR→motion), persisted per serial | `viewer.cpp:1422-1556` | Partial (grid; no order/persist) | A | 2 |
| Drag-to-rearrange tiles | `viewer.cpp:2776` | Missing | B | 2 |
| Maximize single tile / fullscreen | `stream-model.cpp:793` | Missing | A | 2 |
| Stream info overlay (res, fps hw+viewer, ts, domain, frame#, sizes, format) | `stream-model.cpp:933-1041` | Done | A | — |
| Frame metadata table with decoders (DDS, safety, HaRa/FuSa) | `stream-model.cpp:1046-1470` | Partial (raw keys; no descriptions / hex / decoders) | A | 2 |
| Pixel readout on hover (m/mm/ft) | `stream-model.cpp:1500` | Partial (m only; REST round-trip per move) | A | 2 |
| Max usable range readout | `stream-model.cpp:1553` | Missing | A | 2 |
| IR reflectivity readout | `stream-model.cpp:1581`, `reflectivity/` | Missing (port estimator to server) | A | 8 (stretch) |
| Colormap ruler with ticks | `viewer.cpp:1831,2311` | Partial (`DepthLegend`, min/max only) | A | 2 |
| Zoom / pan on tile | `stream-model.cpp:2040` | Missing | A | 2 |
| Crosshair / grid overlay | `stream-model.h:109` | Missing | B | 2 |
| Snapshot: PNG + raw + metadata CSV; motion/pose CSV | `stream-model.cpp:1927-2038` | Missing (client only sees compressed video → server endpoint) | A | 2 |
| Colorizer controls | `subdevice-model.cpp:383` | Done (parity branch) | A | — |
| Pause stream per sensor + global Space | `stream-model.cpp:709`, `viewer.cpp:4219` | Missing | A | 2 |
| "No frames received" / paused / unsupported-format overlays | `viewer.cpp:1261-1287` | Partial (connection chip only) | A | 2 |
| IMU readouts + history graphs | `stream-model.cpp:1684`, `graph-model.h` | Partial → Done with #15559 | A | 0 |
| Pose rendering | `stream-model.cpp:1780` | Missing | A (low priority; decision §9) | 2 (stretch) |
| Object-detection overlays | `viewer.cpp:2167` | Missing (RSDEV-14131 separate) | C | — |
| Safety zones 2D (occupancy) | `viewer.cpp:2153` | Missing (D500 depth mapping) | A | 9 |
| Placeholder overlays (no device / no stream) | `viewer.cpp:1270` | Done | A | — |

### 3.5 3D view

| Feature | Legacy anchor | React status | Tier | Phase |
|---|---|---|---|---|
| 2D/3D toggle, persisted | `viewer.cpp:2753` | Partial (not persisted) | A | 1 |
| Point cloud render | `viewer.cpp:2577` | Partial (base64 float32 @10 Hz; RSDEV-11580 design exists) | A | 4 |
| Depth source / texture source selection, auto-switch | `viewer.cpp:363-594` | Missing | A | 4 |
| Shading: points / flat mesh / diffuse | `viewer.cpp:596` | Partial (points) | A | 4 |
| Arcball camera, WASD fly, reset (R) | `viewer.cpp:3672` | Partial (OrbitControls; no reset/fly) | B | 4 |
| Ground grid, world axes, camera frustum, camera mesh | `viewer.cpp:2489-2695` | Missing | A | 4 |
| Skybox / occlusion invalidation | `skybox.*`, `viewer.cpp:3153` | Missing | C (skybox) / A (occlusion) | 4 |
| Sync lock (texture vs pointcloud) | `viewer.cpp:479` | Missing | A | 4 |
| Measurement ruler (chain, area, undo) | `measurement.*` | Missing | A | 4 |
| Export PLY (mesh, normals, binary/text) | `viewer.cpp:68-245` | Partial (client ASCII only) → server `rs.save_to_ply` | A | 4 |
| Labeled point cloud, point size, 3D safety zones | `viewer.cpp:682-751` | Missing (D500) | A | 9 |
| Pose trajectory | `viewer.cpp:336` | Missing | A (stretch) | 4 |

### 3.6 Post-processing filters

| Feature | Legacy anchor | React status | Tier | Phase |
|---|---|---|---|---|
| Recommended filter list, per-filter enable + options, master toggle | `subdevice-model.cpp:278`, `processing-block-model.h` | Done (parity branch) | A | — |
| Default-on/off parity, D405 threshold defaults, HDR-merge gating | `subdevice-model.cpp:285-321` | Missing (all default off) | A | 2 |
| Filter state persistence | `processing-block-model.h:82` | Missing | B | 1 |
| Improved Close Range Depth (host CUDA) | `close-range-depth-improver.*` | Missing | C (package-gated) | — |
| Embedded filters | see §3.3 | Missing | A | 9 |
| Alignment (`rs.align`) in per-sensor mode | only `align_to` on `/stream/start` | Partial | A | 2 |

### 3.7 Calibration

| Feature | Legacy anchor | React status | Tier | Phase |
|---|---|---|---|---|
| OCC (speed, accuracy, scan params, host assist), dry run | `on-chip-calib.cpp:1475-1947` | Missing ("coming soon" toast) | A | 6 |
| Tare + ground-truth calc (target W/H) | `on-chip-calib.cpp:1634-1921` | Missing | A | 6 |
| Focal length, UV mapping, FL-plus | `on-chip-calib.cpp:2018-2452` | Missing | A | 6 |
| Health-check UI, before/after, apply/keep, recalibrate | `on-chip-calib.cpp:2314-2620` | Missing | A | 6 |
| Workspace save/restore around calibration | `on-chip-calib.h:112-155` | Missing | A | 6 |
| D500 OCC + D5x5 interactive (commit / try new / try old) | `d500-on-chip-calib.*` | Missing | A | 6 |
| Calibration table viewer/editor, reset to factory, write gate | `calibration-model.*` | Missing | A | 6 |
| Disclaimer / FL limitation / recommend-calibration notices | `notifications.cpp:1181` | Missing | B | 6 |

### 3.8 Firmware, software updates, logs, notifications

| Feature | Legacy anchor | React status | Tier | Phase |
|---|---|---|---|---|
| Signed FW from file, from recommended, progress | `fw-update-helper.*` | Done | A | — |
| Unsigned FW (D400 unlocked) | `device-model.cpp:1456` | Missing | A | 7 |
| Recovery/DFU-stuck device handling | `fw-update-helper.h:33` | Missing (RSDEV-11404) | A | 7 |
| Versions DB: official vs custom URL, `file://` | `viewer.cpp:3383` | Partial (official only) | B | 7 |
| Updates window: SW + FW sections, essential/recommended badges | `updates-model.*` | Partial (FW toast only) | A | 7 |
| SW recommended-update / up-to-date notifications | `notifications.cpp:1047-1159` | Missing | A | 7 |
| Version-upgrade greeting | `notifications.cpp:701` | Done (`WhatsNew`) | A | — |
| Output console: severity filters, search, copy/save, max entries | `output-model.cpp:279-591` | Missing | A | 8 |
| SDK log stream (`log_to_callback`) | `viewer.cpp:986` | Missing | A | 8 |
| FW logs toggle + XML parser path + recover flash logs | `output-model.cpp:97-158`, `device-model.cpp:3728` | Missing (`firmware_logger` bound) | A | 8 |
| Terminal: hex, `Commands.xml`, history, autocomplete | `output-model.cpp:940-1103` | Partial (`/hwm` raw only; `terminal_parser` bound) | A | 8 |
| Dashboards (frame drops/s, processing vs camera rate) | `output-model.h:28` | Missing | B | 8 (stretch) |
| Notification center: stack, expand, snooze (once / later / never) | `notifications.h:35-232` | Partial (toasts) | B | 7 |
| Metadata-disabled warning + Enable action | `notifications.cpp:982` | Done | A | — |
| Error popup with "don't show again" | `viewer.cpp:1134` | Partial | B | 7 |
| Report issue (GitHub prefill) | `viewer.cpp:2861` | Missing | B | 7 |
| udev / Jetson-CUDA hints | `viewer.cpp:760-921` | Missing | C | — |

### 3.9 Settings, persistence, UX

| Feature | Legacy anchor | React status | Tier | Phase |
|---|---|---|---|---|
| Settings dialog (Playback&Record / Performance / General / Online) | `viewer.cpp:2925-3547` | Missing | B | 1 |
| Config file with debounced save, schema `model-views.h:97-207` | `rs-config.*` | Missing | B | 1 |
| Units metric/imperial | `viewer.cpp:3171` | Missing | A | 1 |
| Logging settings (console/file/severity) | `viewer.cpp:3194` | Missing | B | 8 |
| Fullscreen (F8), keyboard shortcuts (Space, R, WASD, Z, Shift, Esc, Up/Down, Tab) | various | Missing | B | 2/4/8 |
| Theme | `model-views.h:21` | Partial → #15559 | B | 0 |
| About dialog with license / source link | `viewer.cpp:3600` | Partial (version only) | B | 1 |
| RS Store link | `viewer.cpp:2866` | Missing | B | 1 |
| RUM / telemetry | `viewer.cpp:3429` | Missing | C (decision §9) | — |
| GLSL/MSAA/VSync/font/window geometry/UI alignment | `viewer.cpp:3043-3161` | — | C | — |

### 3.10 Platform / packaging (required to actually replace the legacy viewer)

| Item | Status | Phase |
|---|---|---|
| Single-port serve (mount `static/` in FastAPI; bundle script copies, nothing mounts) | Missing | 1 |
| Tauri 2 (Ubuntu 24.04+) | Missing | 10 |
| Installer for REST API + viewer (RSDEV-14434), Jenkins release flow (RSDEV-9390) | In progress (#15662) | 10 |
| Platform matrix (RSDEV-13547) | Missing | 10 |
| Live E2E on LibCI (RSDEV-13683) | Missing | 1 (harness), then continuous |
| CMake option to build/package rest-api | Missing | 10 |

## 4. Approaches considered

**A. Vertical slices on the current architecture (recommended).** Keep FastAPI +
pyrealsense2 + React. Land two enabling refactors first (service split of `rs_manager.py`;
store slices + persistence + a generic long-operation "job" pattern), then deliver each gap
cluster as an independent backend-service + endpoint + UI slice with mocked tests and a
live E2E. Pros: continuous delivery, reuses the parity branch's per-resource control
pattern, every phase is shippable. Cons: some legacy behaviours (syncer, ppf thread model)
are re-derived in Python rather than copied.

**B. Port `viewer_model`/`device_model` semantics 1:1 into a Python "viewer core".**
Highest fidelity to legacy corner cases, but a multi-month up-front rewrite that blocks all
user-visible progress, reproduces the god-object shape we want to avoid, and still needs
the same REST/UI surface afterwards. Rejected.

**C. New C++ REST server reusing `common/`.** `common/` is ImGui-bound (`viewer_model`
mixes rendering and state); nothing is reusable without the same decomposition. Would also
fork the Python backend already shipped in Tauri. Rejected.

## 5. Target architecture (approach A)

### 5.1 Backend layout (`wrappers/rest-api/app/`)

`services/rs_manager.py` (~2700 lines) becomes a thin device registry + frame pump;
feature logic moves to focused services, each with a matching endpoint module and model
file. Naming follows the parity branch (`services/options.py`, `services/advanced_mode.py`).

```
services/
  devices.py        registry, hot-plug, device info, reset, DFU tracking   (from rs_manager)
  streaming.py      per-sensor open/start/stop, frame queues, colorizer, filter chain, syncer
  options.py        (exists)          advanced_mode.py (exists)
  presets.py        serialize_json / load_json / presets folder
  record.py         rs.recorder lifecycle, pause/resume, file naming, compression
  playback.py       context.load_device / unload_device, transport, status callback → socket
  snapshot.py       last-frame PNG/raw/CSV bundle (zip) per stream
  calibration.py    OCC/Tare/FL/UV/FL+ (D400), D500 OCC interactive, tables, workspace save/restore
  jobs.py           generic long-op registry: id, state, progress, result, cancel; socket events
  logs.py           rs.log_to_callback ring buffer → socket 'log'; firmware_logger thread; flash log
  terminal.py       terminal_parser + Commands.xml; wraps existing /hwm
  updates.py        versions DB (SW + FW), custom URL, essential/recommended (extends firmware.py)
  notifications.py  sensor notification callbacks → socket 'notification'
  settings.py       server-side JSON settings (keys mirror model-views.h where relevant)
  eth_config.py     eth_config_device read/write (D555)
  pointcloud.py     intrinsics, texture source, occlusion option, save_to_ply
```

Conventions (carried from the parity branch):
- Every resource is `GET` (state) + `PUT/POST` (mutation) that **returns the applied
  state**; no `{"success": true}` payloads.
- Blocking SDK calls run in `run_in_threadpool`; long operations (>1 s: FW update,
  calibration, PLY export, record finalize, playback load) go through `jobs.py` and report
  over socket.io `job_<id>` events (generalising today's `firmware_progress_<id>`).
- Trailing-slash policy: one form only, no 307 reliance (fix the 9 mismatched client calls).
- Every new endpoint module gets a mocked pytest at the HTTP layer (the parity branch's
  filter / colorizer / advanced-mode endpoints currently lack this) and a live test.
- Feature detection: `hasattr(rs, "embedded_filter")` etc.; `/health` reports
  `capabilities` so the UI hides unsupported sections instead of erroring.

### 5.2 Transport

| Data | Transport | Change |
|---|---|---|
| Video | WebRTC (keep) | Pause = freeze last frame server-side; "no frames" detection from server timestamps |
| Metadata / IMU / options_changed / notifications / logs / job progress / playback status | socket.io (keep) | New events: `options_changed`, `notification`, `log`, `job_<id>`, `playback_status` |
| Point cloud | socket base64 today | GPU unprojection (RSDEV-11580 option B1): depth delivered as a lossless 16-bit stream (second WebRTC track with 16-bit-in-RGB packing, or binary WebSocket) + intrinsics endpoint; client shader unprojects. Base64 path kept as fallback |
| Files (bag, JSON preset, PLY, snapshot zip, FW bin, XML) | HTTP upload/download | Browser: `<input type=file>` + `Content-Disposition`. Tauri: native dialogs + server path |

### 5.3 Frontend layout (`tools/react-viewer/src/`)

- Store split into slices (Zustand slice pattern): `devices`, `streaming`, `controls`,
  `view2d`, `view3d`, `record`, `playback`, `calibration`, `jobs`, `console`,
  `notifications`, `settings`, `chat`. Each in `store/<slice>.ts`; `store/index.ts`
  composes. `persist` middleware (localStorage) for UI-only state: view mode, tile order
  per serial, expanded sections, units, console open, snoozed notifications.
- Server-side settings (`/settings`) for what the backend needs or that must survive
  browser changes in Tauri: record path/mode/compression, versions DB URL, log settings,
  FW-log XML / Commands.xml paths, DDS enable/domain, calibration write gate,
  post-processing performance mode.
- `DevicePanel.tsx` (1150 lines) splits into `device/DeviceCard.tsx`,
  `device/DeviceMenu.tsx`, `sensor/SensorPanel.tsx`, `sensor/StreamConfig.tsx`,
  `controls/ControlSection.tsx`, `controls/OptionControl.tsx`. New folders:
  `playback/`, `record/`, `console/`, `calibration/`, `settings/`, `notifications/`,
  `view3d/`.
- Keyboard shortcut registry (`utils/shortcuts.ts`) with the legacy map: Space pause all,
  R reset 3D, W/A/S/D fly, Shift chain measure, Z undo, F8 fullscreen, Esc close console,
  Up/Down terminal history, Tab autocomplete, arrows nudge composite fields.

### 5.4 Feature designs (Tier A clusters)

**Record / playback.** Record wraps the live device: `rs.recorder(path, device)` created
on record start while sensors are streaming (legacy gates record on streaming; keep).
Pause/resume map to recorder methods. Stop releases the recorder, sensors keep running.
Playback: `POST /playback/load` (multipart upload into a server `recordings/` folder, or
`{path}` for Tauri) → `context.load_device(path)`; the playback device then appears in
`/devices` with `is_playback: true` and streams through the normal per-sensor API.
`playback.set_status_changed_callback` → socket `playback_status`; loop implemented server
side like `realsense-viewer.cpp:59-115` (restart streaming sensors on STOPPED when
`repeat`). Transport endpoints: `play`, `pause`, `stop`, `seek {ns}`, `speed {x}`,
`step {+1|-1}` (seek by 1/max_fps while paused), `repeat {bool}`, `GET status
{position, duration, state, speed, repeat, file}`. Unload = `context.unload_device`.

**Snapshot.** `POST /devices/{d}/sensors/{s}/snapshot?stream=depth` → zip with
`<name>.png` (colorized for depth/IR via cached colorizer), `<name>.raw`,
`<name>_metadata.csv`; motion/pose streams return `.csv`. Server already holds the last
raw frame per stream; reuse. Frontend: header button per tile, download response.

**Calibration.** All flows are `jobs`. `POST /calibration/{flow}` with the flow's JSON
params (speed, accuracy, scan, host assist, ground truth, target W/H…) mirrors
`run_on_chip_calibration(json, timeout, progress_cb)` etc. Server does workspace
save/restore (`on-chip-calib.h:112-155`): stop non-depth sensors, force the required depth
profile, laser/thermal-loop off, restore after. Result payload: health numbers plus a
server-held `new_table` id; `POST /calibration/apply {keep|new}` writes via
`write_calibration` or restores. D5x5 interactive: `try_new`/`try_old`/`commit` actions on
the same job. Tables: `GET/PUT /calibration/table` (`get_calibration_table` /
`set_calibration_table` + `write_calibration`), `POST /calibration/reset_factory`, gated
by settings `calibration.enable_writing`. Ground truth: `calculate_target_z` fed from the
depth frame queue.

**Presets.** `GET /presets` (folder listing + visual preset enum), `GET /presets/current`
(download `serialize_json` + `viewer` section), `POST /presets/load` (upload JSON →
`load_json`, set `RS2_OPTION_VISUAL_PRESET` to Custom, refetch controls). Requires advanced
mode → 409 `{reason: "advanced_mode_required"}`; UI offers the toggle.

**3D.** Server: `GET /devices/{d}/intrinsics?stream=depth`, `PUT /point_cloud/texture
{stream}` (color/IR/none), `PUT /point_cloud/occlusion {bool}`, `POST /point_cloud/export
{mesh, normals, binary}` → job → PLY download (`rs.save_to_ply` with
`OPTION_PLY_MESH/NORMALS/BINARY`). Client: unproject in a shader from the depth texture
(RSDEV-11580), texture from the selected WebRTC track, shading modes (points / mesh via
index buffer / diffuse), ground grid + axes + frustum (from intrinsics), reset viewport,
measurement tool (raycast pick, Shift chain, area, Z undo), sync lock.

**Console / logs / terminal.** `logs.py` installs `rs.log_to_callback` at startup into a
bounded deque (setting `console.max_entries`) and emits `log` events; `GET /logs?since=`
for backfill. FW logs: `POST /devices/{d}/fw_logs/start|stop` with XML path from settings,
a thread polls `get_firmware_log` + `parse_log`, emits `log` with `source: "fw"`.
`POST /devices/{d}/fw_logs/flash` returns the parsed flash log. Terminal:
`POST /devices/{d}/terminal {line}` → `terminal_parser(Commands.xml).parse_command` →
existing HWM send → `parse_response`; `GET /terminal/commands` for autocomplete. UI:
bottom panel with severity counters as filters, search, copy/save, command line with
history and Tab completion.

**Notifications.** Server subscribes `sensor.set_notifications_callback` per sensor →
`notification` events (category, severity, description, serialized data, timestamp).
Client `NotificationCenter` holds sticky items with legacy snooze semantics (just once /
remind later N days / never), persisted in localStorage; toasts stay for transient info.

**Updates.** Extend versions-DB parsing to the SW section; `GET /updates/{d}` returns FW +
SW candidates with essential/recommended classification; settings for custom DB URL and
`file://`. UI: Updates dialog like `updates-model.cpp` (two sections, download, release
link), plus up-to-date and recommended-update notifications.

**DDS / Ethernet.** Settings `context.dds.enabled|domain` → passed to `rs.context(json)`
at startup (server restart; UI says so). `GET/PUT /devices/{d}/eth_config` via
`eth_config_device` bindings for D555; UI dialog mirroring `dds-model.h:15-64`.

**Embedded filters (D500).** `GET /devices/{d}/sensors/{s}/embedded_filters` from
`sensor.query_embedded_filters()`; composite options exposed as one group per filter with
`PUT .../embedded_filters/{type}` taking the whole composite struct (atomic apply, like the
legacy editors). Feature-detected.

### 5.5 Testing strategy

- Backend: pytest with `pyrealsense_mock.py` extended per service (recorder/playback,
  auto_calibrated_device, firmware_logger, terminal_parser, eth_config). HTTP-layer test
  for every endpoint module. `tests/live/` gains one file per phase.
- Frontend: Vitest + MSW per slice and component; Playwright smoke stays camera-less.
  `real-device.spec.ts` grows per phase and runs on LibCI (RSDEV-13683) with a D455 and,
  where present, a D555.
- Definition of done per work package: mocked tests green in `rest-api-CI.yaml`; live
  scenario in `real-device.spec.ts`; the gap-matrix row flipped to Done in this doc.

## 6. Non-goals / dropped (Tier C)

OpenVINO face detection and object overlays (tracked separately as RSDEV-14131), host CUDA
close-range improver (package-gated), GLSL/MSAA/VSync/font/window geometry/UI-alignment
check, udev and Jetson CUDA hints (server-side; surfaced as `/health` warnings if at all),
skybox, in-app `.bag`→`.db3` conversion (stretch via `rs-convert` subprocess).

## 7. Risks

- **pyrealsense2 version skew.** Embedded filters, `eth_config_device`, `labeled_points`,
  `enable_metadata` exist in the repo bindings but not in the PyPI 2.56.4 wheel. Server
  feature-detects; packaged builds must ship the repo-built wheel.
- **WebRTC and 16-bit depth.** GPU unprojection needs lossless depth; if the codec path
  fails, fall back to a binary WebSocket for Z16 (RSDEV-11580 B2/B3).
- **Calibration blocks the device** for tens of seconds and changes stream state; the job
  model plus server-side workspace restore must survive client disconnects.
- **Concurrent option access wedges the D455 on Windows.** Measured 2026-09-11: the SDK's
  `on_options_changed` watcher (one polling thread per sensor) and any second thread or
  process touching options leave the camera answering every write with
  `0x8007001f` until a hardware reset. The server therefore never registers the SDK
  watcher; a single poller thread reads options one sensor at a time under a per-device
  lock that REST reads/writes share (`services/options_poller.py`). Worth an SDK ticket.
- **Multi-camera** (RSDEV-12011) must be solid before record/playback and calibration,
  which each add device-lifecycle transitions.
- **`rs_manager` split** is a large refactor under active PRs (#15402, #15559); sequence it
  right after they merge and before new services land.

## 8. Hardware needed for verification

Estimates in the plan assume the following is checked live before Phase 2 starts. None of
it can be verified against mocks. No camera is connected on this machine right now
(`rs.context().query_devices()` returned empty).

| Camera | Verifies |
|---|---|
| D455 (or D435i) | Advanced-mode toggle reconnect (PR #15402 open item); HDR sequence-id options; AE ROI; recorder/playback round trip through the sensor API; `on_options_changed` push; hardware-event notification callback; `firmware_logger` + XML; `terminal_parser` with `Commands.xml`; OCC/Tare/FL/UV job flow and workspace restore; multi-cam with a second D4xx |
| D555 or D585 | Embedded filters / composite options via repo-built bindings; D500 OCC interactive; safety/occupancy + labeled point cloud; DDS discovery and `eth_config_device`; dual-RGB mode switch (D585 2C) |
| Any D400 in recovery mode | DFU-stuck handling (RSDEV-11404), unsigned FW path |

## 9. Decisions needed from the owner

1. **RUM telemetry** (`common/rum-uploader`): drop (Tier C) or port as server-side opt-in?
   Recommendation: drop for parity; revisit with product.
2. **Playback file source in browser mode**: upload into server `recordings/` (simple,
   copies multi-GB files) vs server-path text field (no copy, exposes filesystem).
   Recommendation: both; Tauri uses native dialog + path.
3. **Point cloud transport**: commit to GPU unprojection (RSDEV-11580 B1) in Phase 4, or
   keep base64 and only add texture/measure/export? Recommendation: B1.
4. **Pose (T265-class) rendering and trajectory**: no current hardware in the line-up;
   propose Tier C unless a pose device is still supported.
5. **Default post-processing filter state**: parity branch defaults all filters off to save
   server CPU. Match legacy defaults (spatial/temporal/decimation on for depth) or keep
   off? Recommendation: match legacy, add a server "performance mode" setting.
6. **Settings JSON location**: server user dir in both browser and Tauri modes
   (`~/.realsense/rest-api-settings.json`) — recommended — or Tauri app-data dir.
