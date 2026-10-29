RUM (Real User Monitoring) is the SDK's anonymous usage-statistics feature. librealsense keeps aggregated counts on the local machine: which devices were opened, which stream configurations ran and for how long, which options and post-processing filters were used, and which notification categories were raised. The data leaves the machine only when the RealSense Viewer uploads it, and only after the user has agreed. This page is for users who want to know what is recorded and how to switch it off, and for developers who build the SDK or use the `rs2_rum_*` API.

## Quick answers

| Question | Answer |
|---|---|
| Is anything collected by default? | Yes, locally, in builds made with `ENABLE_STATS=ON` (the default, except on Android). |
| Is anything uploaded by default? | No. Upload needs saved consent (`rum_cloud_enabled`). |
| Who uploads? | The RealSense Viewer. The SDK library writes a local file and never uploads it. |
| Where is the local report? | `<app-data>/rum/rum.json`. See [Where the data is stored](#where-the-data-is-stored). |
| How do I turn upload off? | Untick the option in Viewer **Settings → Online Services**, call `rs2_rum_set_cloud_enabled(0, …)`, or set `RS2_RUM_CLOUD_ENABLED=0`. |
| How do I turn collection off? | Build with `-DENABLE_STATS=OFF`. |

## Turn cloud upload on or off

Upload consent is one setting, `rum_cloud_enabled`, kept in the shared per-user `realsense-config.json`. You can set it in several ways. All of them write the same key.

### In the RealSense Viewer

- **First run:** if the config file has no `rum_cloud_enabled` value, the Viewer opens a "Help improve RealSense" dialog. **Accept** is enabled only after you tick the consent box. It saves consent and starts an upload of any saved report. **Decline** saves the opt-out. The dialog links to the [privacy policy](https://realsenseai.github.io/librealsense/privacy-policy.html). Once a value is saved, the dialog does not appear again.
- **Later:** open **Settings → Online Services**. Under **Real User Monitoring (RUM)**, tick or untick **Enable anonymous cloud upload**, then click **Apply** or **OK**.

The RUM section exists only in Viewers built with `ENABLE_STATS`. The **Online Services** tab itself appears whenever the Viewer is built with libcurl (`CHECK_FOR_UPDATES`, `ENABLE_AI_ASSISTANT` or `ENABLE_STATS`). See [[Realsense Viewer Manual]] for the rest of the Settings window.

### From code

```cpp
#include <librealsense2/rs.hpp>

rs2::rum::set_cloud_enabled( false );            // save the opt-out to realsense-config.json
bool upload_allowed = rs2::rum::is_cloud_enabled();  // resolved value, env var included
```

Python (`pyrealsense2`):

```python
import pyrealsense2 as rs

rs.rum.set_cloud_enabled(False)
print(rs.rum.is_cloud_enabled())
print(rs.rum.get_report_path())
```

### With the `RS2_RUM_CLOUD_ENABLED` environment variable

`rs2_rum_is_cloud_enabled` checks the environment variable before it reads the config file ([`src/rum/rum-config.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/rum/rum-config.cpp)):

| `RS2_RUM_CLOUD_ENABLED` | Saved `rum_cloud_enabled` | Upload allowed? |
|---|---|---|
| unset or empty | `"1"` | yes |
| unset or empty | `"0"` or missing | no |
| `1` | missing or `"1"` | yes |
| `1` | `"0"` | no (the variable cannot override a saved opt-out) |
| any other non-empty value (for example `0`) | any | no (kill switch) |

The variable controls only whether upload is allowed. It does not change what is saved. The Viewer's first-run dialog and its **Upload now** button read only the saved config value, so the dialog may still appear and the button may still be enabled. Even so, the upload is refused whenever the resolved value is false.

## Turn collection off (build time)

Collection is controlled by the CMake option `ENABLE_STATS` in [`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake):

```
option(ENABLE_STATS "Enable RUM (Real User Monitoring) usage-statistics collection" ON)
```

```sh
cmake .. -DENABLE_STATS=OFF
```

What the option controls:

- When it is ON, [`CMake/global_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/global_config.cmake) adds the `ENABLE_STATS` compile definition. ON is also one of the options that pull in libcurl (`BUILD_WITH_LIBCURL`), along with `CHECK_FOR_UPDATES` and `ENABLE_AI_ASSISTANT`.
- When it is OFF, every hook in [`src/rum/rum-hooks.h`](https://github.com/realsenseai/librealsense/blob/development/src/rum/rum-hooks.h) becomes an inline no-op. That includes the save that runs when a context closes, so the library records nothing and writes no report. In the Viewer, the uploader becomes a set of empty functions and the RUM settings section is not drawn.
- The `rs2_rum_*` functions are built either way. The path query and the consent get/set keep working.
- Android builds always set `ENABLE_STATS` to OFF ([`CMake/android_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/android_config.cmake)).

For the other build options, see [[Build Configuration]].

## What is collected

The SDK collects data through a small set of hooks. The table lists every place that calls them:

| Data | Recorded when | Call site |
|---|---|---|
| Device | `rs2_create_device` creates a device | [`src/rs.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/rs.cpp) |
| Stream configuration (count) | A sensor is opened with a set of profiles | [`src/sensor.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/sensor.cpp), [`src/software-sensor.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/software-sensor.cpp) |
| Stream duration | A sensor stops, closes or is destroyed while streaming (time from start to stop) | same files |
| Option change | `rs2_set_option` on a device sensor sets a value different from the option's default | [`src/rs.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/rs.cpp) |
| Filter use | A processing block processes its first frame | [`src/proc/synthetic-stream.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/proc/synthetic-stream.cpp) |
| Notification | The SDK raises a notification | [`src/rs.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/rs.cpp) |
| Save to disk | A context is destroyed | [`src/context.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/context.cpp) |

Details:

- **Devices.** Each device is keyed as `<name>-<connection>`, built from the camera-info fields `RS2_CAMERA_INFO_NAME` and `RS2_CAMERA_INFO_CONNECTION_TYPE`. The record stores the connection type, firmware version and MIPI driver version. The serial number is read only to count how many distinct units of that model were seen in the session. The serial number itself is never written to the report.
- **Streams.** Each stream configuration is labelled `<stream>-<format>-<width>x<height>@<fps>`. For non-video streams the resolution part is empty. The record keeps how many times the configuration was opened and the total seconds streamed. The total is rounded to 0.1 s *(new in the next release)*.
- **Options.** Only options set on a device sensor are recorded. Options on processing blocks are not. The record keeps the number of sets and the last value.
- **Filters.** Only filters whose name matches one of the device sensors' recommended processing blocks are counted. Viewer or internal blocks are ignored. Each processing block instance counts once, on its first processed frame.
- **Notifications.** Notifications are counted per category, across all devices.

The SDK does **not** record serial numbers, frame content (images, depth, point clouds), IP addresses or user names. The only identifiers in the report are two random values:

- `source_id` is created once and kept in `rum.json`.
- `session_id` is new for each process run.

### Report format

The report is a JSON object built in [`src/rum/rum-collector.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/rum/rum-collector.cpp):

| Field | Content |
|---|---|
| `schema_version` | `1` |
| `source_id` | Random ID shaped like a UUID. Created on first use, then reused from `rum.json` |
| `session_id` | Random ID for this run |
| `generated_at` | Unix time in seconds |
| `sdk.version` | `RS2_API_VERSION_STR` |
| `sdk.build_type` | `Release` or `Debug` (from `NDEBUG`) |
| `sdk.backend` | `wmf`, `v4l2`, `libuvc`, `winusb_uvc`, `android` or `unknown` |
| `sdk.cmake_flags` | `ENABLE_STATS`, `BUILD_WITH_DDS`, `BUILD_WITH_CUDA` (booleans) |
| `system.os`, `system.arch` | OS name and CPU architecture |
| `devices.<name>-<connection>` | `connection`, `fw_version`, `mipi_driver_version`, `count`, `streams`, `options_changed`, `filters` |
| `…streams.<label>` | `count`, `duration_seconds` |
| `…options_changed.<option>` | `set_count`, `last_value` |
| `…filters.<name>` | use count |
| `notifications` | Array of `{ "category", "count" }` |

**Size limit** *(new in the next release)*: the report is capped at 64 KB, measured on its compact form. When it is larger, the SDK drops the least-used stream entries until it fits and logs a warning that starts with `RUM report over`. In v2.59.1 the report had no size limit.

## Where the data is stored

| File | Linux / macOS | Windows |
|---|---|---|
| Report | `~/.rum/rum.json` | `%APPDATA%\rum\rum.json` |
| Consent and Viewer settings | `~/.realsense-config.json` | `%APPDATA%\realsense-config.json` |

These paths are built from the app-data folder (`$HOME/.` on Linux and macOS, the roaming AppData folder on Windows). Call `rs2_rum_get_report_path` to get the report path from code instead of building it yourself.

**When the report is written:** each time an SDK context is destroyed, in builds with `ENABLE_STATS` only. On the first save in a process, the SDK merges the report already on disk into the in-memory counts, so data from several sessions accumulates in one file:

- counts and durations are added;
- the device `count` keeps the highest value seen;
- an option keeps its newer `last_value`.

Each save replaces the whole file in one step (atomic write).

## When and how data is sent

Only the RealSense Viewer sends data ([`common/rum-uploader/rum-uploader.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/rum-uploader/rum-uploader.cpp)). Every upload first checks `rs2::rum::is_cloud_enabled()` and is refused, with a log warning, if that returns false. The report is sent over HTTPS as compact JSON *(changed in the next release; v2.59.1 sent the file contents exactly as stored)*.

- **Startup upload.** When the Viewer starts and consent is saved, a background thread uploads the saved `rum.json` if both of these are true:
  - the report contains device or notification data;
  - the last successful startup upload was at least `rum_upload_interval_hours` ago.

  After a successful upload, `rum.json` is reset to `{ "source_id": … }` and `rum_last_upload` (Unix seconds) is saved.
- **First run.** Nothing is uploaded until the consent dialog is accepted. Accepting it starts the same upload.
- **Upload now.** **Settings → Online Services → Upload now** sends the saved report immediately in the background and shows the notification `RUM report uploaded` or `RUM upload failed`. If the saved report has no usage data, it shows `No RUM report saved yet` instead. If an upload is already running, the click does nothing. The button is enabled only when consent is saved. This upload does not reset `rum.json` and does not update `rum_last_upload`.
- **Export RUM data...** saves a copy of the current `rum.json` to a JSON file you choose. If the saved report has no usage data, the Viewer shows the notification `No RUM report saved yet` instead. These messages come from [`common/viewer.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/viewer.cpp).

The current session is saved to disk only when its context closes. As a result, the startup upload, **Upload now** and **Export** all handle sessions that have already ended.

### Config keys

These keys live in `realsense-config.json` (defined in [`common/device-model.h`](https://github.com/realsenseai/librealsense/blob/development/common/device-model.h)):

| Key | Meaning | Default |
|---|---|---|
| `rum_cloud_enabled` | Upload consent. The SDK writes `"1"` or `"0"` | missing (no upload; the Viewer asks on first run) |
| `rum_upload_interval_hours` | Minimum hours between startup uploads. `0` turns off the throttle. Config file only, not shown in the UI | `24` |
| `rum_last_upload` | Time of the last successful startup upload (Unix seconds) | `0` |

## Public API

The C API is in [`include/librealsense2/h/rs_rum.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_rum.h). The C++ wrappers are in [`include/librealsense2/hpp/rs_rum.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_rum.hpp). Both are included by `rs.h` and `rs.hpp`. The Python bindings are in [`wrappers/python/pyrs_rum.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/pyrs_rum.cpp).

<!-- wikigen:table-api -->
| C | C++ (`rs2::rum`) | Python (`pyrealsense2.rum`) | Description |
|---|---|---|---|
| `const rs2_raw_data_buffer* rs2_rum_get_report_path(rs2_error** error)` | `std::string get_report_path()` | `get_report_path()` | Path of the on-disk report file (UTF-8). Release the C buffer with `rs2_delete_raw_data`. Does not upload |
| `void rs2_rum_set_cloud_enabled(int enabled, rs2_error** error)` | `void set_cloud_enabled(bool enabled)` | `set_cloud_enabled(enabled)` | Save upload consent to `realsense-config.json` |
| `int rs2_rum_is_cloud_enabled(rs2_error** error)` | `bool is_cloud_enabled()` | `is_cloud_enabled()` | Resolved consent: the saved `rum_cloud_enabled` value, unless `RS2_RUM_CLOUD_ENABLED` is set. Any non-empty value other than `1` disables; `1` enables unless the config file saved an opt-out |
<!-- /wikigen:table-api -->

No API call returns the report contents. To inspect the report, read the file at the returned path. Tests for the API and the report are in [`unit-tests/rum`](https://github.com/realsenseai/librealsense/tree/development/unit-tests/rum).

## RUM dev server (`tools/rum-uploader`)

[`tools/rum-uploader/dev-server/rum_dev_server.py`](https://github.com/realsenseai/librealsense/blob/development/tools/rum-uploader/dev-server/rum_dev_server.py) is a small local receiver for development. It is not a command-line uploader. It listens on `127.0.0.1`, accepts `POST /v1/rum`, saves each body as `rum-<timestamp>-<n>.json` in the output folder, prints a summary and replies `200`. A POST to any other path gets `404`.

```sh
python rum_dev_server.py --port 8080 --dir received
```

The Viewer always sends to the address compiled into `common/rum-uploader/rum-uploader.cpp`. To use the dev server, change the `RUM_ENDPOINT` constant there to the dev server's address and rebuild the Viewer.

The console summary in the script expects an older, flat report layout. With a current report that contains devices, the script saves the raw body and logs it as non-JSON. The data is still kept.

<!-- wikigen:table-dev-server-flags -->
| Flag | Default | Description |
|---|---|---|
| `-h`, `--help` | | Show help and exit (added by `argparse`) |
| `--port` | `8080` | Port to listen on (at `127.0.0.1`) |
| `--dir` | `received` | Folder for the received reports |
<!-- /wikigen:table-dev-server-flags -->

The script has no `--version` flag and does not use `common/cli.h`.

## Source

- [`src/rum/`](https://github.com/realsenseai/librealsense/tree/development/src/rum): collector, consent config and hooks
- [`common/rum-uploader/`](https://github.com/realsenseai/librealsense/tree/development/common/rum-uploader): the Viewer's uploader and consent dialog
- [`doc/rum.md`](https://github.com/realsenseai/librealsense/blob/development/doc/rum.md): the repository's overview

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`46cf1a6`](https://github.com/realsenseai/librealsense/commit/46cf1a67cc441ad9f55ce339a0de30f2a1c6fed0)._
<!-- /wikigen:footer -->
