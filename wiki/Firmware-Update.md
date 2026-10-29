RealSense depth cameras run firmware that can be replaced from the host. librealsense provides three ways to do it: the RealSense Viewer, the `rs-fw-update` command-line tool, and a public API that both of them use. You need this page when a release asks for newer firmware, when the Viewer recommends an update, or when a camera is stuck in recovery mode and has to be reflashed.

## Before you start

- Get a firmware image (`.bin`) for your camera. The SDK does not ship firmware images; they are published on the RealSense firmware releases page, https://dev.realsenseai.com/docs/firmware-updates.
- Close every other application that uses the camera. When the library cannot write a firmware block to a USB update device, the Viewer and `rs-fw-update` show an error ending in `Please verify that no other librealsense application is running` (with `-r`, `rs-fw-update` prints only `Failed to recover device`; see [Troubleshooting](#troubleshooting)).
- On Linux, the udev rules must be installed (`./scripts/setup_udev_rules.sh` when building from source). See [[Installation Linux]].
- Do not disconnect the camera while an update is running.

Which devices can be updated is decided by the device classes in the library: D400 and D500 devices implement the update interface ([`src/ds/d400/d400-device.h`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-device.h), [`src/ds/d500/d500-device.h`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-device.h)), and so do Ethernet (DDS) devices ([`src/dds/rs-dds-device-proxy.h`](https://github.com/realsenseai/librealsense/blob/development/src/dds/rs-dds-device-proxy.h)). Recovery-mode devices of the D400 and D500 product lines, over USB and over GMSL (MIPI), are handled by [`src/fw-update/fw-update-factory.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/fw-update/fw-update-factory.cpp). See [[Supported Devices]] for the camera list.

## Update firmware with the RealSense Viewer

1. Connect the camera and open the RealSense Viewer. Stop all streams: the firmware items are disabled while any sensor is streaming (the tooltip then says `(Disabled while streaming)`).
2. Open the device's **More** menu (the `[=]` icon in the device panel) and select **Update Firmware**. The item is shown for devices that are updatable and for devices already in recovery mode.
3. Pick the image in the `Signed Firmware Image` file dialog (`*.bin` or `*.img`).
4. A `Firmware Update` window shows the progress and a log. Wait until it reports `FW update process completed successfully`, then press **OK**.

What the Viewer does for a signed image on a USB device ([`common/fw-update-helper.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/fw-update-helper.cpp)):

1. Checks that the image is compatible with the device. If not, it fails with `The firmware version is not compatible with <device name>`.
2. Tries to back up the camera flash (see [Flash backup](#flash-backup)).
3. Logs `Requesting to switch to recovery mode`, moves the device to its update state and waits up to 60 seconds for the update device with the same firmware update ID (`DFU device '<id>' found`).
4. Writes the image (`Recovery device connected, starting update..`), then waits up to 60 seconds for the camera to come back (`Discovered connection of the original device`, then `Device reconnected successfully!`).

For GMSL (MIPI) devices with a signed image the Viewer skips the backup and the switch to update state, and writes the image directly. When the device was in GMSL recovery mode, the Viewer cannot see the device come back by itself and logs:

```
For GMSL MIPI device please reboot, or reload d4xx driver
sudo rmmod d4xx && sudo modprobe d4xx
and restart the realsense-viewer
```

### Unsigned firmware in the Viewer

**Update Unsigned Firmware...** appears in the same menu only when the device is updatable, reports `RS2_CAMERA_INFO_CAMERA_LOCKED` with a value other than `YES` (a device that does not report it is treated as locked), and has product line `D400` ([`common/device-model.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/device-model.cpp)). It opens an `Unsigned Firmware Image` file dialog and writes the image straight to flash. See [Signed and unsigned images](#signed-and-unsigned-images).

## Update firmware with rs-fw-update

`rs-fw-update` is built with the other tools when `BUILD_TOOLS` is ON (the default) and is installed to the binary directory. Source: [`tools/fw-update/rs-fw-update.cpp`](https://github.com/realsenseai/librealsense/blob/development/tools/fw-update/rs-fw-update.cpp). See also [[Command Line Tools]].

### List the connected devices

```
rs-fw-update -l
```

Each line shows the device description, its `update serial number` (the firmware update ID) and `firmware version`, plus `SMCU firmware version` when the device reports one:

```
Connected devices:
1) [<connection type>] <name> s/n <serial>, update serial number: <update id>, firmware version: <version>
```

Running the tool with no arguments prints `Nothing to do, run again with -h for help` and then the same list.

### Update one camera

With only one camera connected:

```
rs-fw-update -f <path-to-image>.bin
```

With several cameras connected, select one by serial number:

```
rs-fw-update -s <serial_number> -f <path-to-image>.bin
```

The tool then:

1. Reads the file and prints `Updating device FW:` with the device line.
2. Checks compatibility; if the image does not fit the device it prints `This firmware version is not compatible with <device name>` and stops.
3. For devices that are not GMSL (MIPI), asks the device to enter its update state. A device that already is an update device (GMSL devices, Ethernet (DDS) devices) is used directly; otherwise the tool waits up to 15 seconds for it to re-enumerate as an update device.
4. Writes the image, showing `Firmware update progress: <n>[%]` when the output is a terminal, then prints `Firmware update done`.
5. Prints `Waiting for device to reconnect...`, waits up to 15 seconds, and prints `Device <serial> successfully updated to FW: <version>` for the reconnected camera.

If the camera is connected through a USB 2 port, the tool prints a warning first and still continues:

```
Warning! the camera is connected via USB 2 port, in case the process fails, connect the camera to a USB 3 port and try again
```

### Command-line flags

The tool uses the shared parser in [`common/cli.h`](https://github.com/realsenseai/librealsense/blob/development/common/cli.h) (`rs2::cli`), so it also takes the shared DDS flags and the parser's own `--`, `--version` and `-h`/`--help`.

<!-- wikigen:table-rs-fw-update-flags -->
| Flag | Value | Description |
|---|---|---|
| `-h`, `--help` | | Print usage and exit. |
| `--version` | | Print the librealsense version and exit. |
| `--`, `--ignore_rest` | | Ignore the rest of the labeled arguments after this flag. |
| `--debug` | | Turn on librealsense debug logs. Registered only when built with `BUILD_EASYLOGGINGPP` (ON by default). Without it the tool logs at warning level. |
| `--eth` | | Detect DDS devices, even if disabled in the configuration. Ignored when not built with DDS support. |
| `--eth-only` | | Use only DDS devices; do not look for USB/MIPI devices. Ignored when not built with DDS support. |
| `--no-eth` | | Do not detect DDS devices, even if enabled in the configuration. No-op when not built with DDS support. |
| `--domain-id` | `0-232` | Domain ID to use with DDS devices (default 0). Ignored when not built with DDS support. |
| `-l`, `--list_devices` | | List all available devices and exit. |
| `-r`, `--recover` | | Recover a device that is in recovery mode. Needs `-f`. See [Recovery mode](#recovery-mode). |
| `-u`, `--unsigned` | | Write the image given with `-f` as unsigned firmware. Available only for unlocked cameras. |
| `-f`, `--file` | `path` | Path of the firmware image file. |
| `-s`, `--serial_number` | `string` | Serial number of the device to update. Required when more than one device is connected. With `-r` it is matched against the update serial number. |
| `-b`, `--backup` | `path` | Back up the camera flash and save it to the given path. |
<!-- /wikigen:table-rs-fw-update-flags -->

When built with DDS support, `--eth`, `--eth-only` and `--no-eth` are mutually exclusive, and `--domain-id` cannot be combined with `--no-eth`. See [[Ethernet and DDS Cameras]].

Flag combinations, as the tool handles them:

- `-l` wins over everything else.
- With other flags (such as `-r`, `-u` or `-s`) but without `-f` or `-b`, the tool prints `Nothing to do, run again with -h for help` and exits with an error.
- `-b` alone only makes the backup; `-b` together with `-f` makes the backup first and then updates.
- `-u` only has an effect together with `-f`.

## Recovery mode

A camera whose firmware is missing or damaged comes up as a recovery (DFU) device. It shows in `rs-fw-update -l` with its update serial number; its firmware version shows as `unknown` when the device does not report one. If you run a normal update on a USB or GMSL recovery device that is the only connected device, the tool prints `Device is in recovery mode, use -r to recover`.

Recover it with:

```
rs-fw-update -r -f <path-to-image>.bin
```

If more than one recovery device is connected, add `-s <update serial number>` (the ID shown as `update serial number`), otherwise the tool prints `More than one recovery device is connected; serial number must be specified`.

The tool prints `Update to FW: <file>`, `Recovering device:` with the device line, writes the image, prints `Waiting for new device...` and waits up to 15 seconds for a normal (non-recovery) device with the same update serial number. On success it prints `Recovery done`.

GMSL (MIPI) recovery devices do not reconnect by themselves. For them the tool does not wait, and after `Recovery done` it prints:

```
For GMSL MIPI device please reboot, or reload d4xx driver
sudo rmmod d4xx && sudo modprobe d4xx
or reboot the system
```

For GMSL devices the library writes the whole image to the device's DFU character device and reports progress estimated from the image size, held below 100% until the write finishes *(changed in the next release)*. In v2.59.1 the image was written in 1 KB blocks with progress reported per block.

In the Viewer, a recovery device shows the same **Update Firmware** menu item; the Viewer writes the image to it directly and then waits for the camera to reconnect, except for GMSL recovery devices, where it logs the driver-reload message shown above instead.

## Signed and unsigned images

**Signed images** are the official firmware releases. They go through the DFU (device firmware upgrade) path: the device switches to an update device, the library checks that the image is compatible, then streams it to the device ([`src/fw-update/fw-update-device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/fw-update/fw-update-device.cpp)). If a device is locked to a minimum version, the library reports `Device: <serial> is locked for update.` followed by `Use firmware version higher than: <version>`.

**Unsigned images** are written directly to the camera flash, bypassing DFU. This works only on unlocked cameras:

- A locked camera fails with `this camera is locked and doesn't allow direct flash write, for firmware update use rs2_update_firmware method (DFU)`.
- An image of the wrong size fails with `Unsupported firmware binary image (unsigned) provided - <n> bytes`.
- D500 devices do not support it: `D500 device does not support unsigned FW update`.

These messages come from [`src/ds/ds-device-common.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/ds-device-common.cpp) and [`src/ds/d500/d500-device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-device.cpp). `rs-fw-update -u` does not run the compatibility check before writing.

The API's unsigned call takes an `update_mode`: `RS2_UNSIGNED_UPDATE_MODE_UPDATE` (0, the default), `RS2_UNSIGNED_UPDATE_MODE_READ_ONLY` (1) or `RS2_UNSIGNED_UPDATE_MODE_FULL` (2). The header warns that any mode other than `RS2_UNSIGNED_UPDATE_MODE_UPDATE` is unsafe and might damage the camera. The Viewer and `rs-fw-update` always use the default.

## Flash backup

A flash backup is a copy of the camera flash. It is not a valid firmware image and cannot be loaded back to the device, but it contains the calibration and device information (`rs2::updatable::create_flash_backup` in [`include/librealsense2/hpp/rs_device.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_device.hpp)).

- **rs-fw-update:** `-b <path>` writes the backup to `<path>` and, when the output is a terminal, prints `Flash backup progress: <n>[%]`. If the device returns no data it prints `Backup flash is not supported`; if the file cannot be written, `Creating backup file failed`.
- **Viewer:** before a signed or unsigned update of a non-GMSL device, the Viewer tries a backup automatically and saves it as `<update id>.<timestamp>.bin` in the application data folder: the roaming AppData folder on Windows, and the home directory as a hidden file (`~/.<update id>.<timestamp>.bin`) on Linux and macOS. The log shows `Backup completed and saved as '<path>'`, or `Backup flash is not supported`. If the backup fails the Viewer logs `WARNING: backup failed; continuing without it...` and continues.

D500 devices return an empty backup ([`src/ds/d500/d500-device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-device.cpp)), so for them both tools report that backup is not supported.

## Update firmware from your own code

The public API is in [`include/librealsense2/h/rs_device.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_device.h) (C) and [`include/librealsense2/hpp/rs_device.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_device.hpp) (C++).

| C++ | C | What it does |
|---|---|---|
| `rs2::updatable` | `RS2_EXTENSION_UPDATABLE` | A normal device that can be updated. |
| `rs2::updatable::check_firmware_compatibility(image)` | `rs2_check_firmware_compatibility` | `true` if the image fits the device. |
| `rs2::updatable::enter_update_state()` | `rs2_enter_update_state` | The device disconnects and reconnects as an update device. |
| `rs2::updatable::create_flash_backup([callback])` | `rs2_create_flash_backup`, `rs2_create_flash_backup_cpp` | Read the camera flash. |
| `rs2::updatable::update_unsigned(image, [callback], [update_mode])` | `rs2_update_firmware_unsigned`, `rs2_update_firmware_unsigned_cpp` | Write an unsigned image to flash (unlocked cameras only). |
| `rs2::update_device` | `RS2_EXTENSION_UPDATE_DEVICE` | A device in update (recovery/DFU) state. |
| `rs2::update_device::update(image, [callback])` | `rs2_update_firmware`, `rs2_update_firmware_cpp` | Write a signed image. Runs on the caller's thread; the callback gets progress from 0 to 1. |
| `rs2::device::is_in_recovery_mode()` | `rs2_is_in_recovery_mode` | `true` for a device in recovery mode. |
| `rs2::device::get_firmware_min_version()` | `rs2_get_firmware_min_version` | Minimum firmware version supported by the device's SKU, as a dotted string. |

`rs2_update_firmware` and `rs2_update_firmware_cpp` work only on a device for which `is<rs2::update_device>()` is true. For a USB camera, call `enter_update_state()` first and find the device again by `RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID`, which both tools use to match the device across the switch. A short C++ version of what `rs-fw-update` does for one USB camera:

```cpp
#include <librealsense2/rs.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>
#include <vector>

int main(int argc, char** argv) try
{
    std::ifstream file(argv[1], std::ios::binary);
    std::vector<uint8_t> image((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    rs2::context ctx;
    rs2::device dev = ctx.query_devices().front();
    std::string update_id = dev.get_info(RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID);

    rs2::updatable upd = dev.as<rs2::updatable>();
    if (!upd.check_firmware_compatibility(image))
        return 1;                                   // image is not for this device

    upd.enter_update_state();                       // device disconnects and returns as an update device

    rs2::update_device fwu;
    for (int i = 0; i < 150 && !fwu; ++i)           // wait up to ~15 s
    {
        for (auto&& d : ctx.query_devices())
            if (d.is<rs2::update_device>() && d.get_info(RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID) == update_id)
                fwu = d;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!fwu)
        return 1;

    fwu.update(image, [](float progress) {
        std::cout << "\rFirmware update progress: " << int(progress * 100) << "%" << std::flush;
    });
    std::cout << std::endl;
    return 0;
}
catch (const rs2::error& e)
{
    std::cerr << e.what() << std::endl;
    return 1;
}
```

For the full flow, including recovery devices, GMSL devices, several cameras and device-change callbacks, read [`tools/fw-update/rs-fw-update.cpp`](https://github.com/realsenseai/librealsense/blob/development/tools/fw-update/rs-fw-update.cpp). The Python wrapper exposes the same classes: `updatable` (`enter_update_state`, `create_flash_backup`, `update_unsigned`, `check_firmware_compatibility`), `update_device` (`update`) and `device.is_in_recovery_mode` ([`wrappers/python/pyrs_device.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/pyrs_device.cpp)). See [[Python Wrapper]].

## SW/FW update notifications in the Viewer

The Viewer can tell you that newer software or firmware is available ([`common/device-model.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/device-model.cpp), [`common/updates-model.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/updates-model.cpp), [`common/sw-update/`](https://github.com/realsenseai/librealsense/tree/development/common/sw-update)).

- **Build option:** the feature is compiled in only with `CHECK_FOR_UPDATES`, which is ON by default except on macOS, and is turned off for Android builds and when `BUILD_GRAPHICAL_EXAMPLES` is OFF ([`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake), [`CMake/global_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/global_config.cmake)). See [[Build Configuration]].
- **Triggers:** automatically when a device is added to the Viewer (not for playback devices), and manually with **Check For Updates** in the device's More menu.
- **Versions database:** the Viewer downloads a versions database and compares the device's software and firmware versions with it. The server is set in **Settings > Online Services > SW/FW Updates From Server**: **Official Server** or **Custom Server**. A custom URL that starts with `file://` is read as a local file.
- **Recommended firmware:** the Viewer downloads the recommended image (up to 3 tries) and shows a `Firmware Update Recommended!` notification with the current and recommended versions and an **Install** button. Install stops any streaming on the device and runs the same signed update as **Update Firmware**. If the download fails, the Viewer logs `Error in downloading FW binary file: <link>`.
- **Recommended software:** a `Software Update Recommended!` notification.
- **Essential updates:** when an update is marked essential, the Viewer opens the `SOFTWARE UPDATES` window instead of a notification. Its firmware part has a **Download & Install** button that downloads the image and runs the signed update. On failure it shows `Firmware download failed, check connection and press to retry` or `Firmware update process failed, press to retry`.
- **Nothing new:** after a manual check, the Viewer shows `SW/FW Versions All Up To Date`. If the database cannot be reached during a manual check it shows `Unable to retrieve updates.` / `Please check your network connection.`

Update notifications can be dismissed or postponed. A postponed notification is keyed by the recommended version and the device serial number, so a newer recommended version is shown again. The online database may lag behind the [GitHub releases page](https://github.com/realsenseai/librealsense/releases) and the firmware releases page.

The repository's diagram of this flow:

![SW/FW updates flow](https://raw.githubusercontent.com/realsenseai/librealsense/development/doc/img/updates/updates.png)

## Troubleshooting

| Message | Printed by | What to do |
|---|---|---|
| `Several devices are connected, serial number must be selected using -s <serial_number>` | `rs-fw-update` ([`rs-fw-update.cpp`](https://github.com/realsenseai/librealsense/blob/development/tools/fw-update/rs-fw-update.cpp)) | Add `-s`, or disconnect the other cameras. |
| `Couldn't find the requested serial number` | `rs-fw-update` | Check the serial number with `-l`. |
| `No devices were found` | `rs-fw-update` | Check the connection, cable and (on Linux) udev rules. |
| `Device is in recovery mode, use -r to recover` | `rs-fw-update` | Run again with `-r -f <image>`. |
| `No recovery devices were found!` | `rs-fw-update` (with `-r`) | The camera is not in recovery mode; update it without `-r`. |
| `Failed to locate a device in FW update mode` | `rs-fw-update` | The device did not come back as an update device within 15 seconds. Reconnect it and run `-l`; if it now shows as a recovery device, use `-r`. |
| `... timed out!` | `rs-fw-update` (with `-r`) | The recovered device did not reconnect within 15 seconds. Reconnect it and check with `-l`. |
| `Failed to recover device` | `rs-fw-update` (with `-r`) | Run again with `--debug` to see the library error. |
| `Failed to read firmware file` / `Error reading firmware file` | `rs-fw-update` | Check the path given with `-f`. |
| `This firmware version is not compatible with <device name>` | `rs-fw-update` | Use an image made for this camera. |
| `The firmware version is not compatible with <device name>` | Viewer ([`fw-update-helper.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/fw-update-helper.cpp)) | Use an image made for this camera. |
| `Recovery device did not connect in time!` | Viewer | The device did not show up as an update device within 60 seconds. Reconnect it and try again. |
| `Original device did not reconnect in time!` | Viewer | The camera did not return within 60 seconds after the write. Reconnect it; if it comes up in recovery mode, update it again. |
| `Unknown error during update.` / `Please reconnect the camera to exit recovery mode` | Viewer | Reconnect the camera and retry. |
| `Device: <serial> failed to update firmware` / `Image is unsupported for this device or corrupted` | librealsense ([`fw-update-device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/fw-update/fw-update-device.cpp)) | Use an image made for this camera; download it again if it may be damaged. |
| `Device: <serial> failed to download firmware` / `Please verify that no other librealsense application is running` | librealsense | Close other applications that use the camera and retry. |
| `Device: <serial> is locked for update.` / `Use firmware version higher than: <version>` | librealsense | Use a firmware version above the one shown. |
| `Permission Denied!` / `This is often an indication of outdated or missing udev-rules.` | librealsense (Linux) | Install the udev rules: `sudo apt-get install librealsense2-dkms` for the Debian package, or `./scripts/setup_udev_rules.sh` when building from source. |
| `Firmware manifest failed with an error` / `Firmware manifest failed. Timeout waiting for device reset.` | librealsense | The device did not finish installing the image. Reconnect it and check whether it is in recovery mode. |
| `Firmware Update failed - wrong path or permissions missing` | librealsense (GMSL devices) | The DFU character device cannot be opened; check the driver and the permissions on the device node. |

For other camera problems see [[Troubleshooting Q&A]].

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`0d4fcab`](https://github.com/realsenseai/librealsense/commit/0d4fcabf634650b821200ad38eedaa13aa4cb704)._
<!-- /wikigen:footer -->
