The librealsense API gives applications access to configuration, control and streaming of data from RealSense cameras. It is built in layers: a high-level `pipeline` that picks settings and delivers synchronized frames, and a low-level device API (context, device, sensor, stream profile, frame) that gives full control over each sensor. This page explains how those layers fit together, how the C API and the C++ wrapper relate, and how errors, device events and software-only devices work. It is meant for developers writing their own applications on top of the SDK.

For task-oriented snippets see [[API How To]]; for runnable programs see [[Code Samples]].

## Headers

The public headers live in [`include/librealsense2`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2):

| Header | Language | Contents |
|---|---|---|
| [`rs.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/rs.h) | C | Includes the C headers under `h/` (types, context, device, frame, option, processing, record/playback, sensor and others) |
| [`rs.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/rs.hpp) | C++ | Includes `rs.h` and the C++ wrappers under `hpp/` (types, context, device, frame, processing, record/playback, sensor, pipeline and others) |
| [`hpp/rs_internal.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_internal.hpp) / [`h/rs_internal.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_internal.h) | C++ / C | Software device API. Not included by `rs.hpp`; include it separately |

The C++ classes in `hpp/` are header-only wrappers: their methods call the matching `rs2_…` C functions (some return cached values or combine other calls) and turn any error into an exception.

## The layers

```
rs2::pipeline + rs2::config          (high-level API)
        |
rs2::context                          enumerates devices, reports connect / disconnect
  └── rs2::device                     one camera (or playback / software device)
        └── rs2::sensor               e.g. depth, color, motion
              └── rs2::stream_profile stream type, index, format, fps (+ width/height for video)
                    └── rs2::frame    data delivered while streaming
```

| Layer | C++ class | C handle | Defined in |
|---|---|---|---|
| Context | `rs2::context` | `rs2_context*` | [`rs_context.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_context.hpp), [`rs_context.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_context.h) |
| Device list | `rs2::device_list` | `rs2_device_list*` | [`rs_device.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_device.hpp), [`rs_device.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_device.h) |
| Device | `rs2::device` | `rs2_device*` | same as above |
| Sensor | `rs2::sensor` and subclasses (`depth_sensor`, `color_sensor`, `motion_sensor`, …) | `rs2_sensor*` | [`rs_sensor.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_sensor.hpp), [`rs_sensor.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_sensor.h) |
| Stream profile | `rs2::stream_profile`, `rs2::video_stream_profile`, … | `rs2_stream_profile*` | [`rs_frame.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_frame.hpp) |
| Frame | `rs2::frame`, `rs2::depth_frame`, `rs2::frameset`, … | `rs2_frame*` | [`rs_frame.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_frame.hpp), [`rs_frame.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_frame.h) |
| Pipeline | `rs2::pipeline`, `rs2::pipeline_profile` | `rs2_pipeline*` | [`rs_pipeline.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_pipeline.hpp), [`rs_pipeline.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_pipeline.h) |
| Config | `rs2::config` | `rs2_config*` | same as pipeline, plus [`rs_config.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_config.h) |

Sensors, devices, profiles and frames are extensible: use `is<T>()` / `as<T>()` to check for and get a more specific type (for example `frame.as<rs2::depth_frame>()`). `device.first<T>()` returns the first sensor of type `T` and throws `rs2::error` if there is none.

## Stream with the high-level pipeline

`rs2::pipeline` acquires a device, chooses stream settings, starts the sensors and returns time-synchronized `frameset`s. `rs2::config` narrows the choice (streams, device serial, playback file, recording).

```cpp
#include <librealsense2/rs.hpp>

rs2::pipeline pipe;                       // uses a default rs2::context
rs2::config cfg;
cfg.enable_stream(RS2_STREAM_DEPTH, 640, 480, RS2_FORMAT_Z16, 30);
rs2::pipeline_profile profile = pipe.start(cfg);

rs2::frameset frames = pipe.wait_for_frames();
rs2::depth_frame depth = frames.get_depth_frame();
float meters = depth.get_distance(depth.get_width() / 2, depth.get_height() / 2);
pipe.stop();
```

