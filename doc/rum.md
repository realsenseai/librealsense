# Real User Monitoring (RUM)

RUM collects **anonymous, aggregated** usage statistics about how the RealSense SDK is
used in the field, so the team can prioritize fixes and features on real evidence.
Collection is local; data leaves the machine **only** if you explicitly opt in to cloud
upload.

## What is collected

A small JSON report (a few KB), aggregated — counts and configurations, never raw events:

- **SDK build**: version, build type, backend, and the build-time flags it was compiled with.
- **System**: OS and CPU architecture.
- **Devices**: model, firmware version, connection type, MIPI driver version (where applicable).
- **Streams**: the stream configurations opened (type, format, resolution, fps) and how long they ran.
- **Options changed**: device-sensor options set to a non-default value (name + last value).
- **Filters**: which SDK post-processing filters were actually applied to frames.
- **Notifications**: SDK notification categories, counted.

## What is NOT collected

- No serial numbers, IP addresses, or any device/user identifier beyond a random `source_id`.
- No personal data.
- No image, depth, or point-cloud content.

The `source_id` is a random token generated once per installation to deduplicate reports on
the server. It is not tied to the user or the hardware. Each report also carries a per-run
`session_id` and a `generated_at` timestamp so the server can dedup a session that is uploaded
more than once (e.g. a manual upload followed by the next-boot upload).

## Consent and control

- **Opt-in**: nothing is uploaded until you agree. The viewer shows a one-time consent prompt
  on first run; you can change the choice any time in **Settings → Online Services**.
- **Disable upload at runtime**: turn it off in Settings → Online Services, or via the environment
  variable `RS2_RUM_CLOUD_ENABLED`. The override is asymmetric: `=0` always disables (a kill switch),
  while `=1` only enables when you have not explicitly opted out — the env var can never turn upload
  on against a saved opt-out.
- **Collection is off by default at build time**: build the SDK with `-DENABLE_STATS=ON` to enable
  it. The `rs2_rum_*` API is always available and functional; `ENABLE_STATS` gates only
  the instrumentation hooks that feed the collector, so when off the report's collected lists stay
  empty but the API itself behaves identically.

## Where the data lives

The local report is written to `rum.json` under the SDK's app-data folder
(`%APPDATA%\rum\` on Windows, `~/.rum/` on Linux). Consent and settings are stored in the
shared `realsense-config.json`.

## Uploading

The viewer performs the upload (the SDK itself never opens a network socket).
If you have consented, the viewer uploads the previously saved report in the background at
startup (the server deduplicates repeats via each report's `session_id`). You can also trigger
an immediate upload from **Settings → Online Services → "Upload now"**.

The startup upload is throttled to at most once per `rum_upload_interval_hours` (default 24, `0`
disables the throttle). This is read from `realsense-config.json` and is not exposed in the UI.

The production ingest endpoint is not live yet, so uploads currently target a local dev-server
stub (`tools/rum-uploader/dev-server/rum_dev_server.py`) — see its header for how to run and
point the viewer at it.
