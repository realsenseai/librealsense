librealsense can reach RealSense cameras over Ethernet. Network cameras publish themselves and their streams as DDS (Data Distribution Service) topics, and librealsense, when built with DDS support, discovers them and shows them as ordinary devices next to USB cameras. This page is for users and developers who connect Ethernet cameras such as the D555: it covers building with DDS, turning on DDS discovery, the DDS command-line tools, configuring the camera's network settings and setting the packet size (MTU) on the host.

## Build librealsense with DDS

DDS support is off by default. It is controlled by the CMake option `BUILD_WITH_DDS` in [`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake):

```
option(BUILD_WITH_DDS "Access camera devices through DDS topics (requires CMake 3.16.3)" OFF)
```

Turn it on when you configure the build:

```sh
cmake .. -DBUILD_WITH_DDS=ON
```

With `BUILD_WITH_DDS=ON`, the top-level [`CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/CMakeLists.txt) pulls in [`CMake/external_fastdds.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/external_fastdds.cmake). That file needs CMake 3.16.3 or newer, fetches eProsima Fast DDS (tag `v2.10.4-realsense`) and defines `BUILD_WITH_DDS` for the compiler. The library is then linked with the `realdds` library. The DDS tools in [`tools/dds`](https://github.com/realsenseai/librealsense/tree/development/tools/dds) are built only when both `BUILD_TOOLS` and `BUILD_WITH_DDS` are on (see [`tools/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/tools/CMakeLists.txt)).

Other build options are described in [[Build Configuration]].

## Enable DDS discovery in the context

A DDS-enabled build still does not look for network cameras until DDS is enabled in the context settings. When `BUILD_WITH_DDS` is defined, [`src/context.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/context.cpp) adds a DDS device factory ([`src/dds/rsdds-device-factory.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/dds/rsdds-device-factory.cpp)) to every context. The factory reads the `dds` object of the context settings:

| Setting | Default | Meaning |
|---|---|---|
| `dds.enabled` | `false` | Start DDS discovery for this context |
| `dds.domain` | `0` | DDS domain ID to join |
| `dds.participant` | executable name | Name of the DDS participant |
| `dds.device-initialization-timeout-ms` | `5000` | Time a discovered device has to become ready |
| `dds.query-devices-max` | `5` | Most seconds the first device query on a domain waits for devices to appear (0 = no wait; raised to `query-devices-min` if smaller) |
| `dds.query-devices-min` | `3` | Least seconds that first device query waits (unless `query-devices-max` is 0) |

Contexts that use the same domain share one DDS participant and one device watcher. Two contexts on different domains each get their own.

### From code

Pass the settings as JSON when you create the context:

```cpp
#include <librealsense2/rs.hpp>
#include <iostream>

rs2::context ctx( R"({ "dds": { "enabled": true, "domain": 0 } })" );
for( auto && dev : ctx.query_devices() )
    std::cout << dev.get_info( RS2_CAMERA_INFO_NAME ) << std::endl;
```

Setting `"device-mask"` to include `RS2_PRODUCT_LINE_SW_ONLY` stops the context from looking for USB/MIPI devices, so it lists only DDS devices.

### From the configuration file

