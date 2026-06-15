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
the server. It is not tied to the user or the hardware.

## Consent and control

- **Opt-in**: nothing is uploaded until you agree. The viewer shows a one-time consent prompt
  on first run; you can change the choice any time in **Settings → Privacy**.
- **Disable upload at runtime**: turn it off in Settings → Privacy, or set the environment
  variable `RS2_RUM_CLOUD_ENABLED=0` (this overrides the saved setting).
- **Collection is off by default at build time**: build the SDK with `-DENABLED_STATS=ON` to enable
  it. When off, the `rs2_rum_*` API stays available (ABI-stable) but is a no-op — nothing is
  collected, persisted, or uploaded.

## Where the data lives

The local report is written to `rum.json` under the SDK's app-data folder
(`%APPDATA%\rum\` on Windows, `~/.rum/` on Linux). Consent and settings are stored in the
shared `realsense-config.json`.

## Uploading

In Phase 1, the viewer performs the upload (the SDK itself never opens a network socket).
If you have consented, the viewer uploads the previously saved report in the background at
startup, throttled to the configured cadence (default once per 24 h). You can also trigger
an immediate upload from **Settings → Privacy → "Upload now"**.
