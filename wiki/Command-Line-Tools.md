librealsense ships a set of small console tools next to the RealSense Viewer: they list connected cameras, record and convert recordings, collect frame statistics, read firmware logs, send raw commands to the firmware and benchmark the processing blocks. This page is for developers and support engineers who want to script these tasks or debug a camera without a GUI. It lists every tool in [`tools/`](https://github.com/realsenseai/librealsense/tree/development/tools) and how it is built, and for each tool covered here it gives every command-line flag its parser accepts.

The graphical tools have their own pages: [[Realsense Viewer Manual]], [[Depth Quality Tool]] and [[Firmware Update]] (`rs-fw-update`). DDS tools for network cameras are covered in [[Ethernet and DDS Cameras]].

## Which tools are built

The tool list is decided in [`tools/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/tools/CMakeLists.txt), using options from [`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake) (see [[Build Configuration]]):

| Executable | Source folder | Built when | Default build |
|---|---|---|---|
| `rs-convert` | `tools/convert` | `BUILD_TOOLS` | yes |
| `rs-enumerate-devices` | `tools/enumerate-devices` | `BUILD_TOOLS` | yes |
| `rs-fw-logger` | `tools/fw-logger` | `BUILD_TOOLS` | yes |
| `rs-terminal` | `tools/terminal` | `BUILD_TOOLS` | yes |
| `rs-record` | `tools/recorder` | `BUILD_TOOLS` | yes |
| `rs-embed` | `tools/embed` | `BUILD_TOOLS` | yes |
| `rs-fw-update` | `tools/fw-update` | `BUILD_TOOLS` | yes |
| `rs-dds-sniffer`, `rs-dds-adapter`, `rs-dds-config` | `tools/dds` | `BUILD_TOOLS` and `BUILD_WITH_DDS` | no |
| `realsense-viewer` | `tools/realsense-viewer` | `BUILD_EXAMPLES` and `BUILD_GRAPHICAL_EXAMPLES` | yes |
| `rs-depth-quality` | `tools/depth-quality` | `BUILD_EXAMPLES` and `BUILD_GRAPHICAL_EXAMPLES` | yes |
| `rs-imu-calibration.py` | `tools/rs-imu-calibration` | not built (Python script) | n/a |
| `rs-data-collect` | `tools/data-collect` | `BUILD_EXAMPLES` and `BUILD_GRAPHICAL_EXAMPLES` | yes |
| `rs-rosbag-inspector` | `tools/rosbag-inspector` | `BUILD_EXAMPLES` and `BUILD_GRAPHICAL_EXAMPLES` | yes |
| `rs-benchmark` | `tools/benchmark` | `BUILD_EXAMPLES` and `BUILD_GRAPHICAL_EXAMPLES` | yes |
| `rum_dev_server.py` | `tools/rum-uploader/dev-server` | not built (Python script) | n/a |

`BUILD_TOOLS`, `BUILD_EXAMPLES` and `BUILD_GRAPHICAL_EXAMPLES` all default to `ON`. `rs-data-collect` is a console program, but it is built only with the graphical examples. With `-DBUILD_GRAPHICAL_EXAMPLES=OFF` you lose `rs-data-collect`, `rs-rosbag-inspector` and `rs-benchmark`, and keep the `BUILD_TOOLS` set. Each tool's CMake file installs the executable into the runtime `bin` directory.

## Common command-line behaviour

Most tools parse their arguments with `rs2::cli` from [`common/cli.h`](https://github.com/realsenseai/librealsense/blob/development/common/cli.h), a wrapper around TCLAP. The tools use it at three levels:

- `rs2::cli` (used by `rs-convert`, `rs-enumerate-devices`, `rs-terminal`, `rs-record`, `rs-data-collect`, `rs-benchmark`, `rs-fw-update`, `realsense-viewer`, `rs-depth-quality`) adds `--debug` and the DDS flags `--eth`, `--eth-only`, `--no-eth`, `--domain-id`.
- `rs2::cli_no_dds` (used by `rs-fw-logger`, `rs-dds-config`, `rs-dds-adapter`) adds only `--debug`.
- `rs-embed` and `rs-dds-sniffer` use plain TCLAP and have none of the shared flags (`rs-dds-sniffer` defines its own `--debug`).

Every TCLAP parser also registers `-h`/`--help`, `--version` and `--`/`--ignore_rest`. With `rs2::cli`, `--version` prints the librealsense version (`RS2_API_FULL_VERSION_STR`).

Notes on the shared flags:

- `--debug` exists only when the library is built with `BUILD_EASYLOGGINGPP` (default `ON`). Without it, the tools log `ERROR` messages to the console (`WARN` for `rs-convert`, `rs-data-collect` and `rs-fw-update`); with it, they log `DEBUG` messages.
- The DDS flags are always accepted, but they only take effect when librealsense is built with `BUILD_WITH_DDS` (default `OFF`). In a DDS build, `--eth`, `--eth-only` and `--no-eth` are mutually exclusive, `--domain-id` cannot be combined with `--no-eth`, and `--domain-id` must be in the range 0–232.
- The parsed settings are passed to the `rs2::context` (or `rs2::pipeline`) that the tool creates. Exception: `rs-convert` passes them only for `-D` and PLY output; its other converters use a default `rs2::context`.

## Listing devices: rs-enumerate-devices

`rs-enumerate-devices` prints the connected devices with their camera info. By default it also prints the stream profiles (modes) of every sensor. Other flags add options, calibration data or default profiles. It can also open a recording file as a playback device. Source: [`tools/enumerate-devices/rs-enumerate-devices.cpp`](https://github.com/realsenseai/librealsense/blob/development/tools/enumerate-devices/rs-enumerate-devices.cpp).

```
rs-enumerate-devices -s          # one line per device: name, serial number, firmware version
rs-enumerate-devices -o          # add the supported options of every sensor
rs-enumerate-devices -p rec.db3  # enumerate a recording as a playback device
```

- `-s` and `-S` print a table with Device Name, Serial Number and Firmware Version. `-S` also prints the full camera info of each device. Both turn off the stream modes, options and calibration output. If you combine them with `-o` or `-c`, the tool prints a warning that those flags are ignored.
- `-d` marks the default profiles with `+` in the stream mode list; together with `-s` or `-S` (no mode list), it prints a separate list of the default streams instead.
- This tool sets DDS discovery itself: it is on unless `--no-eth` is given (in a DDS build).
- With `--sw-only` the tool lists only software devices (playback, DDS) and retries for about 3 seconds if none is found yet.
- If no device is found, it prints `No device detected. Is it plugged in?` and exits with failure.

<!-- wikigen:table-enumerate-devices -->
| Flag | Argument | Description | Default |
|---|---|---|---|
| `-h`, `--help` | | Displays usage information and exits. | off |
| `--version` | | Displays version information and exits. | off |
| `--`, `--ignore_rest` | | Ignores the rest of the labeled arguments following this flag. | off |
| `--debug` | | Turn on librealsense debug logs (only with `BUILD_EASYLOGGINGPP`). | off |
| `--eth` | | Detect DDS devices, even if disabled in the configuration (DDS builds only). | off |
| `--eth-only` | | Use only DDS devices; do not look for USB/MIPI devices (DDS builds only). | off |
| `--no-eth` | | Do not detect DDS devices, even if enabled in the configuration. | off |
| `--domain-id` | `<0-232>` | Domain ID to use with DDS devices (DDS builds only). | `0` |
| `-s`, `--short` | | Provide a one-line summary of the devices. | off |
| `-S`, `--compact` | | Provide a short summary of the devices. | off |
| `-o`, `--option` | | Show all the supported options per subdevice. | off |
| `-c`, `--calib_data` | | Show extrinsic and intrinsic of all subdevices. | off |
| `--sw-only` | | Show only software devices (playback, DDS, etc.), not USB/HID devices. | off |
| `--format` | `<raw/basic/FULL>` | Choose which format conversion the context uses (sets `format-conversion`). | `full` |
| `-v`, `--verbose` | | Show extra information. | off |
| `-d`, `--defaults` | | Show the default streams configuration. | off |
| `-p`, `--playback_device` | `<path>` | Inspect and enumerate a playback device (from file). | empty |
<!-- /wikigen:table-enumerate-devices -->

## Recording: rs-record

`rs-record` starts a pipeline with the default configuration, records it to a file for a set time, and prints the elapsed time once per second. Source: [`tools/recorder/rs-record.cpp`](https://github.com/realsenseai/librealsense/blob/development/tools/recorder/rs-record.cpp). The recording format depends on the build: with `BUILD_ROSBAG2` (default `ON`) the library writes rosbag2 (`.db3`) files. Without it, the library writes ROS1 `.bag` files and refuses a `.db3` output name. See [[Record and Playback]].

```
rs-record -f recording.db3 -t 5
```

<!-- wikigen:table-recorder -->
| Flag | Argument | Description | Default |
|---|---|---|---|
| `-h`, `--help` | | Displays usage information and exits. | off |
| `--version` | | Displays version information and exits. | off |
| `--`, `--ignore_rest` | | Ignores the rest of the labeled arguments following this flag. | off |
| `--debug` | | Turn on librealsense debug logs (only with `BUILD_EASYLOGGINGPP`). | off |
| `--eth` | | Detect DDS devices, even if disabled in the configuration (DDS builds only). | off |
| `--eth-only` | | Use only DDS devices; do not look for USB/MIPI devices (DDS builds only). | off |
| `--no-eth` | | Do not detect DDS devices, even if enabled in the configuration. | off |
| `--domain-id` | `<0-232>` | Domain ID to use with DDS devices (DDS builds only). | `0` |
| `-t`, `--Time` | `<seconds>` | Amount of time to record, in seconds (fractional values accepted). | `10` |
| `-f`, `--FullFilePath` | `<path>` | The file where the data will be saved. **Required.** | none |
<!-- /wikigen:table-recorder -->

## Converting recordings: rs-convert

`rs-convert` reads a recording and writes its frames as PNG, CSV, 3D CSV, RAW, PLY or BIN files, or as text on stdout. It can also convert a legacy ROS1 `.bag` file to `.db3`. Source: [`tools/convert/rs-convert.cpp`](https://github.com/realsenseai/librealsense/blob/development/tools/convert/rs-convert.cpp) and the writers in [`tools/convert/converters`](https://github.com/realsenseai/librealsense/tree/development/tools/convert/converters).

```
rs-convert -i recording.bag -D recording.db3     # legacy .bag -> .db3, nothing else is done
rs-convert -i recording.db3 -p out/frame -d      # depth frames to PNG
rs-convert -i recording.db3 -l out/cloud         # point clouds to PLY
```

- `-i` is required. You need at least one output flag (`-p`, `-v`, `-V`, `-r`, `-l`, `-b` or `-T`), otherwise the tool stops with `output not defined`.
- If `-D` is given, the tool only converts `.bag` to `.db3` (through `rs2::context::convert_bag_to_db3`) and then exits.
- The output paths are file-name prefixes. For example, PNG files are named `<path>_<stream>_<timestamp>.png`, and each PNG gets a `<path>_<stream>_metadata_<timestamp>.txt` file next to it. Depth frames are colorized before they are saved as PNG.
- `-d` limits the conversion to depth and `-c` to color. If you give neither, all supported streams are converted. `-b` (BIN) writes depth frames only.
- PLY output runs the playback through a pipeline, because each point cloud needs matching depth and color frames.
- `-f`/`-t` filter frames by frame number and `-s`/`-e` by time in seconds, where the first frame is at time 0.

<!-- wikigen:table-convert -->
| Flag | Argument | Description | Default |
|---|---|---|---|
| `-h`, `--help` | | Displays usage information and exits. | off |
| `--version` | | Displays version information and exits. | off |
| `--`, `--ignore_rest` | | Ignores the rest of the labeled arguments following this flag. | off |
| `--debug` | | Turn on librealsense debug logs (only with `BUILD_EASYLOGGINGPP`). | off |
| `--eth` | | Detect DDS devices, even if disabled in the configuration (DDS builds only). | off |
| `--eth-only` | | Use only DDS devices; do not look for USB/MIPI devices (DDS builds only). | off |
| `--no-eth` | | Do not detect DDS devices, even if enabled in the configuration. | off |
| `--domain-id` | `<0-232>` | Domain ID to use with DDS devices (DDS builds only). | `0` |
| `-i`, `--input` | `<ros-bag-file>` | Input recording file name. **Required.** | none |
| `-t`, `--last-framenumber` | `<frame>` | Ignore frames whose frame number is greater than this value. | not set |
| `-f`, `--first-framenumber` | `<frame>` | Ignore frames whose frame number is less than this value. | not set |
| `-e`, `--end-time` | `<seconds>` | Ignore frames whose timestamp is greater than this value (the first frame is at time 0). | not set |
| `-s`, `--start-time` | `<seconds>` | Ignore frames whose timestamp is less than this value (the first frame is at time 0). | not set |
| `-p`, `--output-png` | `<png-path>` | Output PNG file(s) path. | not set |
| `-v`, `--output-csv` | `<csv-path>` | Output CSV (depth matrix) file(s) path. | not set |
| `-V`, `--output-3d-csv` | `<3d-csv-path>` | Output 3D CSV (depth matrix) file(s) path. | not set |
| `-r`, `--output-raw` | `<raw-path>` | Output RAW file(s) path. | not set |
| `-l`, `--output-ply` | `<ply-path>` | Output PLY file(s) path. | not set |
| `-b`, `--output-bin` | `<bin-path>` | Output BIN (depth matrix) file(s) path. | not set |
| `-d`, `--depth` | | Convert depth frames only. | off (all supported) |
| `-c`, `--color` | | Convert color frames only. | off (all supported) |
| `-T`, `--output-text` | | Output text to stdout. | off |
| `-D`, `--output-db3` | `<db3-path>` | Convert a legacy `.bag` file to `.db3` format. | not set |
<!-- /wikigen:table-convert -->

## Inspecting recordings: rs-rosbag-inspector

`rs-rosbag-inspector` is a GUI tool (ImGui/OpenGL window titled "RealSense Rosbag Inspector") for browsing the topics and messages of legacy ROS1 `.bag` files. You open files with the **Load File...** menu item, which filters for `*.bag`, or by dragging them onto the window. It has no command-line flags; `main` ignores its arguments. On Windows it is built as a GUI (`WIN32`) application. For `.db3` recordings, [`tools/rosbag-inspector/readme.md`](https://github.com/realsenseai/librealsense/blob/development/tools/rosbag-inspector/readme.md) suggests third-party viewers that read rosbag2 files. To convert a `.bag` file first, use `rs-convert -D`. Source: [`tools/rosbag-inspector/rs-rosbag-inspector.cpp`](https://github.com/realsenseai/librealsense/blob/development/tools/rosbag-inspector/rs-rosbag-inspector.cpp).

## Frame statistics: rs-data-collect

`rs-data-collect` configures sensors on one device from a CSV configuration file, collects frame statistics through the low-level sensor API, and saves them to a CSV file. It does not save image content. Source: [`tools/data-collect/rs-data-collect.cpp`](https://github.com/realsenseai/librealsense/blob/development/tools/data-collect/rs-data-collect.cpp).

```
rs-data-collect -c ./data_collect.cfg -f ./log.csv -t 60 -m 1000
```

- `-c` is required at run time. Without it, the tool stops with `The tool requires a profile configuration file to proceed`.
- Each configuration line is `STREAM,WIDTH,HEIGHT,FPS,FORMAT[,STREAM_INDEX]`, for example `DEPTH,640,480,30,Z16,0`. Stream and format names are matched case-insensitively against the librealsense names. Lines that do not start with a letter are skipped, so they can hold comments. Two requests for the same stream type and index are rejected.
- The tool uses the device with serial number `-s`, or the first device found. If no device is connected, it waits and retries every 3 seconds.
- When to stop:
  - neither `-t` nor `-m` given: after 100 frames per stream;
  - only `-t` given: after that many seconds, with no frame limit;
  - only `-m` given: after that many frames per stream;
  - both given: at whichever limit comes first.
- If `-f` is not given, the output file is `frames_data.csv`.

<!-- wikigen:table-data-collect -->
| Flag | Argument | Description | Default |
|---|---|---|---|
| `-h`, `--help` | | Displays usage information and exits. | off |
| `--version` | | Displays version information and exits. | off |
| `--`, `--ignore_rest` | | Ignores the rest of the labeled arguments following this flag. | off |
| `--debug` | | Turn on librealsense debug logs (only with `BUILD_EASYLOGGINGPP`). | off |
| `--eth` | | Detect DDS devices, even if disabled in the configuration (DDS builds only). | off |
| `--eth-only` | | Use only DDS devices; do not look for USB/MIPI devices (DDS builds only). | off |
| `--no-eth` | | Do not detect DDS devices, even if enabled in the configuration. | off |
| `--domain-id` | `<0-232>` | Domain ID to use with DDS devices (DDS builds only). | `0` |
| `-t`, `--Timeout` | `<seconds>` | Max amount of time to receive frames, in seconds. | not set (`10` in help) |
| `-m`, `--MaxFrames_Number` | integer (no label) | Maximum number of frames per stream to receive. | `100` |
| `-f`, `--FullFilePath` | `<path>` | The file where the data will be saved. | `frames_data.csv` |
| `-c`, `--ConfigurationFile` | `<path>` | File path with the requested stream configuration. | none (needed) |
| `-s`, `--SerialNumber` | `<serial_number>` | Serial number of the device to use. | first device |
<!-- /wikigen:table-data-collect -->

## Firmware logs: rs-fw-logger

`rs-fw-logger` waits for a device, by serial number or the first one found, then polls its firmware log and prints each message. Messages are printed as raw hex bytes, or parsed when you give an events XML file that the device's parser accepts. With `-f` it reads the flash log instead and exits when no flash log entries are left. Otherwise it keeps logging and waits for the device again if it disconnects. The device must support the `rs2::firmware_logger` extension; if it does not, the tool exits with an error. Library warnings are written to `librealsense.log`. Source: [`tools/fw-logger/rs-fw-logger.cpp`](https://github.com/realsenseai/librealsense/blob/development/tools/fw-logger/rs-fw-logger.cpp).

```
rs-fw-logger -s <serial> -l events.xml -o fw_log.txt
```

- If `-o` is not given (or the file cannot be opened), the output goes to the console.
- The polling interval must be 25–300 ms. A value outside that range is replaced by 100 ms, and the tool prints a notice.
- This tool uses `rs2::cli_no_dds`, so it has no DDS flags.

<!-- wikigen:table-fw-logger -->
| Flag | Argument | Description | Default |
|---|---|---|---|
| `-h`, `--help` | | Displays usage information and exits. | off |
| `--version` | | Displays version information and exits. | off |
| `--`, `--ignore_rest` | | Ignores the rest of the labeled arguments following this flag. | off |
| `--debug` | | Turn on librealsense debug logs (only with `BUILD_EASYLOGGINGPP`). | off |
| `-s`, `--sn` | `<serial-number>` | Camera serial number. | first device |
| `-l`, `--load` | `<path>` | Full path of the firmware log events XML file. | empty (raw output) |
| `-o`, `--out` | `<path>` | Full output file path. | empty (console) |
| `-p`, `--polling_interval` | `<milliseconds>` | Time between log message polls, in milliseconds (valid 25–300). | `100` |
| `-f`, `--flash` | | Read the flash logs instead of the live firmware log. | off |
<!-- /wikigen:table-fw-logger -->

## Raw firmware commands: rs-terminal

`rs-terminal` is a troubleshooting tool that sends commands to the camera firmware through the `rs2::debug_protocol` extension. Without `-l`, commands are typed as hexadecimal bytes separated by spaces. With `-l`, it loads a commands XML file, and you can type commands by name. It runs in one of three modes:

- one command with `-s`;
- a script file with `-r` (hex lines) or `-c` (named commands, needs `-l`);
- an interactive prompt (`#>`) with auto-completion, where commands go to the selected devices, `next` waits for a device from the device hub (commands still go to the selected devices) and `exit` quits.

Source: [`tools/terminal/rs-terminal.cpp`](https://github.com/realsenseai/librealsense/blob/development/tools/terminal/rs-terminal.cpp).

```
rs-terminal -n <serial> -l commands.xml -c script.txt
```

- Device selection: `-a` selects all devices, `-n` a serial number, `-d` an index in the device list. If none is given, the tool uses the only connected device. With more than one device, it asks for `-d`, `-n` or `-a` and exits.
- Library warnings are written to `librealsense.log`.

<!-- wikigen:table-terminal -->
| Flag | Argument | Description | Default |
|---|---|---|---|
| `-h`, `--help` | | Displays usage information and exits. | off |
| `--version` | | Displays version information and exits. | off |
| `--`, `--ignore_rest` | | Ignores the rest of the labeled arguments following this flag. | off |
| `--debug` | | Turn on librealsense debug logs (only with `BUILD_EASYLOGGINGPP`). | off |
| `--eth` | | Detect DDS devices, even if disabled in the configuration (DDS builds only). | off |
| `--eth-only` | | Use only DDS devices; do not look for USB/MIPI devices (DDS builds only). | off |
| `--no-eth` | | Do not detect DDS devices, even if enabled in the configuration. | off |
| `--domain-id` | `<0-232>` | Domain ID to use with DDS devices (DDS builds only). | `0` |
| `-l`, `--load` | `<path>` | Full path of the commands XML file. | empty (hex mode) |
| `-d`, `--deviceId` | integer (no label) | Device index, as listed by `rs-enumerate-devices`. | `0` |
| `-n`, `--serialNum` | `<serial-number>` | Serial number of the device, as listed by `rs-enumerate-devices`. | not set |
| `-a`, `--allDevices` | | Send to all attached RealSense devices. | off |
| `-s`, `--send` | `<hex-data>` | Hexadecimal raw data to send once. | not set |
| `-r`, `--raw` | `<path>` | Full file path of a hexadecimal raw data script. | not set |
| `-c`, `--cmd` | `<path>` | Full file path of a commands script. | not set |
| `-v`, `--verbose` | | Print raw data buffers. | off |
<!-- /wikigen:table-terminal -->

## Processing benchmark: rs-benchmark

`rs-benchmark` needs a device with a depth stream plus a color or infrared stream. It records about 5 seconds of frames per stream, then times each processing block on them. The output is Markdown tables with CPU, GPU, graphics driver and device name, followed by median, mean, standard deviation, max time and max FPS per filter and step.

- CPU blocks: colorizer, pointcloud, spatial, temporal, disparity, threshold and decimation filters for depth, and the YUY decoder for YUYV color.
- GPU blocks: the `rs2::gl` versions of colorizer, pointcloud and YUY decoder, run through a hidden GLFW window. They are skipped on macOS.

The tool has only the shared flags. Source: [`tools/benchmark/rs-benchmark.cpp`](https://github.com/realsenseai/librealsense/blob/development/tools/benchmark/rs-benchmark.cpp).

<!-- wikigen:table-benchmark -->
| Flag | Argument | Description | Default |
|---|---|---|---|
| `-h`, `--help` | | Displays usage information and exits. | off |
| `--version` | | Displays version information and exits. | off |
| `--`, `--ignore_rest` | | Ignores the rest of the labeled arguments following this flag. | off |
| `--debug` | | Turn on librealsense debug logs (only with `BUILD_EASYLOGGINGPP`). | off |
| `--eth` | | Detect DDS devices, even if disabled in the configuration (DDS builds only). | off |
| `--eth-only` | | Use only DDS devices; do not look for USB/MIPI devices (DDS builds only). | off |
| `--no-eth` | | Do not detect DDS devices, even if enabled in the configuration. | off |
| `--domain-id` | `<0-232>` | Domain ID to use with DDS devices (DDS builds only). | `0` |
<!-- /wikigen:table-benchmark -->

## Embedding resources: rs-embed

`rs-embed` is a build helper that turns a resource file into a C++ header:

- For an `.obj` 3D model, it compresses the vertex and index data with LZ4. The header holds a `<name>_obj_data` array and an `uncompress_<name>_obj(...)` function.
- For a `.png` image, the header holds `<name>_png_size` and a `<name>_png_data` byte array.

It uses plain TCLAP, so its `--version` prints the tool's own version (`0.0.0.2`), and it has no `--debug` or DDS flags. Source: [`tools/embed/rs-embed.cpp`](https://github.com/realsenseai/librealsense/blob/development/tools/embed/rs-embed.cpp).

```
rs-embed -i model.obj -o model.h -n model
```

<!-- wikigen:table-embed -->
| Flag | Argument | Description | Default |
|---|---|---|---|
| `-h`, `--help` | | Displays usage information and exits. | off |
| `--version` | | Displays version information and exits. | off |
| `--`, `--ignore_rest` | | Ignores the rest of the labeled arguments following this flag. | off |
| `-i`, `--input` | `<input-file>` | Input file name (`.obj` or `.png`). **Required.** | none |
| `-o`, `--output` | `<output-file>` | Output header file name. | empty |
| `-n`, `--name` | `<object-name>` | Name used for the generated array and function names. | empty |
<!-- /wikigen:table-embed -->

## RUM development server: rum_dev_server.py

[`tools/rum-uploader/dev-server/rum_dev_server.py`](https://github.com/realsenseai/librealsense/blob/development/tools/rum-uploader/dev-server/rum_dev_server.py) is a Python test server for the usage-statistics uploader (see [[Usage Statistics RUM]]). It is not the production server. It listens on `127.0.0.1` for `POST /v1/rum`, saves each body as a JSON file in the output folder, prints a summary of the report, and replies `200`. The Viewer's uploader always targets the production endpoint. To send reports to this server, you must change `RUM_ENDPOINT` in [`common/rum-uploader/rum-uploader.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/rum-uploader/rum-uploader.cpp) and rebuild.

```
python rum_dev_server.py --port 8080 --dir received
```

<!-- wikigen:table-rum-uploader -->
| Flag | Argument | Description | Default |
|---|---|---|---|
| `-h`, `--help` | | Show the argparse help message and exit. | off |
| `--port` | integer | Port to listen on (on `127.0.0.1`). | `8080` |
| `--dir` | path | Folder where received reports are written. | `received` |
<!-- /wikigen:table-rum-uploader -->

The script has no `--version` flag.

## See also

- [`tools/readme.md`](https://github.com/realsenseai/librealsense/blob/development/tools/readme.md): overview of the tool folders
- [[Record and Playback]]
- [[Firmware Update]]
- [[Troubleshooting Q&A]]

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`65ed974`](https://github.com/realsenseai/librealsense/commit/65ed97469471f9d104969d2b7d029506058819bd)._
<!-- /wikigen:footer -->
