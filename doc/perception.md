# Perception on D5x5 Cameras

D5x5 cameras (D585, D535) process depth on the camera itself and stream perception outputs, like occupancy grid and point clouds, instead of only raw depth.

## Table of Contents
* [Sensors](#sensors)
* [Depth Mapping](#depth-mapping)
  * [Streams](#streams)
    * [Point labels](#point-labels)
    * [Stream rules](#stream-rules)
  * [Options](#options)
  * [Depth mapping params](#depth-mapping-params)
    * [API](#api)
    * [How a write works](#how-a-write-works)
    * [Parameters](#parameters)
    * [Errors](#errors)
    * [Examples](#examples)
* [Object Detection](#object-detection)

## Sensors

The perception features are split today between two sensors:

| Sensor | Name | Extension | Streams |
| --- | --- | --- | --- |
| `rs2::depth_mapping_sensor` | `Depth Mapping Camera` | `RS2_EXTENSION_DEPTH_MAPPING_SENSOR` | Occupancy grid, labeled point cloud, point cloud |
| `rs2::perception_sensor` | `Perception` | `RS2_EXTENSION_PERCEPTION_SENSOR` | Object detection |

> **Note:** The depth mapping sensor will be merged into the perception sensor. This document describes the API as it is today. Expect the class, extension and method names in the [Depth Mapping](#depth-mapping) section to change.

Getting the sensors:

```cpp
rs2::device dev = rs2::context().query_devices().front();
auto depth_mapping_sensor = dev.first< rs2::depth_mapping_sensor >();
auto perception_sensor = dev.first< rs2::perception_sensor >();
```

```python
dev = rs.context().query_devices()[0]
depth_mapping_sensor = dev.first_depth_mapping_sensor()
perception_sensor = dev.first_perception_sensor()
```

A camera with FW that predates depth mapping has no depth mapping sensor. The SDK logs a warning and the rest of the device works as usual.

## Depth Mapping

### Streams

The camera builds the depth mapping streams from its depth. The depth mapping interface carries one stream at a time.

| Stream | `rs2_stream` | Description |
| --- | --- | --- |
| Occupancy grid | `RS2_STREAM_OCCUPANCY` | 2D occupancy grid (-1 = unknown, 0 = free, 1 - 100 = occupancy probability) used for robot navigation, fully compatible with the Nav2 OccupancyGrid (`nav_msgs/msg/OccupancyGrid`). |
| Labeled point cloud | `RS2_STREAM_LABELED_POINT_CLOUD` | XYZ points, each with a label. See [Point labels](#point-labels). |
| Point cloud | `RS2_STREAM_POINT_CLOUD` | XYZ points (`RS2_FORMAT_XYZ32F`), at 640x480@30 or 1280x720@15. |

#### Point labels

Each point in the labeled point cloud has one `rs2_point_cloud_label`. D5x5 cameras emit these labels:

| Label | Description |
| --- | --- |
| `RS2_POINT_CLOUD_LABEL_INVALID` | Discarded: no valid depth, too close, or outside the classified region |
| `RS2_POINT_CLOUD_LABEL_GROUND` | Part of the floor |
| `RS2_POINT_CLOUD_LABEL_CLIFF` | Below the floor |
| `RS2_POINT_CLOUD_LABEL_OVERHEAD` | Above the robot height |
| `RS2_POINT_CLOUD_LABEL_OBSTACLE` | A potential obstacle |
| `RS2_POINT_CLOUD_LABEL_MASKED` | A potential obstacle inside a masked region |

The other values of `rs2_point_cloud_label` are not used by D5x5 cameras.

#### Stream rules

* If the depth sensor is already streaming, the occupancy grid can start only if depth streams at 640x360 or 1280x720, at the occupancy grid's frame rate.
* While the occupancy grid streams, depth can start only alone, at 640x360 and the occupancy grid's frame rate.
* Over USB, the occupancy grid and color cannot stream at the same time.

### Options

| Option | Values | Description |
| --- | --- | --- |
| `RS2_OPTION_OCCUPANCY_GRID_SEGMENTATION` | 0 = off, 1 = on | When segmentation is enabled, the occupancy grid flow activates an RGB-D segmentation NN, refining the quality of the floor detection pixels and resulting in a more accurate occupancy grid. |
| `RS2_OPTION_LPCL_SEGMENTATION` | 0 = off, 1 = on | When segmentation is enabled, the labeled point cloud flow activates an RGB-D segmentation NN, refining the quality of the floor detection labeling. |

These options cannot be changed while streaming. FW publishes them over USB only, not over GMSL.

### Depth mapping params

The depth mapping params set how the occupancy grid and labeled point cloud are calculated, such as where the camera is mounted, the grid cell size and the height of the floor band. The camera keeps them in flash, so they survive a reboot.

#### API

| C | C++ / Python | Description |
| --- | --- | --- |
| `rs2_get_depth_mapping_params` | `get_depth_mapping_params()` | Returns all the params, as a JSON string |
| `rs2_set_depth_mapping_params` | `set_depth_mapping_params( json )` | Writes the params in the JSON string. Only the keys in the JSON change. |

```python
import json
import pyrealsense2 as rs

dev = rs.context().query_devices()[0]
depth_mapping_sensor = dev.first_depth_mapping_sensor()

# Read all the params
params = json.loads(depth_mapping_sensor.get_depth_mapping_params())

# Change one param; the camera keeps the rest
depth_mapping_sensor.set_depth_mapping_params('{"occupancy_grid_params": {"surface_height": 0.15}}')
```

#### How a write works

* **Partial update.** Send only the keys you want to change. The camera merges them into the params it already has and writes the result to flash. Values you do not send keep their current value, or their default if they were never set.
* **`camera_position` is mandatory on the first write.** It has no default. Until it is written once, any write without it fails with `MISSING_MANDATORY`. After that, any subset of keys can be written.
* **Before the first write**, `get_depth_mapping_params()` returns the defaults and leaves out `camera_position`. To check whether a camera was configured, check whether `camera_position` is in the reply.
* **Synchronous.** `set_depth_mapping_params()` returns after the camera has validated the JSON and written it to flash (about 25 ms), and throws if the camera rejected it. A rejected JSON writes nothing.
* **Size limit.** The JSON must be 1 to 1016 bytes. Remove whitespace before sending, e.g. `json.dumps(params, separators=(",", ":"))` in Python.
* **Hardware reset** the camera after writing new params (`dev.hardware_reset()`), so it starts with them.

#### Parameters

The JSON has three groups. All keys are optional except `camera_position`, which is mandatory on the first write.

```json
{
    "camera_position": {
        "rotation": [[0.0, 0.0, 1.0],
                     [-1.0, 0.0, 0.0],
                     [0.0, -1.0, 0.0]],
        "translation": [0.0, 0.0, 0.27]
    },
    "occupancy_grid_params": {
        "grid_cell_size": 50,
        "cell_threshold_factor": 0.02,
        "polynomial_bias": 4.0,
        "surface_height": 0.1
    },
    "depth_mapping_params": {
        "enable_ray_casting": true,
        "ray_casting_threshold": 50,
        "enable_cliffs": false,
        "cliffs_threshold": 3,
        "cliffs_max_range": 2.5,
        "cliffs_min_drop": 0.05,
        "segmentation_merge_height": 0.1,
        "surface_height_slope": 0.0,
        "surface_height_slope_start": 1.0,
        "surface_height_max": 0.1
    }
}
```

**`camera_position`** - the camera pose in the robot frame. A point `p_cam` in camera coordinates is `p_robot = rotation · p_cam + translation` in robot coordinates. Mandatory on the first write.

| Key | Type | Units | Range | Description |
| --- | --- | --- | --- | --- |
| `rotation` | float, 3x3 | - | any finite value | Camera to robot rigid-body transformation: rotation, row-major |
| `translation` | float, [3] | m | any finite value | Camera to robot rigid-body transformation: translation vector [x, y, z]. The Z value is the mounting height and is used to calculate the cell threshold. |

**`occupancy_grid_params`**

| Key | Type | Units | Default | Range | Description |
| --- | --- | --- | --- | --- | --- |
| `grid_cell_size` | integer | mm | 50 | 50 - 500, multiple of 10 | Edge size of one (square) grid cell |
| `cell_threshold_factor` | float | - | 0.02 | any finite value | Cell threshold calculation factor |
| `polynomial_bias` | float | - | 4.0 | any finite value | Cell threshold calculation bias |
| `surface_height` | float | m | 0.1 | 0.05 - 1.0 | Minimum height above the floor used for obstacle avoidance |

**`depth_mapping_params`**

| Key | Type | Units | Default | Range | Description |
| --- | --- | --- | --- | --- | --- |
| `enable_ray_casting` | bool | - | true | - | Enable ray casting |
| `ray_casting_threshold` | integer | - | 50 | 0 - 100 | Cell value that stops a visibility ray |
| `enable_cliffs` | bool | - | false | - | Enable cliff handling |
| `cliffs_threshold` | integer | points | 3 | 0 - 20 | Number of points needed to mark a cell as a cliff |
| `cliffs_max_range` | float | m | 2.5 | 0.0 - 10.0 | Maximum distance from the camera at which cliffs are calculated. 0 = no limit. |
| `cliffs_min_drop` | float | m | 0.05 | 0.0 - 0.5 | Minimum depth below the floor at which a point counts as a cliff candidate. Points closer to the floor are treated as ground noise. |
| `segmentation_merge_height` | float | m | 0.1 | 0.05 - 1.0 | Merge threshold between the depth based and the segmentation based floor height |
| `surface_height_slope` | float | m/m | 0.0 | 0.0 - 0.1 | How much the floor band widens per meter of distance from the camera to the point |
| `surface_height_slope_start` | float | m | 1.0 | 0.0 - 10.0 | Distance from the camera where the floor band starts to widen |
| `surface_height_max` | float | m | 0.1 | 0.05 - 1.0 | Maximum height of the floor band |

A bool can be written as `true` / `false` or `1` / `0`. Floats are checked as float32, the type stored in flash. NaN and infinite values are always rejected.

#### Errors

When the camera rejects a write, the error message includes the camera's error code and reason, for example:

```
Failed to set depth mapping params: OUT_OF_RANGE: occupancy_grid_params.grid_cell_size: 40 out of range [50, 500]
```

| Code | Cause | C++ exception |
| --- | --- | --- |
| `PARSE_ERROR` | The string is not valid JSON | `rs2::invalid_value_error` |
| `UNKNOWN_KEY` | A key the camera does not know | `rs2::invalid_value_error` |
| `WRONG_TYPE` | A value of the wrong type, e.g. a fraction for an integer | `rs2::invalid_value_error` |
| `OUT_OF_RANGE` | A value outside its range | `rs2::invalid_value_error` |
| `MISSING_MANDATORY` | First write without `camera_position` | `rs2::invalid_value_error` |
| `UNSUPPORTED_VERSION`, `BAD_LENGTH`, `CRC_MISMATCH` | The data got corrupted on the way to the camera | `rs2::error` |
| `FLASH_ERROR` | Writing to flash failed | `rs2::error` |

A JSON that is empty or longer than 1016 bytes is rejected by the SDK before it is sent, with `rs2::invalid_value_error`. In Python, all of these are raised as `RuntimeError`.

#### Examples

Python examples in [wrappers/python/examples](../wrappers/python/examples):

* [d5x5_get_depth_mapping_params.py](../wrappers/python/examples/d5x5_get_depth_mapping_params.py) - prints the camera's params
* [d5x5_set_depth_mapping_params.py](../wrappers/python/examples/d5x5_set_depth_mapping_params.py) - writes the params from a JSON file, [d5x5_depth_mapping_params.json](../wrappers/python/examples/d5x5_depth_mapping_params.json) by default

## Object Detection

The perception sensor streams object detection frames (`rs2::object_detection_frame`).