Useful pieces of the pipeline API:

- `config::enable_stream(...)` (several overloads), `enable_all_streams()`, `disable_stream(...)`, `disable_all_streams()`.
- `config::enable_device(serial)`, `enable_device_from_file(file, repeat_playback = true)`, `enable_record_to_file(file)`.
- `config::can_resolve(...)` / `resolve(...)` check a config against a pipeline before starting.
- `pipeline::start()`, `start(config)`, `start(callback)`, `start(config, callback)`; `stop()`.
- `pipeline::wait_for_frames(timeout_ms = RS2_DEFAULT_TIMEOUT)`, `poll_for_frames(&fs)`, `try_wait_for_frames(&fs, timeout_ms)`.
- `pipeline_profile::get_device()`, `get_streams()`, `get_stream(type, index)`; `pipeline::get_active_profile()`.

The pipeline is constructed from a context (`rs2::pipeline(context ctx = context())`), so the low-level device remains reachable through `profile.get_device()` for setting options.

When the pipeline is started with a callback, `wait_for_frames()` and `poll_for_frames()` throw (documented on `rs2_pipeline_start_with_callback_cpp` in `rs_pipeline.h`). See the [callback example](https://github.com/realsenseai/librealsense/blob/development/examples/callback/rs-callback.cpp).

Examples: [hello-realsense](https://github.com/realsenseai/librealsense/tree/development/examples/hello-realsense), [callback](https://github.com/realsenseai/librealsense/tree/development/examples/callback), [C distance](https://github.com/realsenseai/librealsense/blob/development/examples/C/distance/rs-distance.c).

## Control sensors with the low-level API

Use the device API when you need to choose exact profiles per sensor, run sensors independently, or receive every frame as it arrives.

```cpp
rs2::context ctx;
rs2::device dev = ctx.query_devices().front();
rs2::depth_sensor depth_sensor = dev.first<rs2::depth_sensor>();

// pick a profile
rs2::stream_profile chosen;
for (auto&& p : depth_sensor.get_stream_profiles())
    if (p.stream_type() == RS2_STREAM_DEPTH && p.format() == RS2_FORMAT_Z16 && p.is_default())
        chosen = p;

depth_sensor.open(chosen);
depth_sensor.start([](rs2::frame f) {
    // called for every frame of this sensor
});
// ...
depth_sensor.stop();
depth_sensor.close();
```

Sensor calls follow the order `open` (one profile or a vector of profiles) → `start` → `stop` → `close`. `sensor::start` takes any callable that accepts an `rs2::frame`; a `rs2::frame_queue` or `rs2::syncer` can be passed instead of a lambda (the software-device example passes a `syncer`). `get_active_streams()` lists the profiles currently opened. For buffering, queues and frame lifetimes see [[Frame Buffering Management in RealSense SDK 2.0]].

Examples: [sensor-control](https://github.com/realsenseai/librealsense/tree/development/examples/sensor-control), [multicam](https://github.com/realsenseai/librealsense/tree/development/examples/multicam).

## Enumerate devices and handle hot-plug

`rs2::context` is the factory for devices:

- `query_devices()` returns a snapshot of the devices connected at the time of the call; `query_devices(int mask)` filters by product mask (`rs2_query_devices_ex` in C).
- `query_all_sensors()` returns the sensors of all those devices.
- `rs2::device_list` supports `size()`, `operator[]`, `front()` and range-for iteration.
- `device.get_info(RS2_CAMERA_INFO_…)` / `supports(...)` read camera information such as name and serial number.

Register a callback to be told when devices are connected or disconnected:

```cpp
rs2::context ctx;
rs2::device my_dev = ctx.query_devices().front();

ctx.set_devices_changed_callback([&](rs2::event_information& info) {
    if (info.was_removed(my_dev))
        std::cout << "device disconnected\n";
    for (auto&& d : info.get_new_devices())
        std::cout << "connected: " << d.get_info(RS2_CAMERA_INFO_NAME) << "\n";
});
```

`event_information` offers `was_removed(dev)`, `was_added(dev)` and `get_new_devices()`. In C, the same is done with `rs2_set_devices_changed_callback(ctx, callback, user, &e)`, which passes the removed and added `rs2_device_list*` to the callback.

`rs2::device_hub` wraps connect / disconnect handling: `wait_for_device()` returns a connected device or waits up to one hour for one to connect, then throws (repeated calls cycle through connected devices), and `is_connected(dev)` checks whether a device is still connected.

## C API and C++ wrapper

### The `rs2_error**` convention

Every C function that can fail takes a final `rs2_error** error` argument. If it is non-null it receives the error, otherwise errors are ignored. Check it after each call and free it with `rs2_free_error`:

```c
#include <librealsense2/rs.h>

rs2_error* e = 0;
rs2_context* ctx = rs2_create_context(RS2_API_VERSION, &e);
if (e)
{
    printf("%s(%s): %s\n", rs2_get_failed_function(e), rs2_get_failed_args(e), rs2_get_error_message(e));
    rs2_free_error(e);
    return EXIT_FAILURE;
}
rs2_device_list* list = rs2_query_devices(ctx, &e);
/* ... */
rs2_delete_device_list(list);
rs2_delete_context(ctx);
```

Error accessors (in [`rs_types.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_types.h)): `rs2_get_error_message`, `rs2_get_failed_function`, `rs2_get_failed_args`, `rs2_get_librealsense_exception_type`, `rs2_free_error`.

Objects returned by the C API must be released by the caller with the matching function (`rs2_delete_context`, `rs2_delete_device_list`, `rs2_delete_device`, `rs2_release_frame`, …). `rs2_create_context` takes `RS2_API_VERSION` so the library can check that the application was built against a compatible version. The [C examples](https://github.com/realsenseai/librealsense/tree/development/examples/C) use a `check_error()` helper from [`example.h`](https://github.com/realsenseai/librealsense/blob/development/examples/C/example.h).

### C++ exceptions

The C++ wrapper owns C handles through `std::shared_ptr` and calls `rs2::error::handle(e)` after each C call. That function throws an exception chosen by the error's `rs2_exception_type` ([`rs_types.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_types.hpp)):

```
std::runtime_error
└── rs2::error
    ├── rs2::recoverable_error
    │   ├── rs2::invalid_value_error            RS2_EXCEPTION_TYPE_INVALID_VALUE
    │   ├── rs2::wrong_api_call_sequence_error  RS2_EXCEPTION_TYPE_WRONG_API_CALL_SEQUENCE
    │   └── rs2::not_implemented_error          RS2_EXCEPTION_TYPE_NOT_IMPLEMENTED
    └── rs2::unrecoverable_error
        ├── rs2::camera_disconnected_error      RS2_EXCEPTION_TYPE_CAMERA_DISCONNECTED
        ├── rs2::backend_error                  RS2_EXCEPTION_TYPE_BACKEND
        └── rs2::device_in_recovery_mode_error  RS2_EXCEPTION_TYPE_DEVICE_IN_RECOVERY_MODE
```

Any other type (for example `RS2_EXCEPTION_TYPE_UNKNOWN` or `RS2_EXCEPTION_TYPE_IO`) is thrown as plain `rs2::error`. `rs2::error` provides `what()`, `get_failed_function()`, `get_failed_args()` and `get_type()`.

Catch from the most specific to the most general:

```cpp
try
{
    rs2::pipeline pipe;
    pipe.start();
}
catch (const rs2::camera_disconnected_error& e)
{
    std::cerr << "Camera disconnected: " << e.what() << std::endl;
}
catch (const rs2::recoverable_error& e)
{
    std::cerr << "Operation failed, try again: " << e.what() << std::endl;
}
catch (const rs2::error& e)
{
    std::cerr << e.get_failed_function() << "(" << e.get_failed_args() << "): " << e.what() << std::endl;
}
```

## Software device

A software device is an `rs2::device` with no hardware behind it. You define its sensors and stream profiles and push your own frame data into it; the frames then work with the rest of the SDK (for example `syncer`, `pointcloud`, `align`, recording). It is declared in [`rs_internal.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_internal.hpp) (C: [`rs_internal.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_internal.h)).

Adapted from the [software-device example](https://github.com/realsenseai/librealsense/blob/development/examples/software-device/rs-software-device.cpp):

```cpp
#include <librealsense2/rs.hpp>
#include <librealsense2/hpp/rs_internal.hpp>

rs2::software_device dev;
auto depth_sensor = dev.add_sensor("Depth");

rs2_intrinsics intr{ W, H, W / 2, H / 2, W, H, RS2_DISTORTION_BROWN_CONRADY, { 0,0,0,0,0 } };
auto depth_stream = depth_sensor.add_video_stream({ RS2_STREAM_DEPTH, 0, 0,
                                                    W, H, 60, BPP, RS2_FORMAT_Z16, intr });

dev.create_matcher(RS2_MATCHER_DEFAULT);
rs2::syncer sync;
depth_sensor.open(depth_stream);
depth_sensor.start(sync);

depth_sensor.on_video_frame({ pixels,            // frame data
                              [](void*) {},      // deleter
                              W * BPP,           // stride
                              BPP,
                              timestamp, RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK, frame_number,
                              depth_stream, 0.001f });   // depth units

rs2::frameset fs = sync.wait_for_frames();
```

Main calls:

| Class | Method | Purpose |
|---|---|---|
| `software_device` | `add_sensor(name)` | Add a `software_sensor` |
| | `create_matcher(rs2_matchers)` | Choose how frames from its streams are matched by a `syncer` |
| | `register_info(info, value)` / `update_info(info, value)` | Set camera info such as name or serial number |
| | `add_to(context&)` | Add the device to a context so it is listed by `query_devices()` |
| | `set_destruction_callback(callback)` | Called when the device is destroyed |
| `software_sensor` | `add_video_stream`, `add_motion_stream`, `add_pose_stream`, `add_perception_stream` | Define stream profiles (optional `is_default` flag) |
| | `on_video_frame`, `on_motion_frame`, `on_pose_frame` | Inject a frame (`rs2_software_video_frame`, `rs2_software_motion_frame`, `rs2_software_pose_frame`) |
| | `set_metadata(value, type)` | Set metadata attached to the next frames |
| | `add_read_only_option`, `set_read_only_option`, `add_option` | Expose options |

The `rs2_software_video_frame` fields are `pixels`, `deleter`, `stride`, `bpp`, `timestamp`, `domain`, `frame_number`, `profile` and `depth_units`. The example notes that the `syncer` expects timestamps to advance by about 1000/FPS milliseconds between frames. See the [example readme](https://github.com/realsenseai/librealsense/blob/development/examples/software-device/readme.md) for the full walk-through.

## Callbacks and threads

- Frame callbacks passed to `sensor::start` or `pipeline::start` are called by the library as frames arrive, not on the thread that started streaming. The [callback example](https://github.com/realsenseai/librealsense/blob/development/examples/callback/rs-callback.cpp) therefore guards data shared with the main loop with a `std::mutex`.
- In the C++ wrapper, ownership of the callback object passes to the library (`rs2_start_cpp`, `rs2_set_devices_changed_callback_cpp`, `rs2_pipeline_start_with_callback_cpp`).
- To consume frames on your own thread, start the sensor with an `rs2::frame_queue` (`rs2_start_queue` in C) and read from the queue. See [[Frame Buffering Management in RealSense SDK 2.0]].

## Related pages

- [[API How To]]: short recipes for common tasks
- [[Code Samples]]: list of examples
- [[Frame Buffering Management in RealSense SDK 2.0]]: frame queues and frame lifetime
- Source documents: [`doc/api_arch.md`](https://github.com/realsenseai/librealsense/blob/development/doc/api_arch.md), [`doc/error_handling.md`](https://github.com/realsenseai/librealsense/blob/development/doc/error_handling.md), [`include/readme.md`](https://github.com/realsenseai/librealsense/blob/development/include/readme.md)

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`46cf1a6`](https://github.com/realsenseai/librealsense/commit/46cf1a67cc441ad9f55ce339a0de30f2a1c6fed0)._
<!-- /wikigen:footer -->
