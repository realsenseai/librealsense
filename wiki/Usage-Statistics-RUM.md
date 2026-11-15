RUM (Real User Monitoring) is the SDK's anonymous usage-statistics feature: librealsense keeps aggregated counts of the devices, stream configurations, options, filters and notifications used, and saves them to a local report file. The report leaves the machine only when the RealSense Viewer uploads it with the user's consent. This page is for users who want to know what is recorded and how to opt out, and for developers who build the SDK or call the `rs2_rum_*` API.

## When you need it

- You want to check or change whether the Viewer may upload usage statistics.
- You build the SDK and want to turn usage-statistics collection off entirely.
- You want to locate or inspect the local report from your own code or from Python.
- You develop the upload path and need a local receiver to test against.

## Key facts

- Collection is controlled by the CMake option `ENABLE_STATS` in [`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/master/CMake/lrs_options.cmake), ON by default; Android builds force it OFF ([`CMake/android_config.cmake`](https://github.com/realsenseai/librealsense/blob/master/CMake/android_config.cmake)). When it is OFF, the hooks in [`src/rum/rum-hooks.h`](https://github.com/realsenseai/librealsense/blob/master/src/rum/rum-hooks.h) compile to inline no-ops.
- The public C API in [`include/librealsense2/h/rs_rum.h`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/h/rs_rum.h) has three functions: `rs2_rum_get_report_path`, `rs2_rum_set_cloud_enabled` and `rs2_rum_is_cloud_enabled`. C++ wrappers live in `rs2::rum` ([`include/librealsense2/hpp/rs_rum.hpp`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/hpp/rs_rum.hpp)), and Python exposes the same calls as `pyrealsense2.rum` ([`wrappers/python/pyrs_rum.cpp`](https://github.com/realsenseai/librealsense/blob/master/wrappers/python/pyrs_rum.cpp)).
- The report is written to `<app-data>/rum/rum.json` ([`src/rum/rum-collector.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/rum/rum-collector.cpp)); it is saved when an SDK context is destroyed ([`src/context.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/context.cpp)).
- Device serial numbers are kept in memory only, to count distinct units per session, and are never written to the report.
- Upload consent is the `rum_cloud_enabled` key in the per-user `realsense-config.json`, read and written by [`src/rum/rum-config.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/rum/rum-config.cpp).
- The `RS2_RUM_CLOUD_ENABLED` environment variable overrides consent at runtime: any non-empty false value disables upload, while a true value cannot override a saved opt-out.
- The SDK library never uploads; the Viewer's uploader in [`common/rum-uploader/rum-uploader.cpp`](https://github.com/realsenseai/librealsense/blob/master/common/rum-uploader/rum-uploader.cpp) checks `rs2::rum::is_cloud_enabled()` before every upload.
- [`tools/rum-uploader/dev-server/rum_dev_server.py`](https://github.com/realsenseai/librealsense/blob/master/tools/rum-uploader/dev-server/rum_dev_server.py) is a local development receiver that saves posted reports to disk; it is not an uploader.

## Full guide

- What is collected, report format, consent and upload behavior: https://github.com/realsenseai/librealsense/blob/master/doc/rum.md

## Related pages

- [[Build Configuration]]
- [[Realsense Viewer Manual]]
- [[Python Wrapper]]
- [[API Architecture]]

<!-- wikigen:footer -->
_Maintained from librealsense `master`; last checked at commit [`c84fe64`](https://github.com/realsenseai/librealsense/commit/c84fe6438bd14a4cc69f46deae92c993268232c6)._
<!-- /wikigen:footer -->
