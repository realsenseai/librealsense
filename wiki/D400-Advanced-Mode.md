Advanced mode exposes the low-level parameters of the depth-from-stereo engine on RealSense stereo cameras (thresholds, penalties, color handling, the depth table and more), grouped into C structs. It is for users who need to tune depth generation beyond the built-in [[D400 Series Visual Presets]], and it is also what lets you save the full parameter set to a JSON file and load it back.

## When you need it

- You want to tune depth parameters that are not exposed as regular sensor options.
- You want to save the camera's depth settings to a JSON preset, or load one, from code or the RealSense Viewer.
- You need to switch a camera into or out of advanced mode.

## Key facts

- The API is `rs400::advanced_mode` in [`include/librealsense2/rs_advanced_mode.hpp`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/rs_advanced_mode.hpp), with C functions in [`include/librealsense2/rs_advanced_mode.h`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/rs_advanced_mode.h) and the control-group structs (`STDepthControlGroup`, `STRsm`, `STDepthTableControl`, `STAdvancedModeControls` and others) in [`include/librealsense2/h/rs_advanced_mode_command.h`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/h/rs_advanced_mode_command.h).
- Support is checked at runtime: a device supports advanced mode when it can be extended to `rs400::advanced_mode`. In the library these are the device classes that derive from `ds_advanced_mode_base`: the D400 and D500 classes in [`src/ds/d400/d400-factory.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/ds/d400/d400-factory.cpp) and [`src/ds/d500/d500-factory.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/ds/d500/d500-factory.cpp), the D400 MIPI classes ([`src/ds/d400/d400-mipi-device.h`](https://github.com/realsenseai/librealsense/blob/master/src/ds/d400/d400-mipi-device.h)) and DDS device proxies ([`src/dds/rs-dds-device-proxy.h`](https://github.com/realsenseai/librealsense/blob/master/src/dds/rs-dds-device-proxy.h)).
- `toggle_advanced_mode(bool)` (C: `rs2_toggle_advanced_mode`) sends the enable command to the firmware and then resets the device, so the device re-enumerates and must be queried again; `is_enabled()` (C: `rs2_is_enabled`) reports the state ([`src/ds/advanced_mode/advanced_mode.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/ds/advanced_mode/advanced_mode.cpp)). The RealSense Viewer does not offer the toggle for D500 devices ([`common/device-model.cpp`](https://github.com/realsenseai/librealsense/blob/master/common/device-model.cpp)).
- Each group has a getter and a setter. Getters take a `mode`: `0` current values, `1` minimum, `2` maximum. `get_all_controls()` (C: `rs2_get_all_advanced_controls`) returns every group as arrays of `RS2_ADVANCED_MODE_MODES` (3) entries.
- Setting a control group, or loading JSON, switches the `RS2_OPTION_VISUAL_PRESET` option to Custom (`RS2_RS400_VISUAL_PRESET_CUSTOM`) when that option is registered.
- JSON presets use `serialize_json()` and `load_json()` from `rs2::serializable_device` ([`include/librealsense2/hpp/rs_serializable_device.hpp`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/hpp/rs_serializable_device.hpp)). On an advanced-mode device both require advanced mode to be on, otherwise they throw "serialize_json() failed! Device is not in Advanced-Mode." or "load_json(...) failed! Device is not in Advanced-Mode.".
- When loading, [`src/serialized-utilities.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/serialized-utilities.cpp) rejects a preset from a different product line ("preset product line does not match the connected device").
- `set_ae_control` throws "set_ae_control(...) failed! AE Setpoint is not supported on D500-family devices" on D500 devices.

## Full guide

- C and C++ usage: https://github.com/realsenseai/librealsense/blob/master/doc/rs400/rs400_advanced_mode.md
- Python example: [`wrappers/python/examples/python-rs400-advanced-mode-example.py`](https://github.com/realsenseai/librealsense/blob/master/wrappers/python/examples/python-rs400-advanced-mode-example.py)

## Related pages

- [[D400 Series Visual Presets]]
- [[Realsense Viewer Manual]]
- [[Depth Quality Tool]]
- [[Python Wrapper]]
- [[HDR]]
- [[Ethernet and DDS Cameras]]

<!-- wikigen:footer -->
_Maintained from librealsense `master`; last checked at commit [`c84fe64`](https://github.com/realsenseai/librealsense/commit/c84fe6438bd14a4cc69f46deae92c993268232c6)._
<!-- /wikigen:footer -->
