Some RealSense D500 cameras compute more than depth: they can send a safety stream with safety-zone status, an occupancy grid and a labeled point cloud built from depth, and object detection results. librealsense exposes these as extra sensors, streams, options and frame types. Read this page if you write an application for a D500 camera and want to stream or configure these features from C or C++.

For the camera models themselves, see [[Supported Devices]]. For frame metadata in general, see [[Frame Metadata]].

## Which cameras provide which feature

The D500 device classes are created in [`d500-factory.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-factory.cpp). Each feature comes from a base class that the device class inherits. Names are the product names from `rs500_sku_names` in [`d500-private.h`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-private.h).

| Device class | Products | Safety sensor | Depth mapping (occupancy, labeled point cloud) | Object detection |
|---|---|---|---|---|
| `rs585s_device` | RealSense D585S | Yes | Yes | No |
| `rs585_legacy_device` | RealSense D585 (PID `0x0B6A`) | No | Yes | Yes |
| `rs555_device` | RealSense D555 | No | Yes | Yes |
| `rs5x5_dedicated_color_device` | RealSense D535, RealSense D535F, RealSense D585 (PID `0x0C05`), RealSense D585F, RealSense D585 Prototype | No | Yes | Yes |
| `rs5x5_device` | RealSense D535 Dual RGB, RealSense D585 Dual RGB, RealSense D585 Proto Dual RGB | No | Yes | Yes |

Two features depend on the camera interface and firmware:

- **Depth mapping** is added only when the camera exposes the mapping interface. If it does not, the SDK logs `depth mapping device not found (expected 1, found N) - occupancy/point-cloud streams will not be available` and creates the device without these streams ([`d500-depth-mapping.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-depth-mapping.cpp)). A code comment there says older firmware is the usual reason. In v2.59.1 depth mapping is also never added on MIPI/GMSL cameras *(changed in the next release)*.
- **Object detection** is added only when the camera exposes the object detection interface. Otherwise the SDK logs `Object detection interface not present - sensor not created` ([`d500-object-detection.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-object-detection.cpp)).

Object detection can also come from a network (DDS) camera. When a DDS device publishes a stream of type `object_detection`, librealsense creates a perception sensor for it and converts the received JSON detections into object detection frames ([`rs-dds-device-proxy.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/dds/rs-dds-device-proxy.cpp), [`rs-dds-sensor-proxy.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/dds/rs-dds-sensor-proxy.cpp)). The DDS path does not provide the safety, occupancy or labeled point cloud streams. See [[Ethernet and DDS Cameras]].

## Sensors and streams

| Feature | Sensor name | Sensor extension | Stream | Format |
|---|---|---|---|---|
| Safety | `Safety Camera` | `RS2_EXTENSION_SAFETY_SENSOR` (`rs2::safety_sensor`) | `RS2_STREAM_SAFETY` | `RS2_FORMAT_Y8` |
| Occupancy grid | `Depth Mapping Camera` | `RS2_EXTENSION_DEPTH_MAPPING_SENSOR` (`rs2::depth_mapping_sensor`) | `RS2_STREAM_OCCUPANCY` | `RS2_FORMAT_Y8` |
| Labeled point cloud | `Depth Mapping Camera` | `RS2_EXTENSION_DEPTH_MAPPING_SENSOR` | `RS2_STREAM_LABELED_POINT_CLOUD` | `RS2_FORMAT_Y8` |
| Device point cloud | `Depth Mapping Camera` | `RS2_EXTENSION_DEPTH_MAPPING_SENSOR` | `RS2_STREAM_POINT_CLOUD` *(new in the next release)* | `RS2_FORMAT_XYZ32F` |
| Object detection | `Perception` (stream name `Object Detection`) | `RS2_EXTENSION_PERCEPTION_SENSOR` (`rs2::perception_sensor`) | `RS2_STREAM_OBJECT_DETECTION` | `RS2_FORMAT_Y8` |

Notes from the code:

- `RS2_STREAM_POINT_CLOUD` is added to the depth mapping streams on every D500 device with depth mapping except `RealSense D585S` and the `RealSense D585` with PID `0x0B6A` (`add_streams_if_active()` in `d500-depth-mapping.cpp`).
- Except on `RealSense D585S` and the `RealSense D585` with PID `0x0B6A`, the depth mapping sensor sends one mapping stream at a time. On every device that has `RS2_STREAM_POINT_CLOUD`, that stream cannot start while the depth sensor is streaming (`Point Cloud cannot start while Depth is streaming; stop Depth first`) (`d500_depth_mapping_sensor::open()`) *(new in the next release)*.
- The `Y8` format on the safety, labeled point cloud and object detection streams is a transport format. Read these frames through the frame classes below, not as images.

## Streaming the labeled point cloud

A labeled point cloud frame holds 3D vertices and one label per vertex. Get it from a frameset with `rs2::frameset::get_labeled_point_cloud_frame()`, which returns an `rs2::labeled_points`:

```cpp
#include <librealsense2/rs.hpp>

rs2::pipeline pipe;
rs2::config cfg;
cfg.enable_stream( RS2_STREAM_LABELED_POINT_CLOUD );
pipe.start( cfg );

rs2::frameset data = pipe.wait_for_frames();
rs2::labeled_points lpc = data.get_labeled_point_cloud_frame();

const rs2::vertex * vertices = lpc.get_vertices();
const uint8_t * labels = lpc.get_labels();   // one label per vertex
for( size_t i = 0; i < lpc.size(); ++i )
{
    auto label = static_cast< rs2_point_cloud_label >( labels[i] );
    // rs2_point_cloud_label_to_string( label ) gives the label name
}
```

The labels are values of `rs2_point_cloud_label` (in [`rs_types.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_types.h)): `UNKNOWN`, `UNDEFINED`, `INVALID`, `GROUND`, `NEAR_GROUND`, `OVERHEAD`, `ABOVE_CEILING_HEIGHT`, `GAP`, `MASKED`, `CLIFF`, `OBSTACLE`, `OBSTACLE_DANGER` and `OBSTACLE_WARNING`, each with the `RS2_POINT_CLOUD_LABEL_` prefix.

