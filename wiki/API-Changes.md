<!-- wikigen:upcoming -->
## Upcoming release (draft)

- New APIs
  - `rs2_get_depth_mapping_params` / `rs2_set_depth_mapping_params` - Get / set the occupancy grid and labeled point cloud params of a depth mapping sensor as a JSON string. Get returns a `rs2_raw_data_buffer` (release with `rs2_delete_raw_data`); set changes only the named fields, blocks until the device has applied the params and fails with the device's message if it rejects them. C++ `rs2::depth_mapping_sensor::get_depth_mapping_params()` / `set_depth_mapping_params()`.
  - `rs2_create_mjpeg_decoder` - Creates an MJPEG decoder processing block: accepts compressed MJPEG frames and outputs RGB8 frames. C++ `rs2::mjpeg_decoder`.
  - `RS2_STREAM_POINT_CLOUD` - added to `rs2_stream`, device-generated point cloud stream. Its frames carry vertices only: `rs2_get_frame_texture_coordinates` / `rs2::points::get_texture_coordinates()` return nullptr for it.
  - `RS2_FRAME_METADATA_OCCUPANCY_GRID_ORIGIN_X` / `RS2_FRAME_METADATA_OCCUPANCY_GRID_ORIGIN_Y` - added to `rs2_frame_metadata_value`, occupancy-grid origin on the forward / lateral axis, in millimeters.
  - `RS2_OPTION_PASSIVE_DEPTH_MODE` - added to `rs2_option`, D585 2C: which exposure classes produce depth (pre-stream only); values in the new `rs2_passive_depth_mode` enum (Disabled = laser on, active depth only / Alternating = laser alternates per frame, active and passive depth both delivered / Full = laser forced off, passive depth only), with `rs2_passive_depth_mode_to_string`.
  - `RS2_OPTION_DUAL_RGB_ENABLE_AUTO_EXPOSURE` - added to `rs2_option`, D585 2C: enable / disable auto-exposure of the RGB cameras, independent of the depth auto-exposure.
  - `RS2_OPTION_DUAL_RGB_EXPOSURE` - added to `rs2_option`, D585 2C: RGB exposure time in 100 microsecond units. Setting any value disables RGB auto-exposure.
  - `RS2_OPTION_DUAL_RGB_GAIN` - added to `rs2_option`, D585 2C: RGB gain. Setting any value disables RGB auto-exposure.
  - `RS2_OPTION_ACCEL_SENSITIVITY` - added to `rs2_option`, accelerometer sensitivity (dynamic range) level; values in the new `rs2_accel_sensitivity` enum (3G / 6G / 12G / 24G full-scale range), with `rs2_accel_sensitivity_to_string`. Selects the saturation point only, the reported unit scale is unchanged.
  - `RS2_OPTION_OCCUPANCY_GRID_SEGMENTATION` - added to `rs2_option`, Off (0) = depth-only labeling, On (1) = the occupancy grid uses the segmentation mode and classes (pre-stream only).
  - `RS2_OPTION_LPCL_SEGMENTATION` - added to `rs2_option`, Off (0) = depth-only labeling, On (1) = the labeled point cloud uses the segmentation mode and classes (pre-stream only).
- Changed APIs
  - `rs2_hdrd_control::threshold_mode` - documented default changed from 0 (Zero range) to 1 (MinZ range, computed by the firmware). Values are still 0 = Zero range, 1 = MinZ range, 2 = Manual.
  - `rs2::save_to_ply` - a device point cloud (`RS2_STREAM_POINT_CLOUD`) has no texture coordinates, so it is exported without texture coordinates or color even when a color frame is given.
  - `rs2_hdrd_control` - only some Depth profiles are supported; others are rejected when the control is enabled or when the stream is opened.
<!-- /wikigen:upcoming -->

## Version [2.59.1](https://github.com/realsenseai/librealsense/releases/tag/v2.59.1) beta

- New APIs
  - `rs2_set_composite_option` / `rs2_get_composite_option` - Composite options: set / get all fields of a multi-parameter control in a single atomic transaction, identified by the new `rs2_composite_option_id` enum. Query support and ranges with `rs2_get_composite_options_list`, `rs2_supports_composite_option` and `rs2_get_composite_option_range`. C++ `rs2::options::set_composite_option()` / `get_composite_option()` and typed helpers.
  - `RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP` - added to `rs2_composite_option_id`, on-device depth decimation filter (payload `rs2_decimation_filter_dpp_config`).
  - `RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP` - added to `rs2_composite_option_id`, on-device depth temporal filter (payload `rs2_temporal_filter_dpp_config`).
  - `RS2_COMPOSITE_OPTION_HDRD_CONTROL` - added to `rs2_composite_option_id`, improved close-range depth control (payload `rs2_hdrd_control`).
  - `rs2_get_all_advanced_controls` - Read every advanced-mode control group (value, minimum and maximum) in one bulk operation, with C++ `rs400::advanced_mode::get_all_controls()`.
  - `rs2_get_option_range_from_list` - Get the range of an option directly from an options list, with C++ `rs2::option_value::has_range()` / `range()`.
  - `rs2_rum_set_cloud_enabled` / `rs2_rum_is_cloud_enabled` - Opt in to or out of cloud upload of usage statistics (RUM), persisted per user; `rs2_rum_get_report_path` returns the local report file. C++ in `rs_rum.hpp`.
  - `RS2_OPTION_EMITTER_MODE` - added to `rs2_option`, selects the emitter mode; values in the new `rs2_emitter_mode` enum (Off / On / On-Off / Always On).
  - `RS2_OPTION_ENABLE_ALIGNED_DEPTH` - added to `rs2_option`, device-side depth-to-color alignment; the depth stream returns Z16 aligned to the color viewport.
  - `rs2_colored_ir_auto_exposure_mode` - new enum of values for `RS2_OPTION_DEPTH_AUTO_EXPOSURE_MODE` on colored-IR devices (Auto / Depth priority / Color priority / Hybrid).
  - `RS2_RS400_VISUAL_PRESET_EDGE_ENHANCEMENT` - added to `rs2_rs400_visual_preset`, Edge Enhancement visual preset.
- Changed APIs
  - `rs2_create_rotation_filter_block` - **breaking**: the streams to rotate are now passed as `const rs2_stream * streams_to_rotate, int stream_count` instead of `rs2_streams_list`.
  - `rs2_object_detection` - new fields `world_position`, `center_of_mass_x`, `center_of_mass_y`, `center_of_mass_valid` (struct size changed). Firmware that does not report center-of-mass returns `center_of_mass_valid = 0`.
- Removed APIs
  - `rs2_streams_list` - replaced by a `rs2_stream` array and count in `rs2_create_rotation_filter_block`.


## Version [2.58.4](https://github.com/realsenseai/librealsense/releases/tag/v2.58.4) beta

* New APIs
  * `rs2_get_device_time_ms` - Retrieve the current device hardware clock time in milliseconds (Can assist users for manually sync multi-devices frames by their own method). 
  * `rs2_get_frame_gpu_data` - Retrieve a CUDA device pointer aliasing the frame data for zero-copy GPU consumption (e.g. feeding a CUDA / TensorRT kernel with no host->device copy). Returns NULL unless built with `BUILD_WITH_CUDA_ZEROCOPY` and running on an integrated GPU (Jetson) where the frame buffer is GPU-mapped. Pointer valid only while the frame is held.
  * `rs2_get_frame_gpu_data_or_upload` - Retrieve a CUDA device pointer for the frame, uploading if needed: zero-copy when the frame is GPU-mapped, otherwise an SDK-managed host->device copy . Returns NULL on non-CUDA builds. Pointer valid only while the frame is held.
  * `rs2_software_sensor_add_perception_stream` - Add a perception stream (`rs2_perception_stream`) to a software sensor. Renamed from `rs2_software_sensor_add_inference_stream`.
  * `rs2_software_sensor_add_perception_stream_ex` - Same as above with an `is_default` flag. Renamed from `rs2_software_sensor_add_inference_stream_ex`.
  * `rs2_perception_stream` - struct defining a perception stream (type, index, uid, fps, format). Renamed from `rs2_inference_stream`.
  * `RS2_EXTENSION_GPU_FRAME` - added to `rs2_extension`, present when the frame's pixels reside in GPU-accessible (zero-copy) memory.
  * `RS2_EXTENSION_PERCEPTION_FRAME` - added to `rs2_extension`, frame carrying attributes inferred from frame content (e.g. object detection results). Renamed from `RS2_EXTENSION_INFERENCE_FRAME`.
  * `RS2_EXTENSION_PERCEPTION_PROFILE` - added to `rs2_extension`, stream profile of a perception stream. Renamed from `RS2_EXTENSION_INFERENCE_PROFILE`.
  * `RS2_EXTENSION_PERCEPTION_SENSOR` - added to `rs2_extension`, sensor producing perception stream,. Renamed from `RS2_EXTENSION_INFERENCE_SENSOR`.
  * `RS2_OPTION_DETECTION_DISTANCE` - added to `rs2_option`, enable firmware calculation of per-detection distance (meters) on the object-detection stream.
  * `RS2_OPTION_READOUT_SHAPING` - added to `rs2_option`, IR/depth sensor readout shaping [0-100%]; higher values slow readout to avoid dropped frames.
* Removed APIs
  * `rs2_software_sensor_add_inference_stream` - renamed to `rs2_software_sensor_add_perception_stream`.
  * `rs2_software_sensor_add_inference_stream_ex` - renamed to `rs2_software_sensor_add_perception_stream_ex`.
  * `rs2_inference_stream` - renamed to `rs2_perception_stream`.
  * `RS2_EXTENSION_INFERENCE_FRAME` - renamed to `RS2_EXTENSION_PERCEPTION_FRAME`; C++ `rs2::inference_frame` renamed to `rs2::perception_frame`.
  * `RS2_EXTENSION_INFERENCE_PROFILE` - renamed to `RS2_EXTENSION_PERCEPTION_PROFILE`; C++ `rs2::inference_stream_profile` renamed to `rs2::perception_stream_profile`.
  * `RS2_EXTENSION_INFERENCE_SENSOR` - renamed to `RS2_EXTENSION_PERCEPTION_SENSOR`; C++ `rs2::inference_sensor` renamed to `rs2::perception_sensor`.
  * `RS2_EXTENSION_OBJECT_DETECTION_SENSOR` - removed along with the C++ `rs2::object_detection_sensor` class; use `rs2::perception_sensor` and check for `RS2_STREAM_OBJECT_DETECTION` profiles instead.



## Version [2.58.3](https://github.com/realsenseai/librealsense/releases/tag/v2.58.3) beta
* New APIs
    * `rs2_get_firmware_min_version` - Get the minimum firmware version supported by this device's SKU, with C++ `rs2::device::get_firmware_min_version()`.
    * `RS2_EMBEDDED_FILTER_TYPE_CLOSE_RANGE` - added to `rs2_embedded_filter_type`, Close Range embedded filter type.
    * `RS2_EXTENSION_CLOSE_RANGE_EMBEDDED_FILTER` - added to `rs2_extension`, extension to check if Close Range embedded filter is supported, with C++ `rs2::embedded_close_range_filter` class.
    * `RS2_OPTION_DISPARITY_SHIFT` - added to `rs2_option`, embedded filter: stereo disparity shift (pre-stream only).
    * `RS2_OPTION_DOWNSCALE_RATIO` - added to `rs2_option`, embedded filter: secondary-frame downscale ratio (pre-stream only).
    * `RS2_OPTION_THRESHOLD` - added to `rs2_option`, embedded filter: merge threshold in mm (pre-stream only).

## Version [2.58.2](https://github.com/IntelRealSense/librealsense/releases/tag/v2.58.2) beta
* New APIs
    * `rs2_get_frame_object_detection_count` - When called on Object Detection frame type, this method returns the number of detections in the frame.
    * `rs2_get_frame_object_detection` - When called on Object Detection frame type, this method returns a specific detection by index.
    * `rs2_create_nv12_decoder` - Creates NV12 decoder processing block.
    * `RS2_FORMAT_NV12` - added to rs2_format
    * `RS2_STREAM_OBJECT_DETECTION` - added to rs2_stream

