Frame metadata is the set of read-only, per-frame attributes that librealsense attaches to each frame, such as frame counters, device and host timestamps, exposure, gain and laser power. You need it when you must know the exact capture conditions of a frame, for example to synchronize streams, detect dropped frames or log camera settings. Whether a frame carries a given attribute depends on the device, its firmware, the stream and whether the host platform delivers the metadata payload.

## When you need it

- You want to read a frame's counter, hardware timestamp, exposure, gain or other capture values.
- `supports_frame_metadata()` returns false for attributes you expected, and you need to know how metadata reaches the host.
- You need metadata on Linux (kernel patches) or Windows (registry keys) and want to know what the SDK expects.
- You want to know which clock a frame timestamp refers to.

## Key facts

- The attributes are the `rs2_frame_metadata_value` enum in [`include/librealsense2/h/rs_frame.h`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/h/rs_frame.h); `RS2_FRAME_METADATA_COUNT` is the number of values, not an attribute. Values are returned as `rs2_metadata_type`, a 64-bit signed integer (`long long`, in [`rs_types.h`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/h/rs_types.h)).
- Check, then query: `rs2::frame::supports_frame_metadata()` and `get_frame_metadata()` in C++, `rs2_supports_frame_metadata()` and `rs2_get_frame_metadata()` in C. Querying an attribute the frame does not carry is an error whose message is built in `rs2_get_frame_metadata` ([`src/rs.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/rs.cpp)) as `<stream> frame does not support metadata "<attribute>"`.
- The [[Python Wrapper]] binds the same calls on `pyrealsense2.frame` and exposes the enum as `rs.frame_metadata_value`.
- Each device registers a parser per supported attribute (`register_metadata`, parser classes in [`src/metadata-parser.h`](https://github.com/realsenseai/librealsense/blob/master/src/metadata-parser.h), payload layouts in [`src/metadata.h`](https://github.com/realsenseai/librealsense/blob/master/src/metadata.h)). A registered attribute can still be missing from some frames, so check per frame.
- `rs2::frame::get_timestamp()` returns milliseconds. Without metadata it is the time of arrival; with metadata it is the hardware timestamp or, with `RS2_OPTION_GLOBAL_TIME_ENABLED`, a host-corrected version of it. `get_frame_timestamp_domain()` returns one of `RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK`, `RS2_TIMESTAMP_DOMAIN_SYSTEM_TIME` or `RS2_TIMESTAMP_DOMAIN_GLOBAL_TIME`.
- On Linux the V4L2 backend reads metadata from a metadata capture node, requesting `V4L2_META_FMT_D4XX` and then `V4L2_META_FMT_UVC` ([`src/linux/backend-v4l2.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/linux/backend-v4l2.cpp)); the `realsense-metadata-*.patch` files in [`scripts/`](https://github.com/realsenseai/librealsense/blob/master/scripts) add RealSense devices to the `uvcvideo` driver.
- On Windows, [`scripts/realsense_metadata_win10.ps1`](https://github.com/realsenseai/librealsense/blob/master/scripts/realsense_metadata_win10.ps1) sets the `MetadataBufferSizeInKB` registry values. A build with WinSDK 8.1 or older compiles without metadata support and prints "The library will be compiled without the metadata support!" ([`src/mf/mf-uvc.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/mf/mf-uvc.cpp)); the CMake option `ENFORCE_METADATA` (default `OFF`) makes that an error.
- The [save-to-disk example](https://github.com/realsenseai/librealsense/blob/master/examples/save-to-disk/rs-save-to-disk.cpp) writes each frame's metadata to a CSV file.

## Full guide

- Design, registration and acquisition flow, query API and OS support: https://github.com/realsenseai/librealsense/blob/master/doc/frame_metadata.md
- Sample CSV output: [doc/metadata/rs-save-to-disk-output-DEPTH-metadata.csv](https://github.com/realsenseai/librealsense/blob/master/doc/metadata/rs-save-to-disk-output-DEPTH-metadata.csv)

## Related pages

- [[Python Wrapper]]
- [[Record and Playback]]
- [[Build Configuration]]
- [[Installation Linux]]
- [[Installation Windows]]
- [[HDR]]
- [[Code Samples]]

<!-- wikigen:footer -->
_Maintained from librealsense `master`; last checked at commit [`c84fe64`](https://github.com/realsenseai/librealsense/commit/c84fe6438bd14a4cc69f46deae92c993268232c6)._
<!-- /wikigen:footer -->
