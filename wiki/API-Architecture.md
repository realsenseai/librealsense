The librealsense API gives applications configuration, control and streaming access to RealSense cameras. It has two layers: a high-level `rs2::pipeline` that picks settings and delivers synchronized frames, and a low-level device API (context, device, sensor, stream profile, frame) that gives full control over each sensor. This page is for developers who build their own applications on the SDK and want to know how the C API, the C++ wrapper and error handling fit together.

## When you need it

- You are deciding between the pipeline and direct sensor control for your application.
- You call the C API and need to know how errors are reported and released.
- You want to catch specific C++ exceptions, such as a camera disconnecting while streaming.
- You want to feed synthetic or externally captured frames into the SDK through a software device.

## Key facts

- [`include/librealsense2/rs.hpp`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/rs.hpp) includes [`rs.h`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/rs.h) plus the C++ wrappers under `hpp/`. The C++ classes are header-only wrappers that call the matching `rs2_...` C functions and turn errors into exceptions.
- The low-level hierarchy is `rs2::context` → `rs2::device` → `rs2::sensor` → `rs2::stream_profile` → `rs2::frame`. `rs2::context::set_devices_changed_callback()` reports devices being connected and disconnected ([`hpp/rs_context.hpp`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/hpp/rs_context.hpp)).
- Devices and sensors are extensible. `is<T>()` and `as<T>()` check for and convert to a more specific type, and `rs2::device::first<T>()` returns the first sensor of type `T` ([`hpp/rs_device.hpp`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/hpp/rs_device.hpp)).
- `rs2::pipeline::start()` accepts an optional `rs2::config` and returns an `rs2::pipeline_profile`. Frames come from `wait_for_frames()` or `poll_for_frames()`, or from a callback passed to `start()`. Both polling calls throw if a callback was used ([`hpp/rs_pipeline.hpp`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/hpp/rs_pipeline.hpp)).
- `rs2::config` can select a device by serial (`enable_device()`), play back a file (`enable_device_from_file()`) or record (`enable_record_to_file()`). Playback and recording cannot be combined in the same config.
- Most C API functions take a trailing `rs2_error**` argument that is set when the call fails. Read it with `rs2_get_error_message()` and `rs2_get_librealsense_exception_type()`, then release it with `rs2_free_error()` ([`h/rs_types.h`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/h/rs_types.h)).
- In C++, `rs2::error` derives from `std::runtime_error` and adds `get_failed_function()`, `get_failed_args()` and `get_type()`. It has two branches: `rs2::recoverable_error` (`invalid_value_error`, `wrong_api_call_sequence_error`, `not_implemented_error`) and `rs2::unrecoverable_error` (`camera_disconnected_error`, `backend_error`, `device_in_recovery_mode_error`) ([`hpp/rs_types.hpp`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/hpp/rs_types.hpp)).
- `rs2::software_device` and `rs2::software_sensor` (for example `add_sensor()` and `on_video_frame()`) live in [`hpp/rs_internal.hpp`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/hpp/rs_internal.hpp), which `rs.hpp` does not include. See the [software-device example](https://github.com/realsenseai/librealsense/blob/master/examples/software-device/rs-software-device.cpp).

## Full guide

- Architecture overview: https://github.com/realsenseai/librealsense/blob/master/doc/api_arch.md; error handling: https://github.com/realsenseai/librealsense/blob/master/doc/error_handling.md; header layout: https://github.com/realsenseai/librealsense/blob/master/include/readme.md

## Related pages

- [[API How To]]
- [[Code Samples]]
- [[Frame Buffering Management in RealSense SDK 2.0]]
- [[Record and Playback]]
- [[Python Wrapper]]

<!-- wikigen:footer -->
_Maintained from librealsense `master`; last checked at commit [`c84fe64`](https://github.com/realsenseai/librealsense/commit/c84fe6438bd14a4cc69f46deae92c993268232c6)._
<!-- /wikigen:footer -->