## Version [2.58.1](https://github.com/IntelRealSense/librealsense/releases/tag/v2.58.1) beta
* New APIs
    * `RS2_CAMERA_INFO_MIPI_DRIVER_VERSION` - Added to RS2_CAMERA_INFO, user can query MIPI driver version when running on Nvidia Jetson with a GMSL camera.
    * `rs2_convert_bag_to_db3` - Added to allow users convert their existing ROS1 Bag files to new ROS2 format

## Version [2.57.7](https://github.com/IntelRealSense/librealsense/releases/tag/v2.57.7) beta
* New APIs
    * `RS2_CAMERA_INFO_IMU_TYPE` - Added to RS2_CAMERA_INFO, user can query the IMU HM (BMI version).

## Version [2.57.6](https://github.com/IntelRealSense/librealsense/releases/tag/v2.57.6) beta
* New APIs
    * `rs2_get_udp_ttl/rs2_set_udp_ttl` - Get/Set UDP packet "time to live" (TTL).

## Version [2.57.5](https://github.com/IntelRealSense/librealsense/releases/tag/v2.57.5) beta
* New APIs
    * `rs2_is_in_recovery_mode` - Check if a camera is in recovery mode.
    * `RS2_FRAME_METADATA_EMBEDDED_FILTERS` - Metadata Enum value that indicates which embedded filter is active.
    * `RS2_OPTION_EMBEDDED_FILTER_ENABLED` - Option Enum value that indicates which if some embedded filter is active.
    * `rs2_create_embedded_filter` - Create new embedded filter.
    * `rs2_delete_embedded_filter` - Delete embedded filter allocated by rs2_create_embedded_filter.
    * `rs2_get_embedded_filter_type` - Get type of embedded filter.
    * `rs2_get_embedded_filters_count` - Get number of embedded filters.
    * `rs2_is_embedded_filter_extendable_to` - Test if the given embedded filter can be extended to the requested extension.
    * `rs2_query_embedded_filters` - Query embedded filters from sensor.
    * `rs2_delete_embedded_filter_list` - Delete embedded filter list allocated by rs2_query_embedded_filters.
    * `RS2_EXTENSION_SUPPORTED_EMBEDDED_FILTERS` - Extension to check if embedded filters are supported.
    * `RS2_EXTENSION_DECIMATION_EMBEDDED_FILTER` - Extension to check if Decimation embedded filter is supported.
    * `RS2_EXTENSION_TEMPORAL_EMBEDDED_FILTER` - Extension to check if Temporal embedded filter is supported.
    * `rs2_embedded_filter_type` - Enum for embedded filters types.

## Version [2.57.3](https://github.com/IntelRealSense/librealsense/releases/tag/v2.57.3) beta
* New APIs
    * `RS2_EXTENSION_ETH_CONFIG` - Ethernet configuration extension for devices that support ethernet configuration.
    * `rs2_supports_eth_config` - Check if device supports the Ethernet Configuration feature
    * `rs2_get_link_speed` - Get Ethernet link speed, 0 if not linked
    * `rs2_get_link_priority` - Get current link priority setting
    * `rs2_set_link_priority` - Set link priority
    * `rs2_get_link_timeout` - Get current link timeout setting in milliseconds
    * `rs2_set_link_timeout` - Set link timeout in milliseconds
    * `rs2_get_dds_domain` - Get current DDS domain (0-232)
    * `rs2_set_dds_domain` - Set DDS domain (0-232)
    * `rs2_get_ip_address` - Get current IP address
    * `rs2_set_ip_address` - Set IP address
    * `rs2_get_netmask` - Get current network mask
    * `rs2_set_netmask` - Set network mask
    * `rs2_get_gateway` - Get current gateway address
    * `rs2_set_gateway` - Set gateway address
    * `rs2_get_dhcp_config` - Get DHCP configuration
    * `rs2_set_dhcp_config` - Set DHCP configuration
    * `rs2_get_mtu` - Get current MTU (Maximum Transmission Unit)
    * `rs2_set_mtu` - Set current MTU (Maximum Transmission Unit)
    * `rs2_get_transmission_delay` - Get current transmission delay
    * `rs2_set_transmission_delay` - Set current transmission delay
    * `rs2_restore_default_eth_config` - Restores configuration to factory settings
    * `rs2_create_m420_decoder` - Creates M420 decoder processing block

## Version [2.56.4](https://github.com/IntelRealSense/librealsense/releases/tag/v2.56.4)
* New APIs
    * `rs2_get_stream_profile_name` - Allows the user to get the required profile name as a string.
    * `RS2_CAMERA_INFO_CONNECTION_TYPE` - New camera info to distinguish between USB / MIPI / DDS connection type, see `rs2_camera_info`
* Changed APIs
    * `rs2_create_rotation_filter_block` is extended to support multiple stream types (color / depth / infrared) see new parameter `rs2_streams_list streams_to_rotate`

## Version [2.56.3](https://github.com/IntelRealSense/librealsense/releases/tag/v2.56.3) beta
_Please note that API changes in beta versions may not be final._
* New APIs
    * `rs2_hw_monitor_get_opcode_string` that allow the user that use raw HW monitor commands convert the error respond code to a string.

## Version [2.56.1](https://github.com/IntelRealSense/librealsense/releases/tag/v2.56.1) beta
Please note that API changes in beta versions may not be final.
* added `RS2_OPTION_REGION_OF_INTEREST` and its type, `RS2_OPTION_TYPE_RECT` and `rs2_option_rect`
* New APIs
    * `rs2_start_collecting_fw_logs` and `rs2_stop_collecting_fw_logs` to start/stop FW logging for a specific device, with C++ `rs2::device::start_collecting()` and `stop_collecting()`
    * `rs2_get_fw_log_parsed_module_name` to get the FW log parsed message module name
    * `rs2_pipeline_set_device` to set the device used by the pipeline, with C++ `rs2::pipeline::set_device()`
    * `rs2_get_calibration_config` and `rs2_set_calibration_config` to get & set calibration configuration using JSON
    * `rs2::motion_frame::get_combined_motion_data()` can be used for access to the plain motion data used with `RS2_STREAM_MOTION` and `RS2_FORMAT_COMBINED_MOTION`


