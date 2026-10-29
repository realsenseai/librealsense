Advanced mode exposes the low-level parameters of the depth-from-stereo engine on RealSense stereo cameras: thresholds, penalties, color handling, the depth table and more, grouped into C structs. It is meant for users who need to tune depth generation beyond the built-in [[D400 Series Visual Presets]], and it is also what makes JSON presets possible: the whole parameter set can be saved to a JSON file and loaded back. This page shows how to turn advanced mode on, how to save and load JSON presets (from code and from the RealSense Viewer), and lists every control group in the API.

## Which devices support it

A device supports advanced mode when it can be extended to `rs400::advanced_mode` (C: `rs2_is_device_extendable_to(dev, RS2_EXTENSION_ADVANCED_MODE, ...)`). In the library this is every device class that derives from `ds_advanced_mode_base`:

- all D400 device classes in [`src/ds/d400/d400-factory.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-factory.cpp). The MIPI/GMSL classes (`rs401_gmsl_device`, `rs415_gmsl_device`, `rs430_gmsl_device`, `rs457_device`) derive from it through `d400_mipi_device`;
- the D500 device classes in [`src/ds/d500/d500-factory.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-factory.cpp);
- DDS (Ethernet) device proxies in [`src/dds/rs-dds-device-proxy.h`](https://github.com/realsenseai/librealsense/blob/development/src/dds/rs-dds-device-proxy.h), see [[Ethernet and DDS Cameras]].

Always check at runtime rather than by model:

```cpp
#include <librealsense2/rs.hpp>
#include <librealsense2/rs_advanced_mode.hpp>

rs2::context ctx;
for (auto&& dev : ctx.query_devices())
{
    if (dev.is<rs400::advanced_mode>())
    {
        auto adv = dev.as<rs400::advanced_mode>();
        std::cout << dev.get_info(RS2_CAMERA_INFO_NAME)
                  << (adv.is_enabled() ? ": advanced mode on\n" : ": advanced mode off\n");
    }
}
```

Supporting the interface is not the same as being able to switch it on and off:

- **D400**: advanced mode is switched with `toggle_advanced_mode()` (see below).
- **D500**: the RealSense Viewer does not offer the toggle for D500 devices (in [`common/device-model.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/device-model.cpp) the menu item is skipped with the comment "D500 cannot toggle Advanced Mode"), and shows "Device FW does not support advanced mode" in the Advanced Controls section when it is off.
- **DDS devices**: `toggle_advanced_mode()` does nothing; the enabled state comes from the `advanced-mode` field of the device information sent by the camera.

## Enabling advanced mode

### From code

`rs400::advanced_mode::toggle_advanced_mode(bool enable)` (C: `rs2_toggle_advanced_mode(dev, enable, &e)`) sends the enable/disable command to the camera firmware and then **resets the device** (`hardware_reset()`). The device disconnects and enumerates again, so the `rs2::device` object you used becomes stale: wait for the device to come back (for example with `rs2::context::set_devices_changed_callback` or `rs2::device_hub`), then query it again and check `is_enabled()`.

```cpp
auto dev = ctx.query_devices().front();
if (dev.is<rs400::advanced_mode>())
{
    auto adv = dev.as<rs400::advanced_mode>();
    if (!adv.is_enabled())
        adv.toggle_advanced_mode(true);   // device resets and re-enumerates
}
```

Do this while the device is not streaming. The Viewer only offers the toggle when the device is not streaming (it shows "Advanced mode cannot be enabled when streaming").

Turning advanced mode off with `toggle_advanced_mode(false)` also resets the device. While it is off, the library does not register the `RS2_OPTION_VISUAL_PRESET` option on the depth sensor of advanced-mode devices: the visual preset option is registered only when advanced mode is enabled ([`src/ds/advanced_mode/advanced_mode.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/advanced_mode/advanced_mode.cpp)).

### From the RealSense Viewer

In the RealSense Viewer (see [[Realsense Viewer Manual]]), either:

- open the device menu (the "bars" icon on the device panel) and select **Advanced Mode**, or
- expand the **Advanced Controls** section of the depth sensor; when advanced mode is off it shows "Device is not in advanced mode" and a **Turn on Advanced Mode** button.

Both ask for confirmation, then call `toggle_advanced_mode()` and the device restarts. The button tooltip reads: "Advanced mode is a persistent camera state unlocking calibration formats and depth generation controls". The [[Depth Quality Tool]] switches a non-DDS device into advanced mode by itself when it starts streaming from it ([`tools/depth-quality/depth-quality-model.cpp`](https://github.com/realsenseai/librealsense/blob/development/tools/depth-quality/depth-quality-model.cpp)).

### From Python

The Python wrapper exposes the class as `pyrealsense2.rs400_advanced_mode` ([`wrappers/python/pyrs_advanced_mode.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/pyrs_advanced_mode.cpp)):

```python
import pyrealsense2 as rs

dev = rs.context().query_devices()[0]
adv = rs.rs400_advanced_mode(dev)
if not adv.is_enabled():
    adv.toggle_advanced_mode(True)   # the device resets; query it again afterwards
```

The full Python tutorial, including waiting for the device to reconnect, is [`wrappers/python/examples/python-rs400-advanced-mode-example.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/python-rs400-advanced-mode-example.py). See also [[Python Wrapper]].

## Saving and loading JSON presets

### From code

The JSON functions live in `rs2::serializable_device` ([`include/librealsense2/hpp/rs_serializable_device.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_serializable_device.hpp)), which `rs400::advanced_mode` derives from:

| C++ | C ([`rs_device.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_device.h)) | What it does |
|---|---|---|
| `std::string serialize_json() const` | `rs2_raw_data_buffer* rs2_serialize_json(rs2_device* dev, rs2_error** error)` | Reads all advanced-mode controls (and the related camera controls) and returns them as a JSON string |
| `void load_json(const std::string& json_content) const` | `void rs2_load_json(rs2_device* dev, const void* json_content, unsigned content_size, rs2_error** error)` | Parses the JSON and applies it to the device |

```cpp
#include <fstream>
#include <sstream>

auto adv = dev.as<rs400::advanced_mode>();      // advanced mode must be enabled

// Save the current settings
std::ofstream out("my_preset.json");
out << adv.serialize_json();
out.close();

// Load them again
std::ifstream in("my_preset.json");
std::stringstream ss;
ss << in.rdbuf();
adv.load_json(ss.str());
```

In C, free the buffer returned by `rs2_serialize_json` with `rs2_delete_raw_data`, and read it with `rs2_get_raw_data_size` / `rs2_get_raw_data`.

In Python, `rs400_advanced_mode` and `serializable_device` both have `serialize_json()` and `load_json(json_content)`.

Behaviour of the advanced-mode implementation ([`advanced_mode.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/advanced_mode/advanced_mode.cpp)):

- Both calls require advanced mode to be enabled. Otherwise they throw "serialize_json() failed! Device is not in Advanced-Mode." or "load_json(...) failed! Device is not in Advanced-Mode.".
- `load_json` starts from the device's current values and changes only the keys present in the file, then sets the visual preset option to **Custom** (not on DDS devices, where advanced mode does not register that option).
- A key the device does not know makes `load_json` throw "*KEY* key is not supported by the connected device!".

### JSON file format

The files are written and read by [`src/serialized-utilities.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/serialized-utilities.cpp) and [`src/ds/advanced_mode/json_loader.hpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/advanced_mode/json_loader.hpp). `serialize_json()` writes:

- `"device"`: `"name"`, `"product line"` and `"fw version"` of the camera;
- `"schema version"`: `1`;
- `"parameters"`: one key per control, for example `"param-depthunits"`, `"param-disparityshift"`, `"param-censusenablereg-udiameter"`, `"controls-laserpower"`, `"controls-autoexposure-auto"`;
- `"hdr-preset"`, only when the device has an HDR preset configured (see [[HDR]]).

When loading:

- A file without `"schema version"` and `"parameters"` is also accepted; its top-level keys are then read as the parameters. A file with only one of the two is rejected with "preset file is corrupt, cannot validate schema", and a different schema version with "mismatch on schema version, expecting: 1 got: ...".
- Only the product line in `"device"` is checked (the name and firmware version are ignored), so a preset saved on one D400 model can be loaded on another. A different product line is rejected with "preset product line does not match the connected device".

Ready-made preset files for D400 cameras are linked from [[D400 Series Visual Presets]].

### From the RealSense Viewer

Every device that supports `serializable_device` gets a preset panel in the Viewer, under the device name:

- the **Load** icon ("Load pre-configured device settings") opens a `.json` or `.preset` file and calls `load_json`. It is disabled while streaming unless the Viewer allows loading during streaming;
- the **Save** icon ("Save current device settings to file") calls `serialize_json`, adds a `"viewer"` section with Viewer settings, and writes a `.json` file;
- files you loaded or saved are added to the preset drop-down so you can select them again.

If the device supports advanced mode but it is off, the Save icon (and the Load icon, except on D500 devices) asks whether to turn it on first ("To use this feature, the device must be in Advanced Mode.").

## Reading and writing control groups

Each control group is a plain C struct from [`include/librealsense2/h/rs_advanced_mode_command.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_advanced_mode_command.h). For each group there is a getter and a setter:

- the getters take a `mode`: `0` returns the current values, `1` the minimum values, `2` the maximum values. In C++ and Python `mode` defaults to `0`;
- the setters write the whole struct, so read the current values first, change the fields you need, and write the struct back;
- every setter that writes to the device sets the visual preset option (when it is registered) to **Custom**.

```cpp
auto adv = dev.as<rs400::advanced_mode>();
STDepthControlGroup cur = adv.get_depth_control();     // mode 0: current values
STDepthControlGroup lo  = adv.get_depth_control(1);    // minimums
STDepthControlGroup hi  = adv.get_depth_control(2);    // maximums

cur.scoreThreshA = (lo.scoreThreshA + hi.scoreThreshA) / 2;
adv.set_depth_control(cur);
```

`get_all_controls()` (C: `rs2_get_all_advanced_controls`) fills an `STAdvancedModeControls` with every group in all three modes in one operation. Each member is an array of `RS2_ADVANCED_MODE_MODES` (3) entries: `[0]` value, `[1]` minimum, `[2]` maximum. Groups for which the firmware does not report a minimum or maximum get the current value in all three entries.

Device-specific behaviour from [`advanced_mode.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/advanced_mode/advanced_mode.cpp):

- `set_ae_control` throws "set_ae_control(...) failed! AE Setpoint is not supported on D500-family devices" on D500 devices. The Viewer also hides the AE setpoint for devices whose name contains "D457".
- On devices without the amplitude-factor feature, `get_amp_factor` returns `0` and `set_amp_factor` does nothing.

Set advanced-mode controls while the device is not streaming when possible.

### Python

In `pyrealsense2`, `rs400_advanced_mode` has the same `get_*` / `set_*` methods as the C++ class, plus `get_all_controls()`. Two Python-only extras are bound in [`pyrs_advanced_mode.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/pyrs_advanced_mode.cpp):

- each group struct can be used like a mapping: `keys()`, `group["field"]`, and `group["field"] = value` (an integer field rounds a float value);
- the advanced-mode object itself is indexed by group name: `adv["depth_control"]` reads the current values, `adv["depth_control", 1]` the minimums, `adv["depth_control", 2]` the maximums, and `adv["depth_control"] = group` writes them. `adv.keys()` lists the group names.

```python
dc = adv["depth_control"]
dc["scoreThreshA"] = 10
adv["depth_control"] = dc
```

In Python, the `STAFactor` field is named `a_factor` (C/C++: `amplitude`). Some `STAdvancedModeControls` members (from `get_all_controls()`) have different names in Python: `rau_support_vector_control`, `rau_thresholds_control`, `slo_color_thresholds_control`, `slo_penalty_control`, `color_correction` and `ae_control` (C/C++: `rsvc`, `rctc`, `sctc`, `spc`, `cc`, `ae`). Each member is a list of 3. The struct supports `keys()` and `["name"]`, but you cannot assign to it.

## Control group reference

One row per struct in [`rs_advanced_mode_command.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_advanced_mode_command.h), in header order. C functions are declared in [`rs_advanced_mode.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/rs_advanced_mode.h), C++ methods of `rs400::advanced_mode` in [`rs_advanced_mode.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/rs_advanced_mode.hpp). Python uses the C++ method names.

<!-- wikigen:table-advanced-mode-groups -->
| Struct | Fields | C get / set | C++ get / set |
|---|---|---|---|
| `STDepthControlGroup` | `plusIncrement`, `minusDecrement`, `deepSeaMedianThreshold`, `scoreThreshA`, `scoreThreshB`, `textureDifferenceThreshold`, `textureCountThreshold`, `deepSeaSecondPeakThreshold`, `deepSeaNeighborThreshold`, `lrAgreeThreshold` | `rs2_get_depth_control` / `rs2_set_depth_control` | `get_depth_control` / `set_depth_control` |
| `STRsm` | `rsmBypass`, `diffThresh`, `sloRauDiffThresh`, `removeThresh` | `rs2_get_rsm` / `rs2_set_rsm` | `get_rsm` / `set_rsm` |
| `STRauSupportVectorControl` | `minWest`, `minEast`, `minWEsum`, `minNorth`, `minSouth`, `minNSsum`, `uShrink`, `vShrink` | `rs2_get_rau_support_vector_control` / `rs2_set_rau_support_vector_control` | `get_rau_support_vector_control` / `set_rau_support_vector_control` |
| `STColorControl` | `disableSADColor`, `disableRAUColor`, `disableSLORightColor`, `disableSLOLeftColor`, `disableSADNormalize` | `rs2_get_color_control` / `rs2_set_color_control` | `get_color_control` / `set_color_control` |
| `STRauColorThresholdsControl` | `rauDiffThresholdRed`, `rauDiffThresholdGreen`, `rauDiffThresholdBlue` | `rs2_get_rau_thresholds_control` / `rs2_set_rau_thresholds_control` | `get_rau_thresholds_control` / `set_rau_thresholds_control` |
| `STSloColorThresholdsControl` | `diffThresholdRed`, `diffThresholdGreen`, `diffThresholdBlue` | `rs2_get_slo_color_thresholds_control` / `rs2_set_slo_color_thresholds_control` | `get_slo_color_thresholds_control` / `set_slo_color_thresholds_control` |
| `STSloPenaltyControl` | `sloK1Penalty`, `sloK2Penalty`, `sloK1PenaltyMod1`, `sloK2PenaltyMod1`, `sloK1PenaltyMod2`, `sloK2PenaltyMod2` | `rs2_get_slo_penalty_control` / `rs2_set_slo_penalty_control` | `get_slo_penalty_control` / `set_slo_penalty_control` |
| `STHdad` | `lambdaCensus`, `lambdaAD`, `ignoreSAD` | `rs2_get_hdad` / `rs2_set_hdad` | `get_hdad` / `set_hdad` |
| `STColorCorrection` | `colorCorrection1` … `colorCorrection12` | `rs2_get_color_correction` / `rs2_set_color_correction` | `get_color_correction` / `set_color_correction` |
| `STAEControl` | `meanIntensitySetPoint` | `rs2_get_ae_control` / `rs2_set_ae_control` | `get_ae_control` / `set_ae_control` |
| `STDepthTableControl` | `depthUnits`, `depthClampMin`, `depthClampMax`, `disparityMode`, `disparityShift` | `rs2_get_depth_table` / `rs2_set_depth_table` | `get_depth_table` / `set_depth_table` |
| `STCensusRadius` | `uDiameter`, `vDiameter` | `rs2_get_census` / `rs2_set_census` | `get_census` / `set_census` |
| `STAFactor` | `amplitude` | `rs2_get_amp_factor` / `rs2_set_amp_factor` | `get_amp_factor` / `set_amp_factor` |
| `STAdvancedModeControls` | `depth_control`, `rsm`, `rsvc`, `color_control`, `rctc`, `sctc`, `spc`, `hdad`, `cc`, `depth_table`, `ae`, `census`, `amp_factor` (each an array of 3: value, min, max) | `rs2_get_all_advanced_controls` / none | `get_all_controls` / none |
<!-- /wikigen:table-advanced-mode-groups -->

Other functions in `rs_advanced_mode.h`: `rs2_toggle_advanced_mode` and `rs2_is_enabled` (C++: `toggle_advanced_mode`, `is_enabled`).

## Source and examples

- Public API: [`rs_advanced_mode.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/rs_advanced_mode.h), [`rs_advanced_mode.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/rs_advanced_mode.hpp), [`h/rs_advanced_mode_command.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_advanced_mode_command.h), [`hpp/rs_serializable_device.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_serializable_device.hpp)
- Implementation: [`src/ds/advanced_mode/`](https://github.com/realsenseai/librealsense/tree/development/src/ds/advanced_mode), [`src/core/advanced_mode.h`](https://github.com/realsenseai/librealsense/blob/development/src/core/advanced_mode.h)
- Older C and C++ snippets: [`doc/rs400/rs400_advanced_mode.md`](https://github.com/realsenseai/librealsense/blob/development/doc/rs400/rs400_advanced_mode.md)
- Python: [`python-rs400-advanced-mode-example.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/python-rs400-advanced-mode-example.py)
- Viewer integration: [`common/device-model.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/device-model.cpp), [`common/realsense-ui-advanced-mode.h`](https://github.com/realsenseai/librealsense/blob/development/common/realsense-ui-advanced-mode.h)

Related pages: [[D400 Series Visual Presets]], [[Realsense Viewer Manual]], [[Depth Quality Tool]], [[Python Wrapper]], [[API How To]].

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`46cf1a6`](https://github.com/realsenseai/librealsense/commit/46cf1a67cc441ad9f55ce339a0de30f2a1c6fed0)._
<!-- /wikigen:footer -->