From C, use `rs2_get_frame_labeled_points_count()`, `rs2_get_frame_labeled_vertices()` and `rs2_get_frame_labels()` on the frame.

### The rs-labeled-pointcloud example

[`examples/labeledpointcloud`](https://github.com/realsenseai/librealsense/tree/development/examples/labeledpointcloud) builds `rs-labeled-pointcloud`. It is built only when both `BUILD_EXAMPLES` and `BUILD_GRAPHICAL_EXAMPLES` are on (see [[Build Configuration]]). The program:

1. opens an OpenGL window titled `RealSense Pointcloud Example`;
2. enables `RS2_STREAM_LABELED_POINT_CLOUD` in an `rs2::config` and starts an `rs2::pipeline`;
3. on each frameset calls `get_labeled_point_cloud_frame()` and passes `get_vertices()`, `get_labels()` and `size()` to `draw_labeled_pointcloud()`;
4. colors each point by its label, using the label-to-color map from `rs2::labeled_point_cloud_utilities::get_label_to_color3f()` in [`common/labeled-point-cloud-utilities.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/labeled-point-cloud-utilities.cpp).

You can rotate and zoom the cloud with the mouse. The drawing code ([`labeled-pointcloud-drawing.hpp`](https://github.com/realsenseai/librealsense/blob/development/examples/labeledpointcloud/labeled-pointcloud-drawing.hpp)) notes that the labeled point cloud is in the robot coordinate system, not the optical one. The example converts it with a hard-coded axis swap (`{ -y, z, -x }`) instead of the extrinsics.

The example's `readme.md` shows older code (for example a `state` struct and a different window title); the `.cpp` file is the reference.

## Streaming the occupancy grid

Enable `RS2_STREAM_OCCUPANCY` like any other stream. Each occupancy frame carries its grid layout as frame metadata:

| Metadata | Meaning (from `rs_frame.h`) |
|---|---|
| `RS2_FRAME_METADATA_OCCUPANCY_GRID_ROWS` | Number of rows in the grid |
| `RS2_FRAME_METADATA_OCCUPANCY_GRID_COLUMNS` | Number of columns in the grid |
| `RS2_FRAME_METADATA_OCCUPANCY_CELL_SIZE` | Edge size of each tile, in cm |
| `RS2_FRAME_METADATA_OCCUPANCY_GRID_ORIGIN_X` *(new in the next release)* | Grid origin on the forward axis, in mm |
| `RS2_FRAME_METADATA_OCCUPANCY_GRID_ORIGIN_Y` *(new in the next release)* | Grid origin on the lateral axis, in mm |

```cpp
rs2::frame og = data.first_or_default( RS2_STREAM_OCCUPANCY );
if( og && og.supports_frame_metadata( RS2_FRAME_METADATA_OCCUPANCY_GRID_ROWS ) )
{
    auto rows = og.get_frame_metadata( RS2_FRAME_METADATA_OCCUPANCY_GRID_ROWS );
    auto cols = og.get_frame_metadata( RS2_FRAME_METADATA_OCCUPANCY_GRID_COLUMNS );
    auto cell_cm = og.get_frame_metadata( RS2_FRAME_METADATA_OCCUPANCY_CELL_SIZE );
}
```

On `RealSense D585S` and the `RealSense D585` with PID `0x0B6A`, the default occupancy profile is 256×320, `Y8`, 30 fps (`add_profile_tag_if_active()`).

### Stream combination rules *(new in the next release)*

On every other D500 device with depth mapping, the SDK checks that the camera can build the occupancy grid from the depth that is already streaming, and rejects a combination it cannot serve. The checks are in `occupancy_allowed_or_throw()` and `depth_sensor_allowed_or_throw()` in [`d500-depth-mapping.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-depth-mapping.cpp):

| Situation | Error message |
|---|---|
| Starting occupancy while depth streams at another size or frame rate (depth must be 640×360 or 1280×720 at the occupancy frame rate) | `Occupancy needs Depth at 640x360 or 1280x720, <fps> fps, while the depth sensor streams` |
| Starting occupancy while the depth sensor streams color (USB only) | `Occupancy cannot start while the depth sensor streams color` |
| Starting depth while occupancy streams, with other streams or at another size or rate | `While Occupancy streams, Depth can start only alone at 640x360, <fps> fps` |
| Starting color while occupancy streams | `Color cannot start while Occupancy streams` |

## Configuring depth mapping

### Depth mapping parameters *(new in the next release)*

`rs2::depth_mapping_sensor` reads and writes the occupancy grid and labeled point cloud parameters as JSON. On `RealSense D585S` both calls fail with `Depth mapping params are not supported on this device`:

```cpp
for( auto && s : dev.query_sensors() )
{
    rs2::depth_mapping_sensor dms( s );
    if( ! dms )
        continue;
    std::string params = dms.get_depth_mapping_params();   // current params as JSON
    // dms.set_depth_mapping_params( partial_json );        // only the named fields change
}
```

From the code ([`rs_sensor.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_sensor.h), `d500-depth-mapping.cpp`):

- `set_depth_mapping_params()` takes partial JSON: only the fields you name change. Read the current JSON with `get_depth_mapping_params()` to see the field names your firmware uses.
- The call blocks until the camera has applied the parameters. If the camera rejects them, the call fails with the camera's message.
- An empty string, or JSON larger than the camera's parameter buffer, is rejected with `Depth mapping params JSON is N bytes; must be 1..M`.

The C functions are `rs2_get_depth_mapping_params()` (release the result with `rs2_delete_raw_data()`) and `rs2_set_depth_mapping_params()`.

### Segmentation options *(new in the next release)*

The `Depth Mapping Camera` sensor can register two options. Both can be changed only while the sensor is not streaming:

| Option | Values |
|---|---|
| `RS2_OPTION_OCCUPANCY_GRID_SEGMENTATION` | Off (0): depth-only labeling. On (1): segmentation labels in the occupancy grid |
| `RS2_OPTION_LPCL_SEGMENTATION` | Off (0): depth-only labeling. On (1): segmentation labels in the labeled point cloud |

They are not registered on `RealSense D585S`, and are registered only when the camera's transport publishes the control (`register_options()` in `d500-depth-mapping.cpp`). Check with `supports( option )` before use.

## Reading object detection results

An object detection frame holds a list of detections. Get it with `rs2::frameset::get_object_detection_frame()`, which returns an `rs2::object_detection_frame`. Each detection is an `rs2_object_detection`:

```cpp
rs2::config cfg;
cfg.enable_stream( RS2_STREAM_OBJECT_DETECTION );
rs2::pipeline pipe;
pipe.start( cfg );

rs2::frameset data = pipe.wait_for_frames();
if( auto od = data.get_object_detection_frame() )
{
    for( unsigned int i = 0; i < od.get_detection_count(); ++i )
    {
        rs2_object_detection d = od.get_detection( i );
        // d.class_id, d.score (0-100), bounding box d.top_left_x/y .. d.bottom_right_x/y (pixels),
        // d.depth (meters)
        if( d.center_of_mass_valid )
        {
            // d.world_position (meters, camera coordinates), d.center_of_mass_x/y (pixels)
        }
    }
}
```

The same in C:

```c
unsigned int n = rs2_get_frame_object_detection_count( frame, &e );
for( unsigned int i = 0; i < n; ++i )
{
    rs2_object_detection d;
    rs2_get_frame_object_detection( frame, i, &d, &e );
}
```

An index outside the detection count fails with `index <i> is out of range (<count>)` ([`rs.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/rs.cpp)). The center-of-mass fields (`world_position`, `center_of_mass_x`, `center_of_mass_y`) are valid only when `center_of_mass_valid` is nonzero; firmware that does not report a center of mass gives `center_of_mass_valid = 0` and zero values.

On D500 USB/MIPI cameras that have a `Perception` sensor and firmware `7.58.40802.12738` or newer, that sensor also has `RS2_OPTION_DETECTION_DISTANCE`, which turns on the firmware calculation of the per-detection distance (the `depth` field).

`rs2::object_detection_frame` derives from `rs2::perception_frame`. Both check the frame type in their constructor and are empty (false) when the frame is not of that type.

## Using the safety camera

The `Safety Camera` sensor of the `RealSense D585S` exposes the `RS2_STREAM_SAFETY` stream, safety options, and JSON read/write of the safety configuration. Get the sensor by wrapping each sensor in `rs2::safety_sensor`:

```cpp
#include <librealsense2/rs.hpp>   // includes rs_safety_sensor.hpp

for( auto && s : dev.query_sensors() )
{
    rs2::safety_sensor ss( s );
    if( ! ss )
        continue;

    // Operational mode: RS2_SAFETY_MODE_RUN, _STANDBY or _SERVICE
    ss.set_option( RS2_OPTION_SAFETY_MODE, RS2_SAFETY_MODE_SERVICE );

    std::string preset = ss.get_safety_preset( 0 );               // JSON
    std::string sic    = ss.get_safety_interface_config();        // from RAM by default
    std::string sic_fl = ss.get_safety_interface_config( RS2_CALIB_LOCATION_FLASH );
    std::string app    = ss.get_application_config();
}
```

What the code does with the configuration calls ([`d500-safety.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-safety.cpp)):

- `get_safety_preset()` reads the preset at an index and checks its CRC; a bad CRC fails with `Safety preset invalid CRC value`.
- `set_safety_preset()` expects JSON with a top-level `safety_preset` object.
- `get_safety_interface_config()` accepts only `RS2_CALIB_LOCATION_RAM` or `RS2_CALIB_LOCATION_FLASH`; other locations fail with `Safety Interface Config can be read only from Flash or RAM`.
- `set_safety_interface_config()` expects a top-level `safety_interface_config` object.
- `set_application_config()` expects a top-level `application_config` object.

The easiest way to get valid JSON is to read the current value, edit it, and write it back.

The safety sensor registers these options:

| Option | Description in code |
|---|---|
| `RS2_OPTION_SAFETY_PRESET_ACTIVE_INDEX` | `Safety Preset Active Index` |
| `RS2_OPTION_SAFETY_MODE` | `Safety camera operational mode`: `Run`, `Standby`, `Service` (`rs2_safety_mode`) |
| `RS2_OPTION_SAFETY_MCU_TEMPERATURE` | `Temperature reading for Safety MCU` |
| `RS2_OPTION_GLOBAL_TIME_ENABLED` | `Enable/Disable global timestamp` |

Safety frames carry many `RS2_FRAME_METADATA_SAFETY_*` fields, for example `RS2_FRAME_METADATA_SAFETY_LEVEL1`, `RS2_FRAME_METADATA_SAFETY_LEVEL2` and `RS2_FRAME_METADATA_SAFETY_PRESET_ID_USED`. The full list is in `register_metadata()` in `d500-safety.cpp`, with descriptions in [`rs_frame.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_frame.h).

## Reference

### Safety sensor API

The C API is declared in [`rs_safety_sensor.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_safety_sensor.h), and the C++ wrapper `rs2::safety_sensor` in [`rs_safety_sensor.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_safety_sensor.hpp). The C getters return an `rs2_raw_data_buffer` holding a JSON string; release it with `rs2_delete_raw_data()`. The C++ methods return `std::string`.

<!-- wikigen:table-safety-api -->
| C function | C++ method | What it does |
|---|---|---|
| `rs2_get_safety_preset(sensor, index, error)` | `std::string get_safety_preset(int index) const` | Read the safety preset at `index` as JSON |
| `rs2_set_safety_preset(sensor, index, sp_json_str, error)` | `void set_safety_preset(int index, const std::string& sp_json_str) const` | Write a safety preset (JSON) to `index` |
| `rs2_get_safety_interface_config(sensor, loc, error)` | `std::string get_safety_interface_config(rs2_calib_location calib_location = RS2_CALIB_LOCATION_RAM) const` | Read the safety interface config as JSON from the given location |
| `rs2_set_safety_interface_config(sensor, sic_json_str, error)` | `void set_safety_interface_config(const std::string& sic_json_str) const` | Write the safety interface config (JSON) |
| `rs2_get_application_config(sensor, error)` | `std::string get_application_config() const` | Read the application config as JSON |
| `rs2_set_application_config(sensor, application_config_json_str, error)` | `void set_application_config(const std::string& application_config_json_str) const` | Write the application config (JSON) |
<!-- /wikigen:table-safety-api -->

### Frame and sensor classes

| C++ class | Header | Main members |
|---|---|---|
| `rs2::labeled_points` | [`rs_frame.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_frame.hpp) | `get_vertices()`, `get_labels()`, `size()`, `get_width()`, `get_height()`, `get_bits_per_pixel()` |
| `rs2::perception_frame` | `rs_frame.hpp` | Base class of perception frames |
| `rs2::object_detection_frame` | `rs_frame.hpp` | `get_detection_count()`, `get_detection(index)` |
| `rs2::perception_stream_profile` | `rs_frame.hpp` | Stream profile of a perception stream |
| `rs2::frameset` | `rs_frame.hpp` | `get_labeled_point_cloud_frame()`, `get_object_detection_frame(index = 0)` |
| `rs2::safety_sensor` | `rs_safety_sensor.hpp` | See the table above |
| `rs2::depth_mapping_sensor` | [`rs_sensor.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_sensor.hpp) | `get_depth_mapping_params()`, `set_depth_mapping_params()` *(both new in the next release)* |
| `rs2::perception_sensor` | `rs_sensor.hpp` | Sensor that produces perception streams |

### C functions for frames

| Function | Frame type |
|---|---|
| `rs2_get_frame_labeled_vertices()` | Labeled point cloud |
| `rs2_get_frame_labeled_points_count()` | Labeled point cloud |
| `rs2_get_frame_labels()` | Labeled point cloud |
| `rs2_get_frame_labeled_points_width()` | Labeled point cloud |
| `rs2_get_frame_labeled_points_height()` | Labeled point cloud |
| `rs2_get_frame_labeled_points_bits_per_pixel()` | Labeled point cloud |
| `rs2_get_frame_object_detection_count()` | Object detection |
| `rs2_get_frame_object_detection()` | Object detection |

### Software devices

To feed perception frames from your own code, a software sensor can add a perception stream with `rs2::software_sensor::add_perception_stream()` (C: `rs2_software_sensor_add_perception_stream()` / `rs2_software_sensor_add_perception_stream_ex()`), described by an `rs2_perception_stream` ([`rs_internal.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_internal.h)).

## Source code

- Device classes: [`src/ds/d500/`](https://github.com/realsenseai/librealsense/tree/development/src/ds/d500) (`d500-safety.cpp`, `d500-depth-mapping.cpp`, `d500-object-detection.cpp`, `d500-factory.cpp`)
- Internal interfaces: [`safety-sensor.h`](https://github.com/realsenseai/librealsense/blob/development/src/safety-sensor.h), [`depth-mapping-sensor.h`](https://github.com/realsenseai/librealsense/blob/development/src/depth-mapping-sensor.h), [`perception-sensor.h`](https://github.com/realsenseai/librealsense/blob/development/src/perception-sensor.h)
- Frame types: [`labeled-points.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/labeled-points.cpp), [`object-detection-frame.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/object-detection-frame.cpp), [`perception-frame.h`](https://github.com/realsenseai/librealsense/blob/development/src/perception-frame.h)
- Example: [`examples/labeledpointcloud`](https://github.com/realsenseai/librealsense/tree/development/examples/labeledpointcloud)

See also [[Code Samples]] and [[API How To]].

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`bc496cd`](https://github.com/realsenseai/librealsense/commit/bc496cdb16beca36c41d2b96170e309fceb38d6e)._
<!-- /wikigen:footer -->