Every context first loads the `context` section of the configuration file `realsense-config.json` (`RS2_CONFIG_FILENAME` in [`rs.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/rs.h)) and then applies the settings passed in code on top. The file lives in the user's application-data folder: `%APPDATA%\realsense-config.json` on Windows and `~/.realsense-config.json` on Linux and macOS. To turn on DDS for all applications:

```json
{
  "context": {
    "dds": {
      "enabled": true,
      "domain": 0
    }
  }
}
```

Passing `"inherit": false` in the context settings makes that context ignore the file. `rs-dds-config` writes `context.dds.enabled` and `context.dds.domain` into this file for you (see [below](#rs-dds-config)).

### From the command line of tools and examples

Tools and examples that use the shared `rs2::cli` parser in [`common/cli.h`](https://github.com/realsenseai/librealsense/blob/development/common/cli.h) accept these flags. Examples are `realsense-viewer` and `rs-eth-config`. Without a DDS build the flags are accepted but do nothing.

| Flag | Effect on the context settings |
|---|---|
| `--eth` | `dds.enabled = true` |
| `--eth-only` | `dds.enabled = true`, and `device-mask` set so that only DDS devices are listed |
| `--no-eth` | `dds.enabled = false` |
| `--domain-id <0-232>` | `dds.domain`; values outside 0–232 are rejected |

`--eth`, `--eth-only` and `--no-eth` are mutually exclusive, and `--domain-id` cannot be combined with `--no-eth`.

## DDS tools

The three tools in [`tools/dds`](https://github.com/realsenseai/librealsense/tree/development/tools/dds) are built with `BUILD_WITH_DDS=ON`. All of them also accept `-h`/`--help` and `--version`. The `--debug` flag of `rs-dds-config` and `rs-dds-adapter` exists when `BUILD_EASYLOGGINGPP` is on (the default). See [[Command Line Tools]] for the other console tools.

### rs-dds-config

[`rs-dds-config`](https://github.com/realsenseai/librealsense/blob/development/tools/dds/dds-config/rs-dds-config.cpp) reads and changes the Ethernet configuration of a camera, and sets the SDK's default DDS settings in the configuration file. It uses the parser without the `--eth`/`--domain-id` context flags. Each run:

1. Writes `context.dds.enabled = true` into the configuration file if it is not set yet. Skipped with `--transient-sdk-domain-id`.
2. Finds the one connected camera that supports Ethernet configuration. If more than one camera is found, it stops and asks for `--serial-number`. Recovery-mode devices are refused.
3. Unless `--quiet` is given, prints the current settings (MAC address, IP, DDS domain, link speed, MTU, link timeout, priority, DHCP, DHCP timeout, transmission delay, UDP TTL), with the requested values next to them.
4. If anything changed, sends the new configuration to the camera, prints `Successfully changed` and resets the camera, unless `--no-reset` is given.

| Flag | Value | Description |
|---|---|---|
| `--debug` | | Turn on librealsense debug logs |
| `--quiet` | | Suppress regular informational (`-I-`) messages |
| `--serial-number <S/N>` | string | Device to use when more than one is available |
| `--reset` | | Hardware reset the camera without making any changes |
| `--disable` | | Remove `context.dds.enabled` from the configuration file, so DDS is off by default, then exit |
| `--golden` | | Show the read-only factory ("golden") values next to the current ones; makes no changes |
| `--factory-reset` | | Write the golden values back to the camera |
| `--usb-first` | | Prefer USB; fall back to Ethernet after the link timeout |
| `--eth-first` | | Prefer Ethernet; fall back to USB after the link timeout |
| `--dynamic-priority` | | Prefer whichever connection worked last |
| `--link-timeout <milliseconds>` | 2000–30000, multiple of 100 | Time before the preferred link falls back to the other one |
| `--dhcp <on/off>` | `on` or `off` | Get the IP address from DHCP |
| `--dhcp-timeout <seconds>` | | Seconds before DHCP gives up and the static IP is used |
| `--mtu <bytes>` | 500–9000, multiple of 500 | Ethernet packet size the camera sends |
| `--transmission-delay <microseconds>` | 0–144, multiple of 3 | Wait after each packet before sending the next one |
| `--ttl <1-255>` | 0–255 (0 = system default); configuration table v3/v4: 0 only | UDP packet TTL field |
| `--ip <address>` | IPv4 address | Static IP address, used when DHCP is off |
| `--mask <1.2.3.4>` | IPv4 address | Static network mask, used when DHCP is off |
| `--gateway <1.2.3.4>` | IPv4 address | Gateway, used when DHCP is off |
| `--domain-id <0-232>` | 0–232 | DDS domain the camera publishes on |
| `--sdk-domain-id <0-232>` | 0–232 | DDS domain the SDK uses; saved to the configuration file as `context.dds.domain` |
| `--transient-sdk-domain-id <0-232>` | 0–232 | DDS domain the SDK uses for this run only; not saved |
| `--no-reset` | | Do not reset the camera after changes |

Rules enforced by the tool:

- `--golden`, `--factory-reset` and `--reset` are mutually exclusive. None of them can be combined with `--ip`, `--mask`, `--dhcp`, `--dhcp-timeout`, `--link-timeout` or a priority flag.
- `--usb-first`, `--eth-first` and `--dynamic-priority` are mutually exclusive.
- `--sdk-domain-id` and `--transient-sdk-domain-id` are mutually exclusive.
- With `--transient-sdk-domain-id`, the configuration file is not changed to enable DDS. Use it to reach a camera on a domain other than the saved one without saving that domain. With `--disable` as well, DDS is still disabled in the file, but the run goes on and works on the camera.
- The values above are checked on the host before the new configuration is sent. If the camera reports configuration table v3, only MTU 9000 and a transmission delay of 0 are allowed; with table v3 or v4, the UDP TTL must be 0. The tool then reports "Camera FW supports only MTU 9000.", "Camera FW does not support transmission delay." or "Camera FW does not support changing UDP TTL value." (messages from [`third-party/rsutils/src/eth-config.cpp`](https://github.com/realsenseai/librealsense/blob/development/third-party/rsutils/src/eth-config.cpp)).

Examples:

```sh
rs-dds-config                                   # show the current settings (and enable DDS by default)
rs-dds-config --dhcp off --ip 192.168.11.55 --mask 255.255.255.0
rs-dds-config --mtu 9000 --eth-first --link-timeout 8000
rs-dds-config --domain-id 5 --sdk-domain-id 5   # move camera and SDK to domain 5
rs-dds-config --golden                          # compare current with factory values
```

### rs-dds-sniffer

[`rs-dds-sniffer`](https://github.com/realsenseai/librealsense/blob/development/tools/dds/dds-sniffer/rs-dds-sniffer.cpp) watches a DDS domain. It prints every participant and data reader/writer it discovers, with the topic name and type. Without `--snapshot` (also implied by `--participants` and `--topics`) it runs until stopped, first printing `rs-dds-sniffer listening on domain <N> (press Ctrl+C to stop)` (unless `--machine-readable` is given).

| Flag | Description |
|---|---|
| `-s`, `--snapshot` | Run for a short time and print a snapshot of the domain |
| `-m`, `--machine-readable` | Print entities in a comma-separated form for automatic parsing |
| `-t`, `--topic-samples` | Register to topics that send TypeObject and print their samples (not in snapshot mode) |
| `--debug` | Enable debug logging |
| `--participants` | Show participants and quit; implies `--snapshot` |
| `--topics` | Show topics and quit; implies `--snapshot` |
| `-d`, `--domain <0-232>` | Domain ID to listen on (default: the configuration file's domain, else 0) |
| `-r`, `--root <path>` | Filter out anything not inside this root path |

### rs-dds-adapter

[`rs-dds-adapter`](https://github.com/realsenseai/librealsense/blob/development/tools/dds/dds-adapter/rs-dds-adapter.cpp) does the opposite job. It runs on the machine the cameras are plugged into (USB) and publishes each one on DDS, so librealsense on another machine can find them as network devices. It opens its own context with DDS turned off, so it serves local devices only. For its own DDS participant (named `rs-dds-adapter`) it uses the `dds` section of the configuration file, ignoring `enabled` there. It also publishes the ROS 2 discovery information, so each camera appears as a ROS 2 node.

| Flag | Description |
|---|---|
| `--domain-id <0-232>` | Domain ID to publish on; values outside 0–232 are rejected. Without it, the domain comes from the configuration file (default 0) |
| `--debug` | Turn on librealsense debug logs |

On start it prints `Starting RS DDS Adapter on domain <N> ...` (`<N>` is -1 when `--domain-id` is not given) and then `Start listening to RS devices..`.

## Configure the camera's Ethernet settings

In the code at this commit, the `RS2_EXTENSION_ETH_CONFIG` extension is exposed by a D555 connected directly over USB or GMSL/MIPI (`rs555_device` in [`src/ds/d500/d500-factory.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-factory.cpp)) and by every DDS device ([`src/dds/rs-dds-device-proxy.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/dds/rs-dds-device-proxy.cpp)). A DDS device reports support only if its name contains `D555`. Even then, `rs2_supports_eth_config` returns true only if the configuration can be read from the camera. See [[Supported Devices]] for the device list.

You have three ways to change the settings:

- `rs-dds-config` from the command line (above).
- The RealSense Viewer: the device menu shows **DDS Configuration** for cameras that support it (disabled while streaming). It covers the domain ID, connection priority and link timeout, DHCP and static IP, MTU, transmission delay and UDP TTL. It also has **Factory Reset** and **No Reset after changes**. See [[Realsense Viewer Manual]].
- The `rs_eth_config` API in your own code.

### The eth-config example

[`examples/eth-config`](https://github.com/realsenseai/librealsense/tree/development/examples/eth-config) (`rs-eth-config`, built with the other examples) uses the C++ wrapper `rs2::eth_config_device` from [`rs_eth_config.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_eth_config.hpp). It finds the first device that supports Ethernet configuration and prints its link speed, priority, timeout, IP, netmask, gateway, DHCP, MTU and transmission delay. It then changes the link priority and DHCP timeout, and finally restores them. It does not change IP settings, which could disconnect a network device. Because it uses `rs2::cli`, run it with `--eth` to find cameras over the network.

A minimal read-out, adapted from the example:

```cpp
#include <librealsense2/rs.hpp>
#include <iostream>

rs2::context ctx( R"({ "dds": { "enabled": true } })" );
for( auto && dev : ctx.query_devices() )
{
    rs2::eth_config_device eth( dev );   // empty if the device has no Ethernet configuration
    if( ! eth.supports_eth_config() )
        continue;
    std::cout << "link: " << eth.get_link_speed() << " Mbps, MTU " << eth.get_mtu()
              << ", priority " << eth.get_link_priority() << std::endl;
    rs2_ip_address configured, actual;
    eth.get_ip_address( configured, actual );   // actual can differ when DHCP is on
}
```

Each setter sends the full configuration to the camera, and the library checks the values first. The domain must be 0–232. The MTU must be 500–9000 in steps of 500. The link timeout must be 2000–30000 ms in steps of 100. The transmission delay must be 0–144 µs in steps of 3. The DHCP timeout must be 0–255 s. The configured IP address and network mask must not be 0.0.0.0. If the camera reports configuration table v3, only MTU 9000 and a transmission delay of 0 are allowed; with table v3 or v4, the UDP TTL must be 0.

### C API reference

The C functions in [`rs_eth_config.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_eth_config.h), in header order. Every function except `rs2_eth_link_priority_to_string` also takes a final `rs2_error ** error` argument. `rs2_ip_address` is `uint8_t[4]`. The C++ class `rs2::eth_config_device` has a method of the same name without the `rs2_` prefix for each (for example `get_mtu()`). The exceptions are `rs2_restore_default_eth_config`, which is `restore_defaults()`, and `rs2_eth_link_priority_to_string`, which is used through `operator<<`.

<!-- wikigen:table-eth-config-api -->
| Function | Parameters | Returns / description |
|---|---|---|
| `rs2_eth_link_priority_to_string` | `rs2_eth_link_priority priority` | Name of the priority value |
| `rs2_supports_eth_config` | `const rs2_device * device` | Non-zero if the device supports Ethernet configuration |
| `rs2_get_link_speed` | `const rs2_device * device` | Current Ethernet link speed in Mbps; 0 if no link |
| `rs2_get_link_priority` | `const rs2_device * device` | Current `rs2_eth_link_priority` |
| `rs2_set_link_priority` | `device`, `rs2_eth_link_priority priority` | Set the link priority |
| `rs2_get_link_timeout` | `const rs2_device * device` | Time in ms before falling back to the other connection type |
| `rs2_set_link_timeout` | `device`, `unsigned int timeout` | Set the link timeout in ms |
| `rs2_get_dds_domain` | `const rs2_device * device` | DDS domain ID (0–232) the camera uses over Ethernet |
| `rs2_set_dds_domain` | `device`, `unsigned int domain` | Set the DDS domain ID (0–232) |
| `rs2_get_ip_address` | `device`, `rs2_ip_address configured_ip`, `rs2_ip_address actual_ip` | Configured and actual IP address (they differ when DHCP is on) |
| `rs2_set_ip_address` | `device`, `const rs2_ip_address ip` | Set the static IP address |
| `rs2_get_netmask` | `device`, `rs2_ip_address configured_netmask`, `rs2_ip_address actual_netmask` | Configured and actual network mask |
| `rs2_set_netmask` | `device`, `const rs2_ip_address netmask` | Set the network mask |
| `rs2_get_gateway` | `device`, `rs2_ip_address configured_gateway`, `rs2_ip_address actual_gateway` | Configured and actual gateway |
| `rs2_set_gateway` | `device`, `const rs2_ip_address gateway` | Set the gateway |
| `rs2_get_dhcp_config` | `device`, `int * enabled`, `unsigned int * timeout` | DHCP on/off and DHCP timeout in seconds (0–255) |
| `rs2_set_dhcp_config` | `device`, `int enabled`, `unsigned int timeout` | Set DHCP on/off and timeout in seconds (0–255) |
| `rs2_get_mtu` | `const rs2_device * device` | MTU in bytes |
| `rs2_set_mtu` | `device`, `unsigned int mtu` | Set the MTU (500–9000, in 500-byte steps) |
| `rs2_get_transmission_delay` | `const rs2_device * device` | Transmission delay in microseconds |
| `rs2_set_transmission_delay` | `device`, `unsigned int delay` | Set the transmission delay (0–144 µs, in 3 µs steps) |
| `rs2_get_udp_ttl` | `const rs2_device * device` | UDP packet TTL |
| `rs2_set_udp_ttl` | `device`, `unsigned int ttl` | Set the UDP TTL (1–255, or 0 for the system default; with configuration table v3 or v4 only 0 is allowed; values above 255 are truncated to 8 bits, not rejected) |
| `rs2_restore_default_eth_config` | `const rs2_device * device` | Write the camera's factory (golden) configuration back as the current one |
<!-- /wikigen:table-eth-config-api -->

`rs2_eth_link_priority` values: `RS2_LINK_PRIORITY_USB_ONLY` (0), `RS2_LINK_PRIORITY_ETH_ONLY` (1), `RS2_LINK_PRIORITY_ETH_FIRST` (2), `RS2_LINK_PRIORITY_USB_FIRST` (3), and the dynamic variants `RS2_LINK_PRIORITY_DYNAMIC_ETH_FIRST` and `RS2_LINK_PRIORITY_DYNAMIC_USB_FIRST` (the base value with the `RS2_LINK_PRIORITY_DYNAMIC_BIT` flag, `0x10`).

## Set the MTU on the host

The camera's MTU (`--mtu` in `rs-dds-config`, `rs2_set_mtu` in the API) sets the size of the Ethernet packets it sends. The repository guide [`doc/stepbystep/setting_mtu_on_host.md`](https://github.com/realsenseai/librealsense/blob/development/doc/stepbystep/setting_mtu_on_host.md) recommends setting the host network adapter to the same packet size. Large ("jumbo") packets can lower transmission time. In multi-camera setups, though, they can overload switches and cause dropped frames. The guide recommends 9000-byte packets, the D555 factory setting.

- **Windows:** Start menu → **View network connections** → double-click the adapter connected to the camera → **Properties** → **Configure** → **Advanced** tab → set **Jumbo Packet**.
- **Linux:** find the adapter with `ifconfig`, then set its MTU, for example `sudo ifconfig eno1 mtu 9000` (replace `eno1` with your adapter and `9000` with your packet size).

![Linux adapter MTU](https://raw.githubusercontent.com/realsenseai/librealsense/development/doc/stepbystep/resources/linux_adapter.png)

## Related pages

- [[Build Configuration]]: CMake options, including `BUILD_WITH_DDS`
- [[Command Line Tools]]: the other console tools
- [[Realsense Viewer Manual]]: the Viewer's **DDS Configuration** window
- [[Troubleshooting Q&A]]

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`e12faf3`](https://github.com/realsenseai/librealsense/commit/e12faf3cf8e19688f1ad2c832037c8a06f319429)._
<!-- /wikigen:footer -->