## Version [2.55.1](https://github.com/IntelRealSense/librealsense/releases/tag/v2.55.1)
* `RS2_FRAME_METADATA_ACTUAL_FPS` better reflects actual HW FPS (based on timestamp & frame counts; same as shown in the Viewer as "Hardware FPS") but with better precision, with each unit being 0.001 FPS (see [#12326](https://github.com/IntelRealSense/librealsense/pull/12326))
* When a context is deleted any callback set with `rs2_set_devices_changed_callback` will no longer get called (see [#12275](https://github.com/IntelRealSense/librealsense/pull/12275))
* In the Python wrapper, `config.enable_stream()` no longer has default arguments (see [#11861](https://github.com/IntelRealSense/librealsense/pull/11861))
* New APIs
    * `rs2_create_context_ex` accepts a JSON configuration to use (see [#10386](https://github.com/IntelRealSense/librealsense/pull/10386)), which can override the default one now loaded from `RS2_CONFIG_FILENAME` (realsense-config.json already used by the Viewer; see [#12432](https://github.com/IntelRealSense/librealsense/pull/12432))
    * `rs2_device_is_connected` is the same as `rs2_device_hub_is_device_connected`, but does not need a `rs2_device_hub` (see [#12240](https://github.com/IntelRealSense/librealsense/pull/12240))
    * Added options-changed callbacks via `rs2_set_options_changed_callback`, periodically (by default, every second) querying all options in the sensor and triggering a callback (see [12483](https://github.com/IntelRealSense/librealsense/pull/12483))
        * The resulting `rs2_options_list` can then be used with `rs2_get_option_value_from_list`, returning `rs2_option_value` comprising an (id, type, and value) (see [#12616](https://github.com/IntelRealSense/librealsense/pull/12616))
        * Options IDs can now be queried by name via `rs2_option_from_string`, the reverse of `rs2_option_to_string` (see [#12163](https://github.com/IntelRealSense/librealsense/pull/12163))
        * Related APIs include `rs2_get_option_value`, `rs2_delete_option_value`, `rs2_set_option_value`, `rs2_option_type_to_string`
        * Related types include `rs2_option_type` enum, `rs2_option_value`
    * Added `rs2_gyro_sensitivity_to_string`, `RS2_OPTION_GYRO_SENSITIVITY`, `rs2_gyro_sensitivity` enum
    * C++ (`rs2::`)
        * Added `context( uninitialized )` constructor syntax to verbosely construct an uninitialized context
        * Added `context( char const * json_settings )` constructor to go with `rs2_create_context_ex`
        * Added `context::operator bool()`
        * Added `device::get_type()`, `::get_description()` to somewhat "standardize" how devices are reported in a common place
        * Added `device::operator<()` to properly order devices
        * Added `device::is_connected()`, calling `rs2_device_is_connected`
        * Added try-catch block inside `calibration_change_callback::on_calibration_change`
        * Added `option_value` class managing `rs2_option_value` lifetime
        * Added `options_list` class managing `rs2_options_list` lifetime, iteration over which results in `option_value`
        * Added `options::get_option_value()` and `::set_option_value()`, calling `rs2_get_option_value` and `rs2_set_option_value`
        * Added `options::get_supported_option_values()`, similar to `get_supported_options()` but returning an `options_list` rather than a vector of option-IDs
        * Added `options::on_options_changed`, making use of new `options_changed_callback`
        * Added `rs2_frame_callback_sptr`, `rs2_frame_processor_callback_sptr`, `rs2_notifications_callback_sptr`, `rs2_software_device_destruction_callback_sptr`, `rs2_log_callback_sptr`, `rs2_calibration_change_callback_sptr`, `rs2_devices_changed_callback_sptr`, `rs2_playback_status_changed_callback_sptr`, `rs2_update_progress_callback_sptr`, `rs2_options_changed_callback` struct and `rs2_options_changed_callback_sptr` shared_ptr typedefs
* Removed APIs
    * `rs2_get_dsm_params` & `rs2_override_dsm_params`, both of which were never really in use, alogside `rs2_dsm_params`, `rs2_dsm_correction_model`
    * Removed `rs2::zero_order_invalidation` filter
    * Removed `rs2::calibrated_sensor` class
- Added `RS2_CONFIG_FILENAME` define containing the filename of the RealSense configuration file (under `AppData` for Windows, and hidden under the user's home directory with Linux)


## Version [2.54.1](https://github.com/IntelRealSense/librealsense/releases/tag/v2.54.1)
* Firmware versions reported by librealsense will no longer have leading zeros; i.e., `05.14.0.0` will now be `5.14.0.0`
* Sqlite, originally meant for use in internal unit-tests, has not been in used for a while and has been removed, and with it `rs2::recording_context` and `rs2::mock_context`


## Version [2.53.1](https://github.com/IntelRealSense/librealsense/releases/tag/v2.53.1)
Functionality:
- OCC/Tare Host-Assistance (HA) mode
	- Calibration algorithms used in both regular and HA modes are similar, however the differences in implementation require minor API adaptation:
	- HA mode shall be applied for RealSense MIPI cameras only; RealSense USB camera shall continue to use existing (on-chip) calibration mode
	- The selection of the HA/Regular calibration mode is done via [calibration Json file](https://github.com/IntelRealSense/librealsense/blob/253/wrappers/python/examples/auto_calibration_config.json) via a new attribute
		- The presence of `host-assistance` parameter is not mandatory for USB cameras
	- Modified Tare calibration routine now also extracts the camera health metric before and after the calibration, to facilitate the user decision and provision for unattended execution/automation
	- OCC/Tare calibration routines the camera shall be configured as following:
		- Depth 1280X720@30FPS
	* For details please refer to [example code](https://github.com/IntelRealSense/librealsense/blob/253/wrappers/python/examples/depth_ucal_example.py)
	- Tare calibration API `rs2_run_tare_calibration` arguments have changed and therefore break backwards compatibility 
 

## Version [2.51.1](https://github.com/IntelRealSense/librealsense/releases/tag/v2.51.1)
Functionality:
 - [rs2_build_debug_protocol_command](https://github.com/IntelRealSense/librealsense/blob/v2.51.1/include/librealsense2/h/rs_device.h#L97) - See [#10261](https://github.com/IntelRealSense/librealsense/pull/10261) and [#10279](https://github.com/IntelRealSense/librealsense/pull/10279) - this API, and `rs2::device::build_command()`, generates a raw data cmd from opcode, params, and data, to help with `rs2_send_and_receive_raw_data`
 

## Version [2.50.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.50.0)
Functionality:
 - [rs2_run_focal_length_calibration_cpp](https://github.com/IntelRealSense/librealsense/blob/v2.50.0/include/librealsense2/h/rs_device.h#L471) - [UCAL] Perform Target-based Focal Length calibration (UCAL). The API call expect to receive Y8 frames gathered from Left and Right IR/Intensity sensors, to find and extract the targets captured by the camera, and then to readjust the focal length of the lef/right sensors. The output is the new calibration table ready to be flashed to the device. The user can pass on a callback that will get the algorithm processing status in percentage. Follow the Release Notes for v2.50.0 and read the accompanying White Paper for in-depth discussion of the camera intrinsic properties  and the provided calibration methods
 - [rs2_run_focal_length_calibration](https://github.com/IntelRealSense/librealsense/blob/v2.50.0/include/librealsense2/h/rs_device.h#L488) - [UCAL]  C-language version of the above call
 - [rs2_run_uv_map_calibration_cpp](https://github.com/IntelRealSense/librealsense/blob/v2.50.0/include/librealsense2/h/rs_device.h#L503) - [UCAL] UV-Mapping calibration is a provisional API for future enhancements (UCAL).
 - [rs2_run_uv_map_calibration](https://github.com/IntelRealSense/librealsense/blob/v2.50.0/include/librealsense2/h/rs_device.h#L519) - [UCAL] UV-Mapping calibration is a provisional API for future enhancements.
 - [rs2_calculate_target_z_cpp](https://github.com/IntelRealSense/librealsense/blob/v2.50.0/include/librealsense2/h/rs_device.h#L531) - [UCAL] The API call accepts frame collected by the Left sensor that include Y8 stream data, extracts the target that was captured by the camera and provides the estimated distance to the target's plane. Note that the accuracy of the algorithm is bounded by the precision of the target dimensions measured and provided by the customer
 - [rs2_calculate_target_z](https://github.com/IntelRealSense/librealsense/blob/v2.50.0/include/librealsense2/h/rs_device.h#L545) - C-language API call for the above algorithm
 - [rs2_frame_queue_size](https://github.com/IntelRealSense/librealsense/blob/v2.50.0/include/librealsense2/h/rs_processing.h#L176) - Retrieve the number of frames that the queue holds.
 - [rs2_matchers_to_string](https://github.com/IntelRealSense/librealsense/blob/v2.50.0/include/librealsense2/h/rs_types.h#L255) -Auxiliary function utilized by the Python wrapper

Enumerations:
 - [RS2_CALIB_TARGET_ROI_RECT_GAUSSIAN_DOT_VERTICES](https://github.com/IntelRealSense/librealsense/blob/v2.50.0/include/librealsense2/h/rs_frame.h#L77) 
 - [RS2_CALIB_TARGET_POS_GAUSSIAN_DOT_VERTICES](https://github.com/IntelRealSense/librealsense/blob/v2.50.0/include/librealsense2/h/rs_frame.h#L78) 
- Printed Target types in use by UCAL

Options:
 - [RS2_OPTION_AUTO_EXPOSURE_LIMIT_TOGGLE](https://github.com/IntelRealSense/librealsense/blob/v2.50.0/include/librealsense2/h/rs_option.h#L116) - Toggles D400 series Auto-exposure limit on/off. When both this and Auto-Exposure controls are set on, changing the value of `RS2_OPTION_AUTO_EXPOSURE_LIMIT` would set the upper bound of exposure values in FW, allowing to keep the required FPS rate.
 - [RS2_OPTION_AUTO_GAIN_LIMIT_TOGGLE](https://github.com/IntelRealSense/librealsense/blob/v2.50.0/include/librealsense2/h/rs_option.h#L117) - Toggles D400 series Auto-gain limit on/off. When both this and Auto-Exposure controls are set on, changing the value of `RS2_OPTION_AUTO_GAIN_LIMIT` would set the upper bound of digital gain values in FW, preventing over-saturation.


## Version [2.47.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.47.0)
Functionality:
- [rs2_check_firmware_compatibility](https://github.com/IntelRealSense/librealsense/blob/v2.47.0/include/librealsense2/h/rs_device.h#L223-L231) - Check the FW image provided by the users and the target device. Prevent flashing devices with incompatible FW images to prevent bricking and malfunctioning. In case of incompatible match a user warning is issued and the DFU process is interrupted.
Note that the protection is not invoked to devices that were originally enumerated in recovery (DFU) mode

New options:
 - [RS2_OPTION_AUTO_RX_SENSITIVITY,RS2_OPTION_TRANSMITTER_FREQUENCY](https://github.com/IntelRealSense/librealsense/blob/v2.47.0/include/librealsense2/h/rs_option.h#L112-L113) - L515-specific options.
- Enable receiver sensitivity according to ambient light, bounded by the Receiver Gain control
- Changes the transmitter frequencies increasing effective range over sharpness
Experimental options deprecated:
 Options:
 - [RS2_OPTION_TRIGGER_CAMERA_ACCURACY_HEALTH,RS2_OPTION_RESET_CAMERA_ACCURACY_HEALTH](https://github.com/IntelRealSense/librealsense/blob/v2.47.0/include/librealsense2/h/rs_option.h#L98-L99) - L515-specific.
The two options are rendered not functional as of v2.46.0 of the SDK.
 Enum: `rs2_cah_trigger`
 Function: `rs2_cah_trigger_to_string`


## Version [2.45.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.45.0)
Documentation updates only

## Version [2.43.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.43.0)
Class:
 - [calibration_change_device](https://github.com/IntelRealSense/librealsense/blob/v2.43.0/include/librealsense2/hpp/rs_device.hpp#L572) - is the new base class that separates calibration updates observer and calibration invocation.
Casting `device` to the type allows to utilize the extension interface and register to thermal adjustment notifications.
- [video_stream_profile](https://github.com/IntelRealSense/librealsense/blob/v2.43.0/include/librealsense2/hpp/rs_frame.hpp#L247-L252) - Adding comparison operator for `video_profile` that takes into account stream resolution

Types:
 - [RS2_CALIBRATION_THERMAL](https://github.com/IntelRealSense/librealsense/blob/v2.43.0/include/librealsense2/h/rs_device.h#L348) - **[D400]** Calibration type supported by selected Realsense Stereo devices.
 - [RS2_EXTENSION_CALIBRATION_CHANGE_DEVICE](https://github.com/IntelRealSense/librealsense/blob/v2.43.0/include/librealsense2/h/rs_types.h#L223)
**[D400]** - Extension API allowing to register user-defined callbacks for notifications of Thermal adjustments invocation. These callbacks are informative in their nature and can be used for environmental monitoring.

## Version [2.42.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.42.0)
Functions:
 - [rs2_reset_logger](https://github.com/IntelRealSense/librealsense/blob/master/include/librealsense2/rs.h#L83) - Cleanup logger selection.
 - [rs2_enable_rolling_log_file](https://github.com/IntelRealSense/librealsense/blob/master/include/librealsense2/rs.h#L83) - Upon reaching a certain limit close and remove the old log and start writing to a new file.
   Requires permissions to remove/rename files in log file directory.

Options:
 - [RS2_OPTION_AUTO_EXPOSURE_LIMIT](https://github.com/IntelRealSense/librealsense/blob/53/include/librealsense2/h/rs_sensor.h#L90) - **[D400]** Limit maximal exposure used in Auto-Exposure to preserve FPS.
 - [RS2_OPTION_AUTO_GAIN_LIMIT](https://github.com/IntelRealSense/librealsense/blob/53/include/librealsense2/h/rs_types.h#L216) - **[D400]** Limit maximal digital gain value used in Auto-Exposure.

## Version [2.41.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.41.0)
Functions:
 - [rs2_get_debug_stream_profiles](https://github.com/IntelRealSense/librealsense/blob/53/include/librealsense2/h/rs_sensor.h#L334) - Retrieve list of debug stream profiles that given sub-device can provide.
Types:
 - [RS2_FORMAT_FG](https://github.com/IntelRealSense/librealsense/blob/53/include/librealsense2/h/rs_sensor.h#L90) - **[L515]** 16-bit per-pixel Frame Grabber format.
 - [RS2_EXTENSION_DEBUG_STREAM_SENSOR](https://github.com/IntelRealSense/librealsense/blob/53/include/librealsense2/h/rs_types.h#L216) - Auxiliary type

 
## Version [2.40.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.40.0)
Functions:
 - [rs2_get_number_of_fw_logs](https://github.com/IntelRealSense/librealsense/blob/v2.40.0/include/librealsense2/h/rs_internal.h#L457) - Returns number of fw logs already polled from device waiting to be parsed by user.
 - [rs2_get_fw_log_parsed_sequence_id](https://github.com/IntelRealSense/librealsense/blob/v2.40.0/include/librealsense2/h/rs_internal.h#L457) - Read logger's sequence id - a cyclic number set by FW in [0..15] range
 
  - [rs2_get_max_usable_depth_range](https://github.com/IntelRealSense/librealsense/blob/v2.40.0/include/librealsense2/h/rs_sensor.h#L655) - **[L515]** Assess the maximum range of the camera given the amount of ambient light in the scene

Types:
 - [RS2_OPTION_DIGITAL_GAIN](https://github.com/IntelRealSense/librealsense/blob/v2.40.0/include/librealsense2/h/rs_option.h#L94) - **[L515]** The option tag `RS2_OPTION_AMBIENT_LIGHT` is deprecated for L515 and replaced by the mentioned enum. The previous value is left intact for back-compatibility
 - [RS2_OPTION_HUMIDITY_TEMPERATURE](https://github.com/IntelRealSense/librealsense/blob/v2.40.0/include/librealsense2/h/rs_option.h#L105) - **[L515]** Humidity sensor temperature reading 
 - [RS2_OPTION_ENABLE_MAX_USABLE_RANGE](https://github.com/IntelRealSense/librealsense/blob/v2.40.0/include/librealsense2/h/rs_option.h#L106) - **[L515]** Turn on/off the maximum usable depth sensor range given the amount of ambient light in the scene
 - [RS2_OPTION_ALTERNATE_IR](https://github.com/IntelRealSense/librealsense/blob/v2.40.0/include/librealsense2/h/rs_option.h#L107) - **[L515]** Toggle the alternate IR, When enabling alternate IR, the IR image is holding the amplitude of the depth correlation.
 - [RS2_OPTION_NOISE_ESTIMATION](https://github.com/IntelRealSense/librealsense/blob/v2.40.0/include/librealsense2/h/rs_option.h#L108) - **[L515]** Indicates the noise level in the IR image.
 - [RS2_OPTION_ENABLE_IR_REFLECTIVITY](https://github.com/IntelRealSense/librealsense/blob/v2.40.0/include/librealsense2/h/rs_option.h#L109) - **[L515]** Enables data collection for calculating IR pixel reflectivity.
- [rs2_digital_gain](https://github.com/IntelRealSense/librealsense/blob/v2.40.0/include/librealsense2/h/rs_option.h#L180-L185) enumeration - provides the range of values applicable for `RS2_OPTION_DIGITAL_GAIN.
 - [RS2_EXTENSION_MAX_USABLE_RANGE_SENSOR](https://github.com/IntelRealSense/librealsense/blob/v2.40.0/include/librealsense2/h/rs_types.h#L215) - **[L515]** Auxiliary type


 ## Version [2.39.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.39.0)
Functions:
 - [rs2_create_hdr_merge_processing_block](https://github.com/IntelRealSense/librealsense/blob/v2.39.0/include/librealsense2/h/rs_processing.h#L259) - Depth HDR Post-processing module (Alpha).
 - [rs2_create_sequence_id_filter](https://github.com/IntelRealSense/librealsense/blob/v2.39.0/include/librealsense2/h/rs_processing.h#L266) - Processing module that filters frame according to metadata attributes. (Alpha)
 - [rs2_host_perf_mode_to_string](https://github.com/IntelRealSense/librealsense/blob/v2.39.0/include/librealsense2/h/rs_option.h#L191)  - Utility function  

Options:
- [RS2_OPTION_HOST_PERFORMANCE](https://github.com/IntelRealSense/librealsense/blob/v2.39.0/include/librealsense2/h/rs_option.h#L99) - Presets low-level USB transactions sizes for selected devices (L515).
- [RS2_OPTION_HDR_ENABLED](https://github.com/IntelRealSense/librealsense/blob/v2.39.0/include/librealsense2/h/rs_option.h#L100) - Toggles Depth HDR functionality for supported devices (D400 with FW 5.12.8.200+)
- [RS2_OPTION_SEQUENCE_NAME](https://github.com/IntelRealSense/librealsense/blob/v2.39.0/include/librealsense2/h/rs_option.h#L101) - Sets Sequence ID to identify the HDR preset configuration
- [RS2_OPTION_SEQUENCE_SIZE](https://github.com/IntelRealSense/librealsense/blob/v2.39.0/include/librealsense2/h/rs_option.h#L102) - Controls the HDR configuration size (currently preset to 2)
- [RS2_OPTION_SEQUENCE_ID](https://github.com/IntelRealSense/librealsense/blob/v2.39.0/include/librealsense2/h/rs_option.h#L103) - Specifies the active configuration item for querying/setting values for HDR preset

Enumerations:
- [rs2_host_perf_mode](https://github.com/IntelRealSense/librealsense/blob/51/include/librealsense2/h/rs_option.h#L184-L191) - Host/Device configuration mode that modifies USB transactions size of transport layer. Allows to optimize traffic balance and improve frame drops on low-end hosts
- [RS2_FRAME_METADATA_SEQUENCE_NAME,\nRS2_FRAME_METADATA_SEQUENCE_ID, \nRS2_FRAME_METADATA_SEQUENCE_SIZE](https://github.com/IntelRealSense/librealsense/blob/v2.39.0/include/librealsense2/h/rs_frame.h#L65-L67) - Metadata attributes utilized in HDR preset mode, supported by D400/FW 5.12.8.200+.

Extension types:
- [RS2_EXTENSION_HDR_MERGE, RS2_EXTENSION_SEQUENCE_ID_FILTER,](https://github.com/IntelRealSense/librealsense/blob/51/include/librealsense2/h/rs_types.h#L213-L214) - New Processing Block types registration



 ## Version [2.38.1](https://github.com/IntelRealSense/librealsense/releases/tag/v2.38.1)
 ### Calibration APIs

 - [rs2_cah_trigger_to_string](https://github.com/IntelRealSense/librealsense/blob/v2.38.1/include/librealsense2/h/rs_option.h#L176) - Utility function
 - [rs2_cah_trigger](https://github.com/IntelRealSense/librealsense/blob/v2.38.1/include/librealsense2/h/rs_option.h#L169-L175) - enumeration. L515-specific

 ## Version [2.37.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.37.0)
 
  - [RS2_FRAME_METADATA_GPIO_INPUT_DATA](https://github.com/IntelRealSense/librealsense/blob/v2.37.0/include/librealsense2/h/rs_frame.h#L64) - Metadata attribute for GPIO state. Applicable for selected SKUs.
 - [points pointcloud::calculate(frame depth) const](
https://github.com/IntelRealSense/librealsense/blob/v2.37.0/include/librealsense2/hpp/rs_processing.hpp#L431) - to use const modifier.


## Version [2.36.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.36.0)

### Calibration APIs
 
 Types:
 - [rs2_calibration_type](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_device.h#L318-L322) - [Enum] Supported Calibration types
 - [rs2_calibration_status](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_device.h#L328-L344) - [Enum]  Calibration result
 - [rs2_calibration_change_callback](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_device.h#L347) - [Struct] Calibration Change
 - [RS2_OPTION_TRIGGER_CAMERA_ACCURACY_HEALTH](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_option.h#L97)[RS2_OPTION_RESET_CAMERA_ACCURACY_HEALTH](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_option.h#L98) - [Option] Provisions for Camera Accuracy Health routine.
 - [rs2_dsm_params](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_types.h#L73-L86) - [Struct] DSM (Digital Sync Module) calibration parameters
 - [rs2_dsm_correction_model](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_types.h#L88-L94) - [Enum] Supported DSM model
 - [RS2_EXTENSION_FW_LOGGER](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_types.h#L209) - Device interface for accessing hardware logs
   [RS2_EXTENSION_AUTO_CALIBRATION_FILTER](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_types.h#L210) - Calibration Utility Interface
   [RS2_EXTENSION_DEVICE_CALIBRATION](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_types.h#L211) - Calibration Extension Interface
   [RS2_EXTENSION_CALIBRATED_SENSOR](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_types.h#L212) - Sensor Calibration Interface
 
API Calls:
- [rs2_register_calibration_change_callback](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_device.h#L357) - C-Callback to be invoked upon calibration completion .
 - [rs2_register_calibration_change_callback_cpp](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_device.h#L365) - CPP-Callback to be invoked upon calibration completion.
 - [rs2_trigger_device_calibration](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_device.h#L373) - Start calibration process.
  - [rs2_override_extrinsics](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_sensor.h#L476) - Replace existing extrinsic.
  - [rs2_override_intrinsics](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_sensor.h#L602) - Replace sensor's intrinsic. 
  - [rs2_get_dsm_params](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_sensor.h#L621) - Retrieve DSM parameters structure.
  - [rs2_override_dsm_params](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_sensor.h#L631) - Replace sensor's DSM parameters.
  - [rs2_reset_sensor_calibration](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_sensor.h#L639) - Reset sensor's DSM parameters.
  - [rs2_calibration_type_to_string](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_device.h#L323),[rs2_calibration_status_to_string](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_device.h#L345) - Utility functions.
  - [rs2_create_fw_log_message](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L357) - Allocate resources to hold FW log entry.
  - [rs2_get_fw_log](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L366) - Retrieve the most recent FW log message.
  - [rs2_get_flash_log](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L375) - Flash logs store data collected during the latest power cycle. The API retrieves a single message from that log stored on Flash.
  - [rs2_delete_fw_log_message](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L381) - Deallocate existing FW log entry.
  - [rs2_fw_log_message_data](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L389) - Retrieve the text message stored in a single FW log entry.
  - [rs2_fw_log_message_size](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L397) - Get size in bytes of the relevant FW log message.
  - [rs2_fw_log_message_timestamp](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L406)  - Get FW-assigned timestamp of the relevant FW log message.
  - [rs2_fw_log_message_severity](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L414) - Retrieve FW log severety level.
  - [rs2_init_fw_log_parser](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L423) - Initialize and load FW log parser with XML dictionary.
  - [rs2_create_fw_log_parsed_message](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L432) - Allocate resources to store and proceed a single FW log message.
  - [rs2_delete_fw_log_parsed_message](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L438) - Dealocate parsed message resources.
  - [rs2_parse_firmware_log](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L449) - Convert raw FW log message to human-readable format.
  - [rs2_get_fw_log_parsed_message](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L457) - Extract the core text from the parsed FW log message.
  - [rs2_get_fw_log_parsed_file_name](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L465) - Extract file name of the message's origin.
  - [rs2_get_fw_log_parsed_thread_name](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L473) - Get thread id. of the message's origin.
  - [rs2_get_fw_log_parsed_severity](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L481) - Read FW log's severety level.
  - [rs2_get_fw_log_parsed_line](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L489) - Retrieve the line number of the message's origin.
  - [rs2_get_fw_log_parsed_timestamp](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L497) - FW-assigned timestamp of the FW log entry.
  - [rs2_create_terminal_parser](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L505) - Create Terminal Command parser.
  - [rs2_delete_terminal_parser](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L511) - Delete Terminal object instance.
  - [rs2_terminal_parse_command](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L521) - Convert Terminal-generated command into binary format compatible with HW-monitor interface.
  - [rs2_terminal_parse_response](https://github.com/IntelRealSense/librealsense/blob/v2.36.0/include/librealsense2/h/rs_internal.h#L534) - Convert results obtained from FW into human-readable format.


## Version [2.35.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.35.0)

### D400
 - [RS2_OPTION_THERMAL_COMPENSATION](https://github.com/IntelRealSense/librealsense/blob/v2.35.0/include/librealsense2/h/rs_option.h#L96) - Provisioning for Thermal Compensation mechanism for selected SKUs.

## Version [2.34.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.34.0)

### Device over Ethernet:
 - [rs2_create_net_device](https://github.com/IntelRealSense/librealsense/blob/v2.34.0/include/librealsense2-net/rs_net.h#L24) - Establish connection to a hosts that runs Realsense server application and create a proxy camera class. The proxy then can be injected to Librealsense context and retrieved and queried as any other device instance (e.g  `query_devices()`).

 - [RS2_CAMERA_INFO_IP_ADDRESS](https://github.com/IntelRealSense/librealsense/blob/v2.34.0/include/librealsense2/h/rs_sensor.h#L36) - Stores the IP address of the remote server. The attribute is applicable for IP device only.
  
### Software Device
 - [rs2_get_active_streams](https://github.com/IntelRealSense/librealsense/blob/v2.34.0/include/librealsense2/h/rs_sensor.h#L333) - Utility API allows to query the streams currently generated by a given sensor
 - [rs2_allocate_synthetic_motion_frame](https://github.com/IntelRealSense/librealsense/blob/v2.34.0/include/librealsense2/h/rs_frame.h#L277) - Allows `Software-device` to produce inertial motion frames. 
 - [rs2_software_device_set_destruction_callback](https://github.com/IntelRealSense/librealsense/blob/v2.34.0/include/librealsense2/h/rs_internal.h#L224) - Execute user-defined code when `software_device` instance's destructor is invoked. The most-common usage is to de-allocate resources and notify user's 3rd-party modules.
- [rs2_software_device_set_destruction_callback_cpp](https://github.com/IntelRealSense/librealsense/blob/v2.34.0/include/librealsense2/h/rs_internal.h#L232) - The C++ version of the above API.
- [rs2_software_device_register_info](https://github.com/IntelRealSense/librealsense/blob/v2.34.0/include/librealsense2/h/rs_internal.h#L249) - Populate `software_device` with user-defined atrtibutes
- [rs2_software_device_update_info](https://github.com/IntelRealSense/librealsense/blob/v2.34.0/include/librealsense2/h/rs_internal.h#L258) - Override `software_device` attributes with user-defined values. The function shal throw if the attribute does not exist
- [rs2_software_sensor_on_notification](https://github.com/IntelRealSense/librealsense/blob/v2.34.0/include/librealsense2/h/rs_internal.h#L207) - Notification injection to emulate events produced by the external code
- [rs2_software_sensor_add_video_stream_ex](https://github.com/IntelRealSense/librealsense/blob/v2.34.0/include/librealsense2/h/rs_internal.h#L275) - Extends `rs2_software_sensor_add_video_stream` with the notion of `default_stream`. The `default_stream` attribute provides a hint for the SDK to activate the specific stream when user requests a parameter-less configuration, e.g. `pipe.start(void)`
- [rs2_software_sensor_add_motion_stream_ex](https://github.com/IntelRealSense/librealsense/blob/v2.34.0/include/librealsense2/h/rs_internal.h#L292) - Extends `rs2_software_sensor_add_motion_stream` with `default_stream` attribute.
- [rs2_software_sensor_add_pose_stream_ex](https://github.com/IntelRealSense/librealsense/blob/v2.34.0/include/librealsense2/h/rs_internal.h#L309) - Extends `rs2_software_sensor_add_pose_stream` with `default_stream` attribute.
- [rs2_software_sensor_add_option](https://github.com/IntelRealSense/librealsense/blob/v2.34.0/include/librealsense2/h/rs_internal.h#L340) - Add user-defined options to `software_device`
 - [rs2_software_sensor_detach](https://github.com/IntelRealSense/librealsense/blob/v2.34.0/include/librealsense2/h/rs_internal.h#L348) - Disconnect the lifetime of `software_sensor` from the embodied `software_device` in order to release the circular dependency. Follow the link for more details.
 - [rs2_software_notification](https://github.com/IntelRealSense/librealsense/blob/v2.34.0/include/librealsense2/h/rs_internal.h#L115) - used to pass user-defined data into the SDK's notifications mechanism.

### Depth Camera
 - [rs2_depth_frame_get_units](https://github.com/IntelRealSense/librealsense/blob/v2.34.0/include/librealsense2/h/rs_frame.h#L157) - Retrieves the Raw Depth -> Metric Depth scaling factor that was previously availabel only via the depth sensor's API.
 - [RS2_OPTION_EMITTER_ALWAYS_ON](https://github.com/IntelRealSense/librealsense/blob/v2.34.0/include/librealsense2/h/rs_option.h#L95) - Enable the projector to remain constantly on. Applies to D400 Global Shutter models (D43x) only.


## Version [2.33.1](https://github.com/IntelRealSense/librealsense/releases/tag/v2.33.1)
### Configuration Serialization:
 - [rs2_serialize_json](https://github.com/IntelRealSense/librealsense/blob/v2.32.1/include/librealsense2/h/rs_sensor.h#L547) - Serialize JSON content, returns ASCII-serialized JSON string on success
 - [rs2_load_json](https://github.com/IntelRealSense/librealsense/blob/v2.32.1/include/librealsense2/h/rs_sensor.h#L547) - Load JSON and apply advanced-mode controls
 
### Logging enhancement:
 - [rs2_log_to_callback](https://github.com/IntelRealSense/librealsense/blob/v2.33.1/include/librealsense2/rs.h#L90) - Propagate SDK-generated logs to user-provided callback function
 - [rs2_log_to_callback_cpp](https://github.com/IntelRealSense/librealsense/blob/v2.33.1/include/librealsense2/rs.h#L78) - Propagate SDK-generated logs using c++ callback object
 - [rs2_get_log_message_line_number](https://github.com/IntelRealSense/librealsense/blob/v2.33.1/include/librealsense2/rs.h#L83) - Retrieve the logged line number
 - [rs2_get_log_message_filename](https://github.com/IntelRealSense/librealsense/blob/v2.33.1/include/librealsense2/rs.h#L84) - Retrieve the logged file name
 - [rs2_get_raw_log_message](https://github.com/IntelRealSense/librealsense/blob/v2.33.1/include/librealsense2/rs.h#L85)
 - [rs2_get_full_log_message](https://github.com/IntelRealSense/librealsense/blob/v2.33.1/include/librealsense2/rs.h#L86)
 
### L500-specific:
Controls:
 - [RS2_OPTION_FREEFALL_DETECTION_ENABLED](https://github.com/IntelRealSense/librealsense/blob/2.33.1/include/librealsense2/h/rs_option.h#L87) - Toggle sensor free-fall protection mechanism (on by default)
 - [RS2_OPTION_AVALANCHE_PHOTO_DIODE](https://github.com/IntelRealSense/librealsense/blob/2.33.1/include/librealsense2/h/rs_option.h#L88) - Set the exposure interval for the receiver's APD
 - [RS2_OPTION_POST_PROCESSING_SHARPENING](https://github.com/IntelRealSense/librealsense/blob/2.33.1/include/librealsense2/h/rs_option.h#L89) - Set the sharpening level in post-processed phase  
 - [RS2_OPTION_PRE_PROCESSING_SHARPENING](https://github.com/IntelRealSense/librealsense/blob/2.33.1/include/librealsense2/h/rs_option.h#L90) - Set the sharpening level in pre-processed phase  
 - [RS2_OPTION_NOISE_FILTERING](https://github.com/IntelRealSense/librealsense/blob/2.33.1/include/librealsense2/h/rs_option.h#L91) - Control the edge thresholds for the noise filtering
 - [RS2_OPTION_INVALIDATION_BYPASS](https://github.com/IntelRealSense/librealsense/blob/2.33.1/include/librealsense2/h/rs_option.h#L92) - Toggle pixels invalidation processing  
 - [RS2_OPTION_AMBIENT_LIGHT](https://github.com/IntelRealSense/librealsense/blob/2.33.1/include/librealsense2/h/rs_option.h#L93) - Select the ambient light settings  
 - [RS2_OPTION_SENSOR_MODE](https://github.com/IntelRealSense/librealsense/blob/2.33.1/include/librealsense2/h/rs_option.h#L94) - Preset the scanning mode resolution
 
Presets:
	/** \brief For L500 devices: provides optimized settings (presets) for specific types of usage. */
 - [rs2_l500_visual_preset](https://github.com/IntelRealSense/librealsense/blob/2.33.1/include/librealsense2/h/rs_option.h#L134) - Provides optimized settings for specific types of usage

Option characterization:
 - [rs2_sensor_mode](https://github.com/IntelRealSense/librealsense/blob/2.33.1/include/librealsense2/h/rs_option.h#L147) - utilized by `RS2_OPTION_SENSOR_MODE`
 - [rs2_ambient_light](https://github.com/IntelRealSense/librealsense/blob/2.33.1/include/librealsense2/h/rs_option.h#L156) - utilized by `RS2_OPTION_AMBIENT_LIGHT`

Enum to string convenience APIs
 - [rs2_l500_visual_preset_to_string](https://github.com/IntelRealSense/librealsense/blob/2.33.1/include/librealsense2/h/rs_option.h#L144)
 - [rs2_sensor_mode_to_string](https://github.com/IntelRealSense/librealsense/blob/2.33.1/include/librealsense2/h/rs_option.h#L153)

## Version [2.32.1](https://github.com/IntelRealSense/librealsense/releases/tag/v2.32.1)
 - [rs2_remove_static_node](https://github.com/IntelRealSense/librealsense/blob/v2.32.1/include/librealsense2/h/rs_sensor.h#L547) - T265: Remove a named location tag
 - [rs2_create_huffman_depth_decompress_block](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_sensor.h#L566) - Generates a processing block dedicated for decoding `Z16H` compressed depth format.
 - [RS2_FORMAT_Z16H](https://github.com/IntelRealSense/librealsense/blob/v2.32.1/include/librealsense2/h/rs_sensor.h#L88) - Provision for `Z16H` format. A variable-length compression dedicated for 16-bit depth values using customized Huffman code implementation.
 - [RS2_FRAME_METADATA_RAW_FRAME_SIZE](https://github.com/IntelRealSense/librealsense/blob/v2.32.1/include/librealsense2/h/rs_frame.h#L63) - Metadata attribute for the size in bytes of the transmitted frame payload (w/o metadata header). For non-compressed video streams is equal to Width\*Height\*BytesPerPixel.
 - [RS2_EXTENSION_COLOR_SENSOR,  
RS2_EXTENSION_MOTION_SENSOR,  
RS2_EXTENSION_FISHEYE_SENSOR,  
RS2_EXTENSION_DEPTH_HUFFMAN_DECODER](https://github.com/IntelRealSense/librealsense/blob/v2.32.1/include/librealsense2/h/rs_types.h#L177-L180) - Extension types used for class instance identification/affiliation.

## Version [2.31.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.31.0)
D4xx Calibration routines:
https://github.com/IntelRealSense/librealsense/commit/e89e4d6477e619291789d5b2be141e55303687e5
 - [rs2_run_on_chip_calibration_cpp](https://github.com/IntelRealSense/librealsense/blob/v2.31.0/include/librealsense2/h/rs_device.h#L264) - Invoke built-in target-less calibration routine (with progress report)
 - [rs2_run_tare_calibration_cpp](https://github.com/IntelRealSense/librealsense/blob/v2.31.0/include/librealsense2/h/rs_device.h#L311) - Adjust camera distance w.r.t. flat target. Requires known ground-truth plane (with progress report)
 - [rs2_run_on_chip_calibration](https://github.com/IntelRealSense/librealsense/blob/v2.31.0/include/librealsense2/h/rs_device.h#L287) - Invoke built-in calibration routine
 - [rs2_run_tare_calibration](https://github.com/IntelRealSense/librealsense/blob/v2.31.0/include/librealsense2/h/rs_device.h#L336) - Adjust camera distance w.r.t. flat target. Requires known ground-truth plane  
 - [rs2_get_calibration_table](https://github.com/IntelRealSense/librealsense/blob/v2.31.0/include/librealsense2/h/rs_device.h#L342) - Read calibration table from device Flash
 - [rs2_set_calibration_table](https://github.com/IntelRealSense/librealsense/blob/v2.31.0/include/librealsense2/h/rs_device.h#L348) - Write calibration table to the device Flash

Metadata attributes:
 - `RS2_FRAME_METADATA_FRAME_LASER_POWER_MODE` has been superseded and will be gradually replaced by [RS2_FRAME_METADATA_FRAME_EMITTER_MODE](https://github.com/IntelRealSense/librealsense/blob/v2.31.0/include/librealsense2/h/rs_frame.h#L61) attribute
 - [RS2_FRAME_METADATA_FRAME_LED_POWER](https://github.com/IntelRealSense/librealsense/blob/v2.31.0/include/librealsense2/h/rs_frame.h#L62) value support has been added for selected SKUs.

Options:
 - [RS2_OPTION_LED_POWER](https://github.com/IntelRealSense/librealsense/blob/v2.31.0/include/librealsense2/h/rs_option.h#L84) - LED Power settings (mW)
 - [RS2_OPTION_ZERO_ORDER_ENABLED](https://github.com/IntelRealSense/librealsense/blob/v2.31.0/include/librealsense2/h/rs_option.h#L85) - L500-specific Depth post-processing filter control (on by default)
 - [RS2_OPTION_ENABLE_MAP_PRESERVATION](https://github.com/IntelRealSense/librealsense/blob/v2.31.0/include/librealsense2/h/rs_option.h#L86) - T265: Preserve previous map when starting
         
Raw Streaming Formats:
 - [RS2_FORMAT_Y8I](https://github.com/IntelRealSense/librealsense/blob/v2.31.0/include/librealsense2/h/rs_sensor.h#L83) - 8-bit per pixel interleaved. 8-bit left, 8-bit right
 - [RS2_FORMAT_Y12I](https://github.com/IntelRealSense/librealsense/blob/v2.31.0/include/librealsense2/h/rs_sensor.h#L84) - up to 12-bit intensity date per pixel, interleaved, packed into 24-bit word (little-endian)
 - [RS2_FORMAT_INZI](https://github.com/IntelRealSense/librealsense/blob/v2.31.0/include/librealsense2/h/rs_sensor.h#L85) - Multi-planar Depth 16bit + IR 10bit (SR3xx)
 - [RS2_FORMAT_INVI](https://github.com/IntelRealSense/librealsense/blob/v2.31.0/include/librealsense2/h/rs_sensor.h#L86) - 8-bit intensity stream (SR3xx)
 - [RS2_FORMAT_W10](https://github.com/IntelRealSense/librealsense/blob/v2.31.0/include/librealsense2/h/rs_sensor.h#L87) - 10 bit per pixel intensity data. Y10BPACK-complient [8,8,8,8,[2,2,2,2]]
	
 - [RS2_EXTENSION_AUTO_CALIBRATED_DEVICE](https://github.com/IntelRealSense/librealsense/blob/v2.31.0/include/librealsense2/h/rs_types.h#L176) - Enabled for D4xx devices that support auto-calibration
	
PLY Exporter customization
 - [OPTION_PLY_MESH](https://github.com/IntelRealSense/librealsense/blob/v2.31.0/include/librealsense2/hpp/rs_export.hpp#L47) - Store Depth as Pointcloud/Polygons (Mesh) selector.
 - [OPTION_PLY_BINARY](https://github.com/IntelRealSense/librealsense/blob/v2.31.0/include/librealsense2/hpp/rs_export.hpp#L48) - Selec ASCII/Binary representation
 - [OPTION_PLY_NORMALS](https://github.com/IntelRealSense/librealsense/blob/v2.31.0/include/librealsense2/hpp/rs_export.hpp#L49) - Generate normals per polygon vertex (applicable Mesh only)
 - [OPTION_PLY_THRESHOLD](https://github.com/IntelRealSense/librealsense/blob/v2.31.0/include/librealsense2/hpp/rs_export.hpp#L50) - Set the threshold for polygon generation


## Version [2.30.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.30.0)

  - [RS2_NOTIFICATION_CATEGORY_POSE_RELOCALIZATION](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_sensor.h#L566) - category added to `rs2_notification_category`. The notification designates T265 performing relocalization event, i.e. finding a match between the current position and a previously recorded map.


## Version [2.29.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.29.0)

Changed enum RS2_CAMERA_INFO_FIRMWARE_UPDATE_SERIAL_NUMBER to [RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_sensor.h#L35)

## Version [2.27.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.27.0)

* **T265 Calibration Write APIs**: Allow custom calibration to be provided to the device and committed to device flash memory. There is also a mechanism to roll-back to factory calibration. Currently these APIs are only implemented for the T265, but similar scheme is likely to be used for other families
  - [rs2_set_intrinsics](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_sensor.h#L555) - set video stream profile intrinsic calibration
  - [rs2_set_extrinsics](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_sensor.h#L566) - set extrinsic calibration between two stream profiles
  - [rs2_set_motion_device_intrinsics](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_sensor.h#L575) - set motion stream profile intrinsics
  - [rs2_reset_to_factory_calibration](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_device.h#L155) - Resets the device to factory calibration when available
  - [rs2_write_calibration](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_device.h#L162) - Commit calibration changes to device flash memory

* [rs2_clone_video_stream_profile](https://github.com/dorodnic/librealsense/blob/d98ab745c20ffeab9b23dd9ce30abd782c10fff2/include/librealsense2/h/rs_sensor.h#L372) - Utility function used when developing custom processing blocks that modify video stream resolution


## Version [2.26.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.26.0)

Firmware Update with non Digitally-signed images:
- [rs2_update_firmware_unsigned_cpp](https://github.com/IntelRealSense/librealsense/blob/v2.26.0/include/librealsense2/h/rs_device.h#L206) - Invoke Firmware Update flow using unsigned DFU image.
- [rs2_update_firmware_unsigned](https://github.com/IntelRealSense/librealsense/blob/v2.26.0/include/librealsense2/h/rs_device.h#L220) - Same as above for C-code users
- [rs2_get_frame_data_size](https://github.com/IntelRealSense/librealsense/blob/v2.26.0/include/librealsense2/h/rs_frame.h#L124) - The actual frame size may differ from the initially-negotiated max size. Required to properly support the compressed data streams, such as MJPEG.
- [RS2_FORMAT_MJPEG](https://github.com/IntelRealSense/librealsense/blob/v2.26.0/include/librealsense2/h/rs_sensor.h#L81) - Adding SDK support for MJPEG compressed stream type
- [RS2_OPTION_DEPTH_OFFSET](https://github.com/IntelRealSense/librealsense/blob/v2.26.0/include/librealsense2/h/rs_option.h#L83)- Provides the offset from the sensor to the depth origin for affected camera types.


## Version [2.25.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.25.0)

Introduce [T265-specific](https://github.com/IntelRealSense/librealsense/blob/d19829788008b8e000870895a068f0c43d58895a/doc/t265.md#are-there-any-t265-specific-options) options
- RS2_OPTION_ENABLE_MAPPING - Enable internal mapping generation required for location correction (feedback). Turning this option off will result in device running in an open loop.
- RS2_OPTION_ENABLE_RELOCALIZATION - Allow the device ti utilize the internal/stored map to correct the current location based on previously recorded data.
- RS2_OPTION_ENABLE_POSE_JUMPING - Allow the device to correct the location by making a discontinuous transformation (jump) 
- RS2_OPTION_ENABLE_DYNAMIC_CALIBRATION 
Read more in the above link

## Version [2.24.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.24.0)

* Firmware Update functionality
- Switch SR300 and D400 camera into DFU (Firmware Update) mode [rs2_enter_update_state](https://github.com/IntelRealSense/librealsense/blob/f112ed78e3917df79047254f864157195d4488b0/include/librealsense2/h/rs_device.h#L196).
When applied, the D400 camera will disconnect from host and reconnect as boot-loader device with a different VID/PID. 
- Perform Firmware Upgrade for D400/SR300 device [rs2_update_firmware](https://github.com/IntelRealSense/librealsense/blob/f112ed78e3917df79047254f864157195d4488b0/include/librealsense2/h/rs_device.h#L161-L170). The functionality requires a digitally-signed image to be provided as an input
- Perform Firmware Upgrade for D400/SR300 device, report progress to user-provided callback [rs2_update_firmware_cpp](https://github.com/IntelRealSense/librealsense/blob/f112ed78e3917df79047254f864157195d4488b0/include/librealsense2/h/rs_device.h#L150-L158)
- Generate a firmware image copy [rs2_create_flash_backup](https://github.com/IntelRealSense/librealsense/blob/f112ed78e3917df79047254f864157195d4488b0/include/librealsense2/h/rs_device.h#L182-L189). Note that the generated backup image format is different from the file required in `rs2_update_firmware/rs2_update_firmware_cpp` and cannot be used with these APIs.
- Generate a firmware image copy, report progress to user-provided callback [rs2_create_flash_backup_cpp](https://github.com/IntelRealSense/librealsense/blob/f112ed78e3917df79047254f864157195d4488b0/include/librealsense2/h/rs_device.h#L173-L179). The limitations are similar to `rs2_create_flash_backup`

## Version [2.23.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.23.0)

* Depth linearity enhancement - Mitigate the half-pixel disparity issue by adjusting the Amplitude factor in the modulation funciton ()
- Adding getter/setter for the Half-disparity modulation function control [rs2_set_amp_factor/rs2_get_amp_factor](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/rs_advanced_mode.h#L91-L94) into the Advanced-mode parameters block.
* Adding a new temperature sensor [`RS2_OPTION_APD_TEMPERATURE`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_option.h#L78) option.
* Enumerating Global Timer-supported sensors with explicit extension type [`RS2_EXTENSION_GLOBAL_TIMER`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_types.h#L170).

## Version [2.22.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.22.0)

* Global Camera Timestamp:
 - Add `rs2_timestamp_domain::RS2_TIMESTAMP_DOMAIN_GLOBAL_TIME` [enumeration type](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_frame.h#L23) that will be used as default when the HW timestamp is available via the appropriate metadata attribute.
 - Add `rs2_sensor* rs2_get_frame_sensor` [function](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_frame.h#L108)
 - Add `RS2_OPTION_GLOBAL_TIME_ENABLED` [option](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_option.h#L77)
* Depth units transformation - Processing block
 - `rs2_processing_block* rs2_create_units_transform` [function](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_processing.h#L69)
 - Add `rs2_format::RS2_FORMAT_DISTANCE` synthetic stream [format](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_sensor.h#L78)
 - Add `units_transform` [class](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/hpp/rs_processing.hpp#L524)

 - Add `rs2_error * rs2_create_error` [functionality](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_types.h#L240)
 - Add `pose_stream_profile` [class](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/hpp/rs_frame.hpp#L284) to handle T265 pose sensor.
 - Add `frame::get_sensor()` [functionality](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/hpp/rs_frame.hpp#L420)
 - Add ` sensor_from_frame(frame f)`  [functionality](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/hpp/rs_sensor.hpp#L332)


* GLSL Processing Blocks Module:
 - New header files with new API for GLSL-supported modules are added to [include/librealsense2-gl](https://github.com/IntelRealSense/librealsense/tree/development/include/librealsense2-gl) directory


## Version [2.20.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.20.0) to [2.21.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.21.0)

New IR format introduced to D415 - `W10`:
- Rectified, 10 bit per-pixels (packed) IR stream
- FullHD configuration only 30 fps
- Can be configured for use in conjunction with Depth stream (which is limited to 720p)
- Available as:
   - parsed 10 bit per pixel [RS2_FORMAT_Y10BPACK](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_sensor.h#L77)
   - Raw packed data (4pixel/5bytes) according to [V4L2-PIX-FMT-Y10BPACK](https://www.linuxtv.org/downloads/v4l-dvb-apis-old/V4L2-PIX-FMT-Y10BPACK.html) specification



## Version [2.19.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.19.0) to [2.20.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.20.0)

New Lens distortion model support added for T265 optical sensors -  
[RS2_DISTORTION_KANNALA_BRANDT4](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_types.h#L51).  
The model is utilized in [Project/Deproject](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/rsutil.h#L84) routines

[rs2_send_wheel_odometry](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_sensor.h#L525) - Wheel odometer API velocity type updated from angular to **linear**.


## From version [2.19.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.19.0) to [2.19.1](https://github.com/IntelRealSense/librealsense/releases/tag/v2.19.1)

No API changes

## From version [2.18.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.18.0) to [2.19.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.19.0)

### Added
#### Processing blocks/filters:
1. [`rs2_get_recommended_processing_blocks`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_sensor.h#L446) Retrieving an ordered list of processing blocks/filters that are recommended to be used for a specific sensor. For instance, for the Depth sensor the list may include (Decimation->Disparity->Spatial->Temporal->HoleFilling->Disparity``) sequence.
2. [`rs2_get_recommended_processing_blocks_count`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_sensor.h#L463) Retrieving the number of elements on the processing blocks list
3. [`rs2_get_processing_block` ](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_sensor.h#L455) Extracting a specific processing block from the list
4. [`rs2_delete_recommended_processing_blocks`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_sensor.h#L469) Deleting the processing blocks list. The function shall be used in conjunction with `rs2_get_recommended_processing_blocks` mentioned above to explicitly release the allocated resources.
5. [`rs2_supports_processing_block_info`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_processing.h#L254) Test for attribute availbility without throwing exception
6. [`rs2_get_processing_block_info`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_processing.h#L245) Retrieve processing block attribute
7. [`rs2_is_processing_block_extendable_to`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_processing.h#L263) Check API extension support
8. [`rs2_create_zero_order_invalidation_block`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_processing.h#L236) Adding zero order invalidation processing block


#### Options API:  
9. [`rs2_get_options_list`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_option.h#L146) Dynamic discovery of the options supported by a librealsense entity (sensor/processing_block/etc`).
10. [`rs2_get_options_list_size`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_option.h#L153) Retrieve the number of elements in the options list.
11. [`rs2_get_option_name`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_option.h#L162) Return human-readable option's name attribute.
12. [`rs2_get_option_from_list`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_option.h#L169) Return an indexed element from the lsit
13. [`rs2_delete_options_list`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_option.h#L175) Deleting the processing blocks list. The function shall be used in conjunction with `rs2_get_options_list` to explicitly release previously allocated resources.
14. Extending RS2_OPTIONS enumeration list with:  
[`RS2_OPTION_ZERO_ORDER_POINT_X`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_option.h#L71),
 [`RS2_OPTION_ZERO_ORDER_POINT_Y`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_option.h#L72),
 [`RS2_OPTION_LLD_TEMPERATURE`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_option.h#L73),
 [`RS2_OPTION_MC_TEMPERATURE`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_option.h#L74), 
[`RS2_OPTION_MA_TEMPERATURE`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_option.h#L75)  
  
#### Device management
15. [`rs2_get_stereo_baseline`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_sensor.h#L169) Retrieve Stereo-based Depth sensor baseline
17. [`rs2_context_add_software_device`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_context.h#L64) Inject a software (mockup) device into the SDK's context to be discoverable via `query_devices` API call.

#### T265-specific APIs
18. [`rs2_context_unload_tracking_module`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_option.h#L169) Unload all perviously acquired tracking device instances. A device query for T265 will automatically take ownership of the connected device.
This new API allows to explicitly release the devices so that they will be available for use by external processes.
19. [`rs2_export_localization_map`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_sensor.h#L489) Advanced feature that allows to export the localization map for later reuse.
20. [`rs2_import_localization_map`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_sensor.h#L479) Import previously-obtained localization map into device.
21. [`rs2_set_static_node`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_sensor.h#L500) Add a positional bookmark (name&location) for positional referencing.
22. [`rs2_get_static_node`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_sensor.h#L511) Retrieve previously stored bookmark position.
23. [`rs2_load_wheel_odometry_config`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_sensor.h#L517) Load the robot platform configuration and calibration data into device. The data includes the rigid body transformation as well as calibration parameters.
24. [`rs2_send_wheel_odometry`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_sensor.h#L525) Feed odometer data generated by a third-party sensor into the tracking device.

### To be deprecated
1. `rs2_option_to_string` - For existing options it will return option name, but for future API additions the user should call rs2_get_option_name instead


## From version [2.17.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.17.0) to [2.18.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.18.0)

### Added
1. [`rs2_create_yuy_decoder`](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_processing.h#L54) Adding YUY2 to RGB processing block [#3056](https://github.com/IntelRealSense/librealsense/pull/3056) :
2. [`rs2_create_error`](https://github.com/IntelRealSense/librealsense/blame/d39b5139ce62efffddee4cd11a87e8927ecdfa0c/src/api.h#L35) Exposing librealsense error to avoid cross-boundary new/delete operations:
3. [`rs2_create_threshold`](https://github.com/IntelRealSense/librealsense/blob/d39b5139ce62efffddee4cd11a87e8927ecdfa0c/include/librealsense2/h/rs_processing.h#L62) Adding depth min/max clamp filter (processing block):
4. [`RS2_OPTION_EMITTER_ON_OFF`](https://github.com/IntelRealSense/librealsense/pull/2829) - adding `RS2_OPTION_EMITTER_ON_OFF` to options enumeration
5. [`rs2_processing_block_register_simple_option`](https://github.com/IntelRealSense/librealsense/pull/2830/files#diff-c47ed929a30eaaa0bfce02ecdcfa2701R64) - adding ability to register custom processing block options
6. Adding [`save_to_ply`](https://github.com/IntelRealSense/librealsense/pull/2830/files#diff-883f598ceaa739d7473cdc28d7995e23R17) and [`save_single_frameset`](https://github.com/IntelRealSense/librealsense/pull/2830/files#diff-883f598ceaa739d7473cdc28d7995e23R161) processing blocks to C++ headers only (staging to be added to the API) under [rs_export.hpp](https://github.com/IntelRealSense/librealsense/blob/master/include/librealsense2/hpp/rs_export.hpp).

### Removed

1. [set_devices_changed_callback](https://github.com/IntelRealSense/librealsense/pull/2823) - was removed from C++ header files

## From version [2.16.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.16.0) to [2.17.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.17.0)

### Added

* [#2773](https://github.com/IntelRealSense/librealsense/pull/2773) introduced asynchronous pipeline API (recommended for high frequency data such as IMU). This change is limited to adding new overloads to `pipeline.start` method:
1. [rs2_pipeline_start_with_callback](https://github.com/matkatz/librealsense/blob/a2406a5c7713bb9de5cd0b28e8558e85a63a19b1/include/librealsense2/h/rs_pipeline.h#L147) - pipeline start with C function pointer and user data, similar to `sensor.start`
2. [rs2_pipeline_start_with_callback_cpp](https://github.com/matkatz/librealsense/blob/a2406a5c7713bb9de5cd0b28e8558e85a63a19b1/include/librealsense2/h/rs_pipeline.h#L160) - pipeline start with C++ frame callback object, similar to `sensor.start`
3. [rs2_pipeline_start_with_config_and_callback](https://github.com/matkatz/librealsense/blob/a2406a5c7713bb9de5cd0b28e8558e85a63a19b1/include/librealsense2/h/rs_pipeline.h#L180) - pipeline start with callback and config
4. [rs2_pipeline_start_with_config_and_callback_cpp](https://github.com/matkatz/librealsense/blob/a2406a5c7713bb9de5cd0b28e8558e85a63a19b1/include/librealsense2/h/rs_pipeline.h#L199) - pipeline start with callback and config (C++ frame callback object)

* [#2687](https://github.com/IntelRealSense/librealsense/pull/2687) introduces new API to control recording compression: 

1. [rs2_create_record_device_ex](https://github.com/belkinirena/librealsense/blob/9d8d3a690ebd5fd4b60c3d3c73a6e11e901f14bc/include/librealsense2/h/rs_record_playback.h#L49) - create recorder and explicitly enable or disable compression. By default, compression will be enabled based on device type. D435i and T265 devices that provide high FPS streams disable compression by default to avoid frame drops during recording. 

* [#2673](https://github.com/IntelRealSense/librealsense/pull/2673) adds API to generate IMU and pose data with `software_device`:

Added support for IMU stream and recording IMU frames in software sensor:

- stream_profile **add_motion_stream**(rs2_motion_stream motion_stream)
- rs2_stream_profile* **rs2_software_sensor_add_motion_stream**(rs2_sensor* sensor, rs2_motion_stream motion_stream, rs2_error** error);
- void **on_motion_frame**(rs2_software_motion_frame frame)
- void **rs2_software_sensor_on_motion_frame**(rs2_sensor* sensor, rs2_software_motion_frame frame, rs2_error** error);

Added support for pose stream and recording pose frames in software sensor:

- stream_profile **add_pose_stream**(rs2_pose_stream pose_stream)
- rs2_stream_profile* **rs2_software_sensor_add_pose_stream**(rs2_sensor* sensor, rs2_pose_stream pose_stream, rs2_error** error);
- void **on_pose_frame**(rs2_software_pose_frame frame)
- void **rs2_software_sensor_on_pose_frame**(rs2_sensor* sensor, rs2_software_pose_frame frame, rs2_error** error);

### Renamed

* [#2757](https://github.com/IntelRealSense/librealsense/pull/2757) is splitting C++ `processing_block` class into `processing_block` and `filter` classes, with `filter` being derived from `processing_block`.
`processing_block` abstraction offers `start` and `invoke` operations and does not guaranty results will be immediately available (`processing_block` can chose to **delay** frames). `filter` is special type of processing block that performs its operation immediately. This lets users compose filters using `apply_filter` operation. 

## From version [2.15.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.15.0) to [2.16.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.16.0)

### Added
* [rs2_project_color_pixel_to_depth_pixel](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/rsutil.h#L117) - map pixel in the color image to pixel in depth image

* [rs2_allocate_points](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_frame.h#L288) - allows the user to write custom processing block that outputs frame of type points

* [frame::apply_filter](https://github.com/IntelRealSense/librealsense/blob/v2.16.0/include/librealsense2/hpp/rs_frame.hpp#L509) - this method allows unified application of processing blocks, regardless of processing or frame type. This allows easy composition of processing blocks. 

### Removed

* [`processing_block::operator()(frame f) const`](https://github.com/IntelRealSense/librealsense/blob/v2.15.0/include/librealsense2/hpp/rs_processing.hpp#L151) was removed to reduce the overall ways processing block can be invoked. All processing blocks still contain `invoke` method for async processing (that sends the results to callback), and can be applied using added `apply_filter` method, in addition to helper methods like `colorize` and `calculate` specific to each block.

## From version [2.14.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.14.0) to [2.15.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.15.0)

### Added
* [rs2_query_devices_ex](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_context.h#L87) - provide a list of connected devices with user-specified mask. This allows to cherry-pick specific types (e.g D400/SR300) during device acquisition stage. 


## Version [2.14.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.14.0) vs [2.13.1](https://github.com/IntelRealSense/librealsense/releases/tag/v2.13.1)

### Added

* [rs2_try_wait_for_frame](https://github.com/IntelRealSense/librealsense/blob/master/include/librealsense2/h/rs_processing.h#L143), [rs2_pipeline_try_wait_for_frames](https://github.com/IntelRealSense/librealsense/blob/master/include/librealsense2/h/rs_pipeline.h#L91) - wait_for_frames overload that does not throw an exception on timeout

* [was_added](https://github.com/BrendanDrew/librealsense/blob/12b42d3eb8d6cd44167134737d96d1b06ea0e8eb/include/librealsense2/hpp/rs_context.hpp#L40) and [get_new_devices](https://github.com/BrendanDrew/librealsense/blob/12b42d3eb8d6cd44167134737d96d1b06ea0e8eb/include/librealsense2/hpp/rs_context.hpp#L57) methods added to [event_information](https://github.com/BrendanDrew/librealsense/blob/12b42d3eb8d6cd44167134737d96d1b06ea0e8eb/include/librealsense2/hpp/rs_context.hpp#L13) C++ class 

## Version [2.13.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.13.0) (vs [2.11.1](https://github.com/IntelRealSense/librealsense/releases/tag/v2.11.1))

### Added 

* [rs2_option::RS2_OPTION_INTER_CAM_SYNC_MODE](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_option.h#L66) - Master/Slave control for multi-cam setup synchronization.

* [rs2_frame_metadata_value](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_frame.h#L42-L59) - Extending available attributes:
    - Depth Sensor:
       - Laser Power, Laser Power Mode, Exposure Priority, Exposure ROI.
    - RGB Sensor:
       - Brightness, Contrast, Saturation, Sharpness, Backlight_Compensation, Hue, Gamma, White_Balance_Mode & Temperature, Powerline Frequency and Low Light Compensation.



## From [2.11.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.11.0) to [2.11.1](https://github.com/IntelRealSense/librealsense/releases/tag/v2.11.1)

### Modified

* [rs2_camera_info::RS2_CAMERA_INFO_RECOMMENDED_FIRMWARE_VERSION](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_sensor.h#L26) - Field was moved next to RS2_CAMERA_INFO_FIRMWARE_VERSION.


## From [2.10.4](https://github.com/IntelRealSense/librealsense/releases/tag/v2.10.4) to [2.11.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.11.0)

### Added 

* [rs2_create_processing_block_fptr](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_processing.h#L61) - Allows to create custom processing blocks using C-bindings (C, LabView, .NET)
* [rs2_start_processing_fptr](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_processing.h#L78) - Allows to start a processing block with a callback
* [rs2_config_enable_device_from_file_repeat_option](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_pipeline.h#L259) - Allows to configure pipeline to play from recording while controlling playback-repeat behavior 
* [rs2_create_hole_filling_filter_block](https://github.com/IntelRealSense/librealsense/blob/development/include/librealsense2/h/rs_processing.h#L177) - Hole-Filling filter supports three modes of operation:
  - Fill from left - fill the hole by the value from an immediate left neighbor
  - Fill from Far - select one of the up/down/left/right pixel neighbors farthest away from the camera
  - Fill from Near - select one of the up/down/left/right neighbors closest to the camera.  
  It is recommended to use this post-processing block last in the filters chain.
The functionality is integrated and can be reviewed in realsense-viewer/post-processing section


## From [2.10.3](https://github.com/IntelRealSense/librealsense/releases/tag/v2.10.3) to [2.10.4](https://github.com/IntelRealSense/librealsense/releases/tag/v2.10.4)

No API changes

## From [2.10.2](https://github.com/IntelRealSense/librealsense/releases/tag/v2.10.2) to [2.10.3](https://github.com/IntelRealSense/librealsense/releases/tag/v2.10.3)


### Modified
Adding `RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR` enumeration to detect USB2 vs USB3 mode (when supported by the firmware). 

## From [2.10.1](https://github.com/IntelRealSense/librealsense/releases/tag/v2.10.1) to [2.10.2](https://github.com/IntelRealSense/librealsense/releases/tag/v2.10.2)


### Modified
Adding `RS2_OPTION_AUTO_EXPOSURE_CONVERGE_STEP` enumeration to control the FishEye Auto-Exposure algorithm
The option is available for TM1-enabled devices only.


## From [2.10.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.10.0) to [2.10.1](https://github.com/IntelRealSense/librealsense/releases/tag/v2.10.1)

No API changes

## From [2.9.1](https://github.com/IntelRealSense/librealsense/releases/tag/v2.9.1) to [2.10.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.10.0)

### Added

* [rs2::frameset::get_infrared_frame()](https://github.com/IntelRealSense/librealsense/blob/master/include/librealsense2/hpp/rs_frame.hpp#L691) - Simpler way for users to get infrared frames from a `rs2::frameset`

### Changed

* Fixed [`rs2_set_devices_changed_callback`](https://github.com/IntelRealSense/librealsense/blob/master/include/librealsense2/h/rs_context.h#L42) - missing `const` specifier and `void** user` parameter were added.
* Change the 'Holes Filling' control for Temporal Post-Processing filter to be activated with `RS2_OPTION_HOLES_FILL` instead of `RS2_OPTION_FILTER_MAGNITUDE`

## From [2.9.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.9.0) to [2.9.1](https://github.com/IntelRealSense/librealsense/releases/tag/v2.9.1)

### Added 

* [rs2_keep_frame](https://github.com/IntelRealSense/librealsense/blob/master/include/librealsense2/hpp/rs_frame.hpp#L269) - this function can be used to preserve specific frame for longer processing. Calling it signals the intention to not return this frame to the pool within next 100ms. 

## From [2.8.3](https://github.com/IntelRealSense/librealsense/releases/tag/v2.8.3) to [2.9.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.9.0)

### Added
* [rs2_create_disparity_transform_block](https://github.com/IntelRealSense/librealsense/blob/v2.9.0/include/librealsense2/h/rs_processing.h#L152) A depth data conversion class that transforms depth data info/from disparity domain for stereo-based depth sensors (D400 series). This functionality allows to run post-processing filters in disparity domain to enhance the filtered outcome.

* [rs2_depth_stereo_frame_get_baseline](https://github.com/IntelRealSense/librealsense/blob/v2.9.0/include/librealsense2/h/rs_sensor.h#L158) Retrieve the stereoscopic baseline in mm for stereo-based depth camera.

* [rs2_export_to_ply](https://github.com/IntelRealSense/librealsense/blob/v2.9.0/include/librealsense2/h/rs_frame.h#L171)
Making the export functionality publicly available, also addressing [#862](https://github.com/IntelRealSense/librealsense/issues/862)



## From [2.8.2](https://github.com/IntelRealSense/librealsense/releases/tag/v2.8.2) to [2.8.3](https://github.com/IntelRealSense/librealsense/releases/tag/v2.8.3)

### Added
* [rs2_log](https://github.com/IntelRealSense/librealsense/blob/v2.8.3/include/librealsense2/rs.h#L80) - Usability function that allows the user to add logs to librealsense internal logger. The feature is useful in debugging and profiling scenarios.

Post-processing depth filters:
* [rs2_create_decimation_filter_block](https://github.com/IntelRealSense/librealsense/blob/v2.8.3/include/librealsense2/h/rs_processing.h#L133) - Down-sampling filter that effectively reduces the depth map resolution.
* [rs2_create_spatial_filter_block](https://github.com/IntelRealSense/librealsense/blob/v2.8.3/include/librealsense2/h/rs_processing.h#L145) - Spatial edge-preserving depth filter.
* [rs2_create_temporal_filter_block](https://github.com/IntelRealSense/librealsense/blob/v2.8.3/include/librealsense2/h/rs_processing.h#L139) - Temporal filter that rectifies depth values based on previously-available frames.

The filters have been integrated into the **realsense-viewer**  and **depth-quality** tools.
    

## From [2.8.1](https://github.com/IntelRealSense/librealsense/releases/tag/v2.8.1) to [2.8.2](https://github.com/IntelRealSense/librealsense/releases/tag/v2.8.2)

No API changes introduced


## From [2.8.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.8.0) to [2.8.1](https://github.com/IntelRealSense/librealsense/releases/tag/v2.8.1)

### Added
* [rs2_start_processing_queue](https://github.com/IntelRealSense/librealsense/tree/v2.8.1/include/librealsense2/h/rs_processing.h#L67) - convenience function that lets the user target the output of a processing block (for example **align**) directly into a **frame_queue**. This helps in languages where function pointers are not available, such as LabView 

> Signature of **rs2_is_option_read_only**, **rs2_get_option**, **rs2_set_option**, **rs2_supports_option**, **rs2_get_option_description** and **rs2_get_option_value_description** was changed. First parameter used to be pointer to **rs2_sensor** and now is pointer to **rs2_options**. However, it is 100% safe to cast pointer to **rs2_sensor** to pointer to **rs2_options**. 

## From [2.7.9](https://github.com/IntelRealSense/librealsense/releases/tag/v2.7.9) to [2.8.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.8.0)

### Moved
> See [Pipeline Changes](https://github.com/IntelRealSense/librealsense/pull/672) pull-request for explanation
* [rs2_stop_pipeline](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_pipeline.h#L64)
is now available as [rs2_pipeline_stop](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L40)
* [rs2_start_pipeline](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_pipeline.h#L40)
is now available as [rs2_pipeline_start](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L94)
* [rs2_pipeline_get_device](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_pipeline.h#L33)
is now available as [rs2_pipeline_profile_get_device](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L142)
* [rs2_pipeline_get_active_streams](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_pipeline.h#L112)
is now available as [rs2_pipeline_profile_get_streams](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L151)
* [rs2_enable_pipeline_stream](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_pipeline.h#L75)
is now available as [rs2_config_enable_stream](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L201)
* [rs2_enable_pipeline_device](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_pipeline.h#L77)
is now available as [rs2_config_enable_device](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L231)
* [rs2_disable_stream_pipeline](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_pipeline.h#L82)
is now available as [rs2_config_disable_stream](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L265)
* [rs2_disable_all_streams_pipeline](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_pipeline.h#L87)
is now available as [rs2_config_disable_all_streams](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L287)

### Added
* [rs2_pipeline_start_with_config](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L115)
* [rs2_pipeline_get_active_profile](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L127)
* [rs2_delete_pipeline_profile](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L158)
* [rs2_create_config](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L171)
* [rs2_delete_config](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L178)
* [rs2_config_enable_all_stream](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L219)
* [rs2_config_enable_device_from_file](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L243)
* [rs2_config_enable_record_to_file](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L253)
* [rs2_config_resolve](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L307)
* [rs2_config_can_resolve](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L318)
* [rs2_config_disable_indexed_stream](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L277)

### Removed
* [rs2_get_stream_profile_size](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_sensor.h#L357) - This function was intended to offer an estimate of USB bandwidth, however USB3 performance is affect by factors other then bytes/second, rendering this method useless
* [rs2_open_pipeline](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_pipeline.h#L42) - This function was offered to force pipeline configuration prior to streaming. This can be more effectively achieved by [rs2_config_resolve](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L307) and [rs2_config_can_resolve](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L318) 
* [rs2_start_pipeline_with_callback](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_pipeline.h#L49) -
 and [rs2_start_pipeline_with_callback_cpp](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_pipeline.h#L57) - Pipeline is always performing synchronization at the moment. If you wish to get minimum latency you can use callback API of the individual sensors
* [rs2_pipeline_get_selection_count](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_pipeline.h#L120), [rs2_pipeline_get_stream_selection](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_pipeline.h#L129),  [rs2_pipeline_get_stream_type_selection](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_pipeline.h#L139),  [rs2_pipeline_delete_selection](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_pipeline.h#L145) - These methods were redundant


***


## From [2.7.7](https://github.com/IntelRealSense/librealsense/releases/tag/v2.7.7) to [2.7.9](https://github.com/IntelRealSense/librealsense/releases/tag/v2.7.9)

### Moved
* [rs2_get_context_time](https://github.com/IntelRealSense/librealsense/tree/v2.7.7/include/librealsense2/h/rs_context.h#L38)
is now available as [rs2_get_time](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/rs.h#L82)
* [rs2_create_pipeline_with_device](https://github.com/IntelRealSense/librealsense/tree/v2.7.7/include/librealsense2/h/rs_pipeline.h#L32)
is now available as [rs2_enable_pipeline_device](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_pipeline.h#L77)
* [rs2_enable_stream_pipeline](https://github.com/IntelRealSense/librealsense/tree/v2.7.7/include/librealsense2/h/rs_pipeline.h#L82)
is now available as [rs2_enable_pipeline_stream](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_pipeline.h#L75)
* [rs2_pipeline_get_selection](https://github.com/IntelRealSense/librealsense/tree/v2.7.7/include/librealsense2/h/rs_pipeline.h#L141)
is now available as [rs2_pipeline_get_active_streams](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_pipeline.h#L112)

### Added
* [rs2_playback_device_stop](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_record_playback.h#L168)
* [rs2_start_queue](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_sensor.h#L223)
* [rs2_open_pipeline](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_pipeline.h#L42)

### Removed
* [rs2_can_enable_stream_pipeline](https://github.com/IntelRealSense/librealsense/tree/v2.7.7/include/librealsense2/h/rs_pipeline.h#L95) - This method, while useful, was not 100% clearly defined. The same goal was more cleanly achieved in a later release - [rs2_config_can_resolve](https://github.com/IntelRealSense/librealsense/tree/v2.8.0/include/librealsense2/h/rs_pipeline.h#L318)
* [rs2_pipeline_get_extrinsics](https://github.com/IntelRealSense/librealsense/tree/v2.7.7/include/librealsense2/h/rs_pipeline.h#L133) - Extrinsics can be queried between stream profiles, and don't require the pipeline or context.
* [rs2_pipeline_get_context](https://github.com/IntelRealSense/librealsense/tree/v2.7.7/include/librealsense2/h/rs_pipeline.h#L176) - Context can be passed to pipeline constructor for dependency injection, but should not be required for anything useful later on.

### Signature Changed
* [rs2_create_pointcloud](https://github.com/IntelRealSense/librealsense/tree/v2.7.7/include/librealsense2/h/rs_processing.h#L41) used to accept 2 parameters but
[rs2_create_pointcloud](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_processing.h#L41) accepts 1 parameters!
* [rs2_create_processing_block](https://github.com/IntelRealSense/librealsense/tree/v2.7.7/include/librealsense2/h/rs_processing.h#L51) used to accept 3 parameters but
[rs2_create_processing_block](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_processing.h#L51) accepts 2 parameters!
* [rs2_create_align](https://github.com/IntelRealSense/librealsense/tree/v2.7.7/include/librealsense2/h/rs_processing.h#L118) used to accept 3 parameters but
[rs2_create_align](https://github.com/IntelRealSense/librealsense/tree/v2.7.9/include/librealsense2/h/rs_processing.h#L118) accepts 2 parameters!
