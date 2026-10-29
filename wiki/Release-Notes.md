<!-- wikigen:upcoming -->
## Upcoming release (draft)

### Highlights

- **Passive depth** - D585 2C passive depth mode (`RS2_OPTION_PASSIVE_DEPTH_MODE`: disabled, alternating or full passive), on USB and GMSL.
- **Dual RGB** - RGB-specific auto-exposure, exposure and gain options on D585 2C, dual-color AE mode and rectification on GMSL, and D401 dual RGB over USB.
- **Depth mapping on D5x5** - get and set the occupancy grid and labeled point cloud parameters as JSON, standalone 2C/3C occupancy on D585 GMSL, occupancy grid origin metadata, and a device point cloud view in the Viewer.
- **JPEG on D500** - JPEG support for D500 devices, with a new MJPEG decoder processing block.
- **IMU** - accelerometer sensitivity option (`RS2_OPTION_ACCEL_SENSITIVITY`) and gyro in fixed physical units on D500 GMSL; D555 DDS IMU axes aligned with USB.
- **Web viewer and REST API** - theme rework and IMU history graphs, real SDK stream profiles, `--host` and `--port` server arguments, and the backend no longer needs OpenCV.
- **Jetson** - support for JetPack 6.2.3 (L4T 36.5.2).

### API

See [[API Changes]].

### Improvements and fixes

- Dual-RGB D500: stream depth and color at different resolutions; both color streams share one resolution, and all streams share one frame rate ([#15878](https://github.com/realsenseai/librealsense/pull/15878))
- Limit improved close range on unsupported profiles ([#15875](https://github.com/realsenseai/librealsense/pull/15875))
- Fix advanced-mode JSON float parsing under comma-decimal locales ([#15865](https://github.com/realsenseai/librealsense/pull/15865))
- Fix the cached depth scale after advanced table writes ([#15879](https://github.com/realsenseai/librealsense/pull/15879))
- D5x5: add `depth_mapping_params` to the depth mapping example JSON ([#15877](https://github.com/realsenseai/librealsense/pull/15877))
- Viewer: Detection Distance as Off / Host / On-Camera ([#15857](https://github.com/realsenseai/librealsense/pull/15857))
- Support for Jetson JetPack 6.2.3 (L4T 36.5.2) ([#15873](https://github.com/realsenseai/librealsense/pull/15873))
- Skip GMSL depth mapping segmentation options the driver does not expose ([#15870](https://github.com/realsenseai/librealsense/pull/15870))
- D585 GMSL: support standalone 2C/3C occupancy ([#15780](https://github.com/realsenseai/librealsense/pull/15780))
- Fix slow sensor close: the dispatcher now wakes sleeping pollers on stop ([#15853](https://github.com/realsenseai/librealsense/pull/15853))
- Security: fix critical and high vulnerabilities found by an automated scan ([#15855](https://github.com/realsenseai/librealsense/pull/15855), [#15745](https://github.com/realsenseai/librealsense/pull/15745), [#15715](https://github.com/realsenseai/librealsense/pull/15715))
- Linux V4L2: stop video before metadata on UVC devices ([#15776](https://github.com/realsenseai/librealsense/pull/15776))
- Start the MIPI video/metadata syncer before the capture thread ([#15866](https://github.com/realsenseai/librealsense/pull/15866))
- D401 dual RGB over USB ([#15835](https://github.com/realsenseai/librealsense/pull/15835))
- Windows: keep Media Foundation media types on a pin with no native USB format ([#15859](https://github.com/realsenseai/librealsense/pull/15859))
- Fix USB video/metadata pairing when V4L2 nodes are interleaved ([#15819](https://github.com/realsenseai/librealsense/pull/15819))
- Viewer: the left control pane can be resized by dragging its edge ([#15851](https://github.com/realsenseai/librealsense/pull/15851))
- Expose the D585 2C passive depth mode on GMSL ([#15856](https://github.com/realsenseai/librealsense/pull/15856))
- Fix aligned depth intrinsics on dual-RGB D500 cameras ([#15852](https://github.com/realsenseai/librealsense/pull/15852))
- Block the dual-RGB mode switch on D535F and D585F ([#15849](https://github.com/realsenseai/librealsense/pull/15849))
- Stop installing the Viewer sample preset into the user's home folder ([#15825](https://github.com/realsenseai/librealsense/pull/15825))
- Fix a pyrealsense2 crash on import in Linux shared builds ([#15848](https://github.com/realsenseai/librealsense/pull/15848))
- D585 2C USB: calculate active and passive FPS separately in alternating passive depth ([#15842](https://github.com/realsenseai/librealsense/pull/15842))
- D5x5: get and set depth mapping params ([#15782](https://github.com/realsenseai/librealsense/pull/15782))
- D500 USB: derive Y8 IR from Y8I, drop GREY/UYVY IR passthrough ([#15847](https://github.com/realsenseai/librealsense/pull/15847))
- Allow embedded DPP filters to run with object detection ([#15839](https://github.com/realsenseai/librealsense/pull/15839))
- Enable firmware error polling on D400 GMSL ([#15823](https://github.com/realsenseai/librealsense/pull/15823))
- D585 GMSL: enable the dual-RGB rectification option ([#15838](https://github.com/realsenseai/librealsense/pull/15838))
- D500 GMSL: get and set the whole DPP struct, keep static ranges ([#15832](https://github.com/realsenseai/librealsense/pull/15832))
- Viewer: don't draw object detections on Color 2 ([#15833](https://github.com/realsenseai/librealsense/pull/15833))
- Keep frame callbacks alive while they are being invoked ([#15809](https://github.com/realsenseai/librealsense/pull/15809))
- D5xx device point cloud and an improved Viewer on the depth mapping sensor ([#15775](https://github.com/realsenseai/librealsense/pull/15775))
- Give each multiplexed color stream its own frame pool ([#15820](https://github.com/realsenseai/librealsense/pull/15820))
- MIPI firmware update with several cameras: match the update device to its camera ([#15703](https://github.com/realsenseai/librealsense/pull/15703))
- Add JPEG support to D500 ([#15796](https://github.com/realsenseai/librealsense/pull/15796))
- REST API: add `--host` and `--port` to the server ([#15806](https://github.com/realsenseai/librealsense/pull/15806))
- rs-fw-update: fix a crash on exit when a device event arrives after the update ([#15810](https://github.com/realsenseai/librealsense/pull/15810))
- Viewer: fix asynchronous object detection overlays ([#15800](https://github.com/realsenseai/librealsense/pull/15800))
- Linux: share one EasyLogging++ storage to fix a double destroy at exit ([#15799](https://github.com/realsenseai/librealsense/pull/15799))
- Fix D436 gyro sensitivity on macOS ([#15802](https://github.com/realsenseai/librealsense/pull/15802))
- Hide Master from the D585 GMSL inter-camera sync mode ([#15803](https://github.com/realsenseai/librealsense/pull/15803))
- REST API and web viewer: use real SDK stream profiles ([#15766](https://github.com/realsenseai/librealsense/pull/15766))
- D585 GMSL: fix RGB8 1280x960@60 frame drops with CUDA zero-copy ([#15797](https://github.com/realsenseai/librealsense/pull/15797))
- MIPI firmware update: one write path for operational and recovery devices ([#15674](https://github.com/realsenseai/librealsense/pull/15674))
- D500 GMSL: report gyro in fixed physical units ([#15794](https://github.com/realsenseai/librealsense/pull/15794))
- Enable the aligned depth option on D585 GMSL ([#15787](https://github.com/realsenseai/librealsense/pull/15787))
- Web viewer desktop (Tauri) app: connect to the bundled local backend, so the connection badge no longer always shows Disconnected ([#15767](https://github.com/realsenseai/librealsense/pull/15767))
- Allow Emitter Enabled and Laser Power while HDR is active ([#15792](https://github.com/realsenseai/librealsense/pull/15792))
- D555 DDS: align IMU axes with USB ([#15781](https://github.com/realsenseai/librealsense/pull/15781))
- Linux V4L2 backend: fix an unlock fd leak, the metadata fd sentinel and a metadata size wrap ([#15764](https://github.com/realsenseai/librealsense/pull/15764))
- D500 GMSL: set the minimum firmware for the int32 IMU path ([#15778](https://github.com/realsenseai/librealsense/pull/15778))
- Multiplex perception streams on GMSL ([#15759](https://github.com/realsenseai/librealsense/pull/15759))
- D500 GMSL: expose the accelerometer sensitivity option ([#15748](https://github.com/realsenseai/librealsense/pull/15748))
- Dual-color auto-exposure mode on GMSL ([#15772](https://github.com/realsenseai/librealsense/pull/15772))
- Viewer: BETA badge on perception streams ([#15762](https://github.com/realsenseai/librealsense/pull/15762))
- D585 GMSL: select the int32 physical-unit IMU path ([#15747](https://github.com/realsenseai/librealsense/pull/15747))
- Viewer: composite-option editors use the standard option look ([#15754](https://github.com/realsenseai/librealsense/pull/15754))
- D585 2C: add RGB-specific auto-exposure, exposure and gain options ([#15750](https://github.com/realsenseai/librealsense/pull/15750))
- Temporal filter resets on a geometry change ([#15763](https://github.com/realsenseai/librealsense/pull/15763))
- Viewer: one-time AI Assistant disclaimer ([#15749](https://github.com/realsenseai/librealsense/pull/15749))
- Pipeline: open D5xx mapping streams after depth ([#15755](https://github.com/realsenseai/librealsense/pull/15755))
- Fix on-chip calibration failing to restart streaming at an unsupported resolution ([#15758](https://github.com/realsenseai/librealsense/pull/15758))
- REST API: drop OpenCV from the backend ([#15753](https://github.com/realsenseai/librealsense/pull/15753))
- D500 tare calibration supports a custom preset ([#15751](https://github.com/realsenseai/librealsense/pull/15751))
- Viewer: fix slowness with passive depth ([#15742](https://github.com/realsenseai/librealsense/pull/15742))
- REST API: drop the pipeline start cache, reuse the device and stop synchronously ([#15743](https://github.com/realsenseai/librealsense/pull/15743))
- Web viewer: theme rework and IMU history graphs ([#15559](https://github.com/realsenseai/librealsense/pull/15559))
- Fix the libcurl build with the Ninja generator and system libidn2 ([#15632](https://github.com/realsenseai/librealsense/pull/15632))
- Viewer: fix a frame-pool error flood when drawing is slow ([#15738](https://github.com/realsenseai/librealsense/pull/15738))
- D585S: keep the hardware-monitor inter-camera sync mode ([#15740](https://github.com/realsenseai/librealsense/pull/15740))
- D585 2C GMSL: demultiplex Color 1 / Color 2 by metadata source index ([#15734](https://github.com/realsenseai/librealsense/pull/15734))
- Viewer: exclude the object detection stream from the frame drops dashboard ([#15741](https://github.com/realsenseai/librealsense/pull/15741))
- Dual color can stream color at 60 FPS in full passive mode ([#15736](https://github.com/realsenseai/librealsense/pull/15736))
- Enable AVX2 YUY2 to RGB/BGR unpacking ([#15730](https://github.com/realsenseai/librealsense/pull/15730))
- Clear the open profiles list when a DDS device goes offline ([#15735](https://github.com/realsenseai/librealsense/pull/15735))
- Reduce and cap the size of the usage statistics (RUM) report ([#15732](https://github.com/realsenseai/librealsense/pull/15732))
- Register extrinsics for motion frames, not only video ([#15728](https://github.com/realsenseai/librealsense/pull/15728))
- Fix the build with GCC 13 ([#15683](https://github.com/realsenseai/librealsense/pull/15683))
- Fix the build on Windows ARM and other non-x86 targets besides Linux, macOS and Android ([#15684](https://github.com/realsenseai/librealsense/pull/15684))
- REST API: speed up streaming on Debug SDK builds ([#15725](https://github.com/realsenseai/librealsense/pull/15725))
- Viewer: rendering no longer blocks IMU capture ([#15692](https://github.com/realsenseai/librealsense/pull/15692))
- DDS: skip the fixed frame-size check for compressed encodings ([#15718](https://github.com/realsenseai/librealsense/pull/15718))
- Viewer: fix the occupancy grid axes ([#15716](https://github.com/realsenseai/librealsense/pull/15716))
- D401 GMSL: fix an rs-capture crash by default-tagging only Color 0 ([#15710](https://github.com/realsenseai/librealsense/pull/15710))
- D500: enable close range, decimation and temporal composite options on GMSL ([#15687](https://github.com/realsenseai/librealsense/pull/15687))
- Viewer: tooltips on post-processing filter controls ([#15673](https://github.com/realsenseai/librealsense/pull/15673))
- Viewer: enforce IR/color pairing when embedded decimation is on ([#15704](https://github.com/realsenseai/librealsense/pull/15704))
- Passive depth ([#15702](https://github.com/realsenseai/librealsense/pull/15702))
- Expose D500 occupancy grid origin metadata ([#15691](https://github.com/realsenseai/librealsense/pull/15691))
- Restore mapping timing metadata and Viewer FPS ([#15667](https://github.com/realsenseai/librealsense/pull/15667))
- Protect the point cloud intrinsics query ([#15700](https://github.com/realsenseai/librealsense/pull/15700))
- Viewer: draw object detections on one stream only ([#15670](https://github.com/realsenseai/librealsense/pull/15670))
<!-- /wikigen:upcoming -->

## Release 2.59.1 beta

Release Date: Oct 11, 2026

### API Changes - [see here](https://github.com/realsenseai/librealsense/wiki/API-Changes#version-2591-beta)

## What's new

### Highlights

- **D401 GMSL dual-RGB raw color** — multiple raw formats, including SBGGR10P (pBAA / RAW10).
- **Aligned depth streams** — support for aligned depth, with matchers handling multiple depth streams.
- **Composite-option API** — generic API for multi-parameter controls (MinZ, Temporal Filter DPP, Decimation Filter DPP, improved close range control).
- **AI Assistant** — chat panel in the RealSense Viewer and in the web-based viewer.
- **Usage statistics (RUM)** — stats engine with privacy controls; cloud upload controlled by `rs2_rum_set_cloud_enabled`.
- **GPU and acceleration** — AMD ROCm / HIP support, zero-copy CUDA align, GPU-visible capture buffers (V4L2 / RSUSB), ARM64 NEON NV12-to-RGB8 conversion.
- **New presets and options** — Edge Enhancement visual preset for D435/D435i, thermal compensation on D457, `RS2_OPTION_EMITTER_MODE` over DDS, D500 BA10 calibration profiles with RAW16 preview.

### API

- Read all advanced-mode controls in one bulk operation; read/write advanced-mode control groups by name from Python.
- `get_supported_option_values` returns option ranges together with values.
- **Breaking:** `rs2_create_rotation_filter_block` takes a `rs2_stream` array and count; `rs2_streams_list` removed.
- `rs2_object_detection` gains world position and center-of-mass fields.

### C++ Viewer

- AI Assistant chat panel.
- Depth color-map ruler adapts to the visible range; ruler fixed for DDS devices.
- Control search now covers all control groups; rendering refactored.
- Embedded Decimation toggle locked while depth is streaming.
- Calibration Data shown on all D500 devices, not only D555.
- Detection overlay drawn on color streams only.
- Fix viewer freeze when enabling a busy sensor.
- Fix playback option restore and saved histogram equalization.
- Fix consent popup layering and viewer exit when a URL fails to open.
- Fix advanced-controls text-edit mode being lost every frame.

### Web based Viewer (react-viewer)

- AI Assistant chat widget.
- One-click firmware update.
- Control parity with the C++ viewer: enum dropdowns, depth visualization, RS400 advanced controls.
- Control search box with aliases.
- Collapsible stream modules; motion enabled by default.
- Fix Windows Tauri desktop-app build.
- REST API: event-driven WebRTC frame delivery, session-leak fix, firmware router restructure, Python 3.10 through 3.14 support.

### Platform & Build

- AMD ROCm / HIP support.
- macOS: RealSense Viewer enabled; USB capture hardened; external libusb CMake configuration fixed.
- Fix CUDA arch flags silently dropped on CMake < 3.18.
- Linux: order V4L2 nodes by USB streaming interface instead of `/dev/videoN`; wait for udev to grant access to re-created video nodes.
- Compressed db3 recordings are natively playable on ROS 2.
- Security: fix critical and high vulnerabilities from automated scan; bump `ws` to 8.21.0.

### Bug Fixes

- Fix global timestamp step and reversal at the hardware clock wrap.
- D401 GMSL: fix Depth + Color1 crash on packed RAW10 color; fix `rs-capture` crash with dual raw color streams.
- Report device removal promptly and hold back a device that is still enumerating.
- Fix recorder crash on frames whose stride is smaller than their format requires.
- Fix out-of-bounds read of HID samples in the RSUSB backend.
- Fix use-after-free crash on exit; stop device watcher before callback teardown.
- Fix high CPU usage when a USB device disconnects during streaming.
- Stop FW error polling when the device is disconnected, not only destroyed.
- Reject two streams that need the same hardware endpoint.
- D500 IMU: fix stale samples after reset, gyro physical-unit processing, and HW FPS 0 on GMSL.
- Fix 64-byte alignment on GMSL.
- Handle variable-size frames reported as compressed in V4L2; replace `get_xu_range` size assert with a logged exception.
- Guard MIPI color-node index and fisheye node list against out-of-range access.
- `rs-fw-update`: bound child processes with a timeout, fix reconnect deadlock, return recovery result.
- HDR: gate the pre-HDR exposure restore workaround on FW version.
- Gate dual-color AE, `DUAL_RGB_MODE` and `DETECTION_DISTANCE` options on minimum FW versions.
- `fw-logger` rejects unsupported devices.

### Known issues

- For non-D555 users: if dependency issues appear related to fastrtps and fastcdr, try building from source with the flag `-DBUILD_WITH_DDS=false`.

[Full Changelog in GitHub](https://github.com/realsenseai/librealsense/compare/v2.58.4...v2.59.1)

* [#15723](https://github.com/realsenseai/librealsense/pull/15723) by [@Nir-Az](https://github.com/Nir-Az) - **Cherry-pick to 2.59.1: Fix global timestamp step and reversal at the HW clock wrap ([#15712](https://github.com/realsenseai/librealsense/issues/15712))** 
* [#15722](https://github.com/realsenseai/librealsense/pull/15722) by [@Nir-Az](https://github.com/Nir-Az) - **Cherry-pick to 2.59.1: D401 GMSL fix Depth+Color1 core dump on packed pBAA (RAW10) color** 
* [#15719](https://github.com/realsenseai/librealsense/pull/15719) by [@Nir-Az](https://github.com/Nir-Az) - **Cherry-pick to 2.59.1: Fix advanced-controls text-edit mode lost since sections are rebuilt per frame** 
* [#15717](https://github.com/realsenseai/librealsense/pull/15717) by [@Nir-Az](https://github.com/Nir-Az) - **Cherry-pick to 2.59.1: D401 GMSL fix rs-capture crash by default-tagging only Color 0** 
* [#15701](https://github.com/realsenseai/librealsense/pull/15701) by [@Nir-Az](https://github.com/Nir-Az) - **Cherry-pick to 2.59.1: D401 GMSL dual-RGB SBGGR10P (pBAA) raw color format** 
* [#15693](https://github.com/realsenseai/librealsense/pull/15693) by [@Nir-Az](https://github.com/Nir-Az) - **Freeze 2.59.1: bump version** 
* [#15527](https://github.com/realsenseai/librealsense/pull/15527) by [@Binyaminp](https://github.com/Binyaminp) - **Add AI Assistant chat panel to RealSense Viewer** 
* [#15662](https://github.com/realsenseai/librealsense/pull/15662) by [@remibettan](https://github.com/remibettan) - **react-viewer: fix Windows Tauri desktop-app build** 
* [#15664](https://github.com/realsenseai/librealsense/pull/15664) by [@remibettan](https://github.com/remibettan) - **realsense-viewer: adapt depth color-map ruler to visible range (RSDEV-14227)** 
* [#15640](https://github.com/realsenseai/librealsense/pull/15640) by [@AviaAv](https://github.com/AviaAv) - **Make control search over all control groups and refactor viewer rendering** 
* [#15637](https://github.com/realsenseai/librealsense/pull/15637) by [@OhadMeir](https://github.com/OhadMeir) - **Add support for aligned depth streams** 
* [#15690](https://github.com/realsenseai/librealsense/pull/15690) by [@lemon6630](https://github.com/lemon6630) - **Add ARM64 NV12-to-RGB8 NEON conversion** 
* [#15514](https://github.com/realsenseai/librealsense/pull/15514) by [@Binyaminp](https://github.com/Binyaminp) - **react-viewer: add RealSense AI Assistant chat widget** 
* [#15665](https://github.com/realsenseai/librealsense/pull/15665) by [@Binyaminp](https://github.com/Binyaminp) - **Add Edge Enhancement visual preset for D435/D435I** 
* [#15679](https://github.com/realsenseai/librealsense/pull/15679) by [@remibettan](https://github.com/remibettan) - **viewer tests: catch stray exceptions in TestFunc instead of aborting the run** 
* [#15689](https://github.com/realsenseai/librealsense/pull/15689) by [@AviaAv](https://github.com/AviaAv) - **Fix consent popup layering and viewer exit on failed URL open** 
* [#15688](https://github.com/realsenseai/librealsense/pull/15688) by [@OhadMeir](https://github.com/OhadMeir) - **Support multiple formats for dual RGB raw** 
* [#15686](https://github.com/realsenseai/librealsense/pull/15686) by [@Nir-Az](https://github.com/Nir-Az) - **Fix dead Intel documentation links and viewer readme title** 
* [#15685](https://github.com/realsenseai/librealsense/pull/15685) by [@Nir-Az](https://github.com/Nir-Az) - **Keep occupancy a default profile only on D585S** 
* [#15650](https://github.com/realsenseai/librealsense/pull/15650) by [@OhadMeir](https://github.com/OhadMeir) - **Initial D585 2C GMSL support** 
* [#15638](https://github.com/realsenseai/librealsense/pull/15638) by [@AlonHirshberg](https://github.com/AlonHirshberg) - **[RSDSO-21316] FW-version-gated accel scale for the new 10 micro-g HID unit** 
* [#15682](https://github.com/realsenseai/librealsense/pull/15682) by [@Binyaminp](https://github.com/Binyaminp) - **Gate SW/FW update checking on CHECK_FOR_UPDATES independent of BUILD_WITH_LIBCURL** 
* [#15402](https://github.com/realsenseai/librealsense/pull/15402) by [@sareluzi](https://github.com/sareluzi) - **react-viewer: control parity — enum dropdowns, depth visualization, RS400 advanced controls** 
* [#15676](https://github.com/realsenseai/librealsense/pull/15676) by [@AviaAv](https://github.com/AviaAv) - **Enable RUM upload to server** 
* [#15681](https://github.com/realsenseai/librealsense/pull/15681) by [@Nir-Az](https://github.com/Nir-Az) - **Run the viewer GUI tests on D400 only until D5xx blockers are fixed** 
* [#15672](https://github.com/realsenseai/librealsense/pull/15672) by [@OhadMeir](https://github.com/OhadMeir) - **Order V4L2 nodes by USB streaming interface, not /dev/videoN** 
* [#15677](https://github.com/realsenseai/librealsense/pull/15677) by [@OhadMeir](https://github.com/OhadMeir) - **Dual color - enable min range embedded filter when color active** 
* [#15668](https://github.com/realsenseai/librealsense/pull/15668) by [@polejoe](https://github.com/polejoe) - **Support D500 BA10 calibration profiles and RAW16 preview** 
* [#15631](https://github.com/realsenseai/librealsense/pull/15631) by [@Nir-Az](https://github.com/Nir-Az) - **Report device removal promptly and hold back a device that is still enumerating** 
* [#15627](https://github.com/realsenseai/librealsense/pull/15627) by [@lemon6630](https://github.com/lemon6630) - **[RSDEV-14097] Support D585 GMSL gyro sensitivity** 
* [#15671](https://github.com/realsenseai/librealsense/pull/15671) by [@remibettan](https://github.com/remibettan) - **Lock embedded Decimation toggle in Viewer while depth streams** 
* [#15639](https://github.com/realsenseai/librealsense/pull/15639) by [@alexkunin-gh](https://github.com/alexkunin-gh) - **occupancygrid view grid with probability values** 
* [#15666](https://github.com/realsenseai/librealsense/pull/15666) by [@OhadMeir](https://github.com/OhadMeir) - **Viewer - fix colormap ruler for DDS** 
* [#15663](https://github.com/realsenseai/librealsense/pull/15663) by [@Nir-Az](https://github.com/Nir-Az) - **Pin the device in the D500 emitter streaming tests** 
* [#15659](https://github.com/realsenseai/librealsense/pull/15659) by [@lemon6630](https://github.com/lemon6630) - **Fix stale IMU samples after reset (RSDEV-14102)** 
* [#15657](https://github.com/realsenseai/librealsense/pull/15657) by [@remibettan](https://github.com/remibettan) - **D585 2C dual-color RGB controls (Windows + Linux)** 
* [#15660](https://github.com/realsenseai/librealsense/pull/15660) by [@AviaAv](https://github.com/AviaAv) - **Read and write advanced-mode control groups by name from python** 
* [#15661](https://github.com/realsenseai/librealsense/pull/15661) by [@remibettan](https://github.com/remibettan) - **viewer: show Calibration Data on all D500 devices, not only D555** 
* [#15649](https://github.com/realsenseai/librealsense/pull/15649) by [@AviaAv](https://github.com/AviaAv) - **Expose DDS emitter mode selector as RS2_OPTION_EMITTER_MODE** 
* [#15656](https://github.com/realsenseai/librealsense/pull/15656) by [@AviaAv](https://github.com/AviaAv) - **Reject two streams that need the same hardware endpoint** 
* [#15658](https://github.com/realsenseai/librealsense/pull/15658) by [@OhadMeir](https://github.com/OhadMeir) - **Fix 64 byte alignment on GMSL** 
* [#15652](https://github.com/realsenseai/librealsense/pull/15652) by [@OhadMeir](https://github.com/OhadMeir) - **Dual RGB block unsupported stream combinations** 
* [#15655](https://github.com/realsenseai/librealsense/pull/15655) by [@remibettan](https://github.com/remibettan) - **viewer: draw detection overlay on color streams only** 
* [#15575](https://github.com/realsenseai/librealsense/pull/15575) by [@remibettan](https://github.com/remibettan) - **Support D5xx GMSL DFU (recovery + operational) via /dev/d4xx-dfu chardev** 
* [#15645](https://github.com/realsenseai/librealsense/pull/15645) by [@Nir-Az](https://github.com/Nir-Az) - **Support thermal compensation control on D457** 
* [#15646](https://github.com/realsenseai/librealsense/pull/15646) by [@Nir-Az](https://github.com/Nir-Az) - **Fix recorder crash on frames whose stride is smaller than their format requires** 
* [#15583](https://github.com/realsenseai/librealsense/pull/15583) by [@jcelerier](https://github.com/jcelerier) - **Fix out-of-bounds read of HID samples in the RSUSB backend** 
* [#15644](https://github.com/realsenseai/librealsense/pull/15644) by [@Nir-Az](https://github.com/Nir-Az) - **bump version to 2.59** 
* [#15642](https://github.com/realsenseai/librealsense/pull/15642) by [@ev-mp](https://github.com/ev-mp) - **Add Decimation Filter DPP composite option (USB/HKR, D500)** 
* [#15643](https://github.com/realsenseai/librealsense/pull/15643) by [@Nir-Az](https://github.com/Nir-Az) - **Skip object detection in the extrinsics consistency test and cover all profiles** 
* [#15641](https://github.com/realsenseai/librealsense/pull/15641) by [@Nir-Az](https://github.com/Nir-Az) - **Raise the FW tool timeout above the real GMSL/MIPI flash time** 
* [#15591](https://github.com/realsenseai/librealsense/pull/15591) by [@ev-mp](https://github.com/ev-mp) - **Generic composite-option API for multi-param XU controls (MinZ/TempFilter)** 
* [#15634](https://github.com/realsenseai/librealsense/pull/15634) by [@Nir-Az](https://github.com/Nir-Az) - **Add Ubuntu 26.04 to issue template OS dropdowns** 
* [#15628](https://github.com/realsenseai/librealsense/pull/15628) by [@Nir-Az](https://github.com/Nir-Az) - **Bound FW tool child processes with a timeout and fix rs-fw-update reconnect deadlock** 
* [#15630](https://github.com/realsenseai/librealsense/pull/15630) by [@Nir-Az](https://github.com/Nir-Az) - **Stop FW error polling when the device is disconnected, not only destroyed** 
* [#15625](https://github.com/realsenseai/librealsense/pull/15625) by [@Nir-Az](https://github.com/Nir-Az) - **d500: add RS2_OPTION_DUAL_RGB_RECTIFICATION for D585 2C USB** 
* [#15635](https://github.com/realsenseai/librealsense/pull/15635) by [@AviaAv](https://github.com/AviaAv) - **Fix use-after-free crash on exit by skipping teardown flush** 
* [#15557](https://github.com/realsenseai/librealsense/pull/15557) by [@alexkunin-gh](https://github.com/alexkunin-gh) - **enabled OG stream for D585** 
* [#15629](https://github.com/realsenseai/librealsense/pull/15629) by [@OhadMeir](https://github.com/OhadMeir) - **Accept H264 as camera published format** 
* [#15249](https://github.com/realsenseai/librealsense/pull/15249) by [@AviaAv](https://github.com/AviaAv) - **Implement core RUM stats engine, API, uploader, and privacy features** 
* [#15626](https://github.com/realsenseai/librealsense/pull/15626) by [@AviaAv](https://github.com/AviaAv) - **Expose Person Detection on D585 2C over USB** 
* [#15622](https://github.com/realsenseai/librealsense/pull/15622) by [@AviaAv](https://github.com/AviaAv) - **Wait for udev to grant access to re-created video nodes** 
* [#14512](https://github.com/realsenseai/librealsense/pull/14512) by [@mbloechli](https://github.com/mbloechli) - **Added covariance calculation to rs-imu-calibration.py** 
* [#15624](https://github.com/realsenseai/librealsense/pull/15624) by [@Nir-Az](https://github.com/Nir-Az) - **Align High-Accuracy sloRauDiffThresh to the FW register step** 
* [#15621](https://github.com/realsenseai/librealsense/pull/15621) by [@OhadMeir](https://github.com/OhadMeir) - **Refactor matchers, also supporting multiple depth streams** 
* [#15615](https://github.com/realsenseai/librealsense/pull/15615) by [@AviaAv](https://github.com/AviaAv) - **Remove the dead react-viewer ControlsPanel component** 
* [#15556](https://github.com/realsenseai/librealsense/pull/15556) by [@AviaAv](https://github.com/AviaAv) - **Make compressed db3 recordings natively playable on ROS2** 
* [#15611](https://github.com/realsenseai/librealsense/pull/15611) by [@remibettan](https://github.com/remibettan) - **rs-fw-update: return recovery result and fix reconnect wait match** 
* [#15613](https://github.com/realsenseai/librealsense/pull/15613) by [@Nir-Az](https://github.com/Nir-Az) - **Merge master (2.58.4 release) back into development** 
* [#15612](https://github.com/realsenseai/librealsense/pull/15612) by [@Nir-Az](https://github.com/Nir-Az) - **Push 2.58.4 release to master** 
* [#15610](https://github.com/realsenseai/librealsense/pull/15610) by [@OhadMeir](https://github.com/OhadMeir) - **Gate dual color AE option on FW version** 
* [#15608](https://github.com/realsenseai/librealsense/pull/15608) by [@OhadMeir](https://github.com/OhadMeir) - **dispatcher: add a name and use it in logs** 
* [#15606](https://github.com/realsenseai/librealsense/pull/15606) by [@ashrafk93](https://github.com/ashrafk93) - **update README dual rgb** 
* [#15604](https://github.com/realsenseai/librealsense/pull/15604) by [@ashrafk93](https://github.com/ashrafk93) - **Dual rgb doc** 
* [#15602](https://github.com/realsenseai/librealsense/pull/15602) by [@OhadMeir](https://github.com/OhadMeir) - **Fix HDR test instability** 
* [#15600](https://github.com/realsenseai/librealsense/pull/15600) by [@ashrafk93](https://github.com/ashrafk93) - **Dual rgb fw guard** 
* [#15601](https://github.com/realsenseai/librealsense/pull/15601) by [@OhadMeir](https://github.com/OhadMeir) - **Fix pytest-emitter-d500 stability for D585** 
* [#15597](https://github.com/realsenseai/librealsense/pull/15597) by [@OhadMeir](https://github.com/OhadMeir) - **Support dual color AE mode** 
* [#15074](https://github.com/realsenseai/librealsense/pull/15074) by [@HPC-Ken](https://github.com/HPC-Ken) - **Adding support for ROCm devices.** 
* [#15598](https://github.com/realsenseai/librealsense/pull/15598) by [@AviaAv](https://github.com/AviaAv) - **Pass rotation filter streams across the C API as an array** 
* [#15599](https://github.com/realsenseai/librealsense/pull/15599) by [@ashrafk93](https://github.com/ashrafk93) - **v4l2: make USERPTR capture buffers GPU-visible for zero-copy** 
* [#15576](https://github.com/realsenseai/librealsense/pull/15576) by [@alexkunin-gh](https://github.com/alexkunin-gh) - **support for new person detection API** 
* [#15584](https://github.com/realsenseai/librealsense/pull/15584) by [@AviaAv](https://github.com/AviaAv) - **Remove pipeline use from react viewer and fix selected AI model** 
* [#15596](https://github.com/realsenseai/librealsense/pull/15596) by [@OhadMeir](https://github.com/OhadMeir) - **Improve pytest-emitter-d500 stability** 
* [#15580](https://github.com/realsenseai/librealsense/pull/15580) by [@ashrafk93](https://github.com/ashrafk93) - **rsusb make capture buffers GPU-visible so zero-copy frames stay zero-…** 
* [#15553](https://github.com/realsenseai/librealsense/pull/15553) by [@keskinonur](https://github.com/keskinonur) - **rs-motion: handle combined motion frames** 
* [#15592](https://github.com/realsenseai/librealsense/pull/15592) by [@Nir-Az](https://github.com/Nir-Az) - **Remove D585 bring-up settle workaround from pytest conftest** 
* [#15588](https://github.com/realsenseai/librealsense/pull/15588) by [@Nir-Az](https://github.com/Nir-Az) - **Fix two auto-exposure warmup issues in the basic-color image-quality test** 
* [#15586](https://github.com/realsenseai/librealsense/pull/15586) by [@ashrafk93](https://github.com/ashrafk93) - **dual rgb ISP mode** 
* [#15562](https://github.com/realsenseai/librealsense/pull/15562) by [@AviaAv](https://github.com/AviaAv) - **Remove dead react-viewer store, API and IMU panel code** 
* [#15525](https://github.com/realsenseai/librealsense/pull/15525) by [@remibettan](https://github.com/remibettan) - **d500 interactive TC: include D585S in the gate** 
* [#15573](https://github.com/realsenseai/librealsense/pull/15573) by [@remibettan](https://github.com/remibettan) - **d5x5: python example for interactive triggered calibration** 
* [#15569](https://github.com/realsenseai/librealsense/pull/15569) by [@AviaAv](https://github.com/AviaAv) - **Read all advanced-mode controls in one bulk operation** 
* [#15558](https://github.com/realsenseai/librealsense/pull/15558) by [@zhoubincalder](https://github.com/zhoubincalder) - **Zero-copy CUDA align, and fix two out-of-range thread guards** 
* [#15574](https://github.com/realsenseai/librealsense/pull/15574) by [@AviaAv](https://github.com/AviaAv) - **Re-enable 3D projection-from-recording test** 
* [#15391](https://github.com/realsenseai/librealsense/pull/15391) by [@sareluzi](https://github.com/sareluzi) - **Add one-click firmware update to the react-viewer** 
* [#15570](https://github.com/realsenseai/librealsense/pull/15570) by [@AviaAv](https://github.com/AviaAv) - **Fix playback option restore and saved histogram equalization** 
* [#15404](https://github.com/realsenseai/librealsense/pull/15404) by [@sareluzi](https://github.com/sareluzi) - **react-viewer: collapsible stream modules + motion enabled by default** 
* [#15572](https://github.com/realsenseai/librealsense/pull/15572) by [@AviaAv](https://github.com/AviaAv) - **Add FPS test for object detection unit tests** 
* [#15571](https://github.com/realsenseai/librealsense/pull/15571) by [@remibettan](https://github.com/remibettan) - **Map D500 external-sync XU selector (0x1A) on GMSL** 
* [#15564](https://github.com/realsenseai/librealsense/pull/15564) by [@AviaAv](https://github.com/AviaAv) - **Handle variable-size frames reported as compressed in v4l2** 
* [#15531](https://github.com/realsenseai/librealsense/pull/15531) by [@ghu3](https://github.com/ghu3) - **Validate object detection frame ABI and CRC** 
* [#15537](https://github.com/realsenseai/librealsense/pull/15537) by [@AviaAv](https://github.com/AviaAv) - **Run REST API and React Viewer tests on GitHub Actions and reorganize test tree** 
* [#15550](https://github.com/realsenseai/librealsense/pull/15550) by [@keskinonur](https://github.com/keskinonur) - **Harden macOS USB capture transitions** 
* [#15385](https://github.com/realsenseai/librealsense/pull/15385) by [@sareluzi](https://github.com/sareluzi) - **react-viewer: add control search box with aliases** 
* [#15506](https://github.com/realsenseai/librealsense/pull/15506) by [@zhoubincalder](https://github.com/zhoubincalder) - **Fix CUDA arch flags being silently dropped on CMake < 3.18** 
* [#15548](https://github.com/realsenseai/librealsense/pull/15548) by [@keskinonur](https://github.com/keskinonur) - **Enable RealSense Viewer on macOS** 
* [#15554](https://github.com/realsenseai/librealsense/pull/15554) by [@keskinonur](https://github.com/keskinonur) - **fw-logger: reject unsupported devices** 
* [#15541](https://github.com/realsenseai/librealsense/pull/15541) by [@Nir-Az](https://github.com/Nir-Az) - **Improve rest-api viewer streaming: event-driven WebRTC frames, skip Debug SDK builds, fix session leak** 
* [#15535](https://github.com/realsenseai/librealsense/pull/15535) by [@keskinonur](https://github.com/keskinonur) - **Fix macOS external libusb CMake configuration** 
* [#15539](https://github.com/realsenseai/librealsense/pull/15539) by [@keskinonur](https://github.com/keskinonur) - **macOS: hold USB capture for the device lifetime** 
* [#15523](https://github.com/realsenseai/librealsense/pull/15523) by [@AviaAv](https://github.com/AviaAv) - **Return option ranges together with values in get_supported_option_values** 
* [#15534](https://github.com/realsenseai/librealsense/pull/15534) by [@Nir-Az](https://github.com/Nir-Az) - **rest-api: support Python 3.10 through 3.14 in requirements pins** 
* [#15536](https://github.com/realsenseai/librealsense/pull/15536) by [@keskinonur](https://github.com/keskinonur) - **Stop device watcher before callback teardown** 
* [#15532](https://github.com/realsenseai/librealsense/pull/15532) by [@ashrafk93](https://github.com/ashrafk93) - **Zerocopy tests** 
* [#15517](https://github.com/realsenseai/librealsense/pull/15517) by [@AviaAv](https://github.com/AviaAv) - **Remove unreachable endpoints and restructure firmware router** 
* [#15533](https://github.com/realsenseai/librealsense/pull/15533) by [@ymodlin](https://github.com/ymodlin) - **hdr test: assert the queried exposure/gain only on FW that restores them** 
* [#15513](https://github.com/realsenseai/librealsense/pull/15513) by [@remibettan](https://github.com/remibettan) - **d500: Align D555/D585 USB Sync Mode enumeration to Internal/External** 
* [#15530](https://github.com/realsenseai/librealsense/pull/15530) by [@remibettan](https://github.com/remibettan) - **v4l2: replace get_xu_range size assert with logged exception** 
* [#15528](https://github.com/realsenseai/librealsense/pull/15528) by [@remibettan](https://github.com/remibettan) - **d500: gate DUAL_RGB_MODE and DETECTION_DISTANCE XU registrations on min FW versions** 
* [#15161](https://github.com/realsenseai/librealsense/pull/15161) by [@AviaAv](https://github.com/AviaAv) - **Fix viewer freeze when enabling a busy sensor** 
* [#15508](https://github.com/realsenseai/librealsense/pull/15508) by [@ymodlin](https://github.com/ymodlin) - **hdr: gate the pre-HDR exposure restore workaround on FW version (DSO-18682)** 
* [#15521](https://github.com/realsenseai/librealsense/pull/15521) by [@OhadMeir](https://github.com/OhadMeir) - **Fix IMU HW FPS 0 for D500 GMSL** 
* [#15526](https://github.com/realsenseai/librealsense/pull/15526) by [@AviaAv](https://github.com/AviaAv) - **Bump ws override to version 8.21.0 for security** 
* [#15511](https://github.com/realsenseai/librealsense/pull/15511) by [@Binyaminp](https://github.com/Binyaminp) - **Fix high CPU usage when USB device disconnects during streaming** 
* [#15524](https://github.com/realsenseai/librealsense/pull/15524) by [@kojokaro](https://github.com/kojokaro) - **Safety signal test: write only the zone polygons into presets 0/1** 
* [#15520](https://github.com/realsenseai/librealsense/pull/15520) by [@sysrsbuild-gh-agentic](https://github.com/sysrsbuild-gh-agentic) - **Security: Fix critical and high vulnerabilities (Aikido automated scan)** 
* [#15522](https://github.com/realsenseai/librealsense/pull/15522) by [@ymodlin](https://github.com/ymodlin) - **close-range: remove test_close_range_threshold (RSDEV-13516)** 
* [#15512](https://github.com/realsenseai/librealsense/pull/15512) by [@lemon6630](https://github.com/lemon6630) - **Fix D500 gyro physical-unit processing** 
* [#15491](https://github.com/realsenseai/librealsense/pull/15491) by [@Nir-Az](https://github.com/Nir-Az) - **Guard the MIPI color-node index and the fisheye node list against out-of-range access** 
* [#15459](https://github.com/realsenseai/librealsense/pull/15459) by [@Nir-Az](https://github.com/Nir-Az) - **Update readme.md to add Perception Studio ([#15458](https://github.com/realsenseai/librealsense/issues/15458))** 


## Release 2.58.4 beta

Release Date: Aug 30, 2026

### API Changes - [see here](https://github.com/realsenseai/librealsense/wiki/API-Changes#version-2584-beta)

## What's new

### Highlights

- **Ubuntu 26.04 (Resolute) support** — build fixes, ROS 2 Lyrical added to the build matrix, and CI running on 26.04.
- **D401 GMSL dual-RGB support** — RGB controls routed to the dedicated color v4l2 node.
- **Zero-copy GPU frames (CUDA)** — new `rs2_get_frame_gpu_data` / `rs2_get_frame_gpu_data_or_upload` and the `rs2::gpu_frame` extension let CUDA/TensorRT consume frames with no host->device copy on integrated-GPU (Jetson) platforms.
- **Device hardware time API** — new `rs2_get_device_time_ms` exposes the device ASIC clock.
- **Inference renamed to Perception** — SDK-wide rename of the inference APIs/extensions to perception; sensor is now "Perception" and the stream "Object Detection". New `RS2_OPTION_DETECTION_DISTANCE` enables firmware-side per-detection distance.
- **New sensor options** — `RS2_OPTION_READOUT_SHAPING` on select D400 devices, Emitter ON/OFF on all D500 active SKUs (except D585S), Emitter Always On for D585/D535, Gyro Sensitivity over USB on D500, D555 emitter ON/OFF over USB, projector temperature on all D535/D585 PIDs.


### C++ Viewer

- Add a search box to filter controls by name.
- Add viewport grid overlay.
- Split depth/IR resolution UI, gated on embedded decimation.

### Web based Viewer
- Add a search box to filter controls by name.

### Platform & Build

- Optionally use a system-provided `nlohmann_json` (`USE_EXTERNAL_NLOHMANN_JSON`).
- Fix building with pybind11 v3.0.2.
- udev: collapse per-PID 0x38E5 rules to VID-only matches.
**JetPack 7.2 (L4T R39.2)** support in `patch-realsense-ubuntu-L4T.sh`.

### Bug Fixes

- Fix global timestamps stepping/slewing/reversing and depth-color splits at the hardware clock wrap.
- Fix numerical instability in pose-inverse computation.
- Fix rotation filter crash on streams without intrinsics.
- Fix GIL deadlock when a pipeline with a Python callback is destroyed;
- Allow a DFU device to reset after update without waiting for a status query.
- Media Foundation: differentiate native from decompressed profiles.
- Expose Depth Auto Exposure Mode on all D400 SKUs with a global shutter.
- `sw-update`: match device names with or without the "Intel" prefix.

### Known issues

- For non-D555 users: if dependency issues appear related to fastrtps and fastcdr, try building from source with the flag `-DBUILD_WITH_DDS=false`.

[Full Changelog in GitHub](https://github.com/realsenseai/librealsense/compare/v2.58.3...v2.58.4)

* [#15607](https://github.com/realsenseai/librealsense/pull/15607) by [@Nir-Az](https://github.com/Nir-Az) - **Cherry-pick [#15606](https://github.com/realsenseai/librealsense/issues/15606): update README dual rgb** 
* [#15605](https://github.com/realsenseai/librealsense/pull/15605) by [@Nir-Az](https://github.com/Nir-Az) - **Cherry-pick [#15604](https://github.com/realsenseai/librealsense/issues/15604): dual RGB doc** 
* [#15603](https://github.com/realsenseai/librealsense/pull/15603) by [@Nir-Az](https://github.com/Nir-Az) - **Cherry-pick [#15600](https://github.com/realsenseai/librealsense/issues/15600): dual RGB firmware guard** 
* [#15593](https://github.com/realsenseai/librealsense/pull/15593) by [@Nir-Az](https://github.com/Nir-Az) - **Cherry-pick [#15586](https://github.com/realsenseai/librealsense/issues/15586): dual rgb ISP mode** 
* [#15519](https://github.com/realsenseai/librealsense/pull/15519) by [@Nir-Az](https://github.com/Nir-Az) - **Freeze 2.58.4: bump version** 
* [#15515](https://github.com/realsenseai/librealsense/pull/15515) by [@Nir-Az](https://github.com/Nir-Az) - **CI: run the LibCI job on Ubuntu 26.04** 
* [#15419](https://github.com/realsenseai/librealsense/pull/15419) by [@ashrafk93](https://github.com/ashrafk93) - **D401 GMSL dual-RGB support** 
* [#15510](https://github.com/realsenseai/librealsense/pull/15510) by [@OhadMeir](https://github.com/OhadMeir) - **Merge rs_5x5_mipi variant into rs_5x5 class** 
* [#15507](https://github.com/realsenseai/librealsense/pull/15507) by [@Nir-Az](https://github.com/Nir-Az) - **pre-release: move industrial_ci host container off EOL noetic/focal** 
* [#15394](https://github.com/realsenseai/librealsense/pull/15394) by [@rhabacker](https://github.com/rhabacker) - **Fix Numerical Instability in “Pose Inverse Compute”** 
* [#15499](https://github.com/realsenseai/librealsense/pull/15499) by [@remibettan](https://github.com/remibettan) - **d500: Sensors Config Mode option (Dual-RGB / Dedicated-Color toggle on D5x5)** 
* [#15505](https://github.com/realsenseai/librealsense/pull/15505) by [@ashrafk93](https://github.com/ashrafk93) - **Reuse project_shader for unmapped GL pointcloud** 
* [#15477](https://github.com/realsenseai/librealsense/pull/15477) by [@Nir-Az](https://github.com/Nir-Az) - **Migrate test-fw-update to pytest** 
* [#15495](https://github.com/realsenseai/librealsense/pull/15495) by [@Binyaminp](https://github.com/Binyaminp) - **Shrink or truncate the device panel name to fit the header width** 
* [#15502](https://github.com/realsenseai/librealsense/pull/15502) by [@Binyaminp](https://github.com/Binyaminp) - **Fix fw err test** 
* [#15498](https://github.com/realsenseai/librealsense/pull/15498) by [@AviaAv](https://github.com/AviaAv) - **Add search box to filter viewer controls by name** 
* [#15500](https://github.com/realsenseai/librealsense/pull/15500) by [@Binyaminp](https://github.com/Binyaminp) - **Exclude D585 Prototype from firmware error monitoring test** 
* [#14155](https://github.com/realsenseai/librealsense/pull/14155) by [@rhabacker](https://github.com/rhabacker) - **Add support to use system provided nlohmann_json library if requested** 
* [#15497](https://github.com/realsenseai/librealsense/pull/15497) by [@Nir-Az](https://github.com/Nir-Az) - **d500: add FW critical error codes 128-139 to notification map** 
* [#15496](https://github.com/realsenseai/librealsense/pull/15496) by [@remibettan](https://github.com/remibettan) - **d5x5 TC: downgrade CRC32 diagnostic logs from INFO to DEBUG (RSDEV-13186)** 
* [#15492](https://github.com/realsenseai/librealsense/pull/15492) by [@OhadMeir](https://github.com/OhadMeir) - **Support D500 GMSL HW reset** 
* [#15436](https://github.com/realsenseai/librealsense/pull/15436) by [@remibettan](https://github.com/remibettan) - **d500: register Gyro Sensitivity option on USB for D500 devices** 
* [#15494](https://github.com/realsenseai/librealsense/pull/15494) by [@OhadMeir](https://github.com/OhadMeir) - **Enable Hue on MIPI devices** 
* [#15471](https://github.com/realsenseai/librealsense/pull/15471) by [@Binyaminp](https://github.com/Binyaminp) - **Fix segfault from racing log level changes with concurrent logging** 
* [#15485](https://github.com/realsenseai/librealsense/pull/15485) by [@AviaAv](https://github.com/AviaAv) - **Change device-list CLI delimiter from space to comma** 
* [#15489](https://github.com/realsenseai/librealsense/pull/15489) by [@Nikolai-L](https://github.com/Nikolai-L) - **Reclassify two known ticketed defects instead of reddening CI** 
* [#15469](https://github.com/realsenseai/librealsense/pull/15469) by [@Binyaminp](https://github.com/Binyaminp) - **Depth fill rate multires** 
* [#15472](https://github.com/realsenseai/librealsense/pull/15472) by [@Binyaminp](https://github.com/Binyaminp) - **Fix rotation filter crash on streams without intrinsics** 
* [#15488](https://github.com/realsenseai/librealsense/pull/15488) by [@OhadMeir](https://github.com/OhadMeir) - **Viewer fix slider snapback on GMSL** 
* [#15486](https://github.com/realsenseai/librealsense/pull/15486) by [@remibettan](https://github.com/remibettan) - **d5x5 TC: post-merge host fixes + FW-side COMMIT-persistence diagnostic (RSDEV-13186)** 
* [#15467](https://github.com/realsenseai/librealsense/pull/15467) by [@Binyaminp](https://github.com/Binyaminp) - **udev: collapse per-PID 0x38E5 rules to VID-only matches** 
* [#15483](https://github.com/realsenseai/librealsense/pull/15483) by [@remibettan](https://github.com/remibettan) - **Expose device hardware time API (rs2_get_device_time_ms)** 
* [#15487](https://github.com/realsenseai/librealsense/pull/15487) by [@sysrsbuild-gh-agentic](https://github.com/sysrsbuild-gh-agentic) - **Security: Fix critical and high vulnerabilities (Aikido automated scan)** 
* [#15481](https://github.com/realsenseai/librealsense/pull/15481) by [@OhadMeir](https://github.com/OhadMeir) - **Identify D500 GMSL devices by actual PID** 
* [#15484](https://github.com/realsenseai/librealsense/pull/15484) by [@OhadMeir](https://github.com/OhadMeir) - **Add support to more D500 GMSL controls** 
* [#15452](https://github.com/realsenseai/librealsense/pull/15452) by [@remibettan](https://github.com/remibettan) - **d5x5 triggered calibration health-gated flow** 
* [#15482](https://github.com/realsenseai/librealsense/pull/15482) by [@Nir-Az](https://github.com/Nir-Az) - **Enable Emitter ON/OFF on all D500 active SKUs but D585S** 
* [#15382](https://github.com/realsenseai/librealsense/pull/15382) by [@remibettan](https://github.com/remibettan) - **ds: split fw_cmd per family + add AE_ACCEL_PARAMS = 0x95 opcode** 
* [#15387](https://github.com/realsenseai/librealsense/pull/15387) by [@rhabacker](https://github.com/rhabacker) - **CI: Fix race condition when building with ROS 2** 
* [#15445](https://github.com/realsenseai/librealsense/pull/15445) by [@AviaAv](https://github.com/AviaAv) - **D555 emitter on/off support over USB** 
* [#15005](https://github.com/realsenseai/librealsense/pull/15005) by [@remibettan](https://github.com/remibettan) - **flash 0.96 updates** 
* [#15422](https://github.com/realsenseai/librealsense/pull/15422) by [@Nir-Az](https://github.com/Nir-Az) - **test infra: abort watchdog so a deadlocked run cannot survive Jenkins abort and hold the device hub** 
* [#15476](https://github.com/realsenseai/librealsense/pull/15476) by [@AviaAv](https://github.com/AviaAv) - **Hide bundled third-party symbols from librealsense2.so exports** 
* [#15475](https://github.com/realsenseai/librealsense/pull/15475) by [@Binyaminp](https://github.com/Binyaminp) - **Recognize NIGHTLY-style FW filenames in extract_version_from_filename** 
* [#15293](https://github.com/realsenseai/librealsense/pull/15293) by [@ashrafk93](https://github.com/ashrafk93) - **Ashraf/zerocopy pr** 
* [#15474](https://github.com/realsenseai/librealsense/pull/15474) by [@OhadMeir](https://github.com/OhadMeir) - **GMSL query actual GVD size** 
* [#15462](https://github.com/realsenseai/librealsense/pull/15462) by [@Nir-Az](https://github.com/Nir-Az) - **Replace pytest-retry with pytest-rerunfailures; recycle devices between rerun attempts** 
* [#15465](https://github.com/realsenseai/librealsense/pull/15465) by [@Binyaminp](https://github.com/Binyaminp) - **Added D585 arg** 
* [#15468](https://github.com/realsenseai/librealsense/pull/15468) by [@AviaAv](https://github.com/AviaAv) - **Add Readout Shaping API-usage unit test** 
* [#15470](https://github.com/realsenseai/librealsense/pull/15470) by [@Nir-Az](https://github.com/Nir-Az) - **viewer: re-enable histogram equalization by default on D585** 
* [#15093](https://github.com/realsenseai/librealsense/pull/15093) by [@ev-mp](https://github.com/ev-mp) - **Realsense-Viewer Viewport Grid Overlay** 
* [#15464](https://github.com/realsenseai/librealsense/pull/15464) by [@OhadMeir](https://github.com/OhadMeir) - **Refactor backend-v4l** 
* [#15440](https://github.com/realsenseai/librealsense/pull/15440) by [@Nir-Az](https://github.com/Nir-Az) - **unit-tests: warn when a UniFi switch port links below 1 Gbps** 
* [#15460](https://github.com/realsenseai/librealsense/pull/15460) by [@Binyaminp](https://github.com/Binyaminp) - **unit-tests: exclude D500 from AE-setpoint test, enable advanced-mode suite in default CI** 
* [#15458](https://github.com/realsenseai/librealsense/pull/15458) by [@Nir-Az](https://github.com/Nir-Az) - **Update readme.md to add Perception Studio** 
* [#15389](https://github.com/realsenseai/librealsense/pull/15389) by [@sareluzi](https://github.com/sareluzi) - **About dialog: correct SDK version source ** 
* [#15298](https://github.com/realsenseai/librealsense/pull/15298) by [@AviaAv](https://github.com/AviaAv) - **Skip slow first clock-read in global-time (startup actual_fps jitter)** 
* [#15450](https://github.com/realsenseai/librealsense/pull/15450) by [@OhadMeir](https://github.com/OhadMeir) - **Add detection distance option** 
* [#15453](https://github.com/realsenseai/librealsense/pull/15453) by [@remibettan](https://github.com/remibettan) - **adding support for depth ae mode for gmsl devices** 
* [#15380](https://github.com/realsenseai/librealsense/pull/15380) by [@Binyaminp](https://github.com/Binyaminp) - **Laser pattern test** 
* [#15421](https://github.com/realsenseai/librealsense/pull/15421) by [@Binyaminp](https://github.com/Binyaminp) - **d500: hide AE-Setpoint control in viewer and reject it via advanced-mode API** 
* [#15439](https://github.com/realsenseai/librealsense/pull/15439) by [@Nir-Az](https://github.com/Nir-Az) - **conftest: settle 4s after enabling D585-family devices (FW accel bring-up window)** 
* [#15360](https://github.com/realsenseai/librealsense/pull/15360) by [@Gilaadb](https://github.com/Gilaadb) - **Fix global timestamps stepping/slewing/reversing and depth-color splits at the HW clock wrap** 
* [#15451](https://github.com/realsenseai/librealsense/pull/15451) by [@Nir-Az](https://github.com/Nir-Az) - **merge master to development** 
* [#15434](https://github.com/realsenseai/librealsense/pull/15434) by [@OhadMeir](https://github.com/OhadMeir) - **Add d585 dedicated color GMSL support** 
* [#15449](https://github.com/realsenseai/librealsense/pull/15449) by [@Nir-Az](https://github.com/Nir-Az) - **docs: add Ubuntu 26.04 to supported platforms** 
* [#15447](https://github.com/realsenseai/librealsense/pull/15447) by [@Nir-Az](https://github.com/Nir-Az) - **push 2.58.3 release to master** 
* [#15375](https://github.com/realsenseai/librealsense/pull/15375) by [@remibettan](https://github.com/remibettan) - **od: rename inference -> perception across SDK; sensor "Perception" / stream "Object Detection"** 
* [#15443](https://github.com/realsenseai/librealsense/pull/15443) by [@Nir-Az](https://github.com/Nir-Az) - **tests: exclude device setup from non-realtime deadlock timeout** 
* [#15430](https://github.com/realsenseai/librealsense/pull/15430) by [@Nir-Az](https://github.com/Nir-Az) - **CI: add ROS build farm pre-release workflow to catch farm-only release build failures** 
* [#15442](https://github.com/realsenseai/librealsense/pull/15442) by [@Nir-Az](https://github.com/Nir-Az) - **cmake: scope FastDDS -include cstdint to its own C++ targets** 
* [#15437](https://github.com/realsenseai/librealsense/pull/15437) by [@ashrafk93](https://github.com/ashrafk93) - **resolute ubuntu 26 support** 
* [#15427](https://github.com/realsenseai/librealsense/pull/15427) by [@Nir-Az](https://github.com/Nir-Az) - **Port live/test-deadlock.cpp to pytest** 
* [#15423](https://github.com/realsenseai/librealsense/pull/15423) by [@Nir-Az](https://github.com/Nir-Az) - **python: opt-in asynchronous log_to_callback eliminating GIL/mutex deadlock on librealsense threads** 
* [#15367](https://github.com/realsenseai/librealsense/pull/15367) by [@remibettan](https://github.com/remibettan) - **Enable pytest-fps-manual-exposure test** 
* [#15410](https://github.com/realsenseai/librealsense/pull/15410) by [@Nir-Az](https://github.com/Nir-Az) - **unifi: reboot the switch on --hub-reset + bound a wedged send (TCP_USER_TIMEOUT)** 
* [#15432](https://github.com/realsenseai/librealsense/pull/15432) by [@Nir-Az](https://github.com/Nir-Az) - **depth-unit test: request depth stream only instead of default config** 
* [#15431](https://github.com/realsenseai/librealsense/pull/15431) by [@Nir-Az](https://github.com/Nir-Az) - **Add D585 family PIDs to IMU calibration platform mapping** 
* [#15420](https://github.com/realsenseai/librealsense/pull/15420) by [@remibettan](https://github.com/remibettan) - **d400: exclude MIPI/GMSL devices from Depth Auto Exposure Mode option** 
* [#15426](https://github.com/realsenseai/librealsense/pull/15426) by [@remibettan](https://github.com/remibettan) - **Enable projector temperature option for all D535/D585 family PIDs** 
* [#15415](https://github.com/realsenseai/librealsense/pull/15415) by [@AviaAv](https://github.com/AviaAv) - **Add Readout Shaping option on some D400 devices** 
* [#15407](https://github.com/realsenseai/librealsense/pull/15407) by [@OhadMeir](https://github.com/OhadMeir) - **Inference and some embedded filters are mutual exclusive** 
* [#15414](https://github.com/realsenseai/librealsense/pull/15414) by [@AviaAv](https://github.com/AviaAv) - **Self-heal DDS camera eth-config at session start** 
* [#15418](https://github.com/realsenseai/librealsense/pull/15418) by [@Nir-Az](https://github.com/Nir-Az) - **tests: exclude D457 from auto-HDR preset test (unmapped MIPI XU 0x11)** 
* [#15416](https://github.com/realsenseai/librealsense/pull/15416) by [@Nir-Az](https://github.com/Nir-Az) - **Remove Download firmware link from viewer device-info panel** 
* [#15413](https://github.com/realsenseai/librealsense/pull/15413) by [@OhadMeir](https://github.com/OhadMeir) - **MF differentiate native from decompressed profiles** 
* [#15409](https://github.com/realsenseai/librealsense/pull/15409) by [@Nir-Az](https://github.com/Nir-Az) - **unit-tests: --rslog works for every test and --repeat pass** 
* [#15406](https://github.com/realsenseai/librealsense/pull/15406) by [@OhadMeir](https://github.com/OhadMeir) - **Handle options-preset test for DDS devices** 
* [#15401](https://github.com/realsenseai/librealsense/pull/15401) by [@AviaAv](https://github.com/AviaAv) - **Fix GIL deadlock when pipeline with Python callback is destroyed** 
* [#15381](https://github.com/realsenseai/librealsense/pull/15381) by [@remibettan](https://github.com/remibettan) - **d400: expose Depth Auto Exposure Mode on all D400 SKUs that have global shutter** 
* [#15354](https://github.com/realsenseai/librealsense/pull/15354) by [@Binyaminp](https://github.com/Binyaminp) - **enabled tests for D585** 
* [#15358](https://github.com/realsenseai/librealsense/pull/15358) by [@Binyaminp](https://github.com/Binyaminp) - **Align advanced example fix** 
* [#15392](https://github.com/realsenseai/librealsense/pull/15392) by [@OhadMeir](https://github.com/OhadMeir) - **DFU device can reset after update without waiting for status query** 
* [#15400](https://github.com/realsenseai/librealsense/pull/15400) by [@OhadMeir](https://github.com/OhadMeir) - **fsp-performance test, longer 5 fps stream time** 
* [#15388](https://github.com/realsenseai/librealsense/pull/15388) by [@remibettan](https://github.com/remibettan) - **viewer: skip resize/maximize when saved window size is zero** 
* [#15328](https://github.com/realsenseai/librealsense/pull/15328) by [@say-paul](https://github.com/say-paul) - **Fix: remove underlying unicode in rmse.py** 
* [#15346](https://github.com/realsenseai/librealsense/pull/15346) by [@alexkunin-gh](https://github.com/alexkunin-gh) - **COM code review: column-histogram median X and Euclidean distance for non-range fallback** 
* [#15260](https://github.com/realsenseai/librealsense/pull/15260) by [@Nir-Az](https://github.com/Nir-Az) - **Enable close-range on dual-RGB (depth-only limitation); keep dedicated-color unconditional** 
* [#15377](https://github.com/realsenseai/librealsense/pull/15377) by [@Nir-Az](https://github.com/Nir-Az) - **Reduce hardware/nightly-only unit tests: profile-eq → sw-device, drop extrinsics, un-gate waiting-on** 
* [#15368](https://github.com/realsenseai/librealsense/pull/15368) by [@Nir-Az](https://github.com/Nir-Az) - **fps-performance: coverage tiers + USB settle to stop the stream-reconfig storm wedging CI agents** 
* [#15370](https://github.com/realsenseai/librealsense/pull/15370) by [@Nir-Az](https://github.com/Nir-Az) - **hw-reset stress: re-acquire live device handle before each reset** 
* [#15373](https://github.com/realsenseai/librealsense/pull/15373) by [@Nir-Az](https://github.com/Nir-Az) - **d500: add Emitter Always On option to D585/D535 5x5 devices** 
* [#15371](https://github.com/realsenseai/librealsense/pull/15371) by [@Nir-Az](https://github.com/Nir-Az) - **align D5x5 visual preset to D585S values** 
* [#15372](https://github.com/realsenseai/librealsense/pull/15372) by [@remibettan](https://github.com/remibettan) - **viewer: split depth/IR resolution UI gated on embedded decimation (always on for D585S)** 
* [#15338](https://github.com/realsenseai/librealsense/pull/15338) by [@remibettan](https://github.com/remibettan) - **UI long disconnect time - mf-backend: don't defer device removal in HID-binding gate** 
* [#15299](https://github.com/realsenseai/librealsense/pull/15299) by [@Binyaminp](https://github.com/Binyaminp) - **changed window state config update** 
* [#15353](https://github.com/realsenseai/librealsense/pull/15353) by [@remibettan](https://github.com/remibettan) - **unit-tests: reset pytest-timeout between pytest-retry attempts** 
* [#15366](https://github.com/realsenseai/librealsense/pull/15366) by [@remibettan](https://github.com/remibettan) - **enable pytest-drops-on-set on D457 and D401** 
* [#15362](https://github.com/realsenseai/librealsense/pull/15362) by [@OhadMeir](https://github.com/OhadMeir) - **MF profiles match USB descriptor. D500 prefer NV12 over M420** 
* [#15361](https://github.com/realsenseai/librealsense/pull/15361) by [@remibettan](https://github.com/remibettan) - **fix M420 and NV12 unpack for non-16-multiple widths** 
* [#15336](https://github.com/realsenseai/librealsense/pull/15336) by [@remibettan](https://github.com/remibettan) - **security: bump axios to 1.18.1 and harden Tauri spawn_api_process** 
* [#15359](https://github.com/realsenseai/librealsense/pull/15359) by [@OhadMeir](https://github.com/OhadMeir) - **Prefer M420 over NV12** 
* [#15337](https://github.com/realsenseai/librealsense/pull/15337) by [@remibettan](https://github.com/remibettan) - **viewer-tests: gate Linux mem-leak threshold by arch (aarch64 12, x86_64 5), skip retry** 
* [#15357](https://github.com/realsenseai/librealsense/pull/15357) by [@remibettan](https://github.com/remibettan) - **viewer: fix hang on close while FW logs are active** 
* [#15352](https://github.com/realsenseai/librealsense/pull/15352) by [@OhadMeir](https://github.com/OhadMeir) - **Add NV12 support to D500** 
* [#15343](https://github.com/realsenseai/librealsense/pull/15343) by [@Nir-Az](https://github.com/Nir-Az) - **pytest: record failures + teardown in per-test logs across retries/repeats** 
* [#15348](https://github.com/realsenseai/librealsense/pull/15348) by [@Nir-Az](https://github.com/Nir-Az) - **Relax depth AE convergence: fast gating (single resolution) + nightly full-matrix** 
* [#15349](https://github.com/realsenseai/librealsense/pull/15349) by [@Nir-Az](https://github.com/Nir-Az) - **pytest: unmask real setup error when pytest-retry reruns a failed module-fixture setup** 
* [#15335](https://github.com/realsenseai/librealsense/pull/15335) by [@Nir-Az](https://github.com/Nir-Az) - **Test all d400 on pytest-fps-performance.py** 
* [#15340](https://github.com/realsenseai/librealsense/pull/15340) by [@Nir-Az](https://github.com/Nir-Az) - **sw-update: match device names with or without Intel prefix** 
* [#15317](https://github.com/realsenseai/librealsense/pull/15317) by [@Nir-Az](https://github.com/Nir-Az) - **Migrate test-motion.py to pytest** 
* [#15314](https://github.com/realsenseai/librealsense/pull/15314) by [@Nir-Az](https://github.com/Nir-Az) - **Migrate test-post-processing-from-bag.py to pytest** 
* [#15330](https://github.com/realsenseai/librealsense/pull/15330) by [@Nir-Az](https://github.com/Nir-Az) - **viewer: fix post-processing filter controls reverting to default value** 
* [#15342](https://github.com/realsenseai/librealsense/pull/15342) by [@Nir-Az](https://github.com/Nir-Az) - **MIPI color: raise RGB controls driver gate to 1.0.4.9** 
* [#15329](https://github.com/realsenseai/librealsense/pull/15329) by [@Nir-Az](https://github.com/Nir-Az) - **Retry D585S service-mode switch to handle post-enumeration FW readiness race** 
* [#15281](https://github.com/realsenseai/librealsense/pull/15281) by [@Nir-Az](https://github.com/Nir-Az) - **Device setup: fixture-owned port lifecycle** 
* [#15313](https://github.com/realsenseai/librealsense/pull/15313) by [@Nir-Az](https://github.com/Nir-Az) - **Migrate test-projection-from-recording.py to pytest** 
* [#15332](https://github.com/realsenseai/librealsense/pull/15332) by [@Nir-Az](https://github.com/Nir-Az) - **CI: remove redundant build permutations** 
* [#15327](https://github.com/realsenseai/librealsense/pull/15327) by [@OhadMeir](https://github.com/OhadMeir) - **Add support to dual RGB metadata on Windows** 
* [#15320](https://github.com/realsenseai/librealsense/pull/15320) by [@remibettan](https://github.com/remibettan) - **D401 GMSL: route RGB controls to dedicated color v4l2 node** 
* [#15311](https://github.com/realsenseai/librealsense/pull/15311) by [@Binyaminp](https://github.com/Binyaminp) - **viewer: terminal fix and enhancement** 
* [#15315](https://github.com/realsenseai/librealsense/pull/15315) by [@Nir-Az](https://github.com/Nir-Az) - **Migrate test-filters.py to pytest** 
* [#15309](https://github.com/realsenseai/librealsense/pull/15309) by [@Nir-Az](https://github.com/Nir-Az) - **Migrate test-unit-transform.py to pytest** 
* [#15022](https://github.com/realsenseai/librealsense/pull/15022) by [@pbsds](https://github.com/pbsds) - **Fix building with pybind11 v3.0.2** 
* [#15312](https://github.com/realsenseai/librealsense/pull/15312) by [@Nir-Az](https://github.com/Nir-Az) - **Migrate test-rotation-filter.py to pytest** 
* [#15284](https://github.com/realsenseai/librealsense/pull/15284) by [@SebastienBreton](https://github.com/SebastienBreton) - **Support L4T R39.2 (JetPack 7.2) in patch-realsense-ubuntu-L4T.sh** 
* [#15316](https://github.com/realsenseai/librealsense/pull/15316) by [@OhadMeir](https://github.com/OhadMeir) - **CI - enable options-watcher test** 
* [#15318](https://github.com/realsenseai/librealsense/pull/15318) by [@Binyaminp](https://github.com/Binyaminp) - **added comma that was missing** 
* [#15306](https://github.com/realsenseai/librealsense/pull/15306) by [@OhadMeir](https://github.com/OhadMeir) - **Fix set-gain-stress-test for D555** 
* [#15231](https://github.com/realsenseai/librealsense/pull/15231) by [@remibettan](https://github.com/remibettan) - **ci: add ROS 2 Lyrical (Ubuntu 26.04 Resolute) to build matrix** 
* [#15277](https://github.com/realsenseai/librealsense/pull/15277) by [@remibettan](https://github.com/remibettan) - **viewer-tests: add depth start/stop memory leak regression test** 
* [#15285](https://github.com/realsenseai/librealsense/pull/15285) by [@alexkunin-gh](https://github.com/alexkunin-gh) - **updated location of enhanced depth range debians** 


## Release 2.58.3 beta
Release Date: Jul 19, 2026


### API Changes - [see here](https://github.com/realsenseai/librealsense/wiki/API-Changes#version-2583-beta)

## What's new

### Highlights

- **Person Detection enhancements** — now supported on USB (in addition to DDS), improved distance accuracy (center-of-mass with nearest-cluster selection and NMS), record & playback support.
- **Improved Close Range Depth expansion** — now available on D555 (USB & DDS) and GMSL SKUs, exposed as a new embedded filter API (`RS2_EXTENSION_CLOSE_RANGE_EMBEDDED_FILTER`), with the library now loaded dynamically at runtime.
- **React-based Viewer preview** — 3D point cloud view, firmware update from a user-supplied .bin file, metadata panel, device connection-state detection, and hardware-reset flow fixes.
- **Native ROS2 playback** — playback of native ROS2 `.db3` recordings is now supported.
- **"Intel" prefix dropped** from camera visible names.

### Viewer

- Add stream-window swapping.
- Fix post-processing filter controls reverting to default value.
- Improve multi-FPS handling and display full API version (with build number).

### Platform & Build

- MIPI/GMSL: add RGB controls (saturation, sharpness, white balance, power-line frequency) 
- Add RealSense-VID (0x38E5) UVC metadata wildcard.
- CUDA pointcloud optimizations.
- Support D555/D500 gold-FW recovery from DFU mode; fix D500 recovery firmware update (recovery PIDs, abort on failed recovery switch).
- Security: remediate critical and high vulnerabilities (automated scans).
- Compiler compatibility: fix GCC build errors, glibc detection with GCC ≥ 14, MinGW-w64 `/W0` flag, consistent `<math.h>` includes.

### Bug Fixes
- Fix use-after-free in DDS device notification teardown.
- Fix FPS issue when no metadata is present.
- Fix dual D401 GMSL depth+color enumeration on a single MAX9296.
- Fix multi-pin camera handling.
- Fix playback stepping.
- Fix rs-enumerate-devices DDS domain bug.
- Re-apply manual exposure on Linux when disabling color auto-exposure.
- Fix Windows slow-IMU partial device (HID gate uses device path).
- Stop FW error polling cleanly on device destruction.
- Fix per-cycle GL texture leak on stream start/stop.
- Config file robustness: atomic updates, serialized access, coalesced background saves.

### Known issues
- For non-D555 users: if some dependencies issues appear related to fastrtps and fastcdr, try build from source with the flag -DBUILD_WITH_DDS=false 

[Full Changelog in GitHub](https://github.com/realsenseai/librealsense/compare/v2.58.2...v2.58.3)

* [#15345](https://github.com/realsenseai/librealsense/pull/15345) - **viewer: fix post-processing filter controls reverting to default value (cherry-pick [#15330](https://github.com/realsenseai/librealsense/issues/15330))** 
* [#15350](https://github.com/realsenseai/librealsense/pull/15350) - **Cherry-Pick sw-update: match device names with or without Intel prefix ([#15340](https://github.com/realsenseai/librealsense/issues/15340))** 
* [#15344](https://github.com/realsenseai/librealsense/pull/15344) - **Cherry-Pick MIPI color: raise RGB controls driver gate to 1.0.4.9 ([#15342](https://github.com/realsenseai/librealsense/issues/15342))** 
* [#15339](https://github.com/realsenseai/librealsense/pull/15339) - **Cherry-Pick D401 GMSL: route RGB controls to dedicated color v4l2 node ([#15320](https://github.com/realsenseai/librealsense/issues/15320))** 
* [#15286](https://github.com/realsenseai/librealsense/pull/15286) - **COM depth accuracy improvement** 
* [#15280](https://github.com/realsenseai/librealsense/pull/15280) - **Viewer - add stream window swapping** 
* [#15273](https://github.com/realsenseai/librealsense/pull/15273) - **rs-config: Defer config file saves to a background thread, coalescing rapid writes** 
* [#15307](https://github.com/realsenseai/librealsense/pull/15307) - **docs: add Code Change Discipline section to copilot-instructions** 
* [#15305](https://github.com/realsenseai/librealsense/pull/15305) - **viewer: fix preset requiring two clicks to update Advanced Controls** 
* [#15304](https://github.com/realsenseai/librealsense/pull/15304) - **updated location of enhanced depth range debians** 
* [#15292](https://github.com/realsenseai/librealsense/pull/15292) - **aligning to YUYV and Y8I improvements in d4xx driver** 
* [#15289](https://github.com/realsenseai/librealsense/pull/15289) - **Change transient temperature log level from ERROR to DEBUG** 
* [#15297](https://github.com/realsenseai/librealsense/pull/15297) - **Fix use-after-free in DDS device notification teardown** 
* [#15262](https://github.com/realsenseai/librealsense/pull/15262) - **Fix hardware reset flow in react-viewer** 
* [#15294](https://github.com/realsenseai/librealsense/pull/15294) - **Fix pytest-check soft-check failures leaking to teardown under pytest-retry** 
* [#15296](https://github.com/realsenseai/librealsense/pull/15296) - **Fix flaky D555 temperatures-xu-vs-hwmc test (post-reboot XU warm-up)** 
* [#15270](https://github.com/realsenseai/librealsense/pull/15270) - **pytest: per-camera D585S safety-mode fixture + guard against cross-camera module fixtures** 
* [#15254](https://github.com/realsenseai/librealsense/pull/15254) - **Support D555/D500 gold-FW recovery from DFU mode** 
* [#15110](https://github.com/realsenseai/librealsense/pull/15110) - **react-viewer: enable 3D point cloud view (RSDEV-9239)** 
* [#15104](https://github.com/realsenseai/librealsense/pull/15104) - **Migrate LibCI dds tests to pytest** 
* [#15282](https://github.com/realsenseai/librealsense/pull/15282) - **Embed tests' logs into JUnit XML for the Jenkins Test Result page** 
* [#15269](https://github.com/realsenseai/librealsense/pull/15269) by [@apocelipes](https://github.com/apocelipes) - **Fix compiler errors on GCC** 
* [#15275](https://github.com/realsenseai/librealsense/pull/15275) - **Viewer: fix per-cycle GL texture leak on stream start/stop (RSDEV-12247)** 
* [#15283](https://github.com/realsenseai/librealsense/pull/15283) - **Merge master into development (2.58.2 release sync)** 
* [#15279](https://github.com/realsenseai/librealsense/pull/15279) - **Allign color streams index with infrared index** 
* [#15274](https://github.com/realsenseai/librealsense/pull/15274) - **Fix multi pin camera handling** 
* [#15272](https://github.com/realsenseai/librealsense/pull/15272) - **Center of mass - use Euclidean distance and support non-aligned depth input** 
* [#14940](https://github.com/realsenseai/librealsense/pull/14940) by [@mengyui](https://github.com/mengyui) - **Fix dual D401 GMSL with depth and color enumeration on single MAX9296** 
* [#15178](https://github.com/realsenseai/librealsense/pull/15178) - **Drop 'Intel' prefix from camera visible names** 
* [#15266](https://github.com/realsenseai/librealsense/pull/15266) - **Rediscovered DDS device name can change** 
* [#15261](https://github.com/realsenseai/librealsense/pull/15261) - **Viewer - fix device name in stream window** 
* [#15259](https://github.com/realsenseai/librealsense/pull/15259) - **Fix FPS issue when no metadata present** 
* [#15257](https://github.com/realsenseai/librealsense/pull/15257) - **DDS: do not wait for hw-reset reply on D555 (RSDEV-11519)** 
* [#15253](https://github.com/realsenseai/librealsense/pull/15253) - **Poc/d585 dual rgb phase 2** 
* [#15191](https://github.com/realsenseai/librealsense/pull/15191) - **Add AI signature footer to PR skills** 
* [#15258](https://github.com/realsenseai/librealsense/pull/15258) - **Revert "align D5x5 visual preset to D585S values"** 
* [#15255](https://github.com/realsenseai/librealsense/pull/15255) - **improve center of mass accuracy (pick the nearest cluster)** 
* [#15256](https://github.com/realsenseai/librealsense/pull/15256) - **exclude advanced mode test for D401 device** 
* [#15248](https://github.com/realsenseai/librealsense/pull/15248) - **Mark projection test as live to run it with CUDA and GPU on Jenkins Jetson** 
* [#15223](https://github.com/realsenseai/librealsense/pull/15223) - **center of mass + NMS for person detection** 
* [#15247](https://github.com/realsenseai/librealsense/pull/15247) - **Viewer - improve multi FPS handling** 
* [#15230](https://github.com/realsenseai/librealsense/pull/15230) - **Add M420 and NV12 to DDS video encoding translation** 
* [#15234](https://github.com/realsenseai/librealsense/pull/15234) - **Display full API version (with build number) in viewer, tools and examples** 
* [#15209](https://github.com/realsenseai/librealsense/pull/15209) by [@StephanTLavavej](https://github.com/StephanTLavavej) - **Consistently include `<math.h>` with angle brackets** 
* [#15245](https://github.com/realsenseai/librealsense/pull/15245) - **Security: Fix critical and high vulnerabilities (Aikido automated scan)** 
* [#15237](https://github.com/realsenseai/librealsense/pull/15237) - **Ensure rs-dds-adapter is killed in finally block to prevent leaks** 
* [#15236](https://github.com/realsenseai/librealsense/pull/15236) - **D5x5 Change default FPS for dual RGB** 
* [#15232](https://github.com/realsenseai/librealsense/pull/15232) - **align D5x5 visual preset to D585S values** 
* [#15227](https://github.com/realsenseai/librealsense/pull/15227) - **Support RGB8 infrared on stream index 1 for D500 dual-RGB** 
* [#15225](https://github.com/realsenseai/librealsense/pull/15225) - **Remove redundent conversions** 
* [#15224](https://github.com/realsenseai/librealsense/pull/15224) - **Set default streams for D585 dual rgb** 
* [#15221](https://github.com/realsenseai/librealsense/pull/15221) - **viewer: disable histogram eq for D585 proto (0C07/0C08), rename OD to Person Detection** 
* [#15219](https://github.com/realsenseai/librealsense/pull/15219) - **removing close-range from dual rgb device** 
* [#15203](https://github.com/realsenseai/librealsense/pull/15203) - **pytest-sync: wait for streams to re-synchronize after a frame drop (RSDEV-11482)** 
* [#15003](https://github.com/realsenseai/librealsense/pull/15003) - **Migrate unit-tests/d500 to pytest** 
* [#15217](https://github.com/realsenseai/librealsense/pull/15217) - **skip pytest-ah-configurations on D585S pending RSDEV-12174** 
* [#15215](https://github.com/realsenseai/librealsense/pull/15215) - **OD record/playback: surface Object Detection as inference profile** 
* [#15216](https://github.com/realsenseai/librealsense/pull/15216) - **Fix Windows slow-IMU partial device: HID gate uses device_path** 
* [#15211](https://github.com/realsenseai/librealsense/pull/15211) - **advanced occ test - refined metrics** 
* [#15214](https://github.com/realsenseai/librealsense/pull/15214) - **viewer: add Dual-RGB / Dedicated-RGB mode toggle in More menu** 
* [#15210](https://github.com/realsenseai/librealsense/pull/15210) - **Add object detection on USB** 
* [#15212](https://github.com/realsenseai/librealsense/pull/15212) - **close-range USB: don't force enable state at SDK init** 
* [#15183](https://github.com/realsenseai/librealsense/pull/15183) - **Update only affected device on device change on REST API** 
* [#15169](https://github.com/realsenseai/librealsense/pull/15169) - **update range-depth cpp compilation** 
* [#15208](https://github.com/realsenseai/librealsense/pull/15208) - **Fix D500 recovery FW update: add D500 recovery PIDs and abort on failed recovery switch** 
* [#15207](https://github.com/realsenseai/librealsense/pull/15207) - **Port MinGW-w64 /W0 fix to development (PR [#15091](https://github.com/realsenseai/librealsense/issues/15091))** 
* [#15204](https://github.com/realsenseai/librealsense/pull/15204) - **close-range: pytest fixes (FW gate + skip metadata-bit pending RSDEV-12008)** 
* [#15180](https://github.com/realsenseai/librealsense/pull/15180) - **Hide stream tiles until streaming starts on react-viewer** 
* [#15206](https://github.com/realsenseai/librealsense/pull/15206) - **Push 2.58.2 to master** 
* [#15205](https://github.com/realsenseai/librealsense/pull/15205) - **Fix close-range UI toggle bounce + register on rs5x5_device** 
* [#15185](https://github.com/realsenseai/librealsense/pull/15185) - **Enhance serialization and migrate bag test fixtures to db3** 
* [#15198](https://github.com/realsenseai/librealsense/pull/15198) - **pytest-pipeline-start-stop: 1s settle after pipe.stop (D585S restart fix)** 
* [#15176](https://github.com/realsenseai/librealsense/pull/15176) - **usb: add MinZ embedded filter (Improved Close Range Depth) on D555 - demo** 
* [#15199](https://github.com/realsenseai/librealsense/pull/15199) - **perform bigger calib modification in occ test** 
* [#15114](https://github.com/realsenseai/librealsense/pull/15114) - **Fix glibc detection probe (gcc>=14)** 
* [#15197](https://github.com/realsenseai/librealsense/pull/15197) - **Add apt repo note and install link to enhanced-depth README** 
* [#15195](https://github.com/realsenseai/librealsense/pull/15195) - **Adding new D585/D535 devices** 
* [#15194](https://github.com/realsenseai/librealsense/pull/15194) - **Replace PR [#15177](https://github.com/realsenseai/librealsense/issues/15177) with PR [#15175](https://github.com/realsenseai/librealsense/issues/15175) to fix OD viewer toggle stall** 
* [#15193](https://github.com/realsenseai/librealsense/pull/15193) - **Skip multi-device streaming PHASE 2 (with color) pending HW investigation** 
* [#15184](https://github.com/realsenseai/librealsense/pull/15184) - **rest-api: check HWM response opcode to detect firmware errors** 
* [#15192](https://github.com/realsenseai/librealsense/pull/15192) - **Skip close-range embedded-filters metadata test pending FW fix** 
* [#15172](https://github.com/realsenseai/librealsense/pull/15172) - **Add MIPI RGB controls (saturation, sharpness, white balance, power line fr�** 
* [#15133](https://github.com/realsenseai/librealsense/pull/15133) - **Re-apply manual exposure on Linux when disabling color sensor auto-exposure** 
* [#15029](https://github.com/realsenseai/librealsense/pull/15029) - **viewer: eliminate UI freeze and slider snap-back on option change** 
* [#15174](https://github.com/realsenseai/librealsense/pull/15174) - **dds: add MinZ embedded filter (Improved Close Range Depth) for D555** 
* [#15146](https://github.com/realsenseai/librealsense/pull/15146) - **[RSDSO-21412] Add RealSense-VID (0x38E5) UVC metadata wildcard** 
* [#15181](https://github.com/realsenseai/librealsense/pull/15181) - **Skip image-quality 60fps tests on D436 (color stream FW bug)** 
* [#15170](https://github.com/realsenseai/librealsense/pull/15170) - **hub-recycle-imu: skip on hubless Jetson instead of failing** 
* [#15177](https://github.com/realsenseai/librealsense/pull/15177) - **Fix matcher for inference** 
* [#15173](https://github.com/realsenseai/librealsense/pull/15173) - **react-viewer: Show popup to enable metadata on Windows** 
* [#15096](https://github.com/realsenseai/librealsense/pull/15096) - **React viewer: FW update from user-supplied .bin file** 
* [#15171](https://github.com/realsenseai/librealsense/pull/15171) - **advanced occ test: fix fill rate assertion to require post > modified** 
* [#15168](https://github.com/realsenseai/librealsense/pull/15168) - **Migrate test-record-software-device to pytest** 
* [#15164](https://github.com/realsenseai/librealsense/pull/15164) - **Fix D555 ros2-compression test** 
* [#15162](https://github.com/realsenseai/librealsense/pull/15162) - **Clean up temp recordings left in OS temp dir by rec-play tests** 
* [#15151](https://github.com/realsenseai/librealsense/pull/15151) - **Migrate rsutils tests to pytest** 
* [#15163](https://github.com/realsenseai/librealsense/pull/15163) - **test-operational-mode: 1s settle after every pipe.stop (D585S restart fix)** 
* [#15156](https://github.com/realsenseai/librealsense/pull/15156) - **Fix playback stepping** 
* [#15159](https://github.com/realsenseai/librealsense/pull/15159) - **Fix use of dds related flags on non DDS build** 
* [#15128](https://github.com/realsenseai/librealsense/pull/15128) - **Allow metadata on multiple devices in parallel for REST** 
* [#15152](https://github.com/realsenseai/librealsense/pull/15152) - **advanced occ test: assert fill rate higher vs modified** 
* [#15145](https://github.com/realsenseai/librealsense/pull/15145) - **Pre-install ROS 2 apt source in GHA to avoid setup-ros rate-limit failures** 
* [#15118](https://github.com/realsenseai/librealsense/pull/15118) - **CUDA pointcloud optimizations** 
* [#15155](https://github.com/realsenseai/librealsense/pull/15155) - **Rename MinZ to Improved Close Range Depth** 
* [#15124](https://github.com/realsenseai/librealsense/pull/15124) - **Add REST API endpoint for sending hardware monitor (HWM) commands** 
* [#15141](https://github.com/realsenseai/librealsense/pull/15141) - **test-fw-update: pre-recover D400 devices stuck in DFU with gold signed FW + FWID fallback in find_first_device_or_exit** 
* [#15153](https://github.com/realsenseai/librealsense/pull/15153) - **Show person detection distance up to 5m only** 
* [#15148](https://github.com/realsenseai/librealsense/pull/15148) - **added minz viewer description to readme** 
* [#15147](https://github.com/realsenseai/librealsense/pull/15147) - **Bump brainstem to 2.12.4 to stop locked BrainStemLog.txt blocking workspace cleanup** 
* [#15143](https://github.com/realsenseai/librealsense/pull/15143) - **load minz library (librs_depth_range.so) at runtime dynamically** 
* [#15139](https://github.com/realsenseai/librealsense/pull/15139) - **Register RS2_OPTION_STEREO_BASELINE for MIPI devices** 
* [#15142](https://github.com/realsenseai/librealsense/pull/15142) - **D555 on USB will start with custom preset** 
* [#15134](https://github.com/realsenseai/librealsense/pull/15134) - **Fix rs-enumerate-devices DDS domain bug** 
* [#15138](https://github.com/realsenseai/librealsense/pull/15138) - **Reduce DDS multiple option registration error to debug log** 
* [#15140](https://github.com/realsenseai/librealsense/pull/15140) - **Remediate critical and high vulnerabilities from Aikido scan** 
* [#15136](https://github.com/realsenseai/librealsense/pull/15136) - **conftest + iq_helper: per-device fixture + per-device failure snapshots** 
* [#15079](https://github.com/realsenseai/librealsense/pull/15079) by [@hsuys](https://github.com/hsuys) - **Explicitly add enhanced minz feature support for GMSL sku** 
* [#15116](https://github.com/realsenseai/librealsense/pull/15116) - **Detect device connection state change on react viewer** 
* [#15038](https://github.com/realsenseai/librealsense/pull/15038) - **run-unit-tests: handle test-fw-update on devices below min FW** 
* [#15131](https://github.com/realsenseai/librealsense/pull/15131) - **multidev streaming test: enable rs debug log file** 
* [#15078](https://github.com/realsenseai/librealsense/pull/15078) - **Config atomic update** 
* [#15117](https://github.com/realsenseai/librealsense/pull/15117) - **Switch --retries to native pytest-retry; drop pytest-repeat hack** 
* [#15122](https://github.com/realsenseai/librealsense/pull/15122) - **Advanced OCC tests uses fill rate (not average depth)** 
* [#15109](https://github.com/realsenseai/librealsense/pull/15109) - **Improve test metadata-sync logging** 
* [#15127](https://github.com/realsenseai/librealsense/pull/15127) - **Enable extrinsics-imu test for D555** 
* [#15115](https://github.com/realsenseai/librealsense/pull/15115) - **Adapt D585S safety tests to flash 0.95 spec tables** 
* [#15105](https://github.com/realsenseai/librealsense/pull/15105) - **Show actual_fps in metadata panel, add unit test** 
* [#15106](https://github.com/realsenseai/librealsense/pull/15106) - **update README to enhanced depth** 
* [#15085](https://github.com/realsenseai/librealsense/pull/15085) - **Add color convertor test** 
* [#15101](https://github.com/realsenseai/librealsense/pull/15101) - **Pytest - module scoped fixtures** 
* [#15092](https://github.com/realsenseai/librealsense/pull/15092) - **viewer: serialize config_file access with a recursive mutex** 
* [#15094](https://github.com/realsenseai/librealsense/pull/15094) - **add metadata panel to react viewer** 
* [#15060](https://github.com/realsenseai/librealsense/pull/15060) - **Enable tare calibration tests** 
* [#15097](https://github.com/realsenseai/librealsense/pull/15097) - **react-viewer: force re-enumeration on manual Refresh** 
* [#15102](https://github.com/realsenseai/librealsense/pull/15102) - **update live minz example resolution** 
* [#15091](https://github.com/realsenseai/librealsense/pull/15091) by [@brechtsanders](https://github.com/brechtsanders) - **Don't use /W0 flag when building with MinGW-w64** 
* [#15082](https://github.com/realsenseai/librealsense/pull/15082) - **Security: Fix critical and high vulnerabilities (Aikido automated scan)** 
* [#15103](https://github.com/realsenseai/librealsense/pull/15103) - **skip multidev streaming test until depth-drops issue on Windows is stable** 
* [#15095](https://github.com/realsenseai/librealsense/pull/15095) - **pytest: add HH:MM:SS.mmm timestamp prefix to log lines** 
* [#15099](https://github.com/realsenseai/librealsense/pull/15099) - **rspy: bump wait_until_all_ports_disabled default to 10s for DDS removal** 
* [#15089](https://github.com/realsenseai/librealsense/pull/15089) - **ci: bump ros-tooling actions to Node 24 versions** 
* [#15084](https://github.com/realsenseai/librealsense/pull/15084) - **multidev streaming test - adding case without color** 
* [#15083](https://github.com/realsenseai/librealsense/pull/15083) - **Disable global timestamp option for DDS devices** 
* [#15088](https://github.com/realsenseai/librealsense/pull/15088) - **Push 2.58.1 to master** 
* [#15077](https://github.com/realsenseai/librealsense/pull/15077) - **Add --fw-update flag to explicitly fw-update on testing** 
* [#15064](https://github.com/realsenseai/librealsense/pull/15064) - **pyrealsense2: add build_wheel.py helper for local wheel builds** 
* [#15056](https://github.com/realsenseai/librealsense/pull/15056) - **Stop FW error polling cleanly on device destruction** 
* [#15054](https://github.com/realsenseai/librealsense/pull/15054) - **Support playback of native ROS2 .db3 recordings** 
* [#15073](https://github.com/realsenseai/librealsense/pull/15073) - **enabling multidev stream test** 
* [#15072](https://github.com/realsenseai/librealsense/pull/15072) - **CP: ds: default partial-device-allowed to true across SDK and viewer ([#15070](https://github.com/realsenseai/librealsense/issues/15070))** 
* [#15068](https://github.com/realsenseai/librealsense/pull/15068) - **CP: Remove D400 bundeled FW ([#15063](https://github.com/realsenseai/librealsense/issues/15063))** 
* [#15067](https://github.com/realsenseai/librealsense/pull/15067) - **CP: mf-backend: defer device-watcher callback until HID Sensor binding completes ([#15058](https://github.com/realsenseai/librealsense/issues/15058))** 
* [#15065](https://github.com/realsenseai/librealsense/pull/15065) - **CP: ds: honor partial-device-allowed setting for d400/d500 USB enumeration ([#15057](https://github.com/realsenseai/librealsense/issues/15057))** 
* [#15049](https://github.com/realsenseai/librealsense/pull/15049) - **Cherry Pick PR [#15012](https://github.com/realsenseai/librealsense/issues/15012) from OhadMeir: Fix Python wrapper possible deadlockas** 
* [#15031](https://github.com/realsenseai/librealsense/pull/15031) - **Cherry Pick to 58.1: [Jetson JP6] No Metadata when camera is connected to JP6 via USB3 ([#15024](https://github.com/realsenseai/librealsense/issues/15024))** 
* [#15025](https://github.com/realsenseai/librealsense/pull/15025) - **CP: Fix options stuck read-only after stopping streams in viewer** 
* [#14986](https://github.com/realsenseai/librealsense/pull/14986) - **Cherry Pick PRs [#14907](https://github.com/realsenseai/librealsense/issues/14907) [#14908](https://github.com/realsenseai/librealsense/issues/14908): rs-enum IR formats + media-ctl Tegra stream type** 
* [#14955](https://github.com/realsenseai/librealsense/pull/14955) - **Cherry Pick PR [#14934](https://github.com/realsenseai/librealsense/issues/14934) from Nir-Az: cmake: refactor realsense-file cmake** 
* [#14952](https://github.com/realsenseai/librealsense/pull/14952) - **CP: Fix D555 DB3 playback: motion crash, depth_units, texture** 
* [#14923](https://github.com/realsenseai/librealsense/pull/14923) - **CP: Fix loading recordings on Viewer on Linux** 


-------------------------------------------------------------------------------------------------------------------------------

## Release 2.58.2 beta
Release Date: Jun 14, 2026


### API Changes - [see here](https://github.com/realsenseai/librealsense/wiki/API-Changes#version-2582-beta)

#### What's new
### Highlights

- **Improved Close Range Depth**— new close-range depth-improvement feature integrated into the libRS Viewer, enabled on D400 devices using a dedicated Debian package on an Nvidia Jetson platform
- **New React-based Viewer Preview**.
- **Person detection on D555** — on chip NN person detection .
- **Bundled D400 firmware removed** from the SDK package.
- **Test framework modernization** — large-scale migration of the legacy LibCI/unit test suites to pytest, with new multi-device support and regression tests.

### Viewer

- Fix: options stuck read-only after stopping streams.
- GLSL rendering for M420 and NV12; NV12 platform-camera support.

### Platform & Build

- Jetson JP6: fix missing metadata when camera is connected via USB3
- D405: fix USB RGB Auto-Exposure metadata on FW ≥ 5.17.3.
- Auto Exposure / Gain Limit enabled for global-shutter GMSL cameras.
- GCC 12/13 compatibility (missing standard-library includes).
- CMake refactor of realsense-file; expose `rs_lz4`.
- rs-enum: Y8I/Y12I/Y16I IR formats; media-ctl for Tegra stream identification.
- ROS2: enable rosbag2 compression for recording/playback; replace EOL Iron with Kilted.

### Bug Fixes
- Python wrapper: fix possible deadlock.
- record_sensor: fix two callback-lifetime bugs causing nightly SIGSEGV
- mf-backend: defer device-watcher callback until HID Sensor binding completes.
- D555: fix playback with motion stream.

### Known issues
- Some users reported that on Windows machine with certain Graphic drivers, streaming depth images with RealSense Viewer lead to some memory leaks - workaround for that would be to turn OFF "GLSL for rendering" in the Viewer's settings
- For non-D555 users: if some dependencies issues appear related to fastrtps and fastcdr, try build from source with the flag -DBUILD_WITH_DDS=false 
- MIPI devices firmware update on RealSense-Viewer tool might fail randomly, if you encounter this issue please flash firmware using the rs-fw-update tool 

[Full Changelog in GitHub](https://github.com/IntelRealSense/librealsense/compare/v2.58.1...v2.58.2)

* [#15200](https://github.com/realsenseai/librealsense/pull/15200) - **Cherry-pick [#15197](https://github.com/realsenseai/librealsense/issues/15197) to r/2.58.2: enhanced-depth README apt note**
* [#15196](https://github.com/realsenseai/librealsense/pull/15196) - **Cherry Pick PR [#15194](https://github.com/realsenseai/librealsense/issues/15194): Replace PR [#15177](https://github.com/realsenseai/librealsense/issues/15177) with PR [#15175](https://github.com/realsenseai/librealsense/issues/15175) to fix OD viewer toggle stall**
* [#15179](https://github.com/realsenseai/librealsense/pull/15179) - **Cherry Pick PR [#15177](https://github.com/realsenseai/librealsense/issues/15177) from OhadMeir: Fix matcher for inference**
* [#15158](https://github.com/realsenseai/librealsense/pull/15158) - **Cherry Pick PR [#15155](https://github.com/realsenseai/librealsense/issues/15155): Rename MinZ to Improved Close Range Depth**
* [#15154](https://github.com/realsenseai/librealsense/pull/15154) - **Cherry Pick PR [#15153](https://github.com/realsenseai/librealsense/issues/15153) from OhadMeir: Show person detection distance up to 5m only**
* [#15150](https://github.com/realsenseai/librealsense/pull/15150) - **Cherry Pick PR [#15148](https://github.com/realsenseai/librealsense/issues/15148) from alexkunin-gh: added minz viewer description to readme to 2.58.2**
* [#15144](https://github.com/realsenseai/librealsense/pull/15144) - **Cherry-pick MinZ GMSL fixes to 2.58.2**
* [#15071](https://github.com/realsenseai/librealsense/pull/15071) - **enable minz on D405 and make it as the first filter**
* [#15070](https://github.com/realsenseai/librealsense/pull/15070) - **Default partial-device-allowed to true across SDK and viewer - for back compat IMU BMI 088 support**
* [#15069](https://github.com/realsenseai/librealsense/pull/15069) - **Update live minz example**
* [#15055](https://github.com/realsenseai/librealsense/pull/15055) - **Convert tests to pytest**
* [#15063](https://github.com/realsenseai/librealsense/pull/15063) - **Remove D400 bundeled FW**
* [#15061](https://github.com/realsenseai/librealsense/pull/15061) - **save one failure snapshot per test, not one per run**
* [#15062](https://github.com/realsenseai/librealsense/pull/15062) - **pyrealsense2: expose __version__ and __full_version__ at package level**
* [#15058](https://github.com/realsenseai/librealsense/pull/15058) - **mf-backend: defer device-watcher callback until HID Sensor binding completes**
* [#15057](https://github.com/realsenseai/librealsense/pull/15057) - **ds: honor partial-device-allowed setting for d400/d500 USB enumeration**
* [#15059](https://github.com/realsenseai/librealsense/pull/15059) - **update README enhanced-depth**
* [#15034](https://github.com/realsenseai/librealsense/pull/15034) - **add feature minz close range docs & examples**
* [#15015](https://github.com/realsenseai/librealsense/pull/15015) - **calib + image-quality: gate by --context, dedicate IQ depth test**
* [#15046](https://github.com/realsenseai/librealsense/pull/15046) - **Drop hard-coded VS 2022 generator from Windows CI**
* [#15051](https://github.com/realsenseai/librealsense/pull/15051) - **react-viewer: bump openssl 0.10.79, tar 0.4.45, bytes 1.11.1 - Aikido  CVEs, lockfile only**
* [#15027](https://github.com/realsenseai/librealsense/pull/15027) - **minZ feature integration into libRS viewer**
* [#15047](https://github.com/realsenseai/librealsense/pull/15047) - **test-eth-config: toggle link timeout instead of doubling**
* [#15042](https://github.com/realsenseai/librealsense/pull/15042) - **conftest: guard against ROS 2 launch.logging breaking pytest log_cli**
* [#15045](https://github.com/realsenseai/librealsense/pull/15045) - **rsutils: robust detect runtime CUDA probe**
* [#15039](https://github.com/realsenseai/librealsense/pull/15039) - **Fix flaky pytest-advanced-mode-toggle on MIPI devices**
* [#15044](https://github.com/realsenseai/librealsense/pull/15044) - **Fix options-watcher test**
* [#15043](https://github.com/realsenseai/librealsense/pull/15043) - **viewer: jetson cuda-not-used hint as info, not warning**
* [#15041](https://github.com/realsenseai/librealsense/pull/15041) - **Temporary disable pytest-options-watcher in CI**
* [#14972](https://github.com/realsenseai/librealsense/pull/14972) - **Feature/react viewer**
* [#15037](https://github.com/realsenseai/librealsense/pull/15037) - **Disable notifications-callback-gil test on Jetson**
* [#15010](https://github.com/realsenseai/librealsense/pull/15010) - **viewer: popup on Jetson when running without CUDA**
* [#15035](https://github.com/realsenseai/librealsense/pull/15035) - **Migrate fw/test-fw-errors to pytest**
* [#15036](https://github.com/realsenseai/librealsense/pull/15036) - **Migrate hdr tests to pytest**
* [#15020](https://github.com/realsenseai/librealsense/pull/15020) - **Migrate fw-logs tests to pytest**
* [#15012](https://github.com/realsenseai/librealsense/pull/15012) - **Fix Python wrapper possible deadlock**
* [#15019](https://github.com/realsenseai/librealsense/pull/15019) - **Migrate extrinsics tests to pytest (test-imu + port test-consistency.cpp)**
* [#15018](https://github.com/realsenseai/librealsense/pull/15018) - **Migrate dfu/test-device-fw-compatibility to pytest**
* [#15032](https://github.com/realsenseai/librealsense/pull/15032) - **Enforce 2s minimum gap between port disable and re-enable in enable_only**
* [#15033](https://github.com/realsenseai/librealsense/pull/15033) - **Add FirstSensor logic to C# wrapper**
* [#15024](https://github.com/realsenseai/librealsense/pull/15024) - ** tracked on RSDSO-21463: [Jetson JP6] No Metadata when camera is connected to JP6 via USB3**
* [#15021](https://github.com/realsenseai/librealsense/pull/15021) - **Fail fast when enable_only times out waiting for device enumeration**
* [#15026](https://github.com/realsenseai/librealsense/pull/15026) - **build skill improved to use parrallel and specific projects**
* [#15023](https://github.com/realsenseai/librealsense/pull/15023) - **Revert "Integration with minZ improver library ([#15008](https://github.com/realsenseai/librealsense/issues/15008))"**
* [#15008](https://github.com/realsenseai/librealsense/pull/15008) - **Integration with minZ improver library**
* [#15009](https://github.com/realsenseai/librealsense/pull/15009) - **Fix D405 USB RGB Auto Exposure metadata on FW >= 5.17.3**
* [#15017](https://github.com/realsenseai/librealsense/pull/15017) - **Fix options stuck read-only after stopping streams in viewer**
* [#15011](https://github.com/realsenseai/librealsense/pull/15011) - **threshold relaxation for advanced calib tests**
* [#15004](https://github.com/realsenseai/librealsense/pull/15004) - **Migrate LibCI calibration tests to pytest**
* [#14998](https://github.com/realsenseai/librealsense/pull/14998) - **Migrate streamin tests to pytest**
* [#14993](https://github.com/realsenseai/librealsense/pull/14993) - **hdr option supported checked and test skipped if not supported**
* [#14984](https://github.com/realsenseai/librealsense/pull/14984) - **Add pytest LibCI step to GHA workflows mirroring run-unit-tests.py**
* [#15006](https://github.com/realsenseai/librealsense/pull/15006) - **exclude D401 from depth test**
* [#14971](https://github.com/realsenseai/librealsense/pull/14971) - **Add object detection to DDS devices**
* [#14997](https://github.com/realsenseai/librealsense/pull/14997) - **ros2_writer: replace spaces in notification topic names**
* [#14989](https://github.com/realsenseai/librealsense/pull/14989) - **multi device test restoring**
* [#14987](https://github.com/realsenseai/librealsense/pull/14987) - **Migrate live/options tests to pytest**
* [#14967](https://github.com/realsenseai/librealsense/pull/14967) - **Update safety flash tables structs and tests to flash v0.95**
* [#14995](https://github.com/realsenseai/librealsense/pull/14995) - **record_sensor: fix two callback-lifetime bugs causing nightly SIGSEGV (RSDSO-21449)**
* [#14981](https://github.com/realsenseai/librealsense/pull/14981) - **Add test for recording with compression**
* [#14976](https://github.com/realsenseai/librealsense/pull/14976) - **Migrate unit-tests/log/ to pytest**
* [#14985](https://github.com/realsenseai/librealsense/pull/14985) - **Fix UnboundLocalError in hub creators when import fails non-trivially**
* [#14990](https://github.com/realsenseai/librealsense/pull/14990) - **pytest-sync: drop redundant per-iteration hardware_reset (RSDSO-21449)**
* [#14991](https://github.com/realsenseai/librealsense/pull/14991) - **Exclude D401 from pytest-hdr-long**
* [#14975](https://github.com/realsenseai/librealsense/pull/14975) - **Migrate LibCI rec-play tests to pytest**
* [#14974](https://github.com/realsenseai/librealsense/pull/14974) - **live/hw-reset: add advanced-mode toggle test**
* [#14983](https://github.com/realsenseai/librealsense/pull/14983) - **rest-api wrapper test: forward pyrealsense2 path to subprocess via PYTHONPATH**
* [#14979](https://github.com/realsenseai/librealsense/pull/14979) - **unit-tests: surgically shadow compiled pkgs from user-site (preserve paramiko/pykush)**
* [#14977](https://github.com/realsenseai/librealsense/pull/14977) - **depth mapping viewer: fix 128x128 hidden resolution and auto-select resolution on stream switch**
* [#14980](https://github.com/realsenseai/librealsense/pull/14980) - **disable test-xml-helper until fw issue resolved**
* [#14973](https://github.com/realsenseai/librealsense/pull/14973) - **Migrate live/tools and rest-api wrapper tests to pytest**
* [#14965](https://github.com/realsenseai/librealsense/pull/14965) - **migrating d400 folder tests to pytest infrastructure**
* [#14966](https://github.com/realsenseai/librealsense/pull/14966) - **Migrate LibCI image-quality tests to pytest**
* [#14962](https://github.com/realsenseai/librealsense/pull/14962) - **Migrate sw-dev and syncer legacy tests to pytest**
* [#14956](https://github.com/realsenseai/librealsense/pull/14956) - **rest-api: co-locate test + requirements under unit-tests/wrappers/rest-api/**
* [#14964](https://github.com/realsenseai/librealsense/pull/14964) - **Disable depth drop check in test-got-playback-frames**
* [#14961](https://github.com/realsenseai/librealsense/pull/14961) - **hw_reset: always settle MIPI driver even if hardware_reset() fails**
* [#14939](https://github.com/realsenseai/librealsense/pull/14939) - **Include directory path in pytest log file names**
* [#14960](https://github.com/realsenseai/librealsense/pull/14960) - **hiding lpc debug resolutions in viewer**
* [#14958](https://github.com/realsenseai/librealsense/pull/14958) - **pytest-depth-unit: log SDK error on test failure**
* [#14948](https://github.com/realsenseai/librealsense/pull/14948) - **Enable stable tests for D401**
* [#14933](https://github.com/realsenseai/librealsense/pull/14933) - **Enable Auto Exposure/Gain Limit for Global-Shutter GMSL cameras**
* [#14953](https://github.com/realsenseai/librealsense/pull/14953) - **test-basic-depth: stabilize depth sampling to remove false alarms**
* [#14949](https://github.com/realsenseai/librealsense/pull/14949) - **trigger device watcher on bind and unbind events**
* [#14954](https://github.com/realsenseai/librealsense/pull/14954) - **pytest: fail fast if a required plugin from requirements.txt is missing**
* [#14938](https://github.com/realsenseai/librealsense/pull/14938) - **exposing lpc resolution 2880x32**
* [#14951](https://github.com/realsenseai/librealsense/pull/14951) - **Migrate test-live-syncer-throughput to python**
* [#14944](https://github.com/realsenseai/librealsense/pull/14944) by [@SebastianMunozP](https://github.com/SebastianMunozP) - **Fix missing standard library includes for GCC 12/13 compatibility**
* [#14934](https://github.com/realsenseai/librealsense/pull/14934) - **cmake: refactor realsense-file to use add_subdirectory and expose rs_lz4**
* [#14937](https://github.com/realsenseai/librealsense/pull/14937) - **Update GitHub Actions versions and replace EOL ROS2 iron with Kilted**
* [#14947](https://github.com/realsenseai/librealsense/pull/14947) - **Fix D555 playback with motion stream**
* [#14945](https://github.com/realsenseai/librealsense/pull/14945) - **Exclude D401 from fps-performance test**
* [#14936](https://github.com/realsenseai/librealsense/pull/14936) - **depth sensor frames tests cleanup better**
* [#14935](https://github.com/realsenseai/librealsense/pull/14935) - **Rebaseline basic-color IQ test expected values**
* [#14921](https://github.com/realsenseai/librealsense/pull/14921) - **Migrate hw-reset tests to pytest**
* [#14924](https://github.com/realsenseai/librealsense/pull/14924) - **Enable compression for rosbag2 recording and playback**
* [#14912](https://github.com/realsenseai/librealsense/pull/14912) - **Save IQ failure snapshots to test log directory**
* [#14931](https://github.com/realsenseai/librealsense/pull/14931) by [@ghu3](https://github.com/ghu3) - **fix(test-eth-config): restore link.timeout after version-5 validation tests**
* [#14909](https://github.com/realsenseai/librealsense/pull/14909) - **Migrate metadata and multi_devices tests to pytest, add multi-device framework support**
* [#14919](https://github.com/realsenseai/librealsense/pull/14919) - **Defer hub creation so discovery prints are visible in pytest**
* [#14926](https://github.com/realsenseai/librealsense/pull/14926) - **Fix --exclude-device to support space-separated values and absent product types**
* [#14920](https://github.com/realsenseai/librealsense/pull/14920) - **Fix loading recordings on Viewer on Linux**
* [#14867](https://github.com/realsenseai/librealsense/pull/14867) - **Add regression tests for pytest infrastructure**
* [#14917](https://github.com/realsenseai/librealsense/pull/14917) - **Add settle delay between FPS test permutations**
* [#14687](https://github.com/realsenseai/librealsense/pull/14687) - **Add more configurations to sync test**
* [#14913](https://github.com/realsenseai/librealsense/pull/14913) - **Implement GLSL rendering for M420 and NV12**
* [#14897](https://github.com/realsenseai/librealsense/pull/14897) - **Add NV12 format support for platform camera**
* [#14873](https://github.com/realsenseai/librealsense/pull/14873) - **Improve viewer test logs**
* [#14868](https://github.com/realsenseai/librealsense/pull/14868) - **Break early on DFU errors**
* [#14908](https://github.com/realsenseai/librealsense/pull/14908) - **rs-enum: use media-ctl for Tegra stream type identification**
* [#14910](https://github.com/realsenseai/librealsense/pull/14910) - **Update Security.md target point of contact**
* [#14895](https://github.com/realsenseai/librealsense/pull/14895) - **Inline git workflow rules into copilot-instructions**
* [#14906](https://github.com/realsenseai/librealsense/pull/14906) - **Clean up temp files in rs-convert test**
* [#14907](https://github.com/realsenseai/librealsense/pull/14907) - **rs-enum: add Y8I, Y12I, Y16I as IR pixel formats**OS2 based**
* [#14887](https://github.com/realsenseai/librealsense/pull/14887) - **Fix MIPI FW update in viewer: verify write and reconnect**
* [#14861](https://github.com/realsenseai/librealsense/pull/14861) - **fix named_mutex to open file descriptors lazily**
* [#14885](https://github.com/realsenseai/librealsense/pull/14885) - **Fix stop_wrapper masking real test failures**
* [#14880](https://github.com/realsenseai/librealsense/pull/14880) - **Disable all hub ports after device discovery**
* [#14878](https://github.com/realsenseai/librealsense/pull/14878) - **Disable muti camera streaming test until stable**
* [#14879](https://github.com/realsenseai/librealsense/pull/14879) - **Align record-playback to development 30/03**
* [#14874](https://github.com/realsenseai/librealsense/pull/14874) - **Enable D555 for fps permutations**
* [#14869](https://github.com/realsenseai/librealsense/pull/14869) - **Fix MIPI FW update silent failure**
* [#14872](https://github.com/realsenseai/librealsense/pull/14872) - **Skip downgrade counter reset on unsigned FW**
* [#14843](https://github.com/realsenseai/librealsense/pull/14843) - **advanced tare tests stabilization**
* [#14782](https://github.com/realsenseai/librealsense/pull/14782) - **Add support for Jetson 6.2.2**
* [#14859](https://github.com/realsenseai/librealsense/pull/14859) - **Use sensor API on multi device test**
* [#14866](https://github.com/realsenseai/librealsense/pull/14866) - **Fix device_each to skip instead of fail when no matching device**
* [#14833](https://github.com/realsenseai/librealsense/pull/14833) - **fix: use correct DFU downgrade counter opcode and fix flash-lock detection**
* [#14865](https://github.com/realsenseai/librealsense/pull/14865) - **Increase MIPI hw reset sleep from 3 to 8 seconds**
* [#14854](https://github.com/realsenseai/librealsense/pull/14854) - **Fail tests instead of skipping when required device is missing**
* [#14848](https://github.com/realsenseai/librealsense/pull/14848) - **Fix D435 missed on initial device query**
* [#14768](https://github.com/realsenseai/librealsense/pull/14768) - **Add tests for viewer UI**
* [#14852](https://github.com/realsenseai/librealsense/pull/14852) - **Fix signal handler re-entrancy and hang on Ctrl+C**
* [#14841](https://github.com/realsenseai/librealsense/pull/14841) - **Optimize hub port management and fix test-device-hub hang**
* [#14853](https://github.com/realsenseai/librealsense/pull/14853) - **Fix -r flag support in pytest**
* [#14849](https://github.com/realsenseai/librealsense/pull/14849) - **Patch CI D555 domain restore**
* [#14809](https://github.com/realsenseai/librealsense/pull/14809) - **Hdr long test add check**
* [#14608](https://github.com/realsenseai/librealsense/pull/14608) - **Fix: Typos in sse-align**
* [#14847](https://github.com/realsenseai/librealsense/pull/14847) - **Modify rs-enum for multicamera per deserializer**
* [#14845](https://github.com/realsenseai/librealsense/pull/14845) - **Fix rs-dds-adapter process leak on Windows**
* [#14846](https://github.com/realsenseai/librealsense/pull/14846) - **checking mipi driver version for ext sync xu, py mipi driver moved to repr**
* [#14825](https://github.com/realsenseai/librealsense/pull/14825) - **mipi driver version added to camera info, via device**
* [#14816](https://github.com/realsenseai/librealsense/pull/14816) - **BUILD_WITH_NEON flag added in cmake config and in code**
* [#14840](https://github.com/realsenseai/librealsense/pull/14840) - **fix modpost**
* [#14810](https://github.com/realsenseai/librealsense/pull/14810) - **Capture image on IQ tests failure**
* [#14817](https://github.com/realsenseai/librealsense/pull/14817) - **mipi fw update via viewer - done flag true only after hw reset**
* [#14829](https://github.com/realsenseai/librealsense/pull/14829) - **scripts/Tegra: add GitLab fallback for deprecated nv-tegra.nvidia.com**
* [#14828](https://github.com/realsenseai/librealsense/pull/14828) - **test-device-hub waits for device reconnection**
* [#14820](https://github.com/realsenseai/librealsense/pull/14820) - **dfu device path info only available for mipi devices**
* [#14831](https://github.com/realsenseai/librealsense/pull/14831) - **d401_gmsl device min fw updated to 5.17.2.2**
* [#14812](https://github.com/realsenseai/librealsense/pull/14812) - **Migrate all frames/ tests from legacy to pytest**
* [#14818](https://github.com/realsenseai/librealsense/pull/14818) - **Disable D401 tests till failure is checked**
* [#14801](https://github.com/realsenseai/librealsense/pull/14801) - **Cuda fallback to neon instead no opt**
* [#14819](https://github.com/realsenseai/librealsense/pull/14819) - **Update installation.md on Jetson Orin JP 6.2**
* [#14814](https://github.com/realsenseai/librealsense/pull/14814) - **correcting control number to be readable in log**
* [#14806](https://github.com/realsenseai/librealsense/pull/14806) - **ext sync xu USB - fixing device capabilities for d401**
* [#14811](https://github.com/realsenseai/librealsense/pull/14811) - **Rename --device-exclude to --exclude-device to match legacy flag**
* [#14802](https://github.com/realsenseai/librealsense/pull/14802) - **Add pytest infra and convert t2ff-pipeline test to run in it**
* [#14797](https://github.com/realsenseai/librealsense/pull/14797) - **Neon pointcloud correcting**
* [#14800](https://github.com/realsenseai/librealsense/pull/14800) - **Detect flash lock and validate FW update result in test-fw-update**
* [#14778](https://github.com/realsenseai/librealsense/pull/14778) - **multi devices tests added**
* [#14808](https://github.com/realsenseai/librealsense/pull/14808) - **Align record-playback to development 15/03**
* [#14785](https://github.com/realsenseai/librealsense/pull/14785) - **Remove obsolete EtherSense ethernet_client_server example**
* [#14695](https://github.com/realsenseai/librealsense/pull/14695) - **Make viewer stop-stream non-blocking for immediate UI response**
* [#14795](https://github.com/realsenseai/librealsense/pull/14795) - **Linux popup when no libudev dev**
* [#14796](https://github.com/realsenseai/librealsense/pull/14796) - **viewer - fixing the device detected message**
* [#14793](https://github.com/realsenseai/librealsense/pull/14793) - **Enable D555 on fps-permutations test**
* [#14794](https://github.com/realsenseai/librealsense/pull/14794) - **Split CI requirements according to platform and OS**
* [#14789](https://github.com/realsenseai/librealsense/pull/14789) - **CI hubs enhancements - try to fix occasional hangs**
* [#14788](https://github.com/realsenseai/librealsense/pull/14788) - **Change setting log to warning**
* [#14769](https://github.com/realsenseai/librealsense/pull/14769) - **T2ff sensor enhance info + skills update**
* [#14783](https://github.com/realsenseai/librealsense/pull/14783) - **Sleep between t2ff depth and color tests**
* [#14777](https://github.com/realsenseai/librealsense/pull/14777) - **Update UT frequencies to reduce run time**
* [#14770](https://github.com/realsenseai/librealsense/pull/14770) - **Add hw reset stress test**
* [#14766](https://github.com/realsenseai/librealsense/pull/14766) - **Fix crash in WMF backend during hardware reset**
* [#14726](https://github.com/realsenseai/librealsense/pull/14726) - **Ext sync new xu**
* [#14776](https://github.com/realsenseai/librealsense/pull/14776) - **fix test-fps-performance.py**
* [#14757](https://github.com/realsenseai/librealsense/pull/14757) - **Enable texture mapping test**
* [#14758](https://github.com/realsenseai/librealsense/pull/14758) - **D457 unit testing**
* [#14754](https://github.com/realsenseai/librealsense/pull/14754) - **fix unify stuck and add retry**

-------------------------------------------------------------------------------------------------------------------------------

## Release 2.58.1
Release Date: May 24, 2026


### API Changes - [see here](https://github.com/realsenseai/librealsense/wiki/API-Changes#version-2581-beta)

#### What's new
- Features :
   - Native ROS2 Record and Playback - See [record-and-playback.md](https://github.com/realsenseai/librealsense/blob/r/2.58.1/doc/record-and-playback.md)

### Bug Fixes
Various bugs fixes - see below PR list.

### Known issues
- Some users reported that on Windows machine with certain Graphic drivers, streaming depth images with RealSense Viewer lead to some memory leaks - workaround for that would be to turn OFF "GLSL for rendering" in the Viewer's settings
- For non-D555 users: if some dependencies issues appear related to fastrtps and fastcdr, try build from source with the flag -DBUILD_WITH_DDS=false 
- MIPI devices firmware update on RealSense-Viewer tool might fail randomly, if you encounter this issue please flash firmware using the rs-fw-update tool 

[Full Changelog in GitHub](https://github.com/IntelRealSense/librealsense/compare/v2.57.7...v2.58.1)

-I- 86 Pull Requests:
* [#15072](https://github.com/realsenseai/librealsense/pull/15072) - **CP: ds: default partial-device-allowed to true across SDK and viewer ([#15070](https://github.com/realsenseai/librealsense/issues/15070))**
* [#15068](https://github.com/realsenseai/librealsense/pull/15068) - **CP: Remove D400 bundeled FW ([#15063](https://github.com/realsenseai/librealsense/issues/15063))**
* [#15067](https://github.com/realsenseai/librealsense/pull/15067) - **CP: mf-backend: defer device-watcher callback until HID Sensor binding completes ([#15058](https://github.com/realsenseai/librealsense/issues/15058))**
* [#15065](https://github.com/realsenseai/librealsense/pull/15065) - **CP: ds: honor partial-device-allowed setting for d400/d500 USB enumeration ([#15057](https://github.com/realsenseai/librealsense/issues/15057))**
* [#15049](https://github.com/realsenseai/librealsense/pull/15049) - **Cherry Pick PR [#15012](https://github.com/realsenseai/librealsense/issues/15012) from OhadMeir: Fix Python wrapper possible deadlockas**
* [#15031](https://github.com/realsenseai/librealsense/pull/15031) - **Cherry Pick to 58.1: [Jetson JP6] No Metadata when camera is connected to JP6 via USB3 ([#15024](https://github.com/realsenseai/librealsense/issues/15024))**
* [#15025](https://github.com/realsenseai/librealsense/pull/15025) - **CP: Fix options stuck read-only after stopping streams in viewer**
* [#14986](https://github.com/realsenseai/librealsense/pull/14986) - **Cherry Pick PRs [#14907](https://github.com/realsenseai/librealsense/issues/14907) [#14908](https://github.com/realsenseai/librealsense/issues/14908): rs-enum IR formats + media-ctl Tegra stream type**
* [#14955](https://github.com/realsenseai/librealsense/pull/14955) - **Cherry Pick PR [#14934](https://github.com/realsenseai/librealsense/issues/14934) from Nir-Az: cmake: refactor realsense-file cmake**
* [#14952](https://github.com/realsenseai/librealsense/pull/14952) - **CP: Fix D555 DB3 playback: motion crash, depth_units, texture**
* [#14923](https://github.com/realsenseai/librealsense/pull/14923) - **CP: Fix loading recordings on Viewer on Linux**
* [#14904](https://github.com/realsenseai/librealsense/pull/14904) - **Remove Intel branding from wrappers docs and comments**
* [#14905](https://github.com/realsenseai/librealsense/pull/14905) - **Fix Windows 11 taskbar icon not showing**
* [#14894](https://github.com/realsenseai/librealsense/pull/14894) - **Add platform camera IR sensor (GREY/Y8) support**
* [#14875](https://github.com/realsenseai/librealsense/pull/14875) - **Remove Intel records from python wrapper**
* [#14893](https://github.com/realsenseai/librealsense/pull/14893) - **Fix CMake 4.0 compatibility for third-party deps**
* [#14890](https://github.com/realsenseai/librealsense/pull/14890) - **Missed adding the new file to cmake**
* [#14882](https://github.com/realsenseai/librealsense/pull/14882) - **Upgrade record-playback system to ROS2 based**
* [#14887](https://github.com/realsenseai/librealsense/pull/14887) - **Fix MIPI FW update in viewer: verify write and reconnect**
* [#14861](https://github.com/realsenseai/librealsense/pull/14861) - **fix named_mutex to open file descriptors lazily**
* [#14885](https://github.com/realsenseai/librealsense/pull/14885) - **Fix stop_wrapper masking real test failures**
* [#14880](https://github.com/realsenseai/librealsense/pull/14880) - **Disable all hub ports after device discovery**
* [#14878](https://github.com/realsenseai/librealsense/pull/14878) - **Disable muti camera streaming test until stable**
* [#14879](https://github.com/realsenseai/librealsense/pull/14879) - **Align record-playback to development 30/03**
* [#14874](https://github.com/realsenseai/librealsense/pull/14874) - **Enable D555 for fps permutations**
* [#14869](https://github.com/realsenseai/librealsense/pull/14869) - **Fix MIPI FW update silent failure**
* [#14872](https://github.com/realsenseai/librealsense/pull/14872) - **Skip downgrade counter reset on unsigned FW**
* [#14843](https://github.com/realsenseai/librealsense/pull/14843) - **advanced tare tests stabilization**
* [#14782](https://github.com/realsenseai/librealsense/pull/14782) - **Add support for Jetson 6.2.2**
* [#14859](https://github.com/realsenseai/librealsense/pull/14859) - **Use sensor API on multi device test**
* [#14866](https://github.com/realsenseai/librealsense/pull/14866) - **Fix device_each to skip instead of fail when no matching device**
* [#14833](https://github.com/realsenseai/librealsense/pull/14833) - **fix: use correct DFU downgrade counter opcode and fix flash-lock detection**
* [#14865](https://github.com/realsenseai/librealsense/pull/14865) - **Increase MIPI hw reset sleep from 3 to 8 seconds**
* [#14854](https://github.com/realsenseai/librealsense/pull/14854) - **Fail tests instead of skipping when required device is missing**
* [#14848](https://github.com/realsenseai/librealsense/pull/14848) - **Fix D435 missed on initial device query**
* [#14768](https://github.com/realsenseai/librealsense/pull/14768) - **Add tests for viewer UI**
* [#14852](https://github.com/realsenseai/librealsense/pull/14852) - **Fix signal handler re-entrancy and hang on Ctrl+C**
* [#14841](https://github.com/realsenseai/librealsense/pull/14841) - **Optimize hub port management and fix test-device-hub hang**
* [#14853](https://github.com/realsenseai/librealsense/pull/14853) - **Fix -r flag support in pytest**
* [#14849](https://github.com/realsenseai/librealsense/pull/14849) - **Patch CI D555 domain restore**
* [#14809](https://github.com/realsenseai/librealsense/pull/14809) - **Hdr long test add check**
* [#14608](https://github.com/realsenseai/librealsense/pull/14608) - **Fix: Typos in sse-align**
* [#14847](https://github.com/realsenseai/librealsense/pull/14847) - **Modify rs-enum for multicamera per deserializer**
* [#14845](https://github.com/realsenseai/librealsense/pull/14845) - **Fix rs-dds-adapter process leak on Windows**
* [#14846](https://github.com/realsenseai/librealsense/pull/14846) - **checking mipi driver version for ext sync xu, py mipi driver moved to repr**
* [#14825](https://github.com/realsenseai/librealsense/pull/14825) - **mipi driver version added to camera info, via device**
* [#14816](https://github.com/realsenseai/librealsense/pull/14816) - **BUILD_WITH_NEON flag added in cmake config and in code**
* [#14840](https://github.com/realsenseai/librealsense/pull/14840) - **fix modpost**
* [#14810](https://github.com/realsenseai/librealsense/pull/14810) - **Capture image on IQ tests failure**
* [#14817](https://github.com/realsenseai/librealsense/pull/14817) - **mipi fw update via viewer - done flag true only after hw reset**
* [#14829](https://github.com/realsenseai/librealsense/pull/14829) - **scripts/Tegra: add GitLab fallback for deprecated nv-tegra.nvidia.com**
* [#14828](https://github.com/realsenseai/librealsense/pull/14828) - **test-device-hub waits for device reconnection**
* [#14820](https://github.com/realsenseai/librealsense/pull/14820) - **dfu device path info only available for mipi devices**
* [#14831](https://github.com/realsenseai/librealsense/pull/14831) - **d401_gmsl device min fw updated to 5.17.2.2**
* [#14812](https://github.com/realsenseai/librealsense/pull/14812) - **Migrate all frames/ tests from legacy to pytest**
* [#14818](https://github.com/realsenseai/librealsense/pull/14818) - **Disable D401 tests till failure is checked**
* [#14801](https://github.com/realsenseai/librealsense/pull/14801) - **Cuda fallback to neon instead no opt**
* [#14819](https://github.com/realsenseai/librealsense/pull/14819) - **Update installation.md on Jetson Orin JP 6.2**
* [#14814](https://github.com/realsenseai/librealsense/pull/14814) - **correcting control number to be readable in log**
* [#14806](https://github.com/realsenseai/librealsense/pull/14806) - **ext sync xu USB - fixing device capabilities for d401**
* [#14811](https://github.com/realsenseai/librealsense/pull/14811) - **Rename --device-exclude to --exclude-device to match legacy flag**
* [#14802](https://github.com/realsenseai/librealsense/pull/14802) - **Add pytest infra and convert t2ff-pipeline test to run in it**
* [#14797](https://github.com/realsenseai/librealsense/pull/14797) - **Neon pointcloud correcting**
* [#14800](https://github.com/realsenseai/librealsense/pull/14800) - **Detect flash lock and validate FW update result in test-fw-update**
* [#14778](https://github.com/realsenseai/librealsense/pull/14778) - **multi devices tests added**
* [#14808](https://github.com/realsenseai/librealsense/pull/14808) - **Align record-playback to development 15/03**
* [#14785](https://github.com/realsenseai/librealsense/pull/14785) - **Remove obsolete EtherSense ethernet_client_server example**
* [#14695](https://github.com/realsenseai/librealsense/pull/14695) - **Make viewer stop-stream non-blocking for immediate UI response**
* [#14795](https://github.com/realsenseai/librealsense/pull/14795) - **Linux popup when no libudev dev**
* [#14796](https://github.com/realsenseai/librealsense/pull/14796) - **viewer - fixing the device detected message**
* [#14793](https://github.com/realsenseai/librealsense/pull/14793) - **Enable D555 on fps-permutations test**
* [#14794](https://github.com/realsenseai/librealsense/pull/14794) - **Split CI requirements according to platform and OS**
* [#14789](https://github.com/realsenseai/librealsense/pull/14789) - **CI hubs enhancements - try to fix occasional hangs**
* [#14788](https://github.com/realsenseai/librealsense/pull/14788) - **Change setting log to warning**
* [#14769](https://github.com/realsenseai/librealsense/pull/14769) - **T2ff sensor enhance info + skills update**
* [#14783](https://github.com/realsenseai/librealsense/pull/14783) - **Sleep between t2ff depth and color tests**
* [#14777](https://github.com/realsenseai/librealsense/pull/14777) - **Update UT frequencies to reduce run time**
* [#14770](https://github.com/realsenseai/librealsense/pull/14770) - **Add hw reset stress test**
* [#14766](https://github.com/realsenseai/librealsense/pull/14766) - **Fix crash in WMF backend during hardware reset**
* [#14726](https://github.com/realsenseai/librealsense/pull/14726) - **Ext sync new xu**
* [#14776](https://github.com/realsenseai/librealsense/pull/14776) - **fix test-fps-performance.py**
* [#14757](https://github.com/realsenseai/librealsense/pull/14757) - **Enable texture mapping test**
* [#14758](https://github.com/realsenseai/librealsense/pull/14758) - **D457 unit testing**
* [#14754](https://github.com/realsenseai/librealsense/pull/14754) - **fix unify stuck and add retry**

-------------------------------------------------------------------------------------------------------------------------------
## Release 2.57.7 beta
Release Date: Mar 03, 2026

### API Changes - [see here](https://github.com/realsenseai/librealsense/wiki/API-Changes#version-2577-beta)

#### What's new
- SKUs:
  - D436 support
- Features :
  - D555 global timestamp support (currently support single camera, multicamera is WIP)
- Stability:
  - 2.57.7 release was focused on stability and quality

### Bug Fixes
Various bugs fixes - see below PR list.

### Known issues
- Some users reported that on Windows machine with certain Graphic drivers, streaming depth images with RealSense Viewer lead to some memory leaks - workaround for that would be to turn OFF "GLSL for rendering" in the Viewer's settings
- For non-D555 users: if some dependencies issues appear related to fastrtps and fastcdr, try build from source with the flag -DBUILD_WITH_DDS=false 
- MIPI devices firmware update on RealSense-Viewer tool might fail randomly, if you encounter this issue please flash firmware using the rs-fw-update tool 

[Full Changelog in GitHub](https://github.com/IntelRealSense/librealsense/compare/v2.57.6...v2.57.7)

* [#14832](https://github.com/realsenseai/librealsense/pull/14832) - **d401_gmsl device min fw updated to 5.17.2.2**
* [#14756](https://github.com/realsenseai/librealsense/pull/14756) - **correcting neon_pointcloud**
* [#14755](https://github.com/realsenseai/librealsense/pull/14755) - **CI initial query not in loop, remove some CI logs**
* [#14753](https://github.com/realsenseai/librealsense/pull/14753) - **Stablize depth test**
* [#14752](https://github.com/realsenseai/librealsense/pull/14752) - **Fix t2ff UT timing**
* [#14747](https://github.com/realsenseai/librealsense/pull/14747) - **Add agent instructions + skills**
* [#14746](https://github.com/realsenseai/librealsense/pull/14746) - **Fix unit test reading config on Linux**
* [#14748](https://github.com/realsenseai/librealsense/pull/14748) - **New GH issue templates[#14742](https://github.com/realsenseai/librealsense/issues/14742)**
* [#14711](https://github.com/realsenseai/librealsense/pull/14711) by [@GeorgeFkd](https://github.com/GeorgeFkd) - **fixed exporting to textual .ply file, now interops with PCL properly**
* [#14744](https://github.com/realsenseai/librealsense/pull/14744) - **avoid recommendation acc to pid**
* [#14737](https://github.com/realsenseai/librealsense/pull/14737) - **Restore CI D555 domain if needed**
* [#14698](https://github.com/realsenseai/librealsense/pull/14698) - **Fix LibCI fail to recover MIPI device**
* [#14738](https://github.com/realsenseai/librealsense/pull/14738) - **d401 gmsl inheriting from nonmonochrome commented out**
* [#14740](https://github.com/realsenseai/librealsense/pull/14740) - **Stabilize ci**
* [#14741](https://github.com/realsenseai/librealsense/pull/14741) - **added sleep in advanced calib tests after applying calibration table**
* [#14728](https://github.com/realsenseai/librealsense/pull/14728) - **bigger modification in advanced calibration tests**
* [#14734](https://github.com/realsenseai/librealsense/pull/14734) - **Add support for python3.14 and remove EOL python 3.9**
* [#14731](https://github.com/realsenseai/librealsense/pull/14731) - **is_extendable checking only extension, error handled in error handler**
* [#14733](https://github.com/realsenseai/librealsense/pull/14733) - **Fix DDS devices global time stamp**
* [#14699](https://github.com/realsenseai/librealsense/pull/14699) - **fix compilation of python wrapper when librs is a cmake subproject**
* [#14729](https://github.com/realsenseai/librealsense/pull/14729) - **using pipe start api instead of removed safe_start_pipe**
* [#14725](https://github.com/realsenseai/librealsense/pull/14725) - **fix silenced warning**
* [#14727](https://github.com/realsenseai/librealsense/pull/14727) - **handle - timed out waiting for devices to be removed**
* [#14723](https://github.com/realsenseai/librealsense/pull/14723) - **[LibCI] Fix handling None ports**
* [#14722](https://github.com/realsenseai/librealsense/pull/14722) - **DDS adapter force kill - fix indent**
* [#14713](https://github.com/realsenseai/librealsense/pull/14713) - **[LibCI] Force kill rs-dds-adapter if stay alive**
* [#14717](https://github.com/realsenseai/librealsense/pull/14717) - **Wait for device at end of fw update process**
* [#14707](https://github.com/realsenseai/librealsense/pull/14707) - **domain 123 replaced in all not live dds tests to domain in config file**
* [#14716](https://github.com/realsenseai/librealsense/pull/14716) - **Fix error on recording load**
* [#14715](https://github.com/realsenseai/librealsense/pull/14715) - **Flexible topic support up to 32KB**
* [#14709](https://github.com/realsenseai/librealsense/pull/14709) - **avoid toggling adv mode via adv controls when streaming**
* [#14710](https://github.com/realsenseai/librealsense/pull/14710) - **Default preset enable + emitter freq remove for  d401**
* [#14704](https://github.com/realsenseai/librealsense/pull/14704) - **.github/agents/ path added to gitignore file**
* [#14705](https://github.com/realsenseai/librealsense/pull/14705) - **d555 test fw**
* [#14665](https://github.com/realsenseai/librealsense/pull/14665) - **d455 viewer advanced mode toggling during stream fix**
* [#14700](https://github.com/realsenseai/librealsense/pull/14700) - **Increase depth test timeout**
* [#14691](https://github.com/realsenseai/librealsense/pull/14691) - **Removing YUYV to Y8 in mapping to color**
* [#14690](https://github.com/realsenseai/librealsense/pull/14690) - **Add global timestamp handling to DDS devices**
* [#14688](https://github.com/realsenseai/librealsense/pull/14688) - **Minimal FW version for D436 update**
* [#14684](https://github.com/realsenseai/librealsense/pull/14684) - **Increase enumeration time in LibCI and add more logs info**
* [#14663](https://github.com/realsenseai/librealsense/pull/14663) - **Enable depth test**
* [#14686](https://github.com/realsenseai/librealsense/pull/14686) - **avoid from discarding the whole device because motion sensor failure**
* [#14683](https://github.com/realsenseai/librealsense/pull/14683) - **D400 mipi improvement wrap opt watch pause**
* [#14677](https://github.com/realsenseai/librealsense/pull/14677) - **Add sdk-domain-id flag to rs-dds-config**
* [#14664](https://github.com/realsenseai/librealsense/pull/14664) - **Mipi fixing fw update**
* [#14678](https://github.com/realsenseai/librealsense/pull/14678) - **Check is streaming on viewer close**
* [#14672](https://github.com/realsenseai/librealsense/pull/14672) - **enable calib for D457 and advanced tests**
* [#14676](https://github.com/realsenseai/librealsense/pull/14676) - **D401 gmsl color formats to left ir**
* [#14671](https://github.com/realsenseai/librealsense/pull/14671) - **Align development to master**
* [#14673](https://github.com/realsenseai/librealsense/pull/14673) - **Enable hw-reset tests for D555**
* [#14667](https://github.com/realsenseai/librealsense/pull/14667) - **Push 2.57.6 release to master**
* [#14658](https://github.com/realsenseai/librealsense/pull/14658) - **D405 d401 usb differentiation**
* [#14657](https://github.com/realsenseai/librealsense/pull/14657) - **using new field from device_info notification - when available**
* [#14655](https://github.com/realsenseai/librealsense/pull/14655) - **LibCI Hub Sleep on change**
* [#14656](https://github.com/realsenseai/librealsense/pull/14656) - **Stop notification reader when DDS device goes offline**
* [#14605](https://github.com/realsenseai/librealsense/pull/14605) - **matching d401_gmsl defaults to d405 defaults**
* [#14682](https://github.com/realsenseai/librealsense/pull/14682) - **Align record-playback to development**
* [#14650](https://github.com/realsenseai/librealsense/pull/14650) - **Enabling failing sensors**
* [#14612](https://github.com/realsenseai/librealsense/pull/14612) - **rs-terminal can now handle Integer type and some enhancments**
* [#14596](https://github.com/realsenseai/librealsense/pull/14596) by [@ludwig7959](https://github.com/ludwig7959) - **Add L4T 36.4.7 to JetPack 6 branch**
* [#14647](https://github.com/realsenseai/librealsense/pull/14647) - **Skip all 60FPS configurations.**
* [#14640](https://github.com/realsenseai/librealsense/pull/14640) - **Cherry Pick PR [#14639](https://github.com/realsenseai/librealsense/issues/14639) from Nir-Az/ Update installation_jetson.md key**
* [#14639](https://github.com/realsenseai/librealsense/pull/14639) - **Update installation_jetson.md key**
* [#14638](https://github.com/realsenseai/librealsense/pull/14638) - **Cherry Pick PR [#14618](https://github.com/realsenseai/librealsense/issues/14618) from ashrafk93: update public key instructions**
* [#14637](https://github.com/realsenseai/librealsense/pull/14637) - **Fix AE convergence test conditions**
* [#14630](https://github.com/realsenseai/librealsense/pull/14630) - **Enabled test-intra-camera-sync and test-depth_ae_convergance**
* [#14618](https://github.com/realsenseai/librealsense/pull/14618) - **update public key instructions**
* [#14631](https://github.com/realsenseai/librealsense/pull/14631) - **Update Doxyfile - Remove Intel**
* [#14591](https://github.com/realsenseai/librealsense/pull/14591) - **Docs refinements**
* [#14627](https://github.com/realsenseai/librealsense/pull/14627) - **disable advanced calib tests**
* [#14626](https://github.com/realsenseai/librealsense/pull/14626) - **Disable test-intra-camera-sync.py until stabilized**
* [#14625](https://github.com/realsenseai/librealsense/pull/14625) - **disable test-depth_ae_convergence.py until stabilized**
* [#14621](https://github.com/realsenseai/librealsense/pull/14621) - **Enable Depth AE Convergence Test with Firmware Version Check**
* [#14620](https://github.com/realsenseai/librealsense/pull/14620) - **Add intra-camera depth-color synchronization test**
* [#14611](https://github.com/realsenseai/librealsense/pull/14611) - **added back test.fail() in occ calib tests**
* [#14622](https://github.com/realsenseai/librealsense/pull/14622) - **Fix imu issue**
* [#14616](https://github.com/realsenseai/librealsense/pull/14616) - **moving d400 data from ds common to d400 scope**
* [#14602](https://github.com/realsenseai/librealsense/pull/14602) - **Add imu to cam info**
* [#14598](https://github.com/realsenseai/librealsense/pull/14598) - **Better handling of DFU errors for DDS devices**
* [#14613](https://github.com/realsenseai/librealsense/pull/14613) - **Add test to detect DDS camera on CI runs**
* [#14599](https://github.com/realsenseai/librealsense/pull/14599) - **correcting system call in linux and ios open_url**
* [#14603](https://github.com/realsenseai/librealsense/pull/14603) - **Disable d400-emitter-on-off test till fixed**
* [#14597](https://github.com/realsenseai/librealsense/pull/14597) - **Update fastapi version for REST API**
* [#14496](https://github.com/realsenseai/librealsense/pull/14496) - **Add D400 emitter-on-off test**
* [#14593](https://github.com/realsenseai/librealsense/pull/14593) - **Pause playback device as part of subdevice model**
* [#14527](https://github.com/realsenseai/librealsense/pull/14527) by [@johnnynunez](https://github.com/johnnynunez) - **Update CUDA NVCC flags for newer architectures**
* [#14580](https://github.com/realsenseai/librealsense/pull/14580) - **enable occ tests**
* [#14584](https://github.com/realsenseai/librealsense/pull/14584) - **rs-enum adjusted to support both single and multi cameras**
* [#14563](https://github.com/realsenseai/librealsense/pull/14563) - **branch policy update**


## Release 2.57.6 beta
Release Date: January 28, 2026

### API Changes - [see here](https://github.com/realsenseai/librealsense/wiki/API-Changes#version-2576-beta)

### What's new
- Features -:
  - Updated MacOS support (See [here](https://github.com/realsenseai/librealsense/blob/r/257_6/doc/installation_osx.md))
  - Supporting NVIDIA JetPack 7.1 for USB SKUs
  - New D555 early access FW released (see FW table below), with improved support for Native ROS2, and enhanced stability for multicamera use case

### Bug Fixes
Various bugs fixes - see below PR list.

### Known issues
- Some users reported that on Windows machine with certain Graphic drivers, streaming depth images with RealSense Viewer lead to some memory leaks - workaround for that would be to turn OFF "GLSL for rendering" in the Viewer's settings
- For non-D555 users: if some dependencies issues appear related to fastrtps and fastcdr, try build from source with the flag -DBUILD_WITH_DDS=false 
- MIPI devices firmware update on RealSense-Viewer tool might fail randomly, if you encounter this issue please flash firmware using the rs-fw-update tool 

[Full Changelog in GitHub](https://github.com/IntelRealSense/librealsense/compare/v2.57.5...v2.57.6)

* [#14585](https://github.com/realsenseai/librealsense/pull/14585) - **rs-enum adjusted to support both single and multi cameras**
* [#14582](https://github.com/realsenseai/librealsense/pull/14582) - **D401 gmsl fixes**
* [#14583](https://github.com/realsenseai/librealsense/pull/14583) - **disable test-depth_ae_convergence**
* [#14579](https://github.com/realsenseai/librealsense/pull/14579) - **Support JP7.1**
* [#14570](https://github.com/realsenseai/librealsense/pull/14570) - **Enable UT for D555**
* [#14568](https://github.com/realsenseai/librealsense/pull/14568) - **Support partial discovery for DDS devices**
* [#14557](https://github.com/realsenseai/librealsense/pull/14557) - **Update rs-enum for multi-camera setups**
* [#14558](https://github.com/realsenseai/librealsense/pull/14558) - **Support Nvidia R38.2.2**
* [#14567](https://github.com/realsenseai/librealsense/pull/14567) - **enabling partial devices, using new value in viewer json config**
* [#14566](https://github.com/realsenseai/librealsense/pull/14566) - **bitwise OR changed to logical OR**
* [#14564](https://github.com/realsenseai/librealsense/pull/14564) - **branch policy update**
* [#14559](https://github.com/realsenseai/librealsense/pull/14559) - **Update python instructions**
* [#14552](https://github.com/realsenseai/librealsense/pull/14552) - **Embedded filters python example**
* [#14555](https://github.com/realsenseai/librealsense/pull/14555) - **tests failing on D555 new FW disabled until passing**
* [#14550](https://github.com/realsenseai/librealsense/pull/14550) - **Update DDS configuration**
* [#14549](https://github.com/realsenseai/librealsense/pull/14549) - **D555 temporal filter viewer UI**
* [#14546](https://github.com/realsenseai/librealsense/pull/14546) - **Fix error C2880 with /std:c++latest option in VS2022**
* [#14539](https://github.com/realsenseai/librealsense/pull/14539) - **ae tests disabled until issue resolved in fw**
* [#14485](https://github.com/realsenseai/librealsense/pull/14485) - **Viewer no Advanced Mode toggle for D500**
* [#14538](https://github.com/realsenseai/librealsense/pull/14538) - **D457 define default imu profiles**
* [#14499](https://github.com/realsenseai/librealsense/pull/14499) - **Add D400 auto exposure metadata test**
* [#14500](https://github.com/realsenseai/librealsense/pull/14500) - **Add D400 auto exposure toggling test**
* [#14528](https://github.com/realsenseai/librealsense/pull/14528) - **flag for skipping d457 in test removed**
* [#14523](https://github.com/realsenseai/librealsense/pull/14523) - **Jetson - enable manual patches script for L4T 36.4.7**
* [#14454](https://github.com/realsenseai/librealsense/pull/14454) - **Fix libusb crash on macOS**
* [#14506](https://github.com/realsenseai/librealsense/pull/14506) - **Disabling test manual exposure, disabling 90 and 300 fps in ae convergence test**
* [#14503](https://github.com/realsenseai/librealsense/pull/14503) - **unit-tests - fw-update - fixing filename to fw version method**
* [#14502](https://github.com/realsenseai/librealsense/pull/14502) - **Fix test-hdr-preset failures**
* [#14497](https://github.com/realsenseai/librealsense/pull/14497) - **test-frames-depth modified to work in front of wall**
* [#14498](https://github.com/realsenseai/librealsense/pull/14498) - **log debug added for configuration file and content**
* [#14494](https://github.com/realsenseai/librealsense/pull/14494) - **Remove redundant conditions**
* [#14490](https://github.com/realsenseai/librealsense/pull/14490) - **Fix resolution not showing on DQT on D500**
* [#14483](https://github.com/realsenseai/librealsense/pull/14483) by [@brianferri](https://github.com/brianferri) - **Avoid transitive includes**
* [#14480](https://github.com/realsenseai/librealsense/pull/14480) - **Fix D555 not displaying resolutions on DQT**
* [#14463](https://github.com/realsenseai/librealsense/pull/14463) - **DDS fix dependency on open-start order**
* [#14481](https://github.com/realsenseai/librealsense/pull/14481) - **GH migration - add links**
* [#14479](https://github.com/realsenseai/librealsense/pull/14479) - **Align docs to latest**
* [#14476](https://github.com/realsenseai/librealsense/pull/14476) - **Master - Update realsense links**
* [#14429](https://github.com/realsenseai/librealsense/pull/14429) - **Modernize Librealsense SDK readme**
* [#14460](https://github.com/realsenseai/librealsense/pull/14460) - **Round float value to int when passed to int option**
* [#14465](https://github.com/realsenseai/librealsense/pull/14465) - **Viewer - fix frame location switch for multiple color streams**
* [#14467](https://github.com/realsenseai/librealsense/pull/14467) - **update links to realsenseai-migration**
* [#14466](https://github.com/realsenseai/librealsense/pull/14466) - **Update links to realsenseai**
* [#14462](https://github.com/realsenseai/librealsense/pull/14462) - **D585s - default res for occupancy stream**
* [#14445](https://github.com/realsenseai/librealsense/pull/14445) - **adding d401_gmsl device is SDK and linux backend**
* [#14458](https://github.com/realsenseai/librealsense/pull/14458) - **dashboard open close icons corrected**
* [#14450](https://github.com/realsenseai/librealsense/pull/14450) - **catching errors when parsing stream_options DDS  message**
* [#14455](https://github.com/realsenseai/librealsense/pull/14455) - **migrate s3 links**
* [#14451](https://github.com/realsenseai/librealsense/pull/14451) - **update expected filename on fw update test**
* [#14420](https://github.com/realsenseai/librealsense/pull/14420) - **add test-fps-manual-exposure.py**
* [#14428](https://github.com/realsenseai/librealsense/pull/14428) - **add D555 to test-fw-update**
* [#14446](https://github.com/realsenseai/librealsense/pull/14446) - **Add test:type to skip by connection type**
* [#14443](https://github.com/realsenseai/librealsense/pull/14443) - **Ubuntu tegra manual patches d436**
* [#14442](https://github.com/realsenseai/librealsense/pull/14442) - **patches script for jetson - replacing uvcvideo in its relevant place**
* [#14413](https://github.com/realsenseai/librealsense/pull/14413) - **calibration tests (phase 2)**
* [#14441](https://github.com/realsenseai/librealsense/pull/14441) - **Disable unused warning on libcurl**

***

## Release 2.57.5 beta
Release Date: December 24, 2025

### API Changes - [see here](https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2575)

### What's new
- Feature - D555 supporting Decimation Embedded Filter

### Bug Fixes
Various bugs fixes - see below PR list.

### Known issues
- Some users reported that on Windows machine with certain Graphic drivers, streaming depth images with RealSense Viewer lead to some memory leaks - workaround for that would be to turn OFF "GLSL for rendering" in the Viewer's settings
- For non-D555 users: if some dependencies issues appear related to fastrtps and fastcdr, try build from source with the flag -DBUILD_WITH_DDS=false 

[Full Changelog in GitHub](https://github.com/IntelRealSense/librealsense/compare/v2.57.4...v2.57.5)

* [#14488](https://github.com/realsenseai/librealsense/pull/14488) - **Fix D555 not displaying resolutions on DQT**
* [#14430](https://github.com/realsenseai/librealsense/pull/14430) - **Protect jpeg unpack**
* [#14427](https://github.com/realsenseai/librealsense/pull/14427) - **Fix string pedantic issues**
* [#14423](https://github.com/realsenseai/librealsense/pull/14423) - **Update package.xml before Realsense migration**
* [#14416](https://github.com/realsenseai/librealsense/pull/14416) - **Add extra security flags on Linux**
* [#14417](https://github.com/realsenseai/librealsense/pull/14417) - **fix wrong string compare on python device repr**
* [#14407](https://github.com/realsenseai/librealsense/pull/14407) - **adding GHA for mac DDS**
* [#14405](https://github.com/realsenseai/librealsense/pull/14405) - **fix D555 error with -r parameter on rs-fw-update**
* [#14414](https://github.com/realsenseai/librealsense/pull/14414) - **Avoid out-of-bounds access on global data**
* [#14419](https://github.com/realsenseai/librealsense/pull/14419) - **test-fw-errors.py: corrected firmware version comparison**
* [#14377](https://github.com/realsenseai/librealsense/pull/14377) - **test-fw-errors.py: Add known errors based on FW version**
* [#14408](https://github.com/realsenseai/librealsense/pull/14408) - **Fix iq tests not finishing test after unsupported config**
* [#14403](https://github.com/realsenseai/librealsense/pull/14403) - **Release 2 57 4 b to dev**
* [#14389](https://github.com/realsenseai/librealsense/pull/14389) - **Add page detection to texture mapping test**
* [#14383](https://github.com/realsenseai/librealsense/pull/14383) - **Embedded filter tests**
* [#14398](https://github.com/realsenseai/librealsense/pull/14398) - **fix D555 throwing on Linux in Debug build**
* [#14397](https://github.com/realsenseai/librealsense/pull/14397) - **D500 ir1 to ir2 extrinsics fix**
* [#14394](https://github.com/realsenseai/librealsense/pull/14394) - **mipi color md type changed in fw side adjustments**
* [#14387](https://github.com/realsenseai/librealsense/pull/14387) - **test enabled for D555 and D585s**
* [#14386](https://github.com/realsenseai/librealsense/pull/14386) by [@openminddev](https://github.com/openminddev) - **Update kernel release and patches revision for L4T 38.2 support**
* [#14385](https://github.com/realsenseai/librealsense/pull/14385) - **Embedded filters metadata**
* [#14381](https://github.com/realsenseai/librealsense/pull/14381) - **Embedded filters options changes cb**
* [#14371](https://github.com/realsenseai/librealsense/pull/14371) - **Embedded filters in viewer**
* [#14369](https://github.com/realsenseai/librealsense/pull/14369) - **Fix DQT startup for DDS devices that support advanced mode**
* [#14374](https://github.com/realsenseai/librealsense/pull/14374) - **topic-send python script improved to get json file**
* [#14375](https://github.com/realsenseai/librealsense/pull/14375) - **Fix D555 recovery mode not recognized**
* [#14379](https://github.com/realsenseai/librealsense/pull/14379) - **Disable unstable tests on D457 & D555**
* [#14368](https://github.com/realsenseai/librealsense/pull/14368) - **post processing filters hardcoded in DDS code instead of getting from FW**
* [#14370](https://github.com/realsenseai/librealsense/pull/14370) - **Increase number of initial frames on basic-color test**
* [#14312](https://github.com/realsenseai/librealsense/pull/14312) - **Dds dpp decimation**
* [#14366](https://github.com/realsenseai/librealsense/pull/14366) - **Fix auto selecting 256x144 resolution for UCAL**
* [#14348](https://github.com/realsenseai/librealsense/pull/14348) - **Create iq_helper.py, detect page on depth test**
* [#14363](https://github.com/realsenseai/librealsense/pull/14363) by [@torstenbaer](https://github.com/torstenbaer) - **Avoid out-of-bounds access on global data**
* [#14364](https://github.com/realsenseai/librealsense/pull/14364) - **Increase tolerance in color test, adjust some colors**

***

## Release 2.57.4 beta
Release Date: October 28, 2025

### API Changes - None.
For previous API changes, [see here](https://github.com/IntelRealSense/librealsense/wiki/API-Changes)

### What's new
- Platform - Permit unprivileged container
- Platform - Enhance support for Nvidia JP 7.0
- Behavior change - Disable Threshold Filter from Default Post-Processing

### Bug Fixes
Various bugs fixes - see below PR list.

### Known issues
- Some users reported that on Windows machine with certain Graphic drivers, streaming depth images with RealSense Viewer lead to some memory leaks - workaround for that would be to turn OFF "GLSL for rendering" in the Viewer's settings
- For non-D555 users: if some dependencies issues appear related to fastrtps and fastcdr, try build from source with the flag -DBUILD_WITH_DDS=false 

[Full Changelog in GitHub](https://github.com/IntelRealSense/librealsense/compare/v2.57.3...v2.57.4)

* [#14399](https://github.com/IntelRealSense/librealsense/pull/14399) - **Fix D555 throwing on Linux in Debug build**
* [#14384](https://github.com/IntelRealSense/librealsense/pull/14384) - **Fix DQT startup for DDS devices that support advanced mode**
* [#14382](https://github.com/IntelRealSense/librealsense/pull/14382) - **Fix D555 recovery mode not recognized**
* [#14362](https://github.com/IntelRealSense/librealsense/pull/14362) - **disable threshold filter by default**
* [#14361](https://github.com/IntelRealSense/librealsense/pull/14361) - **Add JP7 support on docs**
* [#14327](https://github.com/IntelRealSense/librealsense/pull/14327) - **Add missing constants & fixed 6fps bug for live/FPS weekly CI**
* [#14346](https://github.com/IntelRealSense/librealsense/pull/14346) - **D457 - Add test to ensure pre-minimal fw**
* [#14334](https://github.com/IntelRealSense/librealsense/pull/14334) - **CI: Add AE convergence test**
* [#14349](https://github.com/IntelRealSense/librealsense/pull/14349) by [@shakkar23](https://github.com/shakkar23) - **fix spelling in /examples/CMakeLists.txt**
* [#14313](https://github.com/IntelRealSense/librealsense/pull/14313) - **Advanced controls over DDS**
* [#14343](https://github.com/IntelRealSense/librealsense/pull/14343) - **[LibCI] stable fps permutation test**
* [#14340](https://github.com/IntelRealSense/librealsense/pull/14340) - **disable occ test**
* [#14338](https://github.com/IntelRealSense/librealsense/pull/14338) by [@hsuys](https://github.com/hsuys) - **Document Update for Frame Syncer**
* [#14337](https://github.com/IntelRealSense/librealsense/pull/14337) - **scan only in calib tests**
* [#14324](https://github.com/IntelRealSense/librealsense/pull/14324) by [@wsmlby](https://github.com/wsmlby) - **fix infinite loop at the end even if depth = 0**
* [#10566](https://github.com/IntelRealSense/librealsense/pull/10566) - **Add frame data copies section to frame lifetime docs**
* [#14333](https://github.com/IntelRealSense/librealsense/pull/14333) - **Fix UI slider_float edit mode**
* [#14318](https://github.com/IntelRealSense/librealsense/pull/14318) - **Add FW errors monitor test**
* [#14319](https://github.com/IntelRealSense/librealsense/pull/14319) - **throw when frame size is different from expected**
* [#14320](https://github.com/IntelRealSense/librealsense/pull/14320) - **Tracked-on-RSDSO-20171: sensor-vs-frame-ts fail**
* [#14321](https://github.com/IntelRealSense/librealsense/pull/14321) by [@alexk1976](https://github.com/alexk1976) - **threshold relaxation for occ calib**
* [#14315](https://github.com/IntelRealSense/librealsense/pull/14315) - **Curl disable PSL**
* [#14314](https://github.com/IntelRealSense/librealsense/pull/14314) - **add hubs packages to requirements.txt**
* [#14296](https://github.com/IntelRealSense/librealsense/pull/14296) - **improve and enable basic color test**
* [#14316](https://github.com/IntelRealSense/librealsense/pull/14316) - **sensor start after open, other simplifications**
* [#14311](https://github.com/IntelRealSense/librealsense/pull/14311) - **Stable timestamp sync test**
* [#14304](https://github.com/IntelRealSense/librealsense/pull/14304) - **Add dependencies troubleshooting tips**
* [#14299](https://github.com/IntelRealSense/librealsense/pull/14299) by [@BillyONeal](https://github.com/BillyONeal) - **Add include of stdexcept to avoid failure in build over ubuntu 24**
* [#14295](https://github.com/IntelRealSense/librealsense/pull/14295) by [@wsmlby](https://github.com/wsmlby) - **fix potential infinite loop in rs2_project_color_pixel_to_depth_pixel**
* [#14289](https://github.com/IntelRealSense/librealsense/pull/14289) - **Schedule long tests to run on nightly only and not gating**
* [#14285](https://github.com/IntelRealSense/librealsense/pull/14285) - **enable occ tests**
* [#14292](https://github.com/IntelRealSense/librealsense/pull/14292) - **Curl fix - remove docs**
* [#14294](https://github.com/IntelRealSense/librealsense/pull/14294) - **enable device S/N option for data-collect**
* [#14290](https://github.com/IntelRealSense/librealsense/pull/14290) - **Only download FW file if needed**
* [#14283](https://github.com/IntelRealSense/librealsense/pull/14283) - **Change RC to UTF8**
* [#14271](https://github.com/IntelRealSense/librealsense/pull/14271) - **searching for major and minor to map between /sys to /dev devices**
* [#14279](https://github.com/IntelRealSense/librealsense/pull/14279) - **fixed version comparison issues**
* [#14263](https://github.com/IntelRealSense/librealsense/pull/14263) - **using lut for performance improvement**
* [#14212](https://github.com/IntelRealSense/librealsense/pull/14212) - **Add FPS performance test to libCI**
* [#14276](https://github.com/IntelRealSense/librealsense/pull/14276) - **Support jp7.0**
***

## Release 2.57.3 beta
Release Date: September 15, 2025

### API Changes: [see here](https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2573-beta)

### What's new
- Feature - Ethernet Configuration Device
- Platform - Support Ubuntu kernels 6.11, 6.14
- Platform - Jetson JP 6.1, 6.2, 7.0
- UI - RealSense Inc. Logo 
- UI - librealsense github page updated


### Bug Fixes
Various bugs fixes - see below PR list.

### Known issues
- Some users reported that on Windows machine with certain Graphic drivers, streaming depth images with RealSense Viewer lead to some memory leaks - workaround for that would be to turn OFF "GLSL for rendering" in the Viewer's settings
- If some dependencies issues appear related to fastrtps and fastcdr, try build from source with the flag -DBUILD_WITH_DDS=false  
- Consuming the SDK using wget from the release source file zip cause a build error due to a `curl` issue.
WA is to rename the main folder not to include `.3` at the postfix. resolved in PR #[14292](https://github.com/IntelRealSense/librealsense/pull/14292)

### Notes
- The CMake minimal version is now 3.10

[Full Changelog in GitHub](https://github.com/IntelRealSense/librealsense/compare/v2.57.2...v2.57.3)


* [#14288](https://github.com/IntelRealSense/librealsense/pull/14288) - **support for JP7.0 and fixed version comparison issues**
* [#14266](https://github.com/IntelRealSense/librealsense/pull/14266) - **D415 OCC handling**
* [#14264](https://github.com/IntelRealSense/librealsense/pull/14264) by [@nathantsoi](https://github.com/nathantsoi) - **add support for L4T 36.4.4, tested on Jetson Orin Nano 8GB Developer Kit**
* [#14262](https://github.com/IntelRealSense/librealsense/pull/14262) - **LibCI --> exclude-device added**
* [#14225](https://github.com/IntelRealSense/librealsense/pull/14225) - **Readme refactor**
* [#14258](https://github.com/IntelRealSense/librealsense/pull/14258) - **Handle Coverity issue**
* [#14259](https://github.com/IntelRealSense/librealsense/pull/14259) - **support for JP6.1**
* [#14257](https://github.com/IntelRealSense/librealsense/pull/14257) - **Fix Auto HDR unit test**
* [#14256](https://github.com/IntelRealSense/librealsense/pull/14256) - **Support jp6.2**
* [#13311](https://github.com/IntelRealSense/librealsense/pull/13311) by [@mprat](https://github.com/mprat) - **CMake Compatibility with FetchContent**
* [#14250](https://github.com/IntelRealSense/librealsense/pull/14250) - **host assisted UCAL uses HD resolution**
* [#14248](https://github.com/IntelRealSense/librealsense/pull/14248) - **create unique popup name**
* [#14246](https://github.com/IntelRealSense/librealsense/pull/14246) - **hw_reset and toggle_adv_mode methods refactored to new class for mipi**
* [#14229](https://github.com/IntelRealSense/librealsense/pull/14229) - **Fix Check For Updates crash**
* [#14228](https://github.com/IntelRealSense/librealsense/pull/14228) - **Add Ethernet configuration API**
* [#14239](https://github.com/IntelRealSense/librealsense/pull/14239) - **add device_hub to python api**
* [#14245](https://github.com/IntelRealSense/librealsense/pull/14245) - **Logo outlined**
* [#14243](https://github.com/IntelRealSense/librealsense/pull/14243) - **D415_GMSL fw update via viewer corrected**
* [#14224](https://github.com/IntelRealSense/librealsense/pull/14224) - **adding formats and skus to kernel 6.11 and kernel 6.14**
* [#14241](https://github.com/IntelRealSense/librealsense/pull/14241) - **Update min FW version for D415 GMSL**
* [#14236](https://github.com/IntelRealSense/librealsense/pull/14236) - **D415 GMSL HW reset fix**
* [#14227](https://github.com/IntelRealSense/librealsense/pull/14227) - **RW16 added as raw format**
* [#14230](https://github.com/IntelRealSense/librealsense/pull/14230) - **fix device_hub.is_connected issue for DDS**
* [#14233](https://github.com/IntelRealSense/librealsense/pull/14233) - **Fix new compilation warning**
* [#14209](https://github.com/IntelRealSense/librealsense/pull/14209) - **D457 unsigned fw update check**
* [#14213](https://github.com/IntelRealSense/librealsense/pull/14213) - **Develop Auto HDR tests for CI - Basic**
* [#14100](https://github.com/IntelRealSense/librealsense/pull/14100) - **Create timestamp synchronization test**
* [#14221](https://github.com/IntelRealSense/librealsense/pull/14221) - **Disable test-y16-calibration-format for D457 until fixed**
* [#14219](https://github.com/IntelRealSense/librealsense/pull/14219) - **CMake minimal 3.10 + json version 3.12**
* [#14218](https://github.com/IntelRealSense/librealsense/pull/14218) - **DDS device implements calibration_change_device API**
* [#14170](https://github.com/IntelRealSense/librealsense/pull/14170) - **add check for D500 device before HWMC**
* [#14215](https://github.com/IntelRealSense/librealsense/pull/14215) - **Enable test-live-frames-fps-permutations for D555 device**
* [#14205](https://github.com/IntelRealSense/librealsense/pull/14205) - **Enable test-live-options-drops-on-set on D500 devices**
* [#14208](https://github.com/IntelRealSense/librealsense/pull/14208) - **D555 excluded from D585S specific tests**
* [#14185](https://github.com/IntelRealSense/librealsense/pull/14185) by [@martindevans](https://github.com/martindevans) - **Fixed Resource Leak In Point Cloud Example**
* [#14096](https://github.com/IntelRealSense/librealsense/pull/14096) - **IQ basic color test**
* [#14097](https://github.com/IntelRealSense/librealsense/pull/14097) - **IQ basic depth test**
* [#14166](https://github.com/IntelRealSense/librealsense/pull/14166) - **basic calibration tests**
* [#14099](https://github.com/IntelRealSense/librealsense/pull/14099) - **Create test-texture-mapping.py**
* [#14203](https://github.com/IntelRealSense/librealsense/pull/14203) - **fix dds targets issue on debian**
* [#14177](https://github.com/IntelRealSense/librealsense/pull/14177) - **hkr merged to dev**
* [#14202](https://github.com/IntelRealSense/librealsense/pull/14202) - **Re-enable rec-play tests for D500**
* [#14196](https://github.com/IntelRealSense/librealsense/pull/14196) - **Fix extrinsics unit test for D436**
* [#14147](https://github.com/IntelRealSense/librealsense/pull/14147) by [@GYKrauss](https://github.com/GYKrauss) - **Update patch-realsense-ubuntu-lts.sh remote address**
* [#14194](https://github.com/IntelRealSense/librealsense/pull/14194) - **fix REST IMU streaming bug**
***

## Release 2.57.2 beta
Release Date: August 17, 2025

### API Changes - None.
For previous API changes, [see here](https://github.com/IntelRealSense/librealsense/wiki/API-Changes)

### What's new
- Feature - Auto HDR support
- Platform - Android14 support in lrs android wrapper example RS Camera - see PR #13910 
- Wrapper - Rest API Wrapper - see [here](https://github.com/IntelRealSense/librealsense/tree/development/wrappers/rest-api)

### Bug Fixes
Various bugs fixes - see below PR list.

### Known issues
- Check for updates button on the RealSense Viewer does not work

[Full Changelog in GitHub](https://github.com/IntelRealSense/librealsense/compare/v2.56.5...v2.57.2)


* [#14192](https://github.com/IntelRealSense/librealsense/pull/14192) - **Restrict Auto HDR to D45***
* [#14190](https://github.com/IntelRealSense/librealsense/pull/14190) - **Add manual on setting host MTU**
* [#14187](https://github.com/IntelRealSense/librealsense/pull/14187) - **Enhance depth test**
* [#14188](https://github.com/IntelRealSense/librealsense/pull/14188) - **Update Copyrights**
* [#14186](https://github.com/IntelRealSense/librealsense/pull/14186) - **Add bandwidth info message for DDS streams**
* [#14189](https://github.com/IntelRealSense/librealsense/pull/14189) - **Fix Python run_uv_map_calibration API**
* [#14173](https://github.com/IntelRealSense/librealsense/pull/14173) - **Fix access violation bug**
* [#14174](https://github.com/IntelRealSense/librealsense/pull/14174) - **Add eth_config unit test**
* [#14179](https://github.com/IntelRealSense/librealsense/pull/14179) - **fix hdr bug where no depth sensor exists**
* [#14161](https://github.com/IntelRealSense/librealsense/pull/14161) - **Create a test runner for the python rest-api wrapper**
* [#14157](https://github.com/IntelRealSense/librealsense/pull/14157) - **Update RealSense logo and links**
* [#14159](https://github.com/IntelRealSense/librealsense/pull/14159) - **Add Auto HDR support**
* [#14154](https://github.com/IntelRealSense/librealsense/pull/14154) - **Traffic shaping**
* [#14163](https://github.com/IntelRealSense/librealsense/pull/14163) - **Disable rec-play test for D555 until it will be stable**
* [#14143](https://github.com/IntelRealSense/librealsense/pull/14143) - **reload driver after recovery for d457**
* [#14140](https://github.com/IntelRealSense/librealsense/pull/14140) - **Enable playback tests for D555**
* [#14133](https://github.com/IntelRealSense/librealsense/pull/14133) - **Verify depth metrics roi size**
* [#14095](https://github.com/IntelRealSense/librealsense/pull/14095) - **Refactor backend-v4l::named_mutex**
* [#14104](https://github.com/IntelRealSense/librealsense/pull/14104) - **Add UI for HDR configurations**
* [#14128](https://github.com/IntelRealSense/librealsense/pull/14128) - **flash signed fw firsT**
* [#14115](https://github.com/IntelRealSense/librealsense/pull/14115) - **override toggle_advanced_mode to simulate connect/disconnect events**
* [#14120](https://github.com/IntelRealSense/librealsense/pull/14120) - **Rs fw update tool align to viewer flow**
* [#14119](https://github.com/IntelRealSense/librealsense/pull/14119) - **add check for empty ports**
* [#14117](https://github.com/IntelRealSense/librealsense/pull/14117) - **Support adding external Signed FW file path**
* [#14107](https://github.com/IntelRealSense/librealsense/pull/14107) - **I2c unsigned fw retry fix**
* [#13965](https://github.com/IntelRealSense/librealsense/pull/13965) - **add new Rest api wrapper**
* [#14106](https://github.com/IntelRealSense/librealsense/pull/14106) - **fw compatibility added for mipi devices**
* [#14098](https://github.com/IntelRealSense/librealsense/pull/14098) - **Compile pybackend2 library in librealsense**
* [#14069](https://github.com/IntelRealSense/librealsense/pull/14069) by [@JafarAbdi](https://github.com/JafarAbdi) - **Fix `udev-device-watcher` target's include directories for libudev**
* [#14072](https://github.com/IntelRealSense/librealsense/pull/14072) - **added auto-hdr unit test**
* [#14077](https://github.com/IntelRealSense/librealsense/pull/14077) - **Calibrations issue exposure warning to users only if AE is disabled**
* [#14075](https://github.com/IntelRealSense/librealsense/pull/14075) - **fix quick disconnection bug**
* [#14074](https://github.com/IntelRealSense/librealsense/pull/14074) - **Run GT calib at 30 FPS when possible**
* [#14057](https://github.com/IntelRealSense/librealsense/pull/14057) - **fix UI multi camera advanced mode**
* [#14056](https://github.com/IntelRealSense/librealsense/pull/14056) - **Enable tests for D555**
* [#14065](https://github.com/IntelRealSense/librealsense/pull/14065) - **stabilize got-playback-frames**
* [#14046](https://github.com/IntelRealSense/librealsense/pull/14046) - **Allow loading json for auto-hdr**
* [#14059](https://github.com/IntelRealSense/librealsense/pull/14059) - **Handle D400 calibrations that need advanced mode**
* [#14058](https://github.com/IntelRealSense/librealsense/pull/14058) - **Update Windows actions runner image**
* [#14048](https://github.com/IntelRealSense/librealsense/pull/14048) - **min fw updated**
* [#14045](https://github.com/IntelRealSense/librealsense/pull/14045) - **d457 syncer only acc to timestamp, as d455**
* [#14027](https://github.com/IntelRealSense/librealsense/pull/14027) - **Support FW logs dictionary per module**
* [#14037](https://github.com/IntelRealSense/librealsense/pull/14037) - **fixing calib handler param design**
* [#14044](https://github.com/IntelRealSense/librealsense/pull/14044) - **Fix GHA build by updating ros-tooling version**
* [#14036](https://github.com/IntelRealSense/librealsense/pull/14036) - **add option --retry to run-unit-tests**
* [#14032](https://github.com/IntelRealSense/librealsense/pull/14032) - **fix failure log print**
* [#14029](https://github.com/IntelRealSense/librealsense/pull/14029) - **remove check for port status on toggling ports**
* [#14021](https://github.com/IntelRealSense/librealsense/pull/14021) - **Change GLSL for processing to default off**
* [#14019](https://github.com/IntelRealSense/librealsense/pull/14019) - **add signal handling to libci**
* [#14015](https://github.com/IntelRealSense/librealsense/pull/14015) - **Add UYVY to YUYV conversion for D457**
* [#14010](https://github.com/IntelRealSense/librealsense/pull/14010) - **disable OCC when RGB streaming**
* [#14012](https://github.com/IntelRealSense/librealsense/pull/14012) - **remove age and gender from RealSense Viewer**
* [#13991](https://github.com/IntelRealSense/librealsense/pull/13991) - **Enable IMU STREAMING for RS-capture example**
* [#14004](https://github.com/IntelRealSense/librealsense/pull/14004) - **Add unit test for jpeg streaming**
* [#13997](https://github.com/IntelRealSense/librealsense/pull/13997) - **Adding test for be ts after frame ts**
* [#13998](https://github.com/IntelRealSense/librealsense/pull/13998) - **safe pipe use in hdr long test**
* [#14005](https://github.com/IntelRealSense/librealsense/pull/14005) - **libci-mipi-motion-test-relaxing**
* [#13994](https://github.com/IntelRealSense/librealsense/pull/13994) - **d421 added in patches for jetson 5.0.2**
* [#13999](https://github.com/IntelRealSense/librealsense/pull/13999) - **Handle DDS in/extrinsics data errors better**
* [#13995](https://github.com/IntelRealSense/librealsense/pull/13995) - **Viewer - update control from slider only, after dragging it**
* [#13985](https://github.com/IntelRealSense/librealsense/pull/13985) - **optimize RS-Viewer loading time**
* [#13988](https://github.com/IntelRealSense/librealsense/pull/13988) - **Adding python example for color/depth alignment using SW device**
* [#13987](https://github.com/IntelRealSense/librealsense/pull/13987) - **enable config for context on C#**
* [#13986](https://github.com/IntelRealSense/librealsense/pull/13986) - **Disable extrinsics-imu test for D555**
* [#13982](https://github.com/IntelRealSense/librealsense/pull/13982) - **Update GHA ROS build timeout**
* [#13978](https://github.com/IntelRealSense/librealsense/pull/13978) - **add multiple hubs support**
* [#13981](https://github.com/IntelRealSense/librealsense/pull/13981) - **disabling test until bug fixed**
* [#13979](https://github.com/IntelRealSense/librealsense/pull/13979) - **replaced using strncpy with snprintf**
* [#13970](https://github.com/IntelRealSense/librealsense/pull/13970) - **Added parameters and read-write/read-only changes**
* [#13941](https://github.com/IntelRealSense/librealsense/pull/13941) - **combine shared data function**
* [#13961](https://github.com/IntelRealSense/librealsense/pull/13961) - **giving time for the device to be discovered after burning fw**
* [#13967](https://github.com/IntelRealSense/librealsense/pull/13967) - **improve depth test**
* [#13968](https://github.com/IntelRealSense/librealsense/pull/13968) - **Handle DDS devices in Recovery Mode**
* [#13936](https://github.com/IntelRealSense/librealsense/pull/13936) - **add Calibration Data window for D555**
* [#13962](https://github.com/IntelRealSense/librealsense/pull/13962) - **Fix Coverity issue. Check dynamic cast success**
* [#13959](https://github.com/IntelRealSense/librealsense/pull/13959) - **Add exposure value check for on-chip calibration**
* [#13960](https://github.com/IntelRealSense/librealsense/pull/13960) - **Update rs-on-chip-calib example**
* [#13957](https://github.com/IntelRealSense/librealsense/pull/13957) - **Support receiving frames in ROS2 CompressedImage format**
* [#13937](https://github.com/IntelRealSense/librealsense/pull/13937) - **Add numpy-to-frame example**
* [#13940](https://github.com/IntelRealSense/librealsense/pull/13940) - **Rotate_depth_and_ir_frames_together**
* [#13910](https://github.com/IntelRealSense/librealsense/pull/13910) by [@nanotuxi](https://github.com/nanotuxi) - **Add android14 support in lrs android wrapper example RS Camera**
* [#13938](https://github.com/IntelRealSense/librealsense/pull/13938) - **remove 'host assistance' checkbox ==> RSDSO-20145**
* [#13934](https://github.com/IntelRealSense/librealsense/pull/13934) - **Remove unused flexible-reader/writer**
* [#13879](https://github.com/IntelRealSense/librealsense/pull/13879) - **Update maintainers list**
***

## Release 2.56.5
Release Date: July 28, 2025

### API Changes - None.
For previous API changes, [see here](https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2564)

### What's new
New Firmware version 5.17.0.10.


***

## Release 2.56.4
Release Date: July 20, 2025


### API Changes - [see here](https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2564)

### What's new
- Feature - D457 IR stream metadata support *Breaking Change - Require FW + MIPI driver upgrade*
- Feature - Vertical camera - Support rotating color frames 
- Feature - `rs-convert` 3d distance csv support
- Enhancement - Improve `query_devices` API time on high device count systems
- Enhancement - Support CMake version >= 4.0
- New SKUs support: D555 - The first Ethernet based POE camera (See version compatibility table below)
- New SKUs support: D421 - More info [here](https://realsenseai.com/stereo-depth-cameras/stereo-depth-camera-module-d421/)
- Support for D430 GMSL module  
- Wrapper - New support for Python version 3.13
- And many more..

### Bug Fixes
- D457 IMU Extrinsic explicitly handled and defined
- CUDA-enabled librealsense to fallback when running on host without CUDA-GPU
- Fix Self calibration python script got error for D455
- Fix D457 Y8i Left IR Image Corrupted 
- D405 can’t start color streaming as YUYV format under ROS2
- Wrong color VFOV in rs-enumerate-devices
- Depth MIPI metadata - correct order of HW timestamp and sensor timestamp
- D455 Fix Depth AE abnormal while RGB streaming is on
- Fix MIPI mode over exposure at outdoor sunlight
- Change D455 RO of 15fps to the RO of 30fps [Purple corner issue]
- D405 RGB image is dark if start Color streaming only
- wrong color VFOV in rs-enumerate-devices
- INVALID_HANDLE_VALUE or nullptr causes invalid entries in device list and infinite loop or crash
- Fix pipeline returns unsynchronized frame timestamps between depth and IR or RGB

[Full Changelog in GitHub](https://github.com/IntelRealSense/librealsense/compare/v2.56.3...v2.56.4)

* [#14076](https://github.com/IntelRealSense/librealsense/pull/14074) - **Run GT calib at 30 FPS when possible**
* [#14063](https://github.com/IntelRealSense/librealsense/pull/14063) - **Handle D400 calibrations that need advanced mode**
* [#14060](https://github.com/IntelRealSense/librealsense/pull/14060) - **Add UYVY to YUYV conversion for D457**
* [#14052](https://github.com/IntelRealSense/librealsense/pull/14052) - **fixing D457 IMU coordinates**
* [#14053](https://github.com/IntelRealSense/librealsense/pull/14053) - **d457 syncer only according to timestamp and not frame number**
* [#14024](https://github.com/IntelRealSense/librealsense/pull/14024) - **Change GLSL for processing to default off**
* [#14006](https://github.com/IntelRealSense/librealsense/pull/14006) - **d421 added in patches for jetson 5.0.2**
* [#13954](https://github.com/IntelRealSense/librealsense/pull/13954) - **Rotate depth and IR frames together**
* [#13926](https://github.com/IntelRealSense/librealsense/pull/13926) - **Update release branch version to 2.56.4**
* [#13925](https://github.com/IntelRealSense/librealsense/pull/13925) - **Prepare 2.56.4 release - merge development to 256**
* [#13921](https://github.com/IntelRealSense/librealsense/pull/13921) - **Enable UV mapping when rotating frames**
* [#13914](https://github.com/IntelRealSense/librealsense/pull/13914) - **Limit IMU Accelerometer 400 [HZ] for d400 devices only**
* [#13915](https://github.com/IntelRealSense/librealsense/pull/13915) - **reset_to_factory_calibration() implementation**
* [#13894](https://github.com/IntelRealSense/librealsense/pull/13894) - **CI improvement**
* [#13904](https://github.com/IntelRealSense/librealsense/pull/13904) - **Relax json setting enforcement**
* [#13906](https://github.com/IntelRealSense/librealsense/pull/13906) - **static analysis issue fix - check before dereference**
* [#13907](https://github.com/IntelRealSense/librealsense/pull/13907) - **fix env var missing error on Unifi**
* [#13893](https://github.com/IntelRealSense/librealsense/pull/13893) - **Blocking AE control in D457**
* [#13849](https://github.com/IntelRealSense/librealsense/pull/13849) - **added gyro and accel graph**
* [#13895](https://github.com/IntelRealSense/librealsense/pull/13895) - **add unifi switch to libci**
* [#13876](https://github.com/IntelRealSense/librealsense/pull/13876) - **Remove Xenial patches and scripts**
* [#13902](https://github.com/IntelRealSense/librealsense/pull/13902) - **Add rs-on-chip-calib example**
* [#13900](https://github.com/IntelRealSense/librealsense/pull/13900) - **readme update - supported products link**
* [#13889](https://github.com/IntelRealSense/librealsense/pull/13889) - **Unsigned fw for d430 + D457 recovery fix**
* [#13884](https://github.com/IntelRealSense/librealsense/pull/13884) - **Fix get_option_value for string options**
* [#13885](https://github.com/IntelRealSense/librealsense/pull/13885) - **Enable BMI085 and BMI088 Accel 400HZ**
* [#13858](https://github.com/IntelRealSense/librealsense/pull/13858) - **add ASAN to CI tests**
* [#13880](https://github.com/IntelRealSense/librealsense/pull/13880) - **implement WA for mipi y8i-to-y8y8**
* [#13877](https://github.com/IntelRealSense/librealsense/pull/13877) - **Update minimal FW version for MIPI devices to 5.16.8.0**
* [#13868](https://github.com/IntelRealSense/librealsense/pull/13868) - **MIPI Enabling IR metadata - [BREAKING CHANGE]**
* [#13860](https://github.com/IntelRealSense/librealsense/pull/13860) - **Fix LibCI error message on last retry only**
* [#13875](https://github.com/IntelRealSense/librealsense/pull/13875) - **Fix host assistance issue**
* [#13850](https://github.com/IntelRealSense/librealsense/pull/13850) - **removed composite_processing_block class**
* [#13851](https://github.com/IntelRealSense/librealsense/pull/13851) - **Fix LibCI failure counter**
* [#13842](https://github.com/IntelRealSense/librealsense/pull/13842) - **Relax pipeline-start-stop test**
* [#13840](https://github.com/IntelRealSense/librealsense/pull/13840) - **Better handling of DDS streams information**
* [#13843](https://github.com/IntelRealSense/librealsense/pull/13843) - **Add  --skip-regex parameter to LibCI for skipping tests**
* [#13834](https://github.com/IntelRealSense/librealsense/pull/13834) - **update python wrapper readme**
* [#13836](https://github.com/IntelRealSense/librealsense/pull/13836) - **fix Coverity issues**
* [#13827](https://github.com/IntelRealSense/librealsense/pull/13827) - **Add D555 IMU extrinsics info**
* [#13788](https://github.com/IntelRealSense/librealsense/pull/13788) - **ignore build number when checking for software update message**
* [#13817](https://github.com/IntelRealSense/librealsense/pull/13817) - **Issue test error message only on last retry**
* [#13722](https://github.com/IntelRealSense/librealsense/pull/13722) - **Replace setup.py with pyproject.toml**
* [#13772](https://github.com/IntelRealSense/librealsense/pull/13772) - **fixed memory leak + add Valgrind memleak GHA test**
* [#13803](https://github.com/IntelRealSense/librealsense/pull/13803) by [@RobbertProost](https://github.com/RobbertProost) - **[build] add option to use externally build LZ4 library**
* [#13808](https://github.com/IntelRealSense/librealsense/pull/13808) - **Fix incorrect IMU data in SDK**
* [#13783](https://github.com/IntelRealSense/librealsense/pull/13783) - **D430 gmsl rebased**
* [#13786](https://github.com/IntelRealSense/librealsense/pull/13786) - **add pixel temporal noise metric**
* [#13781](https://github.com/IntelRealSense/librealsense/pull/13781) - **Bmi088 imu**
* [#13777](https://github.com/IntelRealSense/librealsense/pull/13777) - **Add null check for ds before access**
* [#13776](https://github.com/IntelRealSense/librealsense/pull/13776) - **LibCI test results might be nested, i.e. not at start of line**
* [#13735](https://github.com/IntelRealSense/librealsense/pull/13735) - **add limitation on number of captures in DQT**
* [#13766](https://github.com/IntelRealSense/librealsense/pull/13766) - **Deprecate U20 on GHA and add latest**
* [#13761](https://github.com/IntelRealSense/librealsense/pull/13761) - **avoid FL calib when working with d421**
* [#13757](https://github.com/IntelRealSense/librealsense/pull/13757) - **frame ts md for mipi rgb fixed**
* [#13733](https://github.com/IntelRealSense/librealsense/pull/13733) - **Enable rotate frames for RGB camera**
* [#13747](https://github.com/IntelRealSense/librealsense/pull/13747) - **Adding python 3.13 support**
* [#13743](https://github.com/IntelRealSense/librealsense/pull/13743) - **deprecate Z16H format**
* [#13744](https://github.com/IntelRealSense/librealsense/pull/13744) - **Fix test-dds-control-reply**
* [#13742](https://github.com/IntelRealSense/librealsense/pull/13742) - **Move rotation filter extension to the end of enum**
* [#13737](https://github.com/IntelRealSense/librealsense/pull/13737) - **Disable FastDDS third party compilation warnings**
* [#13734](https://github.com/IntelRealSense/librealsense/pull/13734) - **Fix examples for D555**
* [#13738](https://github.com/IntelRealSense/librealsense/pull/13738) by [@mvieth](https://github.com/mvieth) - **Add missing include**
* [#13684](https://github.com/IntelRealSense/librealsense/pull/13684) - **enable dds devices on run-unit-tests**
* [#13703](https://github.com/IntelRealSense/librealsense/pull/13703) - **devices.py --list now shows DDS devices; fix error**
* [#13715](https://github.com/IntelRealSense/librealsense/pull/13715) - **adapter ROS2 node & parameters**
* [#13705](https://github.com/IntelRealSense/librealsense/pull/13705) - **add support to 3D format in rs-convert.**
* [#13704](https://github.com/IntelRealSense/librealsense/pull/13704) - **Misc dds & rsutils**
* [#13712](https://github.com/IntelRealSense/librealsense/pull/13712) - **Fix test-dds-librs-connections**
* [#13711](https://github.com/IntelRealSense/librealsense/pull/13711) - **GHA fail due to deprecate v3 version for checkout plugin**
* [#13701](https://github.com/IntelRealSense/librealsense/pull/13701) - **test got-playback-frames enabled for d457**
* [#13702](https://github.com/IntelRealSense/librealsense/pull/13702) - **align ROS2 topics with Humble IDLs**
* [#13700](https://github.com/IntelRealSense/librealsense/pull/13700) - **Add Y16 support to D555**
* [#13693](https://github.com/IntelRealSense/librealsense/pull/13693) - **replace SetTooltip with CustomTooltip**
* [#13691](https://github.com/IntelRealSense/librealsense/pull/13691) - **Fix D457 extrinsic**
* [#13674](https://github.com/IntelRealSense/librealsense/pull/13674) - **changes to support CUDA build w/o GPU in runtime**
* [#13683](https://github.com/IntelRealSense/librealsense/pull/13683) - **removed legacy network device**
* [#13690](https://github.com/IntelRealSense/librealsense/pull/13690) - **add 5 FPS with 10% threshold**
* [#13688](https://github.com/IntelRealSense/librealsense/pull/13688) - **test-dds-md-syncer remove checks for intervined callbacks**
* [#13682](https://github.com/IntelRealSense/librealsense/pull/13682) - **Stabilize DDS tests on LibCI**
* [#13642](https://github.com/IntelRealSense/librealsense/pull/13642) - **Workaround to support FLASH WRITE for MIPI camera D457**
* [#13681](https://github.com/IntelRealSense/librealsense/pull/13681) - **dds config tool repair**
* [#13679](https://github.com/IntelRealSense/librealsense/pull/13679) - **fix-playback-color-stream-format-using-DDS**
* [#13680](https://github.com/IntelRealSense/librealsense/pull/13680) - **avoid viewer exception when creating new device_model fails**
* [#13669](https://github.com/IntelRealSense/librealsense/pull/13669) - **add more info to logs in fps_helper**
* [#13677](https://github.com/IntelRealSense/librealsense/pull/13677) - **Enable DDS unit test retries in LibCI**
* [#13667](https://github.com/IntelRealSense/librealsense/pull/13667) - **replace ImGui::SetTooltip maintain the uniformity of the design of  with RsImGui::RsCustomTooltip**
* [#13658](https://github.com/IntelRealSense/librealsense/pull/13658) - **minimize frame skip so FPS will be calculated ASAP**
* [#13675](https://github.com/IntelRealSense/librealsense/pull/13675) - **Fix playback device**
* [#13628](https://github.com/IntelRealSense/librealsense/pull/13628) - **add dds config tool**
* [#13668](https://github.com/IntelRealSense/librealsense/pull/13668) - **change test-dds-adapter-depth to 25-31 FPS**
* [#13661](https://github.com/IntelRealSense/librealsense/pull/13661) - **update dds-sniffer with --topics, topic-info**
* [#13657](https://github.com/IntelRealSense/librealsense/pull/13657) - **Development improve performance on zero dist frames**
* [#13650](https://github.com/IntelRealSense/librealsense/pull/13650) - **Don't nullify on_frame_ready lambda**
* [#13654](https://github.com/IntelRealSense/librealsense/pull/13654) - **correcting syntax when checking if connection type is GMSL**
* [#13605](https://github.com/IntelRealSense/librealsense/pull/13605) - **add test-dds-adapter-depth test**
* [#13615](https://github.com/IntelRealSense/librealsense/pull/13615) - **improvements to avoids crashes**
* [#13649](https://github.com/IntelRealSense/librealsense/pull/13649) - **dds-config improvements with multiple devices**
* [#13612](https://github.com/IntelRealSense/librealsense/pull/13612) - **Api connection type**
* [#13625](https://github.com/IntelRealSense/librealsense/pull/13625) - **Set DDS processing block settings**
* [#13627](https://github.com/IntelRealSense/librealsense/pull/13627) - **fix abort in some cases with DDS**
* [#13619](https://github.com/IntelRealSense/librealsense/pull/13619) - **Set/get preset as bulk operation reduces operation time**
* [#13609](https://github.com/IntelRealSense/librealsense/pull/13609) - **add note about windows DDS popup message**
* [#13622](https://github.com/IntelRealSense/librealsense/pull/13622) - **Add rotation-filter to post-processing-filter doc**
* [#13595](https://github.com/IntelRealSense/librealsense/pull/13595) - **Update minimal FW version for sensor timestamp update**
* [#13616](https://github.com/IntelRealSense/librealsense/pull/13616) - **Fix termination issue cause by PR # 13614**
* [#13614](https://github.com/IntelRealSense/librealsense/pull/13614) - **backend-v4l2.cpp: Add missing `throw` keyword**
* [#13613](https://github.com/IntelRealSense/librealsense/pull/13613) - **optimize rs2_get_options_list with bulk operation**
* [#13392](https://github.com/IntelRealSense/librealsense/pull/13392) - **Update ImGui v1.90.9**
* [#13604](https://github.com/IntelRealSense/librealsense/pull/13604) - **Handle D457 IMU extrinsic case**
* [#13603](https://github.com/IntelRealSense/librealsense/pull/13603) - **Mark regular expression in python**
* [#13598](https://github.com/IntelRealSense/librealsense/pull/13598) - **fix width instead on height in dds-adapter**
* [#13597](https://github.com/IntelRealSense/librealsense/pull/13597) - **fix error 42 on unicode user names**
* [#13601](https://github.com/IntelRealSense/librealsense/pull/13601) - **Update D500 TC timeout**
* [#13590](https://github.com/IntelRealSense/librealsense/pull/13590) - **Fix Tare calibration very high accuracy bug**
* [#13580](https://github.com/IntelRealSense/librealsense/pull/13580) - **fix compute_progress() bug; improve D500 FW update output**
* [#13576](https://github.com/IntelRealSense/librealsense/pull/13576) - **improve FW-update progress callback frequency & debug**
* [#13575](https://github.com/IntelRealSense/librealsense/pull/13575) - **fix rs-convert bug with 2 IR streams**
* [#13574](https://github.com/IntelRealSense/librealsense/pull/13574) - **stabilize dds tests device-init and librs-device-properties**
* [#13568](https://github.com/IntelRealSense/librealsense/pull/13568) - **Update post-processing example readme**
* [#13558](https://github.com/IntelRealSense/librealsense/pull/13558) - **DQT prevent depth calibration profile streaming**
* [#13549](https://github.com/IntelRealSense/librealsense/pull/13549) - **Deprecate RS2_OPTION_SENSOR_MODE**
* [#13543](https://github.com/IntelRealSense/librealsense/pull/13543) - **Cherry Pick PR [#13533](https://github.com/IntelRealSense/librealsense/issues/13533) from OhadMeir: Fix python UCAL example**
* [#13538](https://github.com/IntelRealSense/librealsense/pull/13538) - **Fix AE ROI Issue: No Change Detected for D405**
* [#13537](https://github.com/IntelRealSense/librealsense/pull/13537) - **Explicitly include `<chrono>`**
* [#13524](https://github.com/IntelRealSense/librealsense/pull/13524) - **Reread calibration tables after applying OCC calibration**
* [#13533](https://github.com/IntelRealSense/librealsense/pull/13533) - **Fix python UCAL example, tare calibration also returns health data**
* [#13525](https://github.com/IntelRealSense/librealsense/pull/13525) - **add dds-config tool readme**
* [#13531](https://github.com/IntelRealSense/librealsense/pull/13531) - **Revert version bump back to 2.56.0**

### Known issues
* [USB2] - Failure to stream in USB2 mode with the following platforms due to the USB2 controller:
  * Intel Chipset: 300 Series/C240 PCH  Chip-ID: 9DED, A36D CPU: 8th/9th Core Mobile/Desktop.
  * Intel Chipset: 600 Series Chip-ID:51ED CPU: 12th Core Mobile.

***

## Release 2.56.3 beta
Release Date: December 3, 2025

### This is a Beta release with source code only, no Debian or PyPi collaterals were published.

### API Changes - [see here](https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2563-beta)


_Please note that API changes in beta versions may not be final._

### What's new
- Feature - Introducing a new depth & IR post processing (90/180/270 deg) frame rotation filter [GitHub #6023, RSDSO-19843]
- Platform - ARM NEON CPU acceleration support included in the SDK (align, pointcloud) [PR #13396 , #13389]
- Platform - Updated minimal Jetson version support to v5.0.2
- Wrapper - Updated minimal Python version support to v3.9 (Python 3.8 is EOL, can be built from source) [LRS-1154]
- Use Case - Water safety alert system using RealSense camera reference (in collaboration with the Technion™)

### Bug Fixes
- Refresh D457 device information after DFU [LRS-686]
- Added support for D457 recovery mode [RSDSO-19297]
- Optimize USB enumeration time [RSDSO-19750]
- Broken metadata timestamp in D457 GMSL mode [RSDSO-19754]
- D405 UV errors and intrinsic misalignment [RSDSO-17488, RSDSO-18999]

### Security
- Switched on security for compilation and linkage flags by default [PR #13378, LRS-1183]
- Windows Installer - Fix insecure folder permissions allows privilege escalation [PSIRT #PTK0005062]
- Windows Installer - Fix Time-Of-Check Time-Of-Use in Windows installer [PSIRT# PTK0004998]

[Full Changelog in GitHub](https://github.com/IntelRealSense/librealsense/compare/v2.56.2...v2.56.3)

* [#13543](https://github.com/IntelRealSense/librealsense/pull/13543) - **Fix python UCAL example**
* [#13526](https://github.com/IntelRealSense/librealsense/pull/13526) - **Fix static analysis issue**
* [#13431](https://github.com/IntelRealSense/librealsense/pull/13431) by [@aaron-evercoast](https://github.com/aaron-evercoast) - **Improve query_devices time on high device count systems**
* [#13499](https://github.com/IntelRealSense/librealsense/pull/13499) - **Add new post processing filter - rotation filter**
* [#13510](https://github.com/IntelRealSense/librealsense/pull/13510) - **remove response size limitation, and remove log prints for cmds with empty responses**
* [#13512](https://github.com/IntelRealSense/librealsense/pull/13512) - **Table read error fallback**
* [#13498](https://github.com/IntelRealSense/librealsense/pull/13498) - **Adding string to opcode mismatch error**
* [#13488](https://github.com/IntelRealSense/librealsense/pull/13488) - **Add retry for get_gvd command**
* [#13479](https://github.com/IntelRealSense/librealsense/pull/13479) - **Remove libusb leftovers**
* [#13476](https://github.com/IntelRealSense/librealsense/pull/13476) by [@bewuethr](https://github.com/bewuethr) - **Add step to change directory**
* [#13378](https://github.com/IntelRealSense/librealsense/pull/13378) - **Use Security Standard Compiler flags**
* [#13444](https://github.com/IntelRealSense/librealsense/pull/13444) - **make hwmon_response product line dependent**
* [#13473](https://github.com/IntelRealSense/librealsense/pull/13473) - **Fix width/height for resolution calculation**
* [#13469](https://github.com/IntelRealSense/librealsense/pull/13469) - **D457 timestamp handling back compatable**
* [#13466](https://github.com/IntelRealSense/librealsense/pull/13466) - **MIPI - Mapping AE attribute value in RGB metadata to 0 or 1**
* [#13465](https://github.com/IntelRealSense/librealsense/pull/13465) - **Hardcoding the AE control range of MIPI device**
* [#13315](https://github.com/IntelRealSense/librealsense/pull/13315) - **Sensor ts metadata mipi corrected**
* [#13418](https://github.com/IntelRealSense/librealsense/pull/13418) - **update libusb**
* [#13441](https://github.com/IntelRealSense/librealsense/pull/13441) - **pyrealdds.image_msg.data as a memoryview; fps.py --save-frames**
* [#13432](https://github.com/IntelRealSense/librealsense/pull/13432) - **update for Android Studio 2024.2.1**
* [#13389](https://github.com/IntelRealSense/librealsense/pull/13389) by [@fateshelled](https://github.com/fateshelled) - **Add ARM NEON intrinsics to align**
* [#13396](https://github.com/IntelRealSense/librealsense/pull/13396) by [@fateshelled](https://github.com/fateshelled) - **Add ARM NEON intrinsics to pointcloud**
* [#13408](https://github.com/IntelRealSense/librealsense/pull/13408) - **remove py3.8 support**
* [#13406](https://github.com/IntelRealSense/librealsense/pull/13406) by [@Arun-Prasad-V](https://github.com/Arun-Prasad-V) - **Bug fix: manual patch script for Noble**
* [#13385](https://github.com/IntelRealSense/librealsense/pull/13385) - **fix rs-fw-update wait for device to reconnect**
* [#13402](https://github.com/IntelRealSense/librealsense/pull/13402) - **Add a drowning detection example reference - Technion Industrial Course**
* [#13388](https://github.com/IntelRealSense/librealsense/pull/13388) - **add flush after printf on rs-fw-update**
* [#13369](https://github.com/IntelRealSense/librealsense/pull/13369) - **check sensors exist**
* [#13359](https://github.com/IntelRealSense/librealsense/pull/13359) - **Update installation_jetson.md**


***

## Release 2.56.2 beta
Release Date: October 16, 2024

### This is a Beta release with source code only, no Debian or PyPi collaterals were published.

### API Changes
**None** (for previous API changes see: [API changes](https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2561-beta))
<br>_Please note that API changes in beta versions may not be final._

### What's new
- Added manual kernel driver patches for Kernel 6.8 on Ubuntu Jammy and Noble
- Bug fixes (see full changelog)
- Introducing ARM NEON intrinsics to unpack_yuy2 (Community work) 
- Introducing RISCV build support (Community work) 

### Bug Fixes and Enhancements
[Full Changelog in GitHub](https://github.com/IntelRealSense/librealsense/compare/v2.56.1...v2.56.2)
* [#13354](https://github.com/IntelRealSense/librealsense/pull/13354) - fix filter BUG
* [#13353](https://github.com/IntelRealSense/librealsense/pull/13353) - fixing pipeline-start-stop failures after Coverity fix
* [#13270](https://github.com/IntelRealSense/librealsense/pull/13270) by @fateshelled: Add ARM NEON intrinsics to unpack_yuy2
* [#13348](https://github.com/IntelRealSense/librealsense/pull/13348) - fix viewer crash when exporting to PLY
* [#13347](https://github.com/IntelRealSense/librealsense/pull/13347) - some Coverity issues fixed
* [#13345](https://github.com/IntelRealSense/librealsense/pull/13345) - move common classes to common.h
* [#13332](https://github.com/IntelRealSense/librealsense/pull/13332) - removing unused calibration data types
* [#13316](https://github.com/IntelRealSense/librealsense/pull/13316) - update FW version display after FW update
* [#13328](https://github.com/IntelRealSense/librealsense/pull/13328) - set_option api checking range restored
* [#13327](https://github.com/IntelRealSense/librealsense/pull/13327) - new depth calibration returned when tc succeeds
* [#13236](https://github.com/IntelRealSense/librealsense/pull/13236) - manual patches for Kernel 6.8 Jammy and Noble
* [#13321](https://github.com/IntelRealSense/librealsense/pull/13321) - remove live-test build
* [#13322](https://github.com/IntelRealSense/librealsense/pull/13322) - depth symmetrization - using MRD command is stream pipe config table' 
* [#13292](https://github.com/IntelRealSense/librealsense/pull/13292) - remove legacy live tests
* [#13296](https://github.com/IntelRealSense/librealsense/pull/13296) - linux v4l2 backend - method get_devname_from_video_path efficiency improved
* [#13234](https://github.com/IntelRealSense/librealsense/pull/13234) - reading depth symmetrization from table using GET_HKR_CONFIG_TABLE instead of MRD added
* [#13267](https://github.com/IntelRealSense/librealsense/pull/13267) - std::move in DFU process
* [#13265](https://github.com/IntelRealSense/librealsense/pull/13265) - validate option setting for calibrations
* [#13258](https://github.com/IntelRealSense/librealsense/pull/13258) - handle small Coverity issues
* [#13255](https://github.com/IntelRealSense/librealsense/pull/13255) - hwm extended buffers: computing number of chunks corrected
* [#5689](https://github.com/IntelRealSense/librealsense/pull/5689) by @radfordi: Augment "Add Source" button with the number available
* [#13239](https://github.com/IntelRealSense/librealsense/pull/13239) - adjust previous hwm api to send_and_receive api
* [#13235](https://github.com/IntelRealSense/librealsense/pull/13235) - handle some Coverity warnings
* [#13218](https://github.com/IntelRealSense/librealsense/pull/13218) - make our own json dump available in Python
* [#13216](https://github.com/IntelRealSense/librealsense/pull/13216) - fix rs2_register_calibration_change_callback interface requirement
* [#13232](https://github.com/IntelRealSense/librealsense/pull/13232) - checking only depth sensor instead of _all sensors present_
* [#13224](https://github.com/IntelRealSense/librealsense/pull/13224) - add --debug and --sw-only to rs-depth-quality
* [#13220](https://github.com/IntelRealSense/librealsense/pull/13220) - update rs-fw-update readme & bug fix
* [#13219](https://github.com/IntelRealSense/librealsense/pull/13219) - viewer displays options in the order reported by librealsense
* [#13217](https://github.com/IntelRealSense/librealsense/pull/13217) - make dfu flow slower based on feedback
* [#13215](https://github.com/IntelRealSense/librealsense/pull/13215) - fix 'UNKNOWN in rs2_get_frame_data' error on activating IMU
* [#13211](https://github.com/IntelRealSense/librealsense/pull/13211) by @per1cycle: Add riscv build support
* [#13156](https://github.com/IntelRealSense/librealsense/pull/13156) - post processing filters tests ported from legacy live tests
* [#13205](https://github.com/IntelRealSense/librealsense/pull/13205) - commenting update_samples_base method
* [#13189](https://github.com/IntelRealSense/librealsense/pull/13189) - calib_engine - set_calibration_table removed from engine
* [#13202](https://github.com/IntelRealSense/librealsense/pull/13202) - fix #13185 & #13192: move inline bool operator to json.h
* [#12451](https://github.com/IntelRealSense/librealsense/pull/12451) - fix global timestamp mapping issue when HW timestamp is rollback to 0
* [#13192](https://github.com/IntelRealSense/librealsense/pull/13192) - fix PR #13185: json bool operator now same as json_ref' 
* [#13185](https://github.com/IntelRealSense/librealsense/pull/13185) - replace default json serializer to improve readability' 
* [#13177](https://github.com/IntelRealSense/librealsense/pull/13177) - update find_first_device_or_exit - hw reset test improve


***


## Release 2.56.1 beta
Release Date: August 20th, 2024

### This is a Beta release with source code only, no Debian or PyPi collaterals were published.

### API Changes
https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2561-beta
<br>Please note that API changes in beta versions may not be final.

### What's new
- SDK API documentation is back: [Doxygen](https://intelrealsense.github.io/librealsense/doxygen/annotated.html) (C++) and [Sphinx](https://intelrealsense.github.io/librealsense/python_docs/_generated/pyrealsense2.html) (Python)
- Introducing Python 3.12 support
- Introducing JetPack 6.0 support
- Introducing Ubuntu 24 support including documentation
- Introducing ROS2 Jazzy distro support on Ubuntu 24.04 with the ROS Wrapper
- Python 3.7, JetPack 4.0 and Ubuntu 18 are all EOL — We are pleased to continue offering the source code for developers who wish to download and compile it for their own use
- Added support for exposure dynamically update in runtime while HDR mode is enabled
- Bug fixes (see full changelog)
- New MIPI driver, version [1.0.1.23](https://github.com/IntelRealSense/realsense_mipi_platform_driver/releases/tag/v1.0.1.23)

### Bug Fixes and Enhancements
[Full Changelog in GitHub](https://github.com/IntelRealSense/librealsense/compare/v2.55.1...v2.56.1)

[#13215](https://github.com/IntelRealSense/librealsense/pull/13215) - fix 'UNKNOWN in rs2_get_frame_data' error on activating IMU


* [#13168](https://github.com/IntelRealSense/librealsense/pull/13168) - **change get/set calibration config API to work with JSONs directly**
* [#13166](https://github.com/IntelRealSense/librealsense/pull/13166) - **enable adding metadata of size bigger than 255 bytes in playback device**
* [#13157](https://github.com/IntelRealSense/librealsense/pull/13157) - **update docs for u24 additon and u18 removal**
* [#12335](https://github.com/IntelRealSense/librealsense/pull/12335) - **fps permutations test**
* [#13149](https://github.com/IntelRealSense/librealsense/pull/13149) - **fix typo in hello-realsense example readme.md**
* [#13135](https://github.com/IntelRealSense/librealsense/pull/13135) - **new API rs2_pipeline_set_device**
* [#13138](https://github.com/IntelRealSense/librealsense/pull/13138) - **display NVIDIA license before syncing source**
* [#13143](https://github.com/IntelRealSense/librealsense/pull/13143) - **remove py3.7; add py3.12**
* [#13128](https://github.com/IntelRealSense/librealsense/pull/13128) - **fix possible bug in D400 coefficient table handling**
* [#13097](https://github.com/IntelRealSense/librealsense/pull/13097) - **scripts: rs-enum: Use media link to get video nodes indexes.**
* [#13104](https://github.com/IntelRealSense/librealsense/pull/13104) - **rs-dds-config**
* [#13102](https://github.com/IntelRealSense/librealsense/pull/13102) - **Ubuntu24 RS-Viewer DFU fix - D457 w/a removed**
* [#13111](https://github.com/IntelRealSense/librealsense/pull/13111) - **SENSOR_TIMESTAMP metadata pulling fixed**
* [#13105](https://github.com/IntelRealSense/librealsense/pull/13105) - **update README regarding ros2-master**
* [#13106](https://github.com/IntelRealSense/librealsense/pull/13106) - **adding githubpages extension to sphinx**
* [#13091](https://github.com/IntelRealSense/librealsense/pull/13091) - **test added for checking sensor timestamp and frame timestamp metadata values**
* [#13084](https://github.com/IntelRealSense/librealsense/pull/13084) - **fix Y12i stream output in MIPI with CUDA case**
* [#13074](https://github.com/IntelRealSense/librealsense/pull/13074) - **viewer presets for dds devices**
* [#13089](https://github.com/IntelRealSense/librealsense/pull/13089) - **adjust dds dfu timeout based on image size**
* [#13092](https://github.com/IntelRealSense/librealsense/pull/13092) - **fix sphinx to publish python API documentation**
* [#13079](https://github.com/IntelRealSense/librealsense/pull/13079) by [@eufrizz](https://github.com/eufrizz) - **Install pyrealsense2 .so and __init__.py**
* [#13060](https://github.com/IntelRealSense/librealsense/pull/13060) - **fix D405 VGA resolution intrinsics calculation**
* [#13075](https://github.com/IntelRealSense/librealsense/pull/13075) - **reduce lease duration to 3sec and announcement period to 1.5**
* [#13076](https://github.com/IntelRealSense/librealsense/pull/13076) - **don't start backend if device-mask specifies sw-only**
* [#13078](https://github.com/IntelRealSense/librealsense/pull/13078) - **re-reduce PNG file size**
* [#13077](https://github.com/IntelRealSense/librealsense/pull/13077) - **remove warning about mismatching arguments**
* [#13073](https://github.com/IntelRealSense/librealsense/pull/13073) - **fix bad CMake syntax at OpenCV example**
* [#13005](https://github.com/IntelRealSense/librealsense/pull/13005) by [@acasalboni](https://github.com/acasalboni) - **align to width and height -1 to prevent SSEGV accessing out of range buffer**
* [#13055](https://github.com/IntelRealSense/librealsense/pull/13055) - **L4T UVC/HID manual patch for JP6**
* [#13056](https://github.com/IntelRealSense/librealsense/pull/13056) - **reduce gifs and images size**
* [#13051](https://github.com/IntelRealSense/librealsense/pull/13051) - **remove py3.7 from documentation**
* [#13054](https://github.com/IntelRealSense/librealsense/pull/13054) - **md parser for always enabled param added**
* [#13033](https://github.com/IntelRealSense/librealsense/pull/13033) - **d500 devices - hw ts from capture stats**
* [#13048](https://github.com/IntelRealSense/librealsense/pull/13048) - **fix 'format not a string literal' warning**
* [#13042](https://github.com/IntelRealSense/librealsense/pull/13042) - **add motion_frame::get_combined_motion_data() for DDS Motion streams**
* [#13045](https://github.com/IntelRealSense/librealsense/pull/13045) - **Update NVIDIA docs**  (Fix NVIDIA B & TM [Aligned to development doc])
* [#13012](https://github.com/IntelRealSense/librealsense/pull/13012) - **Update NVIDIA docs**
* [#13041](https://github.com/IntelRealSense/librealsense/pull/13041) - **fix DDS motion stream to show gyro/accel components**
* [#12251](https://github.com/IntelRealSense/librealsense/pull/12251) by [@hsuys](https://github.com/hsuys) - **Fix D457 RGB sensor exposure control range issue**
* [#13031](https://github.com/IntelRealSense/librealsense/pull/13031) - **Support calibration config conversion from/to JSON string**
* [#13029](https://github.com/IntelRealSense/librealsense/pull/13029) - **D500 calib config to dev**
* [#13024](https://github.com/IntelRealSense/librealsense/pull/13024) - **enable github actions for U24**
* [#13022](https://github.com/IntelRealSense/librealsense/pull/13022) - **enabling ROS2 CI for Jazzy distro**
* [#13007](https://github.com/IntelRealSense/librealsense/pull/13007) - **update libcurl  version to latest version 8.8.0**  (Verified on Win + Linux)
* [#13004](https://github.com/IntelRealSense/librealsense/pull/13004) - **rs2_get_video_stream_intrinsics no longer reports errors if no intrinsics available**
* [#12944](https://github.com/IntelRealSense/librealsense/pull/12944) - **DFU for D457 recovery device**
* [#12982](https://github.com/IntelRealSense/librealsense/pull/12982) - **handle Accel correctly in D500**
* [#12993](https://github.com/IntelRealSense/librealsense/pull/12993) - **add more badges to readme.md**
* [#12983](https://github.com/IntelRealSense/librealsense/pull/12983) - **enable dds flow controller**
* [#12972](https://github.com/IntelRealSense/librealsense/pull/12972) - **fix MacOS M processor compilation + GHA upgrade to use  macos 14 + update upload tool version**
* [#12965](https://github.com/IntelRealSense/librealsense/pull/12965) - **IMU frame index jump in D457-Mipi**
* [#12978](https://github.com/IntelRealSense/librealsense/pull/12978) - **LRS_LOG_LEVEL fix; enable log optimizations**
* [#12971](https://github.com/IntelRealSense/librealsense/pull/12971) - **add new line for error comments in fw-update.cpp**
* [#12964](https://github.com/IntelRealSense/librealsense/pull/12964) - **D500 DFU - cleaning d400 fw update code, avoid stucking of d500 w/o dfu monitoring**
* [#12963](https://github.com/IntelRealSense/librealsense/pull/12963) - **fix json clone for a build directory that contain spaces**
* [#12950](https://github.com/IntelRealSense/librealsense/pull/12950) - **fix typo in rs-fw-update.cpp**
* [#12948](https://github.com/IntelRealSense/librealsense/pull/12948) - **add D500 DFU progress monitoring support**
* [#12956](https://github.com/IntelRealSense/librealsense/pull/12956) - **make json clone more robust**
* [#12961](https://github.com/IntelRealSense/librealsense/pull/12961) - **FastDDS v2.11.2 -> 2.10.4 (LTS)**
* [#12959](https://github.com/IntelRealSense/librealsense/pull/12959) - **Jammy compilation fix with log macro changes**
* [#12949](https://github.com/IntelRealSense/librealsense/pull/12949) - **revert usage of power_for_duration (PR [#12897](https://github.com/IntelRealSense/librealsense/issues/12897)) because of CI failures**
* [#12943](https://github.com/IntelRealSense/librealsense/pull/12943) - **add NO_TLS for FastDDS builds to disable SSL linkage**
* [#12935](https://github.com/IntelRealSense/librealsense/pull/12935) - **refer to students game example**
* [#12862](https://github.com/IntelRealSense/librealsense/pull/12862) by [@rurban](https://github.com/rurban) - **C++ 17 fixes**
* [#12933](https://github.com/IntelRealSense/librealsense/pull/12933) - **fix PR [#12911](https://github.com/IntelRealSense/librealsense/issues/12911): sleep if test is not live**
* [#12924](https://github.com/IntelRealSense/librealsense/pull/12924) - **flow controller & misc**
* [#12911](https://github.com/IntelRealSense/librealsense/pull/12911) - **LibCI - Recycle device on retry**
* [#12927](https://github.com/IntelRealSense/librealsense/pull/12927) - **update minimal CMake version 3.8**
* [#11997](https://github.com/IntelRealSense/librealsense/pull/11997) by [@autoantwort](https://github.com/autoantwort) - **simplify C++ version check**
* [#12914](https://github.com/IntelRealSense/librealsense/pull/12914) - **force shallow clone of json 3-rd party**
* [#12908](https://github.com/IntelRealSense/librealsense/pull/12908) - **enable runtime exposure update in HDR mode**
* [#12905](https://github.com/IntelRealSense/librealsense/pull/12905) - **add uvcvideo version update patch**
* [#12897](https://github.com/IntelRealSense/librealsense/pull/12897) - **power on UVC sensors after creation to speed initialization**
* [#12899](https://github.com/IntelRealSense/librealsense/pull/12899) - **reverse order of sensor open in pipeline API**
* [#12879](https://github.com/IntelRealSense/librealsense/pull/12879) - **DDS metadata: add ToA, backend, actual FPS, raw-frame-size**
* [#12891](https://github.com/IntelRealSense/librealsense/pull/12891) - **improvements added**
* [#12874](https://github.com/IntelRealSense/librealsense/pull/12874) by [@kadiredd](https://github.com/kadiredd) - **added param test_dir in run-unit-tests.py**
* [#12857](https://github.com/IntelRealSense/librealsense/pull/12857) - **RSUSB support for imu sensitivity**
* [#12854](https://github.com/IntelRealSense/librealsense/pull/12854) - **unit-tests: check hwmc error codes**
* [#12856](https://github.com/IntelRealSense/librealsense/pull/12856) - **d500 auto calib added for future capabilities**
* [#12853](https://github.com/IntelRealSense/librealsense/pull/12853) - **fix FW logs bug**
* [#12848](https://github.com/IntelRealSense/librealsense/pull/12848) - **fix settings in rs2_create_context**
* [#12789](https://github.com/IntelRealSense/librealsense/pull/12789) - **size & CRC in dfu-start; topic-reader stop-from-callback crashes**
* [#12819](https://github.com/IntelRealSense/librealsense/pull/12819) - **DDS ROI support; enums in adapter**
* [#12842](https://github.com/IntelRealSense/librealsense/pull/12842) by [@GAFMONKEY](https://github.com/GAFMONKEY) - **update readme.md**
* [#12836](https://github.com/IntelRealSense/librealsense/pull/12836) - **update tests to use default profiles**
* [#12835](https://github.com/IntelRealSense/librealsense/pull/12835) - **update fw_logs in Python wrapper + few fixes**
* [#12741](https://github.com/IntelRealSense/librealsense/pull/12741) - **D500 temperatures as xu**
* [#12822](https://github.com/IntelRealSense/librealsense/pull/12822) - **fix legacy intrinsics serialization and uppercase 'OK' status**
* [#12821](https://github.com/IntelRealSense/librealsense/pull/12821) - **static-analysis fix - Check dynamic cast**
* [#12788](https://github.com/IntelRealSense/librealsense/pull/12788) - **DDS scaled intrinsics**
* [#12813](https://github.com/IntelRealSense/librealsense/pull/12813) - **power line freq option with hardcoded range**
* [#12817](https://github.com/IntelRealSense/librealsense/pull/12817) - **fix wrong gyro values**
* [#12688](https://github.com/IntelRealSense/librealsense/pull/12688) - **support FW logs extended format**
* [#12778](https://github.com/IntelRealSense/librealsense/pull/12778) - **tests: rs-enumerate-devices**
* [#12791](https://github.com/IntelRealSense/librealsense/pull/12791) - **non-float option fixes**
* [#12799](https://github.com/IntelRealSense/librealsense/pull/12799) - **L4T: uvc patch fix localversion**
* [#12798](https://github.com/IntelRealSense/librealsense/pull/12798) - **fix IMU back compat for D457**
* [#12797](https://github.com/IntelRealSense/librealsense/pull/12797) - **IMU sensitivity fixes**
* [#12765](https://github.com/IntelRealSense/librealsense/pull/12765) - **[Colorizer] Check Depth stream format supported or not before process**
* [#12785](https://github.com/IntelRealSense/librealsense/pull/12785) - **relax enumeration logs**
* [#12775](https://github.com/IntelRealSense/librealsense/pull/12775) - **reduce compilation warnings to 0**
* [#12777](https://github.com/IntelRealSense/librealsense/pull/12777) - **Viewer add control to number of logs kept**
* [#12776](https://github.com/IntelRealSense/librealsense/pull/12776) - **correct glsl debug issue**
* [#12772](https://github.com/IntelRealSense/librealsense/pull/12772) - **fix configuration bug in DQT**
* [#12767](https://github.com/IntelRealSense/librealsense/pull/12767) - **fix skipped options showing in viewer**
* [#12760](https://github.com/IntelRealSense/librealsense/pull/12760) - **add byte manipulation functions**
* [#12764](https://github.com/IntelRealSense/librealsense/pull/12764) - **Jetson Orin JP6 install**
* [#12559](https://github.com/IntelRealSense/librealsense/pull/12559) - **set top level permissions to GHA**
* [#12505](https://github.com/IntelRealSense/librealsense/pull/12505) - **update jetson installation doc**  (Update spell errors)
* [#12497](https://github.com/IntelRealSense/librealsense/pull/12497) - **add new supported kernel versions to docs**
* [#12409](https://github.com/IntelRealSense/librealsense/pull/12409) - **fix compile error: ‘uint64_t’ does not name a type**
* [#12309](https://github.com/IntelRealSense/librealsense/pull/12309) by [@oceanusxiv](https://github.com/oceanusxiv) - **fix pybind arg names**


## Release 2.55.1
Release Date: April 21st, 2024

### API Changes
https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2551

### What's new
- Support for variable IMU gyro sensitivity on D455 and D435i
- MIPI
    - Enumerate video nodes by name (JP5 and newer)
    - `rs-fw-update` can update multiple devices
    - Please **reinstall** udev rules and [driver](https://github.com/IntelRealSense/realsense_mipi_platform_driver/releases/tag/v1.0.1.21) (v1.0.1.21)
- Linux compilation collaterals are now placed inside `build/<configuration>` directory (e.g., `build/Release`) rather than spread out in different project directories (like in Windows)
- And additional static compilation target `realsense2-all` is now available, packaging all other dependent libraries for ease of use
- L515 and SR300 Support:
    - The last validated release supporting the cameras is: [Version 2.50.0](https://github.com/IntelRealSense/librealsense/releases/tag/v2.50.0)
    - SDK version [2.54.2](https://github.com/IntelRealSense/librealsense/releases/tag/v2.54.2) supported the cameras but was not validated
    - For more information please refer to https://www.intelrealsense.com/message-to-customers/
- Support for Linux kernels 5.19, 6.2, 6.5
    - Please make sure DKMS is up-to-date
- Support for JetPack 6.0
- Added periodic options-changed notifications (see API changes)
- Auto Exposure Limit & Auto Gain Limit options are back and now available across the D400 line except D415 and D457
- Added SSE optimizations for alignment processing
- Metadata scrollbar in the Viewer
- New rs-enumerate-devices format for better readability

### New FW version **[5.16.0.1](https://dev.intelrealsense.com/docs/firmware-releases)**
- IMU values are now reported by the camera as 32 bits, for both accelerometer and gyroscope values
- This API change will **NOT** work with older versions of LibRealSense!
    | IMU       | SDK >= 5.16                                                                                         | SDK < 5.16                                   |
    | --------- | --------------------------------------------------------------------------------------------------- | -------------------------------------------- |
    | FW = 5.16 | D455/D435i: **dynamic** gyro sensitivity.<br>All other SKUs: static gyro sensitivity (1000 Deg/Sec) | **Corrupted** IMU values (both Gyro + Accel) |
    | FW < 5.16 | Static gyro sensitivity (1000 Deg/Sec)                                                              | Static gyro sensitivity (1000 Deg/Sec)       |


### Bug Fixes and Enhancements
* [#12849](https://github.com/IntelRealSense/librealsense/pull/12849) - **Fix settings in rs2_create_context**
* [#12801](https://github.com/IntelRealSense/librealsense/pull/12801) - **Cherry Pick PR [#12799](https://github.com/IntelRealSense/librealsense/issues/12799) l4t: uvc patch fix localversion**
* [#12800](https://github.com/IntelRealSense/librealsense/pull/12800) - **Cherry Pick PR[#12797](https://github.com/IntelRealSense/librealsense/issues/12797) & PR[#12798](https://github.com/IntelRealSense/librealsense/issues/12798)**: IMU fixes
* [#12773](https://github.com/IntelRealSense/librealsense/pull/12773) - **Cherry Pick PR [#12772](https://github.com/IntelRealSense/librealsense/issues/12772): fix configuration bug in DQT**
* [#12769](https://github.com/IntelRealSense/librealsense/pull/12769) - **Cherry Pick PR [#12764](https://github.com/IntelRealSense/librealsense/issues/12764): Jetson Orin JP6 install**
* [#12768](https://github.com/IntelRealSense/librealsense/pull/12768) - **fix skipped options showing in viewer**
* [#12715](https://github.com/IntelRealSense/librealsense/pull/12715) - **Gyro sensitivity feature**
* [#12759](https://github.com/IntelRealSense/librealsense/pull/12759) - **More Coverity issues handled**
* [#12754](https://github.com/IntelRealSense/librealsense/pull/12754) - **fix coverity issues**
* [#12753](https://github.com/IntelRealSense/librealsense/pull/12753) - **fix slowness with viewer get_option_values() while drawing**
* [#12731](https://github.com/IntelRealSense/librealsense/pull/12731) - **WTLAL move to rsutils** (float3 etc.)
* [#12748](https://github.com/IntelRealSense/librealsense/pull/12748) - **Fix some Coverity issues**
* [#12745](https://github.com/IntelRealSense/librealsense/pull/12745) - **Revert PR [#12671](https://github.com/IntelRealSense/librealsense/issues/12671): Viewer Accel and Gyro dashboards**
* [#12743](https://github.com/IntelRealSense/librealsense/pull/12743) - **backend: mipi dfu default nodes**
* [#12670](https://github.com/IntelRealSense/librealsense/pull/12670) - **hdr test converted to python and stabilized**
* [#12671](https://github.com/IntelRealSense/librealsense/pull/12671) - **[Viewer] Accel & Gyro dashboards**
* [#12722](https://github.com/IntelRealSense/librealsense/pull/12722) - **Auto removed from popup**
* [#12728](https://github.com/IntelRealSense/librealsense/pull/12728) - **Add start_offset param to md parser with crc**
* [#12687](https://github.com/IntelRealSense/librealsense/pull/12687) by [@m-mead](https://github.com/m-mead) - **Replace calls to cudaDeviceSynchronize with calls to only synchronize the default CUDA stream**
* [#12720](https://github.com/IntelRealSense/librealsense/pull/12720) - **rs-enum: fix non-zero video index**
* [#12708](https://github.com/IntelRealSense/librealsense/pull/12708) - **Options: add BOOLEAN & ENUM; rs2_set_option_value**
* [#12714](https://github.com/IntelRealSense/librealsense/pull/12714) - **backend-v4l2: relax mipi filter**
* [#12692](https://github.com/IntelRealSense/librealsense/pull/12692) - **Support strobe HMC (Emitter Always On) **
* [#12661](https://github.com/IntelRealSense/librealsense/pull/12661) - **operator<() on rs2::device uses fw-update-id & check-fw-compat works on recovery devices, too**
* [#12696](https://github.com/IntelRealSense/librealsense/pull/12696) - **V4l2 mipi compatible**
* [#12693](https://github.com/IntelRealSense/librealsense/pull/12693) - **Extended HWM - Reuse first chunk**  (Continue [#12682](https://github.com/IntelRealSense/librealsense/issues/12682))
* [#12683](https://github.com/IntelRealSense/librealsense/pull/12683) - **Small fixes**
* [#12682](https://github.com/IntelRealSense/librealsense/pull/12682) - **Extended HWM commands refactor**
* [#12678](https://github.com/IntelRealSense/librealsense/pull/12678) - **Remove HDR option**
* [#12679](https://github.com/IntelRealSense/librealsense/pull/12679) - **Minimal CMake GHA test fix**
* [#12589](https://github.com/IntelRealSense/librealsense/pull/12589) - **Create fps_helper.py**
* [#12668](https://github.com/IntelRealSense/librealsense/pull/12668) - **Add more logs to MD alive test to debug failures**
* [#12662](https://github.com/IntelRealSense/librealsense/pull/12662) - **Stabilize LibCI with retries until we fix the issues/tests**
* [#12650](https://github.com/IntelRealSense/librealsense/pull/12650) - **Viewer relies on options-changed notifications, and gets ranges even for r/o options**
* [#12637](https://github.com/IntelRealSense/librealsense/pull/12637) - **[LRS-GL] Support for Align-SSE**
* [#12658](https://github.com/IntelRealSense/librealsense/pull/12658) - **Add Auto Exp/Gain Limit feature to all D400 except D415+D457**
* [#12652](https://github.com/IntelRealSense/librealsense/pull/12652) - **FW logs parser should parse :f format as 32 bit float**
* [#12655](https://github.com/IntelRealSense/librealsense/pull/12655) - **Link to libusb repo in Copyright fixed**
* [#12651](https://github.com/IntelRealSense/librealsense/pull/12651) - **Add and update SW release compliance files to the repo**
* [#12646](https://github.com/IntelRealSense/librealsense/pull/12646) - **reverse py_to_json integer types to match json::parse()**
* [#12649](https://github.com/IntelRealSense/librealsense/pull/12649) - **doc: support linux kernel 6.5**
* [#12642](https://github.com/IntelRealSense/librealsense/pull/12642) - **Enhance metadata test to verify sensor timestamp**
* [#12636](https://github.com/IntelRealSense/librealsense/pull/12636) - **[Viewer] Option Open/Close dashboard area**
- [#12616](https://github.com/IntelRealSense/librealsense/pull/12616) - **option-value and usage in options-changed notifications**
* [#12640](https://github.com/IntelRealSense/librealsense/pull/12640) - **[Coverity] Fix Major issues**
* [#12624](https://github.com/IntelRealSense/librealsense/pull/12624) - **Remove SKU D465**
* [#12588](https://github.com/IntelRealSense/librealsense/pull/12588) - **[Viewer] Font size slider**
* [#12628](https://github.com/IntelRealSense/librealsense/pull/12628) - **Add error handling mechanism**
* [#12582](https://github.com/IntelRealSense/librealsense/pull/12582) - **Auto Exp/Gain Limit feature addition to D435i and D455**
* [#12602](https://github.com/IntelRealSense/librealsense/pull/12602) - **RealSense kernel 6.5 support.**
* [#12622](https://github.com/IntelRealSense/librealsense/pull/12622) - **Remove memcpy in HWM & command_transfer**
* [#12621](https://github.com/IntelRealSense/librealsense/pull/12621) - **Enable blocking advanced mode setting**
* [#12618](https://github.com/IntelRealSense/librealsense/pull/12618) - **Fix intrinsics calculation for non BROWN models**
* [#12561](https://github.com/IntelRealSense/librealsense/pull/12561) - **run-unit-tests on ykush devices**
* [#12615](https://github.com/IntelRealSense/librealsense/pull/12615) - **fix json = json_ref**
* [#12610](https://github.com/IntelRealSense/librealsense/pull/12610) - **remove "frame counter reset" log when enabling HID stream**
* [#12600](https://github.com/IntelRealSense/librealsense/pull/12600) - **Fix gated_by_value_option**
* [#12585](https://github.com/IntelRealSense/librealsense/pull/12585) - **Separate frame archives for IR streams**
* [#12556](https://github.com/IntelRealSense/librealsense/pull/12556) - **Fixing Depth Alignment in Align-GL processing block**
* [#12576](https://github.com/IntelRealSense/librealsense/pull/12576) - **[PointCloud] Set texel intrinsic & extrinsic based on selected stream**
* [#12575](https://github.com/IntelRealSense/librealsense/pull/12575) - **Frame queue per stream, not per extension**
* [#12573](https://github.com/IntelRealSense/librealsense/pull/12573) - **Improve test.remote reliability**
* [#12567](https://github.com/IntelRealSense/librealsense/pull/12567) - **fix setting initialization in rs-terminal and -enumerate-devices**
* [#12563](https://github.com/IntelRealSense/librealsense/pull/12563) - **Align t2ff KPI to validation KPI**
* [#12555](https://github.com/IntelRealSense/librealsense/pull/12555) - **rsutils::json cleanup & usage**
* [#12558](https://github.com/IntelRealSense/librealsense/pull/12558) - **Set top level permissions to GHA**
* [#12557](https://github.com/IntelRealSense/librealsense/pull/12557) - **config: udev rules for d555e dfu device.**
* [#12554](https://github.com/IntelRealSense/librealsense/pull/12554) - **Fix exceptions when closing the viewer**
* [#12418](https://github.com/IntelRealSense/librealsense/pull/12418) - **scripts: patch: modify Interface Protocol to UVC1.5 for 5e**
* [#12550](https://github.com/IntelRealSense/librealsense/pull/12550) - **helper functions for formatted values on metadata**
* [#12483](https://github.com/IntelRealSense/librealsense/pull/12483) - **Can update options automatically, get notification of value change**
* [#12541](https://github.com/IntelRealSense/librealsense/pull/12541) - **Unit test for end recording and start streaming again bug fix**
* [#12544](https://github.com/IntelRealSense/librealsense/pull/12544) - **test-live-frames-fps time to measure based on fps**
* [#12549](https://github.com/IntelRealSense/librealsense/pull/12549) - **Add logs to unstable unit test - rgb options metadata consistency**
* [#12540](https://github.com/IntelRealSense/librealsense/pull/12540) - **Add rs2::device type/desc APIs; dynamic col width in rs-enum; json fixes**
* [#12517](https://github.com/IntelRealSense/librealsense/pull/12517) - **Add is_xxx() to json::nested; empty string in context config**
* [#12535](https://github.com/IntelRealSense/librealsense/pull/12535) - **Disable test projection-from-recording from running**
* [#12534](https://github.com/IntelRealSense/librealsense/pull/12534) - **End recording and start streaming again - bug fix **
* [#12533](https://github.com/IntelRealSense/librealsense/pull/12533) - **Fix Coverity issues**
* [#12531](https://github.com/IntelRealSense/librealsense/pull/12531) - **Coverity development validation**
* [#12528](https://github.com/IntelRealSense/librealsense/pull/12528) - **Don't get GVD a second time to check if camera is locked**
* [#12489](https://github.com/IntelRealSense/librealsense/pull/12489) - **Coverity - Uncaught exception**
* [#12424](https://github.com/IntelRealSense/librealsense/pull/12424) - **Added class md_attribute_parser_with_crc to dev**
* [#12511](https://github.com/IntelRealSense/librealsense/pull/12511) - **Fix: Should not set HDR when it is already enabled**
* [#12513](https://github.com/IntelRealSense/librealsense/pull/12513) - **Refactor the logic and hover text for disabling the record button**
* [#12508](https://github.com/IntelRealSense/librealsense/pull/12508) - **backlight compensation is for D400 only**
* [#12481](https://github.com/IntelRealSense/librealsense/pull/12481) - **Test projection from recording**
* [#12507](https://github.com/IntelRealSense/librealsense/pull/12507) - **Move ASIC & Projector temperatures to D400 only**
* [#12482](https://github.com/IntelRealSense/librealsense/pull/12482) - **small refactor for synthetic source**
* [#12498](https://github.com/IntelRealSense/librealsense/pull/12498) - **context construction using make()**
* [#12506](https://github.com/IntelRealSense/librealsense/pull/12506) - **Spell fix on Jetson docs**
* [#12484](https://github.com/IntelRealSense/librealsense/pull/12484) - **Make uvc sensor weak pointer**
* [#12502](https://github.com/IntelRealSense/librealsense/pull/12502) - **udev: fix warnings reported by udevadm verify**
* [#12494](https://github.com/IntelRealSense/librealsense/pull/12494) - **Add kernels 5.19 and 6.2 to docs**
* [#12491](https://github.com/IntelRealSense/librealsense/pull/12491) - **fix auto return value error grrrrr**
* [#12485](https://github.com/IntelRealSense/librealsense/pull/12485) - **[Coverity] Fix an unchecked dynamic cast issues**
* [#12467](https://github.com/IntelRealSense/librealsense/pull/12467) - **Separate base- from rsutils executable-name; clarify check_false output in unit-tests**
* [#12480](https://github.com/IntelRealSense/librealsense/pull/12480) - **depth test - removing comparison to no laser**
* [#12475](https://github.com/IntelRealSense/librealsense/pull/12475) - **Set FW version**
* [#12473](https://github.com/IntelRealSense/librealsense/pull/12473) - **moving test-image-quality & renaming to test-depth**
* [#12472](https://github.com/IntelRealSense/librealsense/pull/12472) - **making terminal only functional when focused**
* [#12466](https://github.com/IntelRealSense/librealsense/pull/12466) - **Add A module (disparity modulation) set and get test**
* [#12471](https://github.com/IntelRealSense/librealsense/pull/12471) - **add run-unit-tests --device <>**
* [#12468](https://github.com/IntelRealSense/librealsense/pull/12468) - **Update rsutils::version from 1-2-1-4 bytes to 2-2-2-2**
* [#12449](https://github.com/IntelRealSense/librealsense/pull/12449) - **image quality test - adding comparison to laser off if depth is detected**
* [#12465](https://github.com/IntelRealSense/librealsense/pull/12465) - **Fix Coverity issues**
* [#12448](https://github.com/IntelRealSense/librealsense/pull/12448) - **Participant & liveliness QoS overrides**
* [#12463](https://github.com/IntelRealSense/librealsense/pull/12463) - **move extract_firmware_version_string() into ds::, d400-private**
* [#12458](https://github.com/IntelRealSense/librealsense/pull/12458) - **Test point-cloud-from-recordng**
* [#12456](https://github.com/IntelRealSense/librealsense/pull/12456) - **WA for pipeline start stop until fixed**
* [#12425](https://github.com/IntelRealSense/librealsense/pull/12425) - **abstract post-processing filter factory via context**
* [#12402](https://github.com/IntelRealSense/librealsense/pull/12402) - **port tests post-processing-from bag to python**
* [#12426](https://github.com/IntelRealSense/librealsense/pull/12426) - **Acroname reset on libCI start**
* [#12447](https://github.com/IntelRealSense/librealsense/pull/12447) - **Fix visual preset depth settings**  (Colored IR settings were set along with RGB settings, moved to be with depth module settings.)
* [#12446](https://github.com/IntelRealSense/librealsense/pull/12446) - **Handle mutex and CV properly on recovery DFU**
* [#12384](https://github.com/IntelRealSense/librealsense/pull/12384) - **Add feature-interface to sensors**
* [#12432](https://github.com/IntelRealSense/librealsense/pull/12432) - **derive context settings from configuration file**
* [#12439](https://github.com/IntelRealSense/librealsense/pull/12439) - **update dds options documentation**
* [#12441](https://github.com/IntelRealSense/librealsense/pull/12441) - **use non-copying get_to() in rsutils::json::get_ex()**
* [#12427](https://github.com/IntelRealSense/librealsense/pull/12427) - **Fix Coverity issues**
* [#12417](https://github.com/IntelRealSense/librealsense/pull/12417) - **move special_folder into rsutils**
* [#12428](https://github.com/IntelRealSense/librealsense/pull/12428) - **move to non-virtual register_color_processing_blocks()**
* [#12423](https://github.com/IntelRealSense/librealsense/pull/12423) - **make sure depth frame is used in hdr check to avoid merged frame from…**
* [#12360](https://github.com/IntelRealSense/librealsense/pull/12360) - **Updating minimal_cmake_version test**
* [#12410](https://github.com/IntelRealSense/librealsense/pull/12410) by [@mattiasbax](https://github.com/mattiasbax) - **FIX: Add iostream explicit dependency for building with older gcc (7.5.0)**
* [#12420](https://github.com/IntelRealSense/librealsense/pull/12420) - **fix PR [#12378](https://github.com/IntelRealSense/librealsense/issues/12378) missing recordable< recommended-pbs >**
* [#12415](https://github.com/IntelRealSense/librealsense/pull/12415) - **Relax start stop test**
* [#12399](https://github.com/IntelRealSense/librealsense/pull/12399) - **Color Y formats**
* [#12395](https://github.com/IntelRealSense/librealsense/pull/12395) - **FW update aligned**
* [#12408](https://github.com/IntelRealSense/librealsense/pull/12408) - **fix json install for debians**
* [#12397](https://github.com/IntelRealSense/librealsense/pull/12397) - **Rename temperatures options + add projector temp**
* [#12398](https://github.com/IntelRealSense/librealsense/pull/12398) - **make json into proper interface lib**
* [#12396](https://github.com/IntelRealSense/librealsense/pull/12396) - **laser change by textbox now changes value in list to 'Laser'**
* [#12394](https://github.com/IntelRealSense/librealsense/pull/12394) - **Fix software-device matcher creation**
* [#12386](https://github.com/IntelRealSense/librealsense/pull/12386) - **third test on image quality removed. now using max laser power**
* [#12378](https://github.com/IntelRealSense/librealsense/pull/12378) - **remove recommended_processing_blocks_base**
* [#12368](https://github.com/IntelRealSense/librealsense/pull/12368) - **remove platform dependency in stream.h**
* [#12372](https://github.com/IntelRealSense/librealsense/pull/12372) - **clean up types.h**
* [#12369](https://github.com/IntelRealSense/librealsense/pull/12369) - **refactor option.h and related**
* [#12361](https://github.com/IntelRealSense/librealsense/pull/12361) by [@rhaschke](https://github.com/rhaschke) - **Fix dependencies in package.xml**
* [#12364](https://github.com/IntelRealSense/librealsense/pull/12364) - **add PR template**
* [#12322](https://github.com/IntelRealSense/librealsense/pull/12322) - **USB missing icon bug fix**
* [#12353](https://github.com/IntelRealSense/librealsense/pull/12353) - **refactor software-device, sensor.h, streaming.h**
* [#12359](https://github.com/IntelRealSense/librealsense/pull/12359) - **Add security.md file**
* [#12355](https://github.com/IntelRealSense/librealsense/pull/12355) - **removed width restriction for fw version**
* [#12358](https://github.com/IntelRealSense/librealsense/pull/12358) - **Gyro sensitivity might not be const for all models**
* [#12357](https://github.com/IntelRealSense/librealsense/pull/12357) - **Align to GVD spec 0.4**
* [#12354](https://github.com/IntelRealSense/librealsense/pull/12354) - **remove deprecated L500 APIs**
* [#12356](https://github.com/IntelRealSense/librealsense/pull/12356) - **Add gated_by_value_option option type**
* [#12350](https://github.com/IntelRealSense/librealsense/pull/12350) - **add tclap interface library**
* [#12349](https://github.com/IntelRealSense/librealsense/pull/12349) - **Add `ensure_set_xu_option` option type**
* [#12346](https://github.com/IntelRealSense/librealsense/pull/12346) - **metadata_helper::is_enabled() now returns true if not a USB device**
* [#12310](https://github.com/IntelRealSense/librealsense/pull/12310) - **realsense2-all**
* [#12345](https://github.com/IntelRealSense/librealsense/pull/12345) - **fix is_port_enable() on libci**
* [#12342](https://github.com/IntelRealSense/librealsense/pull/12342) - **Syncer debug**
* [#12330](https://github.com/IntelRealSense/librealsense/pull/12330) - **fix pybackend missing udev watcher**
* [#12326](https://github.com/IntelRealSense/librealsense/pull/12326) - **Actual FPS simplification & unit change [API change]**
* [#12331](https://github.com/IntelRealSense/librealsense/pull/12331) - **move all ExternalProjectTargets into '3rd Party'**
* [#12334](https://github.com/IntelRealSense/librealsense/pull/12334) - **Removed third test comparing laser on/off**
* [#12332](https://github.com/IntelRealSense/librealsense/pull/12332) - **use RS2_API_FULL_VERSION_STR in tools**
* [#12318](https://github.com/IntelRealSense/librealsense/pull/12318) - **Cherry-pick PR [#12309](https://github.com/IntelRealSense/librealsense/issues/12309) - Fix pybind arg names**
* [#12281](https://github.com/IntelRealSense/librealsense/pull/12281) - **Image quality test**
* [#12303](https://github.com/IntelRealSense/librealsense/pull/12303) - **remove ppa install for GHA and fix U22 log files**
* [#12301](https://github.com/IntelRealSense/librealsense/pull/12301) - **Fix wrong user message when trying to backup camera flash**
* [#12289](https://github.com/IntelRealSense/librealsense/pull/12289) - **remove rs2 structs from context.h**
* [#12298](https://github.com/IntelRealSense/librealsense/pull/12298) - **Use optical module SN + Disable FW backup stage**
* [#12297](https://github.com/IntelRealSense/librealsense/pull/12297) - **Clean up types.h a little**
* [#12283](https://github.com/IntelRealSense/librealsense/pull/12283) - **[GHA] Fix Minimal CMake version test**
* [#12282](https://github.com/IntelRealSense/librealsense/pull/12282) - **Deadlock test & unit-test-config now listens to donotrun**
* [#12275](https://github.com/IntelRealSense/librealsense/pull/12275) - **Context callback changes and device mutex removal [API change]**
* [#12270](https://github.com/IntelRealSense/librealsense/pull/12270) - **static hw_monitor::build_command()**
* [#12272](https://github.com/IntelRealSense/librealsense/pull/12272) - **Setting preset does not update color options**
* [#12248](https://github.com/IntelRealSense/librealsense/pull/12248) - **Fix build DQT with visual studio 2017**
* [#12269](https://github.com/IntelRealSense/librealsense/pull/12269) - **remove context knowledge of playback devices**
* [#12266](https://github.com/IntelRealSense/librealsense/pull/12266) - **Fixing GHA Minimal CMake version check**
* [#12265](https://github.com/IntelRealSense/librealsense/pull/12265) - **rscore/device-factory base & usage in context; fix warning**
* [#12241](https://github.com/IntelRealSense/librealsense/pull/12241) - **hexarray & hexdump**
* [#12259](https://github.com/IntelRealSense/librealsense/pull/12259) - **Device hub fixes and small refactor in backend**
* [#12257](https://github.com/IntelRealSense/librealsense/pull/12257) - **Catch2 v3.4 as static lib; U22 in GHA**
* [#12255](https://github.com/IntelRealSense/librealsense/pull/12255) - **Add support to update multiple MIPI devices in rs-fw-update tool**
* [#12243](https://github.com/IntelRealSense/librealsense/pull/12243) - **Singleton backend/device-watcher**
* [#12247](https://github.com/IntelRealSense/librealsense/pull/12247) - **add unit-test-config --not-live and make use in GHA**
* [#11900](https://github.com/IntelRealSense/librealsense/pull/11900) by [@ciandonovan](https://github.com/ciandonovan) - **Refactor device enumeration for unprivileged containers**
* [#12118](https://github.com/IntelRealSense/librealsense/pull/12118) - **GHA checking CMake minimal version test**
* [#12225](https://github.com/IntelRealSense/librealsense/pull/12225) - **Replaced travis with Github Actions on readme files**
* [#12234](https://github.com/IntelRealSense/librealsense/pull/12234) - **Y16 calibration format test**
* [#12217](https://github.com/IntelRealSense/librealsense/pull/12217) - **backend-device-factory: separation from context**
* [#12240](https://github.com/IntelRealSense/librealsense/pull/12240) - **add rs2_device_is_connected, rs2::device::is_connected [API change]**
* [#12239](https://github.com/IntelRealSense/librealsense/pull/12239) - **enable unit-test-config --live**
* [#12219](https://github.com/IntelRealSense/librealsense/pull/12219) - **Simplify time-service usage**
* [#12228](https://github.com/IntelRealSense/librealsense/pull/12228) - **Signal in rsutils**
* [#12231](https://github.com/IntelRealSense/librealsense/pull/12231) - **master -> development**
* [#12230](https://github.com/IntelRealSense/librealsense/pull/12230) - **promote v2.54.2 to master**
* [#12223](https://github.com/IntelRealSense/librealsense/pull/12223) - **fix unit-test-config cmake file detection; add cmake:dependencies**
* [#12224](https://github.com/IntelRealSense/librealsense/pull/12224) - **New color for button on pop up window Viewer updated**
* [#12200](https://github.com/IntelRealSense/librealsense/pull/12200) - **Refactor device.h**
* [#12212](https://github.com/IntelRealSense/librealsense/pull/12212) - **allow multi filters for fw update via viewer**
* [#12209](https://github.com/IntelRealSense/librealsense/pull/12209) - **Enable D457 FW update on LibCI**
* [#12207](https://github.com/IntelRealSense/librealsense/pull/12207) - **win_event_device_watcher wait before notify**
* [#12210](https://github.com/IntelRealSense/librealsense/pull/12210) - **Handle Coverity issues**
* [#12195](https://github.com/IntelRealSense/librealsense/pull/12195) - **Fix static-analysis issue in frame_holder**
* [#12193](https://github.com/IntelRealSense/librealsense/pull/12193) by [@hsuys](https://github.com/hsuys) - **Fix GetDistance() issue for SoftwareDevice in C#**
* [#12196](https://github.com/IntelRealSense/librealsense/pull/12196) - **SW device depth-units per sensor's DEPTH_UNITS, if set**
* [#12204](https://github.com/IntelRealSense/librealsense/pull/12204) - **Prevent the RGB camera from being disabled during FL calibration**
* [#12201](https://github.com/IntelRealSense/librealsense/pull/12201) - **fix access violation when exiting viewer with playback device**
* [#12191](https://github.com/IntelRealSense/librealsense/pull/12191) by [@AllenIsaacJose](https://github.com/AllenIsaacJose) - **Update installation_jetson.md**
* [#12138](https://github.com/IntelRealSense/librealsense/pull/12138) - **Remove recordable<> from many interfaces**
* [#12175](https://github.com/IntelRealSense/librealsense/pull/12175) - **Add cmdline args to realsense-viewer**
* [#12174](https://github.com/IntelRealSense/librealsense/pull/12174) - **Fix Coverity issue**
* [#12013](https://github.com/IntelRealSense/librealsense/pull/12013) - **Refactor Windows documentation**
* [#12167](https://github.com/IntelRealSense/librealsense/pull/12167) - **calling the continuation method even on frame alloc failure**
* [#12163](https://github.com/IntelRealSense/librealsense/pull/12163) - **Options registry for non-built-in options**
* [#12162](https://github.com/IntelRealSense/librealsense/pull/12162) - **do not use IR frame for hdr merging if it does not support meta data**
* [#12149](https://github.com/IntelRealSense/librealsense/pull/12149) - **Refactor frame interfaces and structures**
* [#12160](https://github.com/IntelRealSense/librealsense/pull/12160) - **fix update_device::get_device_info**
* [#12151](https://github.com/IntelRealSense/librealsense/pull/12151) by [@mengyui](https://github.com/mengyui) - **update project files to Android Studio 2022 with AGP 8.0**
* [#12153](https://github.com/IntelRealSense/librealsense/pull/12153) - **[testing] disable ports only if acroname is connected**
* [#12152](https://github.com/IntelRealSense/librealsense/pull/12152) - **Fw logger multipe devices**
* [#10593](https://github.com/IntelRealSense/librealsense/pull/10593) by [@mengyui](https://github.com/mengyui) - **when launch DQT using default depth/ir profiles**
* [#10965](https://github.com/IntelRealSense/librealsense/pull/10965) by [@kyosho-](https://github.com/kyosho-) - **Android 12 Support**
* [#12141](https://github.com/IntelRealSense/librealsense/pull/12141) - **Devinfo**
* [#12121](https://github.com/IntelRealSense/librealsense/pull/12121) - **Removing tensorflow wrapper example n.4 & n.5**
* [#12143](https://github.com/IntelRealSense/librealsense/pull/12143) - **Move lazy to rsutils**
* [#12060](https://github.com/IntelRealSense/librealsense/pull/12060) - **merge ADL branch to development.**
* [#12146](https://github.com/IntelRealSense/librealsense/pull/12146) - **script: patches: jammy 6.2 append W10 format for D415**
* [#12120](https://github.com/IntelRealSense/librealsense/pull/12120) - **scripts: patches: metadata: append SKU**
* [#12132](https://github.com/IntelRealSense/librealsense/pull/12132) - **[testing] add retries directive**
* [#12124](https://github.com/IntelRealSense/librealsense/pull/12124) - **add BUILD_LEGACY_PYBACKEND default OFF**
* [#12115](https://github.com/IntelRealSense/librealsense/pull/12115) - **Fix get_intrinsics for unrectified image**
* [#12128](https://github.com/IntelRealSense/librealsense/pull/12128) - **Restore calibration changed options only after apply or dismiss**
* [#12126](https://github.com/IntelRealSense/librealsense/pull/12126) - **fix motion_get_intrinsics**
* [#12109](https://github.com/IntelRealSense/librealsense/pull/12109) - **sw-dev profiles work; shorter rs-enum-devs output; use-basic-formats -> format-conversion**
* [#12114](https://github.com/IntelRealSense/librealsense/pull/12114) - **move platform_camera out of context.cpp; remove recovery_info**
* [#12110](https://github.com/IntelRealSense/librealsense/pull/12110) - **scripts: patches: append SKUs**
* [#12108](https://github.com/IntelRealSense/librealsense/pull/12108) - **merge ADL to development**
* [#12105](https://github.com/IntelRealSense/librealsense/pull/12105) - **Draw cross in calibration ROI**
* [#12095](https://github.com/IntelRealSense/librealsense/pull/12095) - **rs-enumerate-devices fix**
* [#12054](https://github.com/IntelRealSense/librealsense/pull/12054) - **rs-fw-update tool - handle D457 MIPI device**
* [#12102](https://github.com/IntelRealSense/librealsense/pull/12102) - **scripts: ipu6 bind commentary**
* [#12096](https://github.com/IntelRealSense/librealsense/pull/12096) - **scripts: support for kernel 6.2**
* [#12101](https://github.com/IntelRealSense/librealsense/pull/12101) - **scripts: patch will skip hid for fixed versions**
* [#12099](https://github.com/IntelRealSense/librealsense/pull/12099) - **Patch 5.4.232 noiio**
* [#12078](https://github.com/IntelRealSense/librealsense/pull/12078) - **[testing] recycle all ports at start & disable all ports when finish**
* [#12040](https://github.com/IntelRealSense/librealsense/pull/12040) - **Port test record-software-device  to python**
* [#12070](https://github.com/IntelRealSense/librealsense/pull/12070) - **scripts: ipu6 aggregated camera support.**
* [#12084](https://github.com/IntelRealSense/librealsense/pull/12084) - **Remove redundant file**
* [#12042](https://github.com/IntelRealSense/librealsense/pull/12042) - **D457 Segfault when calling get_motion_intrinsics**
* [#12083](https://github.com/IntelRealSense/librealsense/pull/12083) - **fix string concats in versions-db-manager.cpp**
* [#12003](https://github.com/IntelRealSense/librealsense/pull/12003) by [@autoantwort](https://github.com/autoantwort) - **CMake: Use plain add_library**
* [#12041](https://github.com/IntelRealSense/librealsense/pull/12041) - **merge to dev**
* [#12052](https://github.com/IntelRealSense/librealsense/pull/12052) - **hdr options and metadata enabled for mipi**
* [#12023](https://github.com/IntelRealSense/librealsense/pull/12023) - **scripts: rs-enum: update enumeration for jetson devices.**
* [#11953](https://github.com/IntelRealSense/librealsense/pull/11953) - **GHA - Remove Foxy (EOL) and Galactic (EOL), add Iron (New Distro)**
* [#12051](https://github.com/IntelRealSense/librealsense/pull/12051) - **remove debug enforcement (-g) from unix compile options**
* [#12038](https://github.com/IntelRealSense/librealsense/pull/12038) - **Fix minimal FW version UT**
* [#12034](https://github.com/IntelRealSense/librealsense/pull/12034) - **Update minimal FW version for d455**
* [#12029](https://github.com/IntelRealSense/librealsense/pull/12029) - **Fix formats-converter bug **
* [#11949](https://github.com/IntelRealSense/librealsense/pull/11949) - **Porting legacy live test - test software-device motion stream**
* [#11969](https://github.com/IntelRealSense/librealsense/pull/11969) - **scripts: ipu6: fix media controller index**
* [#12008](https://github.com/IntelRealSense/librealsense/pull/12008) - **merge to development**
* [#11990](https://github.com/IntelRealSense/librealsense/pull/11990) - **IMU with combined motion stream**
* [#12002](https://github.com/IntelRealSense/librealsense/pull/12002) - **Remove L500**
* [#11995](https://github.com/IntelRealSense/librealsense/pull/11995) - **Close laser emitter on Focal Length calibration**
* [#11992](https://github.com/IntelRealSense/librealsense/pull/11992) - **[RS-Viewer] 3D D455 model added**
* [#11988](https://github.com/IntelRealSense/librealsense/pull/11988) - **Remove SR300**
* [#11989](https://github.com/IntelRealSense/librealsense/pull/11989) - **Update API comment**
* [#11946](https://github.com/IntelRealSense/librealsense/pull/11946) - **DDS using generic format types**
* [#11962](https://github.com/IntelRealSense/librealsense/pull/11962) - **Fix building with -DBUILD_EASYLOGGINGPP=OFF**
* [#11959](https://github.com/IntelRealSense/librealsense/pull/11959) - **Fix the installation target directory of the pyrealsense2**
* [#11952](https://github.com/IntelRealSense/librealsense/pull/11952) - **Add full version attribute for python wrapper**
* [#11880](https://github.com/IntelRealSense/librealsense/pull/11880) - **Legacy live test - Units transform test**
* [#11930](https://github.com/IntelRealSense/librealsense/pull/11930) - **[testing] improve exception handling in rspy.test**
* [#11713](https://github.com/IntelRealSense/librealsense/pull/11713) - **Refactoring Linux installation guide**
* [#11851](https://github.com/IntelRealSense/librealsense/pull/11851) by [@mengyui](https://github.com/mengyui) - **Fix YCrCb2RGB issue in CUDA code.**
* [#11915](https://github.com/IntelRealSense/librealsense/pull/11915) - **Linux collaterals in single build dir**
* [#11850](https://github.com/IntelRealSense/librealsense/pull/11850) - **virtual software_device::add_xxx_stream(); support in formats-converter**
* [#11861](https://github.com/IntelRealSense/librealsense/pull/11861) - **Python wrapper - Explicit config `enable_stream(...)` to have unique arg count [API CHANGE]**
* [#11917](https://github.com/IntelRealSense/librealsense/pull/11917) by [@davids91](https://github.com/davids91) - **Fixed compile error: ‘uint64_t’ does not name a type**
* [#11902](https://github.com/IntelRealSense/librealsense/pull/11902) - **backend-v4l: stop metadata before video for ipu6**
* [#11907](https://github.com/IntelRealSense/librealsense/pull/11907) by [@WurmD](https://github.com/WurmD) - **fix: issue [#11899](https://github.com/IntelRealSense/librealsense/issues/11899) log line fix**
* [#11903](https://github.com/IntelRealSense/librealsense/pull/11903) - **fix cppcheck parser for missing 'file0' in error**
* [#11892](https://github.com/IntelRealSense/librealsense/pull/11892) - **depth ae mode added to options to ignore in test**
* [#11886](https://github.com/IntelRealSense/librealsense/pull/11886) - **merge master to dev**
* [#11858](https://github.com/IntelRealSense/librealsense/pull/11858) - **Update synthetic-stream.cpp**
* [#11856](https://github.com/IntelRealSense/librealsense/pull/11856) - **remove internal unit-tests (with BUILD_INTERNAL_UNIT_TESTS)**
* [#11810](https://github.com/IntelRealSense/librealsense/pull/11810) - **Formats-converter renaming and adding comments for clarity**
* [#11822](https://github.com/IntelRealSense/librealsense/pull/11822) - **Adlp metadata**
* [#11834](https://github.com/IntelRealSense/librealsense/pull/11834) - **log_to_console now logs to console in Windows**
* [#11837](https://github.com/IntelRealSense/librealsense/pull/11837) - **scripts: support for jammy 5.19**
* [#11840](https://github.com/IntelRealSense/librealsense/pull/11840) - **patches: Change device match to uvc1.5 compliant.**
* [#11825](https://github.com/IntelRealSense/librealsense/pull/11825) - **scripts: patch-hid ignore 5.15.0-72+**
* [#11830](https://github.com/IntelRealSense/librealsense/pull/11830) - **create rsutils-os.cpp, rsutils-os.h**
* [#11780](https://github.com/IntelRealSense/librealsense/pull/11780) - **Fix CMake build on a root folder that contains spaces**
* [#11801](https://github.com/IntelRealSense/librealsense/pull/11801) - **using the method both from sensor and ros_reader**
* [#11714](https://github.com/IntelRealSense/librealsense/pull/11714) - **Formats conversion**
* [#11766](https://github.com/IntelRealSense/librealsense/pull/11766) by [@Allius27](https://github.com/Allius27) - **create temporary file in build directory instead of source**
* [#11760](https://github.com/IntelRealSense/librealsense/pull/11760) - **Add GHA timeout**
* [#11637](https://github.com/IntelRealSense/librealsense/pull/11637) - **RS-Viewer : metadata scrollbar added**
* [#11751](https://github.com/IntelRealSense/librealsense/pull/11751) - **[testing] improve rspy.test.remote.Error output**
* [#11707](https://github.com/IntelRealSense/librealsense/pull/11707) - **replace rspy.test abort_on_fail:bool with on_fail:str (ABORT/RAISE/LOG)**
* [#11711](https://github.com/IntelRealSense/librealsense/pull/11711) - **Verify support for options before setting them**
* [#11697](https://github.com/IntelRealSense/librealsense/pull/11697) - **Fix unstable test metadata alive **
* [#11668](https://github.com/IntelRealSense/librealsense/pull/11668) - **live-frames-fps test monitors frame drops**
* [#11695](https://github.com/IntelRealSense/librealsense/pull/11695) - **add metadata-syncer unit-tests**
* [#11699](https://github.com/IntelRealSense/librealsense/pull/11699) - **Allow change AE type while not streaming (even if AE is on)**
* [#11665](https://github.com/IntelRealSense/librealsense/pull/11665) - **improve shorten_json_string; use std::string for json; [testing] improve test.remote & GHA**
* [#11666](https://github.com/IntelRealSense/librealsense/pull/11666) - **Run more tests on more devices**
* [#11664](https://github.com/IntelRealSense/librealsense/pull/11664) - **Add a short stress test for start/stop**
* [#11653](https://github.com/IntelRealSense/librealsense/pull/11653) - **Metadata test alive calibration**
* [#11651](https://github.com/IntelRealSense/librealsense/pull/11651) by [@coolsarne](https://github.com/coolsarne) - **Added support for D405 camera**
* [#11650](https://github.com/IntelRealSense/librealsense/pull/11650) - **add "gha" to rspy.test.context using GITHUB_ACTIONS env var**
* [#11647](https://github.com/IntelRealSense/librealsense/pull/11647) - **Remove unrelated device in metadata test**
* [#11618](https://github.com/IntelRealSense/librealsense/pull/11618) - **adl-p temporary disable metadata node**
* [#11615](https://github.com/IntelRealSense/librealsense/pull/11615) - **refactor & optimize frame creation, invocation, metadata syncing**
* [#11600](https://github.com/IntelRealSense/librealsense/pull/11600) - **MD performance and misc changes**
* [#11598](https://github.com/IntelRealSense/librealsense/pull/11598) - **add slice and shorten_json_string**
* [#11592](https://github.com/IntelRealSense/librealsense/pull/11592) - **rs-enum: support for metadata nodes format updates**
* [#11566](https://github.com/IntelRealSense/librealsense/pull/11566) - **Add support for post processing filters**
* [#11587](https://github.com/IntelRealSense/librealsense/pull/11587) - **external json & pybind11_json**
* [#11561](https://github.com/IntelRealSense/librealsense/pull/11561) - **rsutils::deferred RAII; change archive to move data**
* [#11550](https://github.com/IntelRealSense/librealsense/pull/11550) - **[ADL-P] Automate rs-enum script to udev rules**
* [#11412](https://github.com/IntelRealSense/librealsense/pull/11412) - **Search extra_data for software-device**
* [#11536](https://github.com/IntelRealSense/librealsense/pull/11536) - **add rsutils::os::executable_name()**
* [#11534](https://github.com/IntelRealSense/librealsense/pull/11534) - **add D415 formats**
* [#11472](https://github.com/IntelRealSense/librealsense/pull/11472) - **backend: dfu - add support for multiple cameras**
* [#11489](https://github.com/IntelRealSense/librealsense/pull/11489) - **fix cppcheck warnings**
* [#11446](https://github.com/IntelRealSense/librealsense/pull/11446) - **[testing] add proper indents to rspy.test failures**
* [#11145](https://github.com/IntelRealSense/librealsense/pull/11145) - **Update rsutils::shared_ptr_singleton; [testing] run-unit-tests --tag negation & using manifest**
* [#11319](https://github.com/IntelRealSense/librealsense/pull/11319) - **Enumerate video nodes by name**
* [#11324](https://github.com/IntelRealSense/librealsense/pull/11324) - **[testing] add rspy.test.check_float_lists**
* [#11294](https://github.com/IntelRealSense/librealsense/pull/11294) - **color processing block acc to platform**
* [#11295](https://github.com/IntelRealSense/librealsense/pull/11295) - **[ADL-P] LRS V4L2 Backend MPLANE streaming support**
* [#11165](https://github.com/IntelRealSense/librealsense/pull/11165) - **Build_CI restructure with live-tests on Linux only**
* [#11102](https://github.com/IntelRealSense/librealsense/pull/11102) - **[testing] change rspy.test.check_equal() to throw an exception for lists**
* [#11080](https://github.com/IntelRealSense/librealsense/pull/11080) - **clean up rs-enumerate-devices tab output; add --verbose**
* [#11075](https://github.com/IntelRealSense/librealsense/pull/11075) - **rs-enumerate-devices works**
* [#11063](https://github.com/IntelRealSense/librealsense/pull/11063) - **add operator<() to rs2::device**
* [#11042](https://github.com/IntelRealSense/librealsense/pull/11042) - **move json utils into utilities/json.h**
* [#11036](https://github.com/IntelRealSense/librealsense/pull/11036) - **fix throws; in locked_transfer::send_receive; [testing] fix test-rgb-options-metadata-consistency**
* [#11022](https://github.com/IntelRealSense/librealsense/pull/11022) - **CUDA fixes for dds branch in release flow**
* [#10386](https://github.com/IntelRealSense/librealsense/pull/10386) - **context json settings**

**Full Changelog**: https://github.com/IntelRealSense/librealsense/compare/v2.54.2...v2.55.1


## Release 2.54.2
Release Date: 28th Sep, 2023

### What's new
- This is an incremental bug-fix-only release
- New FW version **5.15.1.0** for the D400 product line
    - Fixed D457 FW CRC signature in metadata
    - Fixed issue with Tare & OCC calibrations at 848x480 resolutions
- Robust OCC / Tare. Note: the calibration convergence time has increased
- Introducing: NEW [D456 camera](https://www.intelrealsense.com/depth-camera-d456/) support
- Enabled manual HDR for D457
- FW update tool support for D457
### Bug Fixes and Enhancements
* [#12169](https://github.com/IntelRealSense/librealsense/pull/12169) - Cherry pick [#12105](https://github.com/IntelRealSense/librealsense/issues/12105) - **Draw cross in calibration ROI**
* [#12165](https://github.com/IntelRealSense/librealsense/pull/12165) - Cherry pick [#11892](https://github.com/IntelRealSense/librealsense/issues/11892) -  **Ignore ae mode in drops on set test**
* [#12164](https://github.com/IntelRealSense/librealsense/pull/12164) - Cherry pick [#12162](https://github.com/IntelRealSense/librealsense/issues/12162) - **rs-hdr fix for d457**
* [#12161](https://github.com/IntelRealSense/librealsense/pull/12161) - Cherry pick [#12128](https://github.com/IntelRealSense/librealsense/issues/12128) - **UCAL - Restore calibration changed options only after apply or dismiss**
* [#12104](https://github.com/IntelRealSense/librealsense/pull/12104) - Cherry pick [#12054](https://github.com/IntelRealSense/librealsense/pull/12054) - **rs-fw-update tool - handle D457 MIPI device**
* [#12085](https://github.com/IntelRealSense/librealsense/pull/12085) - Cherry pick [#12042](https://github.com/IntelRealSense/librealsense/issues/12042) & [#12084](https://github.com/IntelRealSense/librealsense/issues/12084) - **D457 Segfault when calling get_motion_intrinsics**
* [#12079](https://github.com/IntelRealSense/librealsense/pull/12079) - Cherry pick [#12052](https://github.com/IntelRealSense/librealsense/issues/12052) - **hdr options and metadata enabled for mipi**
* [#12073](https://github.com/IntelRealSense/librealsense/pull/12073) - Cherry pick [#12034](https://github.com/IntelRealSense/librealsense/issues/12034) & [#12038](https://github.com/IntelRealSense/librealsense/issues/12038) - **Update minimal FW version for d455**
* [#12074](https://github.com/IntelRealSense/librealsense/pull/12074) - Cherry pick [#11995](https://github.com/IntelRealSense/librealsense/issues/11995) - **Close laser emitter on Focal Length calibration**
* [#12075](https://github.com/IntelRealSense/librealsense/pull/12075) - **Release flow & collateral changes** (no debug info; no C#/Matlab collaterals; etc.)


## Release 2.54.1
Release Date: 6th Jun, 2023

### API Changes
https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2541

### What's new
- New FW version 5.15.0.2 for the D400 product line
- Support for the [D400f family](https://www.intelrealsense.com/stereo-depth-with-ir/) of cameras with filters
- Support for Python 3.11; Python 3.6 is no longer supported
- C++14 default in library
- JetPack 5 collaterals
- Some content has been removed as part of an overall cleanup initiative to minimize the size and complexity of the SDK:
	* Removed deprecated camera PIDs, such as T265, L535
	* The network-device functionality has been removed in preparation for a replacement in the next release
	* Boost has been removed as a dependency of `realsense-file`
	* Sqlite, originally meant for use in internal unit-tests, has not been in quite a while and has been removed, and with it `rs2::recording_context` and `rs2::mock_context`

### Bug Fixes and Enhancements
* [#11703](https://github.com/IntelRealSense/librealsense/pull/11703) - **AE type option hot fix**
* [#11646](https://github.com/IntelRealSense/librealsense/pull/11646) - **Deprecate apt-key in distribution_linux.md to align with Debian policy [#11644](https://github.com/IntelRealSense/librealsense/issues/11644)** 
* [#11643](https://github.com/IntelRealSense/librealsense/pull/11643) - **Remove nodejs support from docs**
* [#11409](https://github.com/IntelRealSense/librealsense/pull/11409) by [@michael-mih](https://github.com/michael-mih) - **Fixed throw misspell**
* [#11636](https://github.com/IntelRealSense/librealsense/pull/11636) - **Fix bug in #11625**
* [#11627](https://github.com/IntelRealSense/librealsense/pull/11627) - **Update GHA linux runners to Ubuntu 20**
* [#11625](https://github.com/IntelRealSense/librealsense/pull/11625) - **Handle exceptions during device creation**
* [#11626](https://github.com/IntelRealSense/librealsense/pull/11626) by [@jirislaby](https://github.com/jirislaby) - **(gcc-13) include cstdint for *int*_t**
* [#11610](https://github.com/IntelRealSense/librealsense/pull/11610) by [@kirkoman](https://github.com/kirkoman) - **fix: close ObjectPool race condition ([#8334](https://github.com/IntelRealSense/librealsense/issues/8334))**
* [#11611](https://github.com/IntelRealSense/librealsense/pull/11611) & [#11604](https://github.com/IntelRealSense/librealsense/pull/11604) - **Fix unsigned fw update for L515**
* [#11602](https://github.com/IntelRealSense/librealsense/pull/11602) - **changes in hw monitor added**
* [#11591](https://github.com/IntelRealSense/librealsense/pull/11591) - **HW reset - response required handling**
* [#11585](https://github.com/IntelRealSense/librealsense/pull/11585) - **Remove unused Hoffman filter**
* [#11567](https://github.com/IntelRealSense/librealsense/pull/11567) - **Fix metadata**
* [#11574](https://github.com/IntelRealSense/librealsense/pull/11574) - **move d400 intrinsics and extrinsics to d400 folder**
* [#11569](https://github.com/IntelRealSense/librealsense/pull/11569) - **Update manual patch to use https**  (Verified on Ubuntu 22 kernel 5.15)
* [#11559](https://github.com/IntelRealSense/librealsense/pull/11559) - **Remove unused code**
* [#11556](https://github.com/IntelRealSense/librealsense/pull/11556) - **remove useless code in context.cpp**
* [#11551](https://github.com/IntelRealSense/librealsense/pull/11551) - **change metadata_blob to fit ALL metadata; software_sensor now uses array indexing**
* [#11541](https://github.com/IntelRealSense/librealsense/pull/11541) - **Type of variable initialization was changed**
* [#11558](https://github.com/IntelRealSense/librealsense/pull/11558) - **remove sw_device_extra_data**  (Now that the metadata can encompass all fields #11551)
* [#11542](https://github.com/IntelRealSense/librealsense/pull/11542) - **multiple resolutions implementation added, never enabled**
* [#11519](https://github.com/IntelRealSense/librealsense/pull/11519) - **Viewer small refact**
* [#11520](https://github.com/IntelRealSense/librealsense/pull/11520) - **limit test-set-gain-stress-test to nightly only**  (LRS-634)
* [#11497](https://github.com/IntelRealSense/librealsense/pull/11497) - **Relocation git install command**
* [#11495](https://github.com/IntelRealSense/librealsense/pull/11495) - **The python support versions were updated**
* [#11488](https://github.com/IntelRealSense/librealsense/pull/11488) - **fix crash from leftover T265**
* [#11484](https://github.com/IntelRealSense/librealsense/pull/11484) - **Pr check add**
* [#11479](https://github.com/IntelRealSense/librealsense/pull/11479) - **Move stuff to and from common DS**
* [#11477](https://github.com/IntelRealSense/librealsense/pull/11477) - **Compilation -pedantic flag disabled for glfw on Unix**
* [#11478](https://github.com/IntelRealSense/librealsense/pull/11478) - **KW - null pointers checks added - other concept**
* [#11475](https://github.com/IntelRealSense/librealsense/pull/11475) - **printings added in hdr tests**
* [#11469](https://github.com/IntelRealSense/librealsense/pull/11469) - **KW - null pointer checks added**  (Tracking on [LRS-696])
* [#11474](https://github.com/IntelRealSense/librealsense/pull/11474) - **Move FW version & serial numbers parsing to ds-common**
* [#11473](https://github.com/IntelRealSense/librealsense/pull/11473) - **Rollback error polling option name**
* [#11417](https://github.com/IntelRealSense/librealsense/pull/11417) - **support for Python 3.11 with pybind11 v2.10.3**
* [#11442](https://github.com/IntelRealSense/librealsense/pull/11442) - **Clean cmake opengl warning ubuntu22**  (Tracking on [LRS-673])
* [#11464](https://github.com/IntelRealSense/librealsense/pull/11464) - **removing register to motion module temperature option**
* [#11448](https://github.com/IntelRealSense/librealsense/pull/11448) - **Update stb image to version 2.28**
* [#11450](https://github.com/IntelRealSense/librealsense/pull/11450) - **Add retry to set/get pu commands**
* [#11461](https://github.com/IntelRealSense/librealsense/pull/11461) by [@alexmehta](https://github.com/alexmehta) - **Update installation_raspbian.md**  (Update the raspbian install slightly)
* [#11397](https://github.com/IntelRealSense/librealsense/pull/11397) - **Cppcheck cleanup**
* [#11447](https://github.com/IntelRealSense/librealsense/pull/11447) - **Fix FW version check on RS-Viewer**
* [#11440](https://github.com/IntelRealSense/librealsense/pull/11440) - **Fix uninitialized variable issue**
* [#11432](https://github.com/IntelRealSense/librealsense/pull/11432) - **Add depth auto exposure mode**
* [#11399](https://github.com/IntelRealSense/librealsense/pull/11399) - **Detect the new products in the SDK and Viewer: D435f, D435if, D455f, D456**
* [#11436](https://github.com/IntelRealSense/librealsense/pull/11436) - **Set different ABI to each python module to fix crash on multiple shared python modules**
* [#11422](https://github.com/IntelRealSense/librealsense/pull/11422) - **refact ds5 to d400**
* [#11416](https://github.com/IntelRealSense/librealsense/pull/11416) by [@rupprecht](https://github.com/rupprecht) - **Fully qualify `std::move` invocations to fix -Wunqualified-std-cast-call**
* [#11408](https://github.com/IntelRealSense/librealsense/pull/11408) - **Metadata permission**
* [#11394](https://github.com/IntelRealSense/librealsense/pull/11394) - **Enumerate video nodes by name**
* [#11404](https://github.com/IntelRealSense/librealsense/pull/11404) by [@cobar001](https://github.com/cobar001) - **Make minor adjustments to depth-metrics.h and a few dependencies to improve modularity)
* [#11396](https://github.com/IntelRealSense/librealsense/pull/11396) - **update third-party:tinyfiledialogs version to 3.9.0:  cleanup format-overflow warning**
* [#11385](https://github.com/IntelRealSense/librealsense/pull/11385) - **D457 MIPI enable multiple cameras for single DES.**
* [#11402](https://github.com/IntelRealSense/librealsense/pull/11402) - **remove debug message in global_timestamp_reader**
* [#11398](https://github.com/IntelRealSense/librealsense/pull/11398) - **fix the streams order in viewer**
* [#11395](https://github.com/IntelRealSense/librealsense/pull/11395) - **third-party:glad: ignore pedantic directive for sensitive code**
* [#11377](https://github.com/IntelRealSense/librealsense/pull/11377) - **calling buffer releasing method earlier**
* [#11378](https://github.com/IntelRealSense/librealsense/pull/11378) - **Retry on "set_xu" when device is busy**
* [#11375](https://github.com/IntelRealSense/librealsense/pull/11375) - **C++latest, step 1**
* [#11373](https://github.com/IntelRealSense/librealsense/pull/11373) - **Initialization of variables added**
* [#11372](https://github.com/IntelRealSense/librealsense/pull/11372) - **validating range before set option**
* [#11369](https://github.com/IntelRealSense/librealsense/pull/11369) - **force update of cppcheck_run.parsed.log in static analysis**
* [#11359](https://github.com/IntelRealSense/librealsense/pull/11359) - **T265 leftovers in docs and src**
* [#11356](https://github.com/IntelRealSense/librealsense/pull/11356) - **removing static analysis errors**
* [#11341](https://github.com/IntelRealSense/librealsense/pull/11341) - **scripts: Add D585 & D585S to metadata patch list in manual scripts**
* [#11345](https://github.com/IntelRealSense/librealsense/pull/11345) - **remove sqlite, rs2::recording_context & ::mock_context**
* [#11343](https://github.com/IntelRealSense/librealsense/pull/11343) - **Variables initialization added**
* [#11344](https://github.com/IntelRealSense/librealsense/pull/11344) - **fix the left pixels column**
* [#11349](https://github.com/IntelRealSense/librealsense/pull/11349) - **turning flag to true when needed**
* [#11342](https://github.com/IntelRealSense/librealsense/pull/11342) - **Update static analysis**
* [#11331](https://github.com/IntelRealSense/librealsense/pull/11331) - **Update missing packages to installation guide**
* [#11334](https://github.com/IntelRealSense/librealsense/pull/11334) - **Division by zero might occur in ds5-auto-calibration.cpp**
* [#11242](https://github.com/IntelRealSense/librealsense/pull/11242) - **scripts: L4T 5.0.2 RLS UVC support**
* [#11318](https://github.com/IntelRealSense/librealsense/pull/11318) - **Remove network device**
* [#11312](https://github.com/IntelRealSense/librealsense/pull/11312) - **adding string comparison in case duration comparison fails**
* [#11305](https://github.com/IntelRealSense/librealsense/pull/11305) - **Remove boost from realsense-file**
* [#11307](https://github.com/IntelRealSense/librealsense/pull/11307) - **Fix crash when advanced mode = OFF**
* [#11221](https://github.com/IntelRealSense/librealsense/pull/11221) by [@martinseilerameria](https://github.com/martinseilerameria) - **Add device iterator to ImGui ID stack for device selection pop up in RealSense Viewer**
* [#11300](https://github.com/IntelRealSense/librealsense/pull/11300) - **add UT verifying no emitter freq option on legacy devices**
* [#11105](https://github.com/IntelRealSense/librealsense/pull/11105) - **C++14**
* [#11071](https://github.com/IntelRealSense/librealsense/pull/11071) - **restoring test for d400 pl**
* [#11287](https://github.com/IntelRealSense/librealsense/pull/11287) - **Remove TM2 (T265) from LibRS**
* [#11285](https://github.com/IntelRealSense/librealsense/pull/11285) - **Fix viewer histogram equalization at startup**
* [#11275](https://github.com/IntelRealSense/librealsense/pull/11275) - **add sample.preset to avoid install problems**
* [#11265](https://github.com/IntelRealSense/librealsense/pull/11265) - **remove L535**
* [#11237](https://github.com/IntelRealSense/librealsense/pull/11237) - **Python log unit-tests, with hang fix and more**
* [#11254](https://github.com/IntelRealSense/librealsense/pull/11254) - **Variable Initialization type was changed**
* [#11235](https://github.com/IntelRealSense/librealsense/pull/11235) - **Stabilize timestamp domain test: sleeping time.**
* [#11222](https://github.com/IntelRealSense/librealsense/pull/11222) - **Hkr other ds rebased dev**
* [#11233](https://github.com/IntelRealSense/librealsense/pull/11233) - **Division by zero might occur in viewer.cpp**
* [#11223](https://github.com/IntelRealSense/librealsense/pull/11223) - **Update emitter frequency minimal fw version to 5.14.0.0**
* [#11216](https://github.com/IntelRealSense/librealsense/pull/11216) - **Remove nodejs wrapper**
* [#11217](https://github.com/IntelRealSense/librealsense/pull/11217) by [@hsuys](https://github.com/hsuys) - **GitHub[#11171](https://github.com/IntelRealSense/librealsense/issues/11171)**
* [#11202](https://github.com/IntelRealSense/librealsense/pull/11202) - **add rsutils::number::running_average<>**
* [#11214](https://github.com/IntelRealSense/librealsense/pull/11214) - **stabilized-value in pyrsutils; replace 9 cpp tests with 1 py**
* [#11209](https://github.com/IntelRealSense/librealsense/pull/11209) - **Relax HDR UT KPI**
* [#11191](https://github.com/IntelRealSense/librealsense/pull/11191) - **rsutils::version, replacing librealsense::firmware_version & sw_update::version**
* [#11193](https://github.com/IntelRealSense/librealsense/pull/11193) - **Add divide by zero protection**
* [#11186](https://github.com/IntelRealSense/librealsense/pull/11186) - **Utilities rename to rsutils**
* [#11179](https://github.com/IntelRealSense/librealsense/pull/11179) - **Reduce cppcheck errors**
* [#11167](https://github.com/IntelRealSense/librealsense/pull/11167) - **Stabilize timestamp domain test**
* [#11160](https://github.com/IntelRealSense/librealsense/pull/11160) - **ELPP and utilities changes**
* [#11136](https://github.com/IntelRealSense/librealsense/pull/11136) - **Hot fix for OCC HA type to use legacy UCAL demo scripts**
* [#11147](https://github.com/IntelRealSense/librealsense/pull/11147) - **pybind as a dependency in own project; utilities-py; test-split**
* [#11137](https://github.com/IntelRealSense/librealsense/pull/11137) - **utilities as a library**
* [#11100](https://github.com/IntelRealSense/librealsense/pull/11100) - **Add ROS2 build step for LibRealSense**
* [#11078](https://github.com/IntelRealSense/librealsense/pull/11078) - **Add GMSL to RS Viewer for D457**
* [#11095](https://github.com/IntelRealSense/librealsense/pull/11095) by [@Endruu](https://github.com/Endruu) - **Fix memory leaks in post processing filters**

### Build Environment
* Solution name in Visual Studio has changed: `librealsense2` -> `realsense2` ... make sure you reopen the new solution after running CMake
* Librealsense headers previously in `include/librealsense2/utilities` have been moved internally into `third-party/rsutils` and are now a statically-linked library
	* This means that `realsense2` dependencies now have an additional `rsutils` dependency
	* If including librealsense via its CMake project, this dependency should be injected automatically and nothing needs to be done
		* An include directory is automatically added, too: just use
		      `#include <rsutils/...>`
	* If `BUILD_PYTHON_BINDINGS` is `ON`, an additional Python library `pyrsutils.pyd` will be built, and includes access to the utilities functions from it... this is mostly for use in CI, but is available for anyone if needed
* Visual Studio solution projects have been moved around a bit, with most of the main ones (rsutils, python wrappers) now under `Library` alongside `realsense2`


## Release 2.53.1
Release Date: 6th Dec, 2022

### API Changes
https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2531

### What's new
- New FW version 5.14.0.0 for the D400 product-line
- Python 3.10 support
- Added support for Ubuntu 2020 kernels 5.13 and 5.15, and Ubuntu 2022 with kernel 5.15
- LibRS ROS wrapper is now supporting ROS Humble - click [here](https://github.com/IntelRealSense/realsense-ros/releases/tag/4.51.1) for more info

- Collaterals for Python 3.5 and Ubuntu 16 are no longer generated or tested
- Added support for MIPI camera OCC/Tare calibration that offloads part of the algorithmic flow to the host, also known as **Host-Assistance (HA) mode** (see [API changes](https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2531))

### Bug Fixes and Enhancements
* [#11166](https://github.com/IntelRealSense/librealsense/pull/11166) - **update D4xx FW to 5.14.0.0**
* [#11001](https://github.com/IntelRealSense/librealsense/pull/11001) - **OCC/Tare integration with MIPI SKU**  (Integration of OCC/Tare host-assisted flow)
* [#11072](https://github.com/IntelRealSense/librealsense/pull/11072) - **Add python 3.10 support**
* [#11070](https://github.com/IntelRealSense/librealsense/pull/11070) by [@hsuys](https://github.com/hsuys) - **Enable multi camera support for D457**
* [#11076](https://github.com/IntelRealSense/librealsense/pull/11076) - **Update supported kernel versions in docs**
* [#11061](https://github.com/IntelRealSense/librealsense/pull/11061) - **Fix jammy debian build**
* [#11041](https://github.com/IntelRealSense/librealsense/pull/11041) by [@codekansas](https://github.com/codekansas) - **Do a shallow git fetch instead of full fetch**
* [#10948](https://github.com/IntelRealSense/librealsense/pull/10948) - **Fix DSO-18454 laser turned off after calibration**
* [#11034](https://github.com/IntelRealSense/librealsense/pull/11034) - **D4xx emitter on off**
* [#11026](https://github.com/IntelRealSense/librealsense/pull/11026) by [@Hsiang0717](https://github.com/Hsiang0717) - **fix rs-server can't compile when -DBUILD_WITH_OPENMP:bool=ON**
* [#11013](https://github.com/IntelRealSense/librealsense/pull/11013) by [@stefanhige](https://github.com/stefanhige) - **fix possibly uninitialized memory. Add documentation.**  (See [#11
006](https://github.com/IntelRealSense/librealsense/issues/11006).)
* [#11000](https://github.com/IntelRealSense/librealsense/pull/11000) - **bump json to v3.11.2**
* [#10953](https://github.com/IntelRealSense/librealsense/pull/10953) - **Add a new Emitter frequency option**
* [#10992](https://github.com/IntelRealSense/librealsense/pull/10992) - **Update RS icons**
* [#10971](https://github.com/IntelRealSense/librealsense/pull/10971) - **Update Intel RealSense logo**
* [#10925](https://github.com/IntelRealSense/librealsense/pull/10925) - **Test timestamp domain**
* [#10931](https://github.com/IntelRealSense/librealsense/pull/10931) by [@aoxu2000](https://github.com/aoxu2000) - **fix type and method err for Python3**
* [#10913](https://github.com/IntelRealSense/librealsense/pull/10913) - **restricting the D405 w/a for calibration resolution to D405 PID only**
* [#10921](https://github.com/IntelRealSense/librealsense/pull/10921) - **attempt at reducing warnings (U20)**
* [#10920](https://github.com/IntelRealSense/librealsense/pull/10920), [#10918](https://github.com/IntelRealSense/librealsense/pull/10918) - **adding gil_scoped_release to sensor open api methods**
* [#10522](https://github.com/IntelRealSense/librealsense/pull/10522) by [@YoniChechik](https://github.com/YoniChechik) - **bugfix: next_pixel_in_line**
* [#10634](https://github.com/IntelRealSense/librealsense/pull/10634) by [@jcremona](https://github.com/jcremona) - **Bash script made an incorrect string comparison**
* [#10898](https://github.com/IntelRealSense/librealsense/pull/10898) by [@dmipx](https://github.com/dmipx) - **docs - Ubuntu 20/22 driver patching note**
* [#10895](https://github.com/IntelRealSense/librealsense/pull/10895) by [@noacoohen](https://github.com/noacoohen) - **Set the metadata hardware size tool tip description**
* [#10865](https://github.com/IntelRealSense/librealsense/pull/10865) by [@mengyui](https://github.com/mengyui) - **add missing dependence for build kernel module**
* [#10893](https://github.com/IntelRealSense/librealsense/pull/10893) by [@35selim](https://github.com/35selim) - **Improved/fixed installation.md syntax**
* [#10874](https://github.com/IntelRealSense/librealsense/pull/10874) - **Fix video md sync issue in v4l2 backend**
* [#10881](https://github.com/IntelRealSense/librealsense/pull/10881) by [@baderouaich](https://github.com/baderouaich) - **Fix build fail due missing operator= in popup**
* [#10809](https://github.com/IntelRealSense/librealsense/pull/10809) by [@baderouaich](https://github.com/baderouaich) - **Some bug fixes**
    - [Fix popup's operator== comparison](https://github.com/IntelRealSense/librealsense/pull/10809/commits/a96a09642dd9a9726106084febdec8ac825536b9)
    - [Changed order of the if condition to avoid the possibility of dereferencing an invalid iterator](https://github.com/IntelRealSense/librealsense/pull/10809/commits/248f9afdbeb9bdc92b628a85a3ad522c0c54adaa)
    - [Fixed wrong usage of memset()](https://github.com/IntelRealSense/librealsense/pull/10809/commits/49be7fddc5d2ec713d7edc0be5603235347520ab)
    - [Fixed standard deviation check must be between range](https://github.com/IntelRealSense/librealsense/pull/10809/commits/ef96f6670de326dbaa919fcf3bf30abb1a98d875)
    - [Fixed wrong usage of memset()](https://github.com/IntelRealSense/librealsense/pull/10809/commits/328c28a37da41a68a4d9b67720c5f8956e738f0e)
    - [Fixed potential memory leak in RsSimpleRTPSink](https://github.com/IntelRealSense/librealsense/pull/10809/commits/4e90d0a93412157421580425753b68c687dcf5a4)
* [#10876](https://github.com/IntelRealSense/librealsense/pull/10876) by [@riking](https://github.com/riking) - **Fix two undefined behavior issues (pipeline/config, elpp)**
* [#10880](https://github.com/IntelRealSense/librealsense/pull/10880) - **upgrade ELPP to v9.96.7**
* [#10818](https://github.com/IntelRealSense/librealsense/pull/10818) by [@chris-durand](https://github.com/chris-durand) - **Fix compilation with GCC 10+**
* [#10737](https://github.com/IntelRealSense/librealsense/pull/10737) - **adding support for y16i - 10 MSB**
* [#10745](https://github.com/IntelRealSense/librealsense/pull/10745) by [@dmipx](https://github.com/dmipx) - **LRS Linux kernel support for Y16i**
* [#10625](https://github.com/IntelRealSense/librealsense/pull/10625) by [@mengyui](https://github.com/mengyui) - **enable kernel patch for Ubuntu 22.04**
* [#10840](https://github.com/IntelRealSense/librealsense/pull/10840) - **Undef v4l debug logs**
* [#10839](https://github.com/IntelRealSense/librealsense/pull/10839) - **Fix cast/conversion issue when using mixed int/float types**
* [#10816](https://github.com/IntelRealSense/librealsense/pull/10816) - **Revert "v4l2 backend - ignoring buffers with flag V4L2_BUF_FLAG_ERROR"**  (Reverts IntelRealSense/librealsense[#10772](https://github.com/IntelRealSense/librealsense/issues/10772))
* [#10797](https://github.com/IntelRealSense/librealsense/pull/10797) - **Addressing LRS-462 Viewer IR frames switch**
* [#10800](https://github.com/IntelRealSense/librealsense/pull/10800) - **cuda code compilation correction**
* [#10798](https://github.com/IntelRealSense/librealsense/pull/10798) - **D4xx ts global hw fix**
* [#10793](https://github.com/IntelRealSense/librealsense/pull/10793) - **mipi fw update ui fix**
* [#10794](https://github.com/IntelRealSense/librealsense/pull/10794) - **wa added in frame size assertion, and processing block for mipi added**
* [#10762](https://github.com/IntelRealSense/librealsense/pull/10762) - **added utilities/string/chrono.h; fix test-waiting-on.cpp to use it**




## Pre-Release 2.52.1
Release Date: 28th Sep, 2022

### What's new
- Collaterals for Python 2.7 and 32-bit are no longer generated or tested
    - NOTE: the Windows installer is still 32-bit
- Introducing support for MIPI devices!

### NOTE
This is a **PRE-release**, off a branch that is yet to be merged to the main development branch, of MIPI support.
Validation of USB devices is ongoing and, while we expect no major issues, this release is intended for use with a MIPI device.


## Release 2.51.1
Release Date: 23rd Aug 2022

### API Changes
https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2511

### What's new
- D405 is now officially supported
- Windows 7 executable is no longer created as part of the release, but the code is still compilable

### Bug Fixes and Enhancements
* [#10740](https://github.com/IntelRealSense/librealsense/pull/10740) - **Remove unused variable pid in ds5_advanced_mode_base constructor**
    See [#10336](https://github.com/IntelRealSense/librealsense/issues/10336): Crash in applications when running x86 in Win10 with D405
* [#10619](https://github.com/IntelRealSense/librealsense/pull/10619) - **fix [#10617](https://github.com/IntelRealSense/librealsense/issues/10617) rs-software-device black output**
* [#10147](https://github.com/IntelRealSense/librealsense/pull/10147) - **Partial frame notif acc to kpi**
* [#10573](https://github.com/IntelRealSense/librealsense/pull/10573) - **Remove redundant frame copies inside librealsense core**
* [#10388](https://github.com/IntelRealSense/librealsense/pull/10388) - **Add easylogging include cmake**
* [#10384](https://github.com/IntelRealSense/librealsense/pull/10384) - **Fix root include path**
* [#10369](https://github.com/IntelRealSense/librealsense/pull/10369) - **Move utilities to librealsense include dir**
* [#10314](https://github.com/IntelRealSense/librealsense/pull/10314) - **Fix LibCI-nightly reset_update_counter opcode**  (wrongly changed in [#10261](https://github.com/IntelRealSense/librealsense/issues/10261))
* [#10296](https://github.com/IntelRealSense/librealsense/pull/10296) - **Remove ENABLE_ZERO_COPY CMake flag**  (wasn't in use)
* [#10261](https://github.com/IntelRealSense/librealsense/pull/10261) - **add debug_protocol::build_command()**
    - add build_raw_data to the debug_protocol API - (this API generates a raw data cmd from opcode, params, and data)
    - add the new API to C# and Python wrappers
* [#10183](https://github.com/IntelRealSense/librealsense/pull/10183) - **tooltip corrected**
* [#10152](https://github.com/IntelRealSense/librealsense/pull/10152) - **Allow retries for USB claim interface +  Increase FW update UT stability**
* [#10130](https://github.com/IntelRealSense/librealsense/pull/10130) - **Force non-realtime playback to be synchronous**
* [#10117](https://github.com/IntelRealSense/librealsense/pull/10117) - **Fix option_set_drop_test**
* [#10091](https://github.com/IntelRealSense/librealsense/pull/10091) - **Fix error polling last value reset logic**
* [#10082](https://github.com/IntelRealSense/librealsense/pull/10082) - **syntax error corrected**
* [#10047](https://github.com/IntelRealSense/librealsense/pull/10047) - **Gh actions update windows label & fix mac-os OpenSSL link**
* [#10032](https://github.com/IntelRealSense/librealsense/pull/10032) - **Concurrency improvements**  (Improve `dispatcher::flush()` and `waiting_on<>` so that the stopped mutex/cv are used -- otherwise the stopped signal may not stop the wait and we get a timeout.)
* [#10024](https://github.com/IntelRealSense/librealsense/pull/10024) - **Fix playback flush halt + add UT**
* [#9936](https://github.com/IntelRealSense/librealsense/pull/9936) - **Adjust the Self-Calibration script for UCAL flows**
* [#9912](https://github.com/IntelRealSense/librealsense/pull/9912) - **Fix UCAL inconsistencies**
    Viewer-specific fixes and refinements:
    - Enable preview with ROI for GT after HW reset event
    - Improve FL robustness in USB2 mode by adding more time for camera's exposure to settle
    -  Add blocking notification when invoking FL-Calib for D455 at USB2 mode.
* [#9727](https://github.com/IntelRealSense/librealsense/pull/9727) by [@kevindehecker](https://github.com/kevindehecker) - **Enable patches for kernel 5.11**
* [#9887](https://github.com/IntelRealSense/librealsense/pull/9887) - **Error on playing paused bag** (Fix issue [#4249](https://github.com/IntelRealSense/librealsense/issues/4249))
* [#9882](https://github.com/IntelRealSense/librealsense/pull/9882) - **Ucal fixes**  (Ucal amendments)


### Documentation
* [#10210](https://github.com/IntelRealSense/librealsense/pull/10210) - **Add Ubuntu 20 to installation guide**
* [#9973](https://github.com/IntelRealSense/librealsense/pull/9973) - **Remove Jetson installation doc Debians built with FORCE_RSUSB comment**
    Jetson Debians no longer build with -DFORCE_RS_USB_BACKEND=true,
    The backend built is V4L2
* [#9895](https://github.com/IntelRealSense/librealsense/pull/9895) - **Add libudev-dev dependency to Jetson installation guide**
* [#9898](https://github.com/IntelRealSense/librealsense/pull/9898) - **Disclaimer notification for D415 and Self-cal**  (Add notification and provide a link to Self-Cal white paper)


## Release 2.50.0
Release Date: 11th Nov 2021

### API Changes
https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2500

### What's new
* [#8925](https://github.com/IntelRealSense/librealsense/pull/8925) - **[D400]** Firmware version 5.13.0.50
  - Intermittent failures in sequential Depth start/stop stress-test scenario (DSO-17659)
  - Fix for Emitter on/off stops on RGB srteam deactivates (DSO-16964)
  - Minor fixes and stability improvements
* [#9336](https://github.com/IntelRealSense/librealsense/pull/9336) - **[D400] Self-Calibration (UCAL) Routines - enhancements and refactoring**  
  - UCAL is an advanced feature<sup>(1)</sup> intended to compensate for camera's calibration misalignments developed over the product's life cycle after it's being deployed. The re-calibration is achieved by running a set of predefined routines - On-Chip, Tare and Focal-Length<sup>(2)</sup>, on the Depth camera. Each routine is designed to address specific Camera's metric and to re-adjust it to a level comparable to factory calibration spec.  
   (1) Read the [White Paper](https://dev.intelrealsense.com/docs/self-calibration-for-depth-cameras) prior to running UCAL steps, as it affects the camera's calibration tables.  
   (2) As part of the Self-Calibration refactoring, Focal-Length calibration and Tare Target-based distance calculation were introduced, that require a printed target to be provided ([Target_for_print](https://librealsense.intel.com/Releases/UCAL/Calibration_Target_v1.pdf)).
* [#9831](https://github.com/IntelRealSense/librealsense/pull/9831) - **[Linux]** Switch to OS Event-driven device discovery instead of polling mechanism.
  - Reduce camera reset cycles delays and improves software responsiveness.
  - Replace Linux `polling_device_watcher` with `udev_device_watcher`. More than one udev notification is sent for each add/remove event. We "aggregate" and enumerate only when a polling period comes up "empty" meaning nothing else is incoming. 
  - Utilizes `libudev-dev` package functionality. (LRS-294)
* [#9268](https://github.com/IntelRealSense/librealsense/pull/9268) - **[Docker]**
  - Provide [tutorial](https://github.com/IntelRealSense/librealsense/tree/master/scripts/Docker) with a demo docker file to generate and deploy a containerized version of the SDK (LRS-85)
* [#9558](https://github.com/IntelRealSense/librealsense/pull/9558) - **[Demo]** [Pointcloud-Stitching](https://github.com/IntelRealSense/librealsense/tree/master/wrappers/pointcloud/pointcloud-stitching).
  - The demo combines Depth live feeds from two cameras and presents a single Wide FOV image.
  - The method used to stitch the images is to transfer both pointclouds into the virtual device's coordinate system and then project them on the virtual device's images.
  - The app assumes the calibration matrix between the devices is known. A method for calculating it is demonstrated in the wrappers\pointcloud\pointcloud-stitching\doc\pointcloud-stitching-demo.md file.
  - The source code is available under `wrapper/pointcloud/pointcloud-stitching/rs-pointcloud-stitching demo` path.

### Bug Fixes and Enhancements
* [#9878](https://github.com/IntelRealSense/librealsense/pull/9878) - **[Core]** fix callback ptr (DSO-17796)  
* [#9876](https://github.com/IntelRealSense/librealsense/pull/9876) - **[Core]** C++17 std::iterator error. Addresses for [#6283](https://github.com/IntelRealSense/librealsense/issues/6283). (DSO-15172)  
* [#9871](https://github.com/IntelRealSense/librealsense/pull/9871) - **[Windows]** Handle SEMAPHORE_TIMEOUT error on set_pu command.  Addresses [#7129](https://github.com/IntelRealSense/librealsense/issues/7129). (DSO-17181)  
* [#9855](https://github.com/IntelRealSense/librealsense/pull/9855) - **[Jetsons]** Support for l4t 32.6.1  (the latest supported jetson version 32.5.1 is not the latest official version from nvidia (32.6.1)  contributed by [@wall0404](https://github.com/wall0404)
* [#9815](https://github.com/IntelRealSense/librealsense/pull/9815) - **[D400]** Group FW calls.  Apply wrapper function "group_multiple_fw_calls" to ds5 devices to speed-up device activation. Speedsup camera's startup in cold boot scenario by 200+ msec (DSO-16629)  
* [#9845](https://github.com/IntelRealSense/librealsense/pull/9845) - **[Video4Linux]** Fix set gain exception raised on Ubuntu 20.  Use Status Interrupt Endpoint Vl42 (async. controls) for verifying `set_pu` command was executed before the function call returns. (DSO-17185)  
* [#9692](https://github.com/IntelRealSense/librealsense/pull/9692) - **[D400]** Reset Auto Exp/Gain Limit back to default value 0. (DSO-17393)  
* [#9844](https://github.com/IntelRealSense/librealsense/pull/9844) - **[Core]** Wrap android log with lrs namespace.  (LRS-316)  
* [#9821](https://github.com/IntelRealSense/librealsense/pull/9821) - **[RSUSB]** Backend - reattach kernel driver.  
 Fixes RSUSB/V4L interoperability:  
 RSUSB backend detaches the device from the V4L2 kernel driver and never reattaches it. Consequently, after using RSUSB backend, applications using the v4l backend can't find the device. (DSO-17429)  
* [#9824](https://github.com/IntelRealSense/librealsense/pull/9824) - **[Viewer]** Add minimal delay between set_option commands**  (LRS-311)  
* [#9804](https://github.com/IntelRealSense/librealsense/pull/9804) - **[Core]** LibCI improvements & VS2019 (glad, atlstr) compatibility. (LRS-303)  
* [#9807](https://github.com/IntelRealSense/librealsense/pull/9807) - **[C#]** Add tutorial for network device.  (Follow-up on PR [#8809](https://github.com/IntelRealSense/librealsense/issues/8809) (LRS-200)  
* [#9792](https://github.com/IntelRealSense/librealsense/pull/9792) - **[Viewer]** Add stream index to stream window title (DSO-17647)  
* [#9794](https://github.com/IntelRealSense/librealsense/pull/9794) - **[Viewer]** Prompt for `advanced mode` when changing depth units  (LRS-202)  
* [#9782](https://github.com/IntelRealSense/librealsense/pull/9782) - **[Android]** Fix Android motion line UX.
  - Draw a centered dot under dead zone values
  - Change line color to white to match the RS Viewer UX. (LRS-122)  
* [#9789](https://github.com/IntelRealSense/librealsense/pull/9789) - **[Viewer]** Settings windows affected by measure tool (DSO-17665) 
* [#9764](https://github.com/IntelRealSense/librealsense/pull/9764) - **[Tools]** Add `rs-embed` tool
 - The tool converts 3D models into a string format suitable for embedding into C++ applications.
  - Update 3d models for the supported camera SKUs.
  - Switch the tool to use the latest LZ4 library version. (LRS-290)  
* [#9642](https://github.com/IntelRealSense/librealsense/pull/9642) - **[D400]** Presets update
  - Update hard-coded presets to the latest recommended configuration.
  - Specify D450 default preset.
  Tested by comparing the JSONs files of the new presets (in the ticket) to the exported JSONs files from the viewer. (DSO-17183)
* [#9763](https://github.com/IntelRealSense/librealsense/pull/9763) - **[Core]** Firmware check when updating in recovery mode
  - Add FW check when the device is in recovery mode.
  - Add D4xx and L5xx FW checks by the FW version when updating in recovery mode.
  - Add heuristic FW size check to sr3xxx when updating in recovery mode. 
  Follow-up on PR: [#9683](https://github.com/IntelRealSense/librealsense/issues/9683) (DSO-17448)  
* [#9770](https://github.com/IntelRealSense/librealsense/pull/9770) - **[CMake]** remove cmake deprecation warnings**  (Newer CMake was complaining of deprecations in certain source files. (LRS-108)  
* [#9767](https://github.com/IntelRealSense/librealsense/pull/9767) - **[Linux]** Lower Linux device polling interval to 2 seconds.
The time for enumeration when there are no changes is on average 0.005s, when there are changes it is 0.15s. So polling every 2s shouldn't cause too much burden on the system. (LRS-234)
* [#9768](https://github.com/IntelRealSense/librealsense/pull/9768) - **[Core]** Another escapee for a mem leak  (Follow-up to [#9762](https://github.com/IntelRealSense/librealsense/issues/9762) )  
* [#9762](https://github.com/IntelRealSense/librealsense/pull/9762) - **[Core]** avoid mem leaks in rs.cpp by taking ownership of callback ptrs. Addresses [#3873](https://github.com/IntelRealSense/librealsense/issues/3873) and [#4447](https://github.com/IntelRealSense/librealsense/issues/4447): potential memory leak in pipeline::start method (callback overloads). (DSO-13704)  
* [#9746](https://github.com/IntelRealSense/librealsense/pull/9746) - **[Viewer]** Switch off histogram equalization when min/max distance is changed.  (LRS-291)  
* [#9688](https://github.com/IntelRealSense/librealsense/pull/9688) - **[Viewer]** Fix Unreadable stream info while multi stream**  (If multiple stream views are packed closely together, the Info line overwrites the view to the right. (DSO-17042)  
* [#9732](https://github.com/IntelRealSense/librealsense/pull/9732) - **[MacOS]** Set default CHECK_FOR_UPDATES=OFF for OSX. 
  Due to a non trivial flow when building with online updates on OSX, we set the default CMake flag to OFF (on OSX only)
  Update the OSX installation guide for users who want the online updates feature on. (DSO-17551)
* [#9731](https://github.com/IntelRealSense/librealsense/pull/9731) - **[Viewer]** Window size callback is called with value of 0 for height and width.  (LRS-292)  
* [#9727](https://github.com/IntelRealSense/librealsense/pull/9727) - **[Linux]** Enable patches for kernel 5.11. Contributed by [@kevindehecker](https://github.com/kevindehecker).
* [#9715](https://github.com/IntelRealSense/librealsense/pull/9715) - **[Core]** Dix crash with desynced frames; add unit-test**  (LRS-289)  
* [#9517](https://github.com/IntelRealSense/librealsense/pull/9517) - **[Android]** UI fixes  
  Make the stream statistic view in camera app scrollable (RS5-11519)
* [#9625](https://github.com/IntelRealSense/librealsense/pull/9625) - **[Core]** Improve unsigned firmware update on android**  (Improve unsigned firmware update:
  1) use file browser to pick the firmware file in any accessible directory
  2) error handling
   (DSO-17007, DSO-17550)  
* [#9662](https://github.com/IntelRealSense/librealsense/pull/9662) - **[Android]** RS Camera viewer restarts the stream when rotating screen.
- Handle device orientation change to avoid interruption during preview, recording and playback
- Fix memory leak during device rotations (DSO-14436)

* [#9716](https://github.com/IntelRealSense/librealsense/pull/9716) - **[Core]** 
  Update frame callback logs  
  - Remove callback-started field in frame  
  - Remove frame release log message  
  - Separate composite frame release log  
  - Split archive into several files (DSO-16993)  
* [#9720](https://github.com/IntelRealSense/librealsense/pull/9720) - **[Viewer]** Depth data hides the content of export window. (DSO-17561)  
* [#9721](https://github.com/IntelRealSense/librealsense/pull/9721) - **[Viewer]** Export window affected by Measure tool  (DSO-17658)  
* [#9689](https://github.com/IntelRealSense/librealsense/pull/9689) - **[Android]** Default Started streams appears off in settings
Changes:
  - added `getActiveStreams` API in sensor and pipeline on android
  - populate default stream profiles in camera viewer settings
  - turn off all settings to reset to default streams. (DSO-15687)
* [#9711](https://github.com/IntelRealSense/librealsense/pull/9711) - **[Viewer]** Windows sometimes goes to 0 width & height.  (LRS-288)  
* [#9710](https://github.com/IntelRealSense/librealsense/pull/9710) - **[Viewer]** Disabling Measure when depth stream is stopped.  (DSO-17043)  
* [#9706](https://github.com/IntelRealSense/librealsense/pull/9706) - **[Viewer]** Measurement line is not disappeared after disabling distance Button*
Revised tooltip to make it clear how to draw a polygon. (DSO-17043)
* [#9668](https://github.com/IntelRealSense/librealsense/pull/9668) - **[Core]** Playback behavior with syncer at EOF.
Added improvements to Python wrapper usability:
  - added `frame_queue`-like `wait_for_frame` etc. to `syncer` so they can be used interchangeably
  - added pyrs.sensor.name and repr
Also:
  -added a text converter to `rs-convert` to easily see the frame content of a rosbag (LRS-172]) 
* [#9704](https://github.com/IntelRealSense/librealsense/pull/9704) - **[Viewer]** - Visual Preset value change when reselecting the default value. (LRS-119)  
* [#9677](https://github.com/IntelRealSense/librealsense/pull/9677) - **[Core]** Crash on stopping context  
Added debug messages, in general. But the issue, it seems, was that `context::stop()` only called `device_watcher::stop()` when there were NO callbacks registered. (LRS-219)
* [#9676](https://github.com/IntelRealSense/librealsense/pull/9676) - **[Demos]** 'rs-measure' example program has missing measurement value.  (DSO-17549)  
* [#9670](https://github.com/IntelRealSense/librealsense/pull/9670) - **[Core]** 
Removed unneeded ; character in serialized-utilities.cpp.  (Removed unneeded ; character) contributed by [@mgeorg](https://github.com/mgeorg)
* [#9669](https://github.com/IntelRealSense/librealsense/pull/9669) - **[Viewer]** Error message when click on pause in 3D Viewer**  (DSO-17558)  
* [#9666](https://github.com/IntelRealSense/librealsense/pull/9666) - **[Viewer]** Flash screen hourglass show a palm (hand) icon.  (LRS-177)  
* [#9656](https://github.com/IntelRealSense/librealsense/pull/9656) - **[Android]]** Controls option not begins with capital letter
Changes: Minor fix controls option not begins with capital letter (DSO-15651)
* [#9665](https://github.com/IntelRealSense/librealsense/pull/9665) - **[Tools]** Update tool dfu alert
  - Capitalize messages
  - Show list of devices without any command-line arguments
  - If device is in recovery mode, give proper message (LRS-33)  
* [#9562](https://github.com/IntelRealSense/librealsense/pull/9562) - **[Viewer]** Correct `get_folder_path` behavior.  
  For `documents` folder use `SHGetFolderPath` instead of `SHGetKnownFolderPath` because `SHGetKnownFolderPath` doesn't give the path to OneDrive documents folder but to the local documents folder. (LRS-229)
* [#9602](https://github.com/IntelRealSense/librealsense/pull/9602) - **[Android]** - Fix resource issue handling issue.  
  This is part b fix to additional issues found when switching between 2d and 3d. Changes:
  - release frame resources when switching between 2d and 3d
  - handle cases when only depth, no rgb, is part of the stream
  - handle cases when no depth is part of the stream (DSO-16584)  
* [#9011](https://github.com/IntelRealSense/librealsense/pull/9011) - **[Python]** Fix software device in Python wrapper.  
  - fix matcher for Python wrapper. contributed by [@mengyui](https://github.com/mengyui)
* [#9616](https://github.com/IntelRealSense/librealsense/pull/9616) - **[Core]** Fix Stepping Forward / Backwards with `playback` device.  (DSO-17562)  
* [#9593](https://github.com/IntelRealSense/librealsense/pull/9593) - **[Core]** Syncer fix for  not releasing frames with inactive stream.  
  - The syncer was comparing to the wrong timestamp.  
  - Reduce the threshold used to detect inactive streams (10 -> 7 gaps), which can also reduce ability to handle big latency. But it's more important to reduce change of reaching the current queue limit of 10 frames.  
  - Unit-test that shows this is WIP, to follow in another PR. (LRS-275)  
* [#9599](https://github.com/IntelRealSense/librealsense/pull/9599) - **[Viewer]** Closing the viewer app while in playback mode causes seg fault.
  - live tests crash fix + dispatcher flush() fix**  (Includes changes from PR [#9570](https://github.com/IntelRealSense/librealsense/issues/9570). (LRS-265)
* [#9595](https://github.com/IntelRealSense/librealsense/pull/9595) - **[Core]** Enable `Global_timestamp` domain with ARM architecture for Raspberry PI.  
  Revert patch of commit e992be3b84f164e0e917f6fa0adad9f50e0. Address issue in [librealsense-ros[#1906](https://github.com/IntelRealSense/librealsense/issues/1906)](https://github.com/IntelRealSense/realsense-ros/issues/1906) (DSO-17352)  
* [#9518](https://github.com/IntelRealSense/librealsense/pull/9518) - **[Android]** Sensor callback improvement.  (DSO-17538)
* [#9575](https://github.com/IntelRealSense/librealsense/pull/9575) - **[Viewer]** - Alert user on recovery device bundled update
  - Expose PID for recovery device
  - Add notification for recovery device update. (DSO-17527)  
* [#9559](https://github.com/IntelRealSense/librealsense/pull/9559) - **[Android]** device watcher add is stopped**  (LRS-257)  
* [#9498](https://github.com/IntelRealSense/librealsense/pull/9498) - **[D400]** HDR merge - discarding merged frame on resume.  (DSO-17341)  
* [#9553](https://github.com/IntelRealSense/librealsense/pull/9553) - **[Core]** Remove circular shared_ptr dependency in Pointcloud class. 
  - Handle memory leak in PointCloud processing block
  - Addresses a long-standing issue [#2860](https://github.com/IntelRealSense/librealsense/issues/2860). (DSO-13695)
* [#9540](https://github.com/IntelRealSense/librealsense/pull/9540) - **[Core]** Update LZ4 compression utility to version 1.9.3**  
  - (**Benchmark results of compress + write RGB 1280x720 frame (frame only no additional data)**
  - Tested on Windows 10. Laptop HP Elitebook 850 G6
  - 1.8.0 benchmark results: AVG time: 53 [ms], Min time: 40 [ms], Max time: 67 [ms]
  - 1.9.3 benchmark results: AVG time: 23 [ms], Min time: 19 [ms], Max time: 39 [ms]
  - Performance improvement of :~ 50% (LRS-34)  
* [#9253](https://github.com/IntelRealSense/librealsense/pull/9253) - **[MacOS]** Fix build on macOS arm64.  
  This fixes the build on macOS running on Apple Silicon (arm64). contributed by [@prusnak](https://github.com/prusnak)  
* [#9496](https://github.com/IntelRealSense/librealsense/pull/9496) - **[Android]** Java example adjusted to Android X**  (LRS-249)  
* [#9525](https://github.com/IntelRealSense/librealsense/pull/9525) - **[Python]** `receiver sensitivity` option added to python wrapper**  (LRS-258)  
* [#9514](https://github.com/IntelRealSense/librealsense/pull/9514) - **[Android]** Memory leak fix. 
  Release point cloud resources when clearing the rendering surface. (DSO-16584)  
* [#9503](https://github.com/IntelRealSense/librealsense/pull/9503) - **[Core]** Fix for `device_watcher` class.
  - start it on register_internal_device_callback**  (Device_hub API registers for events using `register_internal_device_callback` but this didn't start the device-watcher... we weren't getting device change notifications. (DSO-11850).
* [#9502](https://github.com/IntelRealSense/librealsense/pull/9502) - **[Core]** Add missing DS error codes.  (DSO-16036)  
* [#9522](https://github.com/IntelRealSense/librealsense/pull/9522) - **[Core]** Protect playback sensor call to user callback (LRS-255)  
* [#9509](https://github.com/IntelRealSense/librealsense/pull/9509) - **[Viewer]** FW Update via popup fix.  (LRS-250)  
* [#9497](https://github.com/IntelRealSense/librealsense/pull/9497) - **[Core]** Fix zero depth for SR300.
  - Add UT for depth units from metadata. (LRS-248)
* [#9491](https://github.com/IntelRealSense/librealsense/pull/9491) - **[Viewer]** - Handle crash with L515 with USB2.
  Fixes issue introduced in [#9382](https://github.com/IntelRealSense/librealsense/issues/9382)
 (LRS-243).  
* [#9490](https://github.com/IntelRealSense/librealsense/pull/9490) - **[Viewer] Fix pointcloud for L515  
  Rectifies improver assignment of `depth_units`. Addresses [#9154](https://github.com/IntelRealSense/librealsense/issues/9154). (LRS-240)  
* [#9486](https://github.com/IntelRealSense/librealsense/pull/9486) - **[Core]** GCC 7.5 fixes.  
  - Add explicit `#include` reference
  - Remove warning - drop unnecessary struct in signature.  
* [#9482](https://github.com/IntelRealSense/librealsense/pull/9482) - **[CUDA]** Fix building with CUDA accelerators.
  - Hot-fix to propagate depth-units refactoring into CUDA-enabled build. Rectifies [#9154](https://github.com/IntelRealSense/librealsense/issues/9154)


### Documentation
* [#9633](https://github.com/IntelRealSense/librealsense/pull/9633) - **[Jetson]** Fix doc-jetson-installation.  
  This PR fixes the installation for jetson, which is currently broken for the second part. It also has some minor editorial changes to enhance readability.) contributed by [@tkazik](https://github.com/tkazik).
* [#9849](https://github.com/IntelRealSense/librealsense/pull/9849) - **[Core]** Update installation readme for libudev + add ROS dependency**  (LRS-315)  
* [#9834](https://github.com/IntelRealSense/librealsense/pull/9834) - **[Jetson]** Update installation_jetson.md.  Fix markdown grammar error. contributed by [@Sn-Kinos](https://github.com/Sn-Kinos)
* [#9781](https://github.com/IntelRealSense/librealsense/pull/9781) - **[Python]** Update python wrapper docs 
  Add a note about building pyrealsense from source as a self-contained library ([#9760](https://github.com/IntelRealSense/librealsense/issues/9760)) (LRS-293)  
* [#9719](https://github.com/IntelRealSense/librealsense/pull/9719) - **[Jetson]** Update installation_jetson.md. Contributed by [@DestinyOne](https://github.com/DestinyOne)  
* [#9577](https://github.com/IntelRealSense/librealsense/pull/9577) - **[Core]** Document LRS Viewer version update notifications flow**  (LRS-268)  
* [#9547](https://github.com/IntelRealSense/librealsense/pull/9547) - **[Demo]** Fix record and playback readme link**  (LRS-218)  
* [#9235](https://github.com/IntelRealSense/librealsense/pull/9235) - **[MacOS]** Update out-of-date brew command in macOS guide. contributed by [@zgotsch](https://github.com/zgotsch)
* [#9499](https://github.com/IntelRealSense/librealsense/pull/9499) - **[Core]** Merge record and playback readme files.  (LRS-218)

## Release 2.49.0
Release Date: 10th Aug 2021

### API Changes
https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2490

### New Features
* [#8925](https://github.com/IntelRealSense/librealsense/pull/8925) - **[D400]** Fw version 5.12.15.50.
  - Fixes and stability improvements
* [#9273](https://github.com/IntelRealSense/librealsense/pull/9273) - **Wrappers** Support OpenVino 2021  
Add support for OpenVino 2020 and 2021.(LRS-71)

### Bug Fixes and Enhancements
* [#9509](https://github.com/IntelRealSense/librealsense/pull/9509) - **[Tools]** Viewer - fw update via popup fixed. (LRS-250)
* [#9497](https://github.com/IntelRealSense/librealsense/pull/9497) - **[Core]** Fix SR300 - Depth-units control propagation. (LRS-248)  
* [#9491](https://github.com/IntelRealSense/librealsense/pull/9491) - **[Core]** Fix L515 - Depth-units control propagation. (LRS-243)  
* [#9490](https://github.com/IntelRealSense/librealsense/pull/9490) - **[Core]** Fix L515 - Pointcloud generation. (LRS-240)  
* [#9486](https://github.com/IntelRealSense/librealsense/pull/9486) - **[Core]** Fix Linux compilation with GCC v7.5.  
* [#9482](https://github.com/IntelRealSense/librealsense/pull/9482) - **[Core]** Fix CUDA implementation for Pointcloud.  
* [#9435](https://github.com/IntelRealSense/librealsense/pull/9435) - **[C#]** Restructure tutorials & add load-json  
Rebase of [#8653](https://github.com/IntelRealSense/librealsense/issues/8653). (LRS-59)  
* [#9470](https://github.com/IntelRealSense/librealsense/pull/9470) - **[Core]** json preset files format update. (LRS-206)  
* [#9467](https://github.com/IntelRealSense/librealsense/pull/9467) - **[Core]** Adjust cmake target for Presets.  
The presets will be deployed in `$ENV{HOME}/Documents/librealsense2/presets` (LRS-215)
* [#9465](https://github.com/IntelRealSense/librealsense/pull/9465) - **[Core]** Update preset json format + Add device info
Change preset json format to SCHEMA_VERSION = 1
* Reading backward compatible - old style (flat) preset json files is supported. (LRS-210)  
* [#9458](https://github.com/IntelRealSense/librealsense/pull/9458) - **[Core]** Create .preset files for planned presets.  
 Added json files:
  - Max Range With Binning.preset
  - Max Range.preset 
  - Medium Range.preset
  - Min Range.preset.  
 (LRS-206)
* [#9382](https://github.com/IntelRealSense/librealsense/pull/9382) - **[Core]** 0x0B5B to support OEM Cal resolutions
Allow streaming non-native resolutions simultaneously. (DSO-17425)  
* [#9441](https://github.com/IntelRealSense/librealsense/pull/9441) - **[L515] Add RS2_OPTION_RECEIVER_SENSITIVITY control
Integrate into Core and wrappers. (RS5-11828)  
* [#9271](https://github.com/IntelRealSense/librealsense/pull/9271) - **[Tools]** Enhance rs-terminal with files input
Pass binary blobs from ANSI files. (DSO-17305)  
* [#9416](https://github.com/IntelRealSense/librealsense/pull/9416) - **[Core]** Remove unused member
`satisfied_streams` input removed from `map_sub_device method(...`  (LRS-212)  
* [#9414](https://github.com/IntelRealSense/librealsense/pull/9414) - **[CI]** Build CI: re-enable Node.js job
  - Fix errors from `esling` and `cpplint`
  - Added enum checks after linter
  - Changed folder for npm install 
  (LRS-203) contributed by [@whsol](https://github.com/whsol)
* [#9321](https://github.com/IntelRealSense/librealsense/pull/9321) - **[Linux]** Clone only the files of the tag for Tegra.  
Cloning the Tegra kernel sources repository was taking so much time. It couldn't complete even once on my device. With this change*, it only retrieves the tagged version of the repository. The whole process can complete in less than 5 minutes. Refers to https://stackoverflow.com/questions/20280726/how-to-git-clone-a-specific-tag contributed by [@rszengin](https://github.com/rszengin)
* [#9431](https://github.com/IntelRealSense/librealsense/pull/9431) - **[Android]** FW names convention update L5xx -> L51x & L53x
Follows on [#9264](https://github.com/IntelRealSense/librealsense/issues/9264)Fixes Jenkins CI nightly. (LRS-214)
* [#9264](https://github.com/IntelRealSense/librealsense/pull/9264) - **L5XX**  FW bundling
  - Change L5XX_FW_Image to L51X_FW_Image  
  - Added support for L53X FW bundling.
  - Added `recommended_fw_version` for L5XX. (RS5-11533)
* [#9415](https://github.com/IntelRealSense/librealsense/pull/9415) - **[Core]** Add parser for Y411 packed format
Includes [#9065](https://github.com/IntelRealSense/librealsense/issues/9065) and [#9380](https://github.com/IntelRealSense/librealsense/issues/9380)
(LRS-29)
* [#9396](https://github.com/IntelRealSense/librealsense/pull/9396) - **[L53X]** Added min-max fw_version check  
(LRS-143)
* [#9380](https://github.com/IntelRealSense/librealsense/pull/9380) - **[Core]** Tunnel Y411 format as NV12; OpenGL processing**  (LRS-29)  
* [#9394](https://github.com/IntelRealSense/librealsense/pull/9394) - **[Core]** Fix playback deadlock on live tests (Git Actions CI fail)
  - Bring back playback sensor flush on end-of-file or else last frames may get lost.  
  - Move sensor stop outside of mutex lock.  
  - Fix 2 unit-tests that did not stop & close the sensors.  
(LRS-190)
* [#9379](https://github.com/IntelRealSense/librealsense/pull/9379) - **[Core]** Drop `static` from function signature
Follow up on [#9096](https://github.com/IntelRealSense/librealsense/issues/9096). Addresses [#6055](https://github.com/IntelRealSense/librealsense/issues/6055). (DSO-17046)
* [#9352](https://github.com/IntelRealSense/librealsense/pull/9352) - **[Android]** Sensor API `getProfile` added, sensor example using it
(LRS-182)
* [#8873](https://github.com/IntelRealSense/librealsense/pull/8873) - **[Wrappers]** Add lrs-net viewer for Python
(LRS-174)
* [#9339](https://github.com/IntelRealSense/librealsense/pull/9339) - **[Viewer]** Add support for recovery device in online update flow
  - Add recovery device support
  - Fix misaligned UI
  - Remove "Update URL" button from settings
(LRS-64)
* [#9256](https://github.com/IntelRealSense/librealsense/pull/9256) - **[C#]** Add D400 Calibration APIs extensions. (LRS-74/LRS-75/DSO-16811)  
Follow up on @DenisGeek's work in [#8773](https://github.com/IntelRealSense/librealsense/issues/8773)
* [#9320](https://github.com/IntelRealSense/librealsense/pull/9320) - **[Android]** API fixes
  - `openSensor` with multiple profiles API added (LRS-132)
Using Sensor example, we get both depth and infrared profiles from the depth sensor.
* [#9285](https://github.com/IntelRealSense/librealsense/pull/9285) - **[Demos]** Connect to rs-server and render frames by means of GPU  
This sample demonstrates how to connect to remote rs-server over the network and to utilize the GPU for processing of depth frames received from remote camera. This sample is based on `rs-gl` sample and appears when generating solution by cmake with BUILD_GRAPHICAL_EXAMPLE , BUILD_GLSL_EXTENSIONS and BUILD_NETWORK_DEVICE flags is ON) 
* [#9065](https://github.com/IntelRealSense/librealsense/pull/9065) - **[Core]** Support for Y411 format  (Reopened [#8871](https://github.com/IntelRealSense/librealsense/issues/8871)
Native & SSE support included. (RS5-10803)
* [#9301](https://github.com/IntelRealSense/librealsense/pull/9301) - **[Tools]** viewer - add informative tooltip message
...when Measure is disabled (DSO-17011)
* [#9300](https://github.com/IntelRealSense/librealsense/pull/9300) - **[Core]** Depth Units refactoring fix
Fix correct depth units extraction from ROSbag records (lRS-140)
* [#9309](https://github.com/IntelRealSense/librealsense/pull/9309) - **[Core]** Syncer mutex to fix single_consumer_queue::peek() issues
And revised peek() mechanism, too... (LRS-162)
* [#9261](https://github.com/IntelRealSense/librealsense/pull/9261) - **[L515]** Set all the values of amc controls on start up.
(Fixes RS5-11488)
* [#9299](https://github.com/IntelRealSense/librealsense/pull/9299) - **[Core]** Fix playback crash in Dtor 
(LRS-135)
* [#9254](https://github.com/IntelRealSense/librealsense/pull/9254) - **[L515]** Control Vertical binning.
  - Added `RS2_OPTION_VERTICAL_BINNING` option - Enables vertical binning which increases the maximal sensed distance.
  - Added support on wrappers
(RS5-11533)
* [#9276](https://github.com/IntelRealSense/librealsense/pull/9276) - **[Wrappers]** include USB info on pyrs device repr()  
Export data in the following format
  `<pyrealsense2.device: Intel RealSense L515 (S/N: f9440703  FW: 01.05.05.00  UNLOCKED  on USB3.2)>`  
(LRS-129)
* [#9274](https://github.com/IntelRealSense/librealsense/pull/9274) - **[Tools]** Rosbag-Inspector fix.  
Addresses issue introduced with [#9154](https://github.com/IntelRealSense/librealsense/issues/9154)
* [#9258](https://github.com/IntelRealSense/librealsense/pull/9258) - **[Core]** Handle Dispatcher exception + high CPU usage fix**  (Continuation of [#9219](https://github.com/IntelRealSense/librealsense/issues/9219).
(LRS-125)  
* [#9154](https://github.com/IntelRealSense/librealsense/pull/9154) - **[Core]** Add Depth Units value to frame metadata
(DSO-16066)  
* [#9255](https://github.com/IntelRealSense/librealsense/pull/9255) - **[Core]** D4xx Default preset with sub-millimetric units
(DSO-17316)
* [#9240](https://github.com/IntelRealSense/librealsense/pull/9240) - **[Core]**  change default resolutions for 0xB5B SKU**  (LRS-81) 
* [#9228](https://github.com/IntelRealSense/librealsense/pull/9228) - **[Viewer]** FW build version is not properly displayed
Fixed regex that parses the version (DSO-17194)
* [#9238](https://github.com/IntelRealSense/librealsense/pull/9238) - **[Viewer]** - depth units changing fix
(LRS-124)
* [#9230](https://github.com/IntelRealSense/librealsense/pull/9230) - **[D400]** Advanced controls have no min/max
(DSO-17312)
* [#9213](https://github.com/IntelRealSense/librealsense/pull/9213) - **[Viewer]** - Fix decimal point display
* [#9221](https://github.com/IntelRealSense/librealsense/pull/9221) - **[Core]** Memory leak in IMU/Win
Addresses [#8091](https://github.com/IntelRealSense/librealsense/issues/8091) (DSO-17238, DSO-17239)
* [#9225](https://github.com/IntelRealSense/librealsense/pull/9225) - **[Tools]** DQT - Fix IR reflectivity display
Fix issue created on [#9061](https://github.com/IntelRealSense/librealsense/issues/9061) (LRS-117)
* [#9222](https://github.com/IntelRealSense/librealsense/pull/9222) - **[Tools]** Viewer syncer loop correction
Switching to IR in 3D texture source froze the view.
The render_loop() depends only on `synchronization_enable` but sometimes we bypass it and so it cannot get the frames...
(LRS-116)
* [#9223](https://github.com/IntelRealSense/librealsense/pull/9223) - **[Tools]** DQT - do not set sensor mode while streaming
Caused by [#9081](https://github.com/IntelRealSense/librealsense/issues/9081).
(LRS-115)
* [#9209](https://github.com/IntelRealSense/librealsense/pull/9209) - **[Core]** fix CppCheck warning in string-utilities.h

### Documentation
* [#9345](https://github.com/IntelRealSense/librealsense/pull/9345) - **[MacOS]** Update vulkan brew install instructions
`brew cask install` has been superseded by `brew install --cask`  
If you use `brew cask install`, you will be greeted with `Error: Unknown command: cask`.  contributed by [@alkasm](https://github.com/alkasm)
* [#9436](https://github.com/IntelRealSense/librealsense/pull/9436) - **[MacOS]** Update installation_osx.md
Homebrew doesn't use "cask" anymore. IF you try this build process you get  this error `error: Unknown command: cask`
I've updated the doc to use the new way of doing things. (Basically just trash the cask part of the install) Ive tested this works.) contributed by [@martystack](https://github.com/martystack)
* [#9433](https://github.com/IntelRealSense/librealsense/pull/9433) - **[Debians]** Update PPA public server key repo
Fix PPA public key registration for X86 and Jetsons due to the deprecation of `keys.gnu.net`. Uses the [alternative server](https://github.com/IntelRealSense/librealsense/issues/9289#issuecomment-873457374) mentioned in [#9289](https://github.com/IntelRealSense/librealsense/issues/9289) by @hanzheteng. (LRS-204)
* [#9412](https://github.com/IntelRealSense/librealsense/pull/9412) - **[Arch]** Add librealsense2 playback readme file.

### Known issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block.
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized variable. (DSO-13700)
* [#4261](https://github.com/IntelRealSense/librealsense/issues/4261) - [T265] Add ability to open multiple devices from different processes.
* [#4518](https://github.com/IntelRealSense/librealsense/issues/4518) – [T265] Pose data produces `NaNs`. Can still occur in some cases. If detected, please attempt to make a raw data (images + IMU) recording using the [recorder tool](https://github.com/IntelRealSense/librealsense/tree/master/tools/recorder), and attach a link to it in the github issue, to assist our resolution.
* [#6009](https://github.com/IntelRealSense/librealsense/issues/6009) v2.33.1 does not compile with -DBUILDEASYLOGGINGPP=OFF
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* (DSO-13525) - [D400] 3D viewer moved when sliding the tare calibration sliders

## Release 2.48.0
Release Date: 29th June 2021

### API Changes
https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2480

### New Features
* [#8905](https://github.com/IntelRealSense/librealsense/pull/8905) - **Android** - Support Sensor API for streaming  
    - Add sensor open/close/start/stop with callbacks
    - Add callbacks to pipeline API with default and custom configurations
    - Demo apps to use the new APIs
    - Impact to existing API (these are bugs in current implementation and the change should not impact existing apps):
    -  Extend Java class FrameSet from Frame so the frame from pipeline callback can be cast to a set of frames.
    - Add deleter to Java Sensor API with content from old close.  
(DSO-16739, DSO-16740)
* [#8860](https://github.com/IntelRealSense/librealsense/pull/8860) - **[C#]** Example for Pose type stream for T265 device


### Bug Fixes and Enhancements
* [#9219](https://github.com/IntelRealSense/librealsense/pull/9219),[#9159](https://github.com/IntelRealSense/librealsense/issues/9159),[#9210](https://github.com/IntelRealSense/librealsense/issues/9210) - **[Core]**  Concurrency & Syncer rework    Following the deadlock that Nir experienced, reworked some stuff (and not all because of the deadlock:
> * split off dispatcher code into dispatcher.cpp
> * syncer (timestamp) inactive stream handling moved into skip_missing_stream
> * syncer identity_matcher names now include the stream index
> * single_consumer_queue::_mutex is now mutable; size() and empty() const
> * revised logic inside single_consumer_queue
> * revised logic inside dispatcher
> * cancellable_time::try_sleep() is now templated, requires actual chrono duration (and not just its ::Rep)
> * syncer stop() calls now percolate recursively to shut down all queues; new syncs do nothing if syncer was stopped
> * add actual matchers to playback device
> * live-test issues with playback
> * added !mayfail to frame-number syncing test for stability
> * added waiting_on<> in utilities/time/waiting-on.h, with unit-test
> * rework dispatcher::flush()  
DSO-16888
* [#9221](https://github.com/IntelRealSense/librealsense/pull/9221) - **[Core]** Fix Memory leak on IMU sensor query (DSO-17238, DSO-17239 )
* [#9210](https://github.com/IntelRealSense/librealsense/pull/9210) - **[Core]** Fix Composite frame with playback device
  Add actual matchers to playback device
* live-test issues with playback
* added !mayfail to frame-number syncing test for stability
* added waiting_on<> in utilities/time/waiting-on.h, with unit-test
* rework dispatcher::flush()  
DSO-16888
* [#9188](https://github.com/IntelRealSense/librealsense/pull/9188) - **[Python]** box_dimensioner_multicam_demo error fix for L515 (RS5-11319)
* [#8156](https://github.com/IntelRealSense/librealsense/pull/8156) - **[Examples]**  Recording Bag file under "Program Files".  
Now it attemts to write to temp folder first
* [#9207](https://github.com/IntelRealSense/librealsense/pull/9207) - **[Viewer]** Fix crash when opening 'Stereo Module' of a playback

* [#8884](https://github.com/IntelRealSense/librealsense/pull/8884) - **[Examples]** Connect to rs-server and draw frames by means of GPU  
  This sample demonstrates how to connect to remote rs-server over the network and to utilize the GPU for processing of depth frames received from remote camera. This sample is based on rs-gl sample and appears when generating solution by cmake with BUILD_GRAPHICAL_EXAMPLES and BUILD_GLSL_EXTENSIONS flags is ON.  
* [#9185](https://github.com/IntelRealSense/librealsense/pull/9185) - **[L515]** FW version compatibility fix  
L515 FW version prerequisites must be defined with min/max range to avoid cross-product versions reference.
Can be seen as a temporal solution since FW versions may overlap eventually.
Update L515 minimal downgradeable version to 1.5.1.3 instead of 1.4.1.0.  
(RS5-11513), (DSO-16641)

* [#9183](https://github.com/IntelRealSense/librealsense/pull/9183) - **[Core]** Increase the syncer queue on model-views.h to avoid frame drops.  
This queue stores the output from the syncer, and has to be large enough to deal with slowdowns on the processing / rendering threads of the Viewer, to avoid frame drops.
The problem here wasn't slowdowns of the other thread -- it was bust output from the syncer, too fast for any other thread to deal with and causing drops. The bursts are caused by latency introduced on the USB/FW side when different streams have different FPS.  
(DSO-16419)
* [#9145](https://github.com/IntelRealSense/librealsense/pull/9145) - **[Core]** 0x0B5B SKU to display in cm**  (DSO-17052)
* [#9081](https://github.com/IntelRealSense/librealsense/pull/9081) - **[L515]** Make sensor-mode r/o during streaming  (LRS-7)
* [#9153](https://github.com/IntelRealSense/librealsense/pull/9153) - **[Core]** 0x0B5B SKU Fix Color stream calibration retrieval  (DSO-17104)
* [#9132](https://github.com/IntelRealSense/librealsense/pull/9132) - **[Core]** 0x0B5B SKU remove intercam sync support  (DSO-17061)
* [#8674](https://github.com/IntelRealSense/librealsense/pull/8674) - **[Core]** 
Fix Extrinsic map build algorithm  (DSO-16814)
* [#9061](https://github.com/IntelRealSense/librealsense/pull/9061) - **[Viewer]** Fix ROI display rectangle selection  (DSO-16748)

### Known issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block.
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized variable. (DSO-13700)
* [#4261](https://github.com/IntelRealSense/librealsense/issues/4261) - [T265] Add ability to open multiple devices from different processes.
* [#4518](https://github.com/IntelRealSense/librealsense/issues/4518) – [T265] Pose data produces `NaNs`. Can still occur in some cases. If detected, please attempt to make a raw data (images + IMU) recording using the [recorder tool](https://github.com/IntelRealSense/librealsense/tree/master/tools/recorder), and attach a link to it in the github issue, to assist our resolution.
* [#6009](https://github.com/IntelRealSense/librealsense/issues/6009) v2.33.1 does not compile with -DBUILDEASYLOGGINGPP=OFF
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* (DSO-13525) - [D400] 3D viewer moved when sliding the tare calibration sliders

## Release 2.47.0
Release Date: 6th June 2021

### API Changes
https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2470

### New Features
* [#9167](https://github.com/IntelRealSense/librealsense/pull/9167) - **[L515] FW version 1.5.8.1**
* [#9165](https://github.com/IntelRealSense/librealsense/pull/9165) - **[D4xx] FW version 5.12.14.50**  
 - Disparity Modulation enhancements (DSO-17067)
 - Handle pattern under strong light in `Slave` mode. (DSO-17022).  
   Fixes and stability improvements
* [#8948](https://github.com/IntelRealSense/librealsense/pull/8948) - **[Core]** Firmware Compatibility check as part of Firmware Update  
  Check and verify the FW image prior to the firmware update to prevent loss of functionality and/or physical damage to the device.
  (DSO-16958)  
* [#8977](https://github.com/IntelRealSense/librealsense/pull/8977),[#8906](https://github.com/IntelRealSense/librealsense/pull/8906) - **[Tools]** `rs-convert` enhancements.  
Support IMU and Pose serialization into csv format  (DSO-16987).
* Support for Ubuntu20/Kernel 5.8 DKMS package (v1.3.18)

### Bug Fixes and Enhancements
* [#9060](https://github.com/IntelRealSense/librealsense/pull/9060) - **[Linux]** Memory leak with option query  
* [#9008](https://github.com/IntelRealSense/librealsense/pull/9008) - **[Viewer]**
 Online updates  
 Add up to date notification  
 Reset notification delays when user click on check for updates
 (RS5-11310)
 * [#9051](https://github.com/IntelRealSense/librealsense/pull/9051) - **[Viewer]** No 3D image on playback  
  Point cloud frames are ignored during playback / DQT 3D streaming
 The fix allows the Point cloud frames to be uploaded to the rendering window
 (RS5-11334)
* [#8967](https://github.com/IntelRealSense/librealsense/pull/8967) - **[Viewer]** Software Update enhancements
 (DSO-16893)
* [#9019](https://github.com/IntelRealSense/librealsense/pull/9019) - **[D4xx]** Remove IR pattern for relevant devices  (DSO-17102)
* [#9017](https://github.com/IntelRealSense/librealsense/pull/9017) - **[D4xx]** Remove emitter-on-off support from 0x0B5B  (DSO-17048)
* [#9041](https://github.com/IntelRealSense/librealsense/pull/9041) - **[SR300]** Support for alternative 0x0AA2 PID  (DSO-17101)
* [#9034](https://github.com/IntelRealSense/librealsense/pull/9034) - **[L515]** HWM_OVER_XU logic rework
* [#8804](https://github.com/IntelRealSense/librealsense/pull/8804) - **[Core]** Validate Range on controls change  (DSO-13749)
* [#8875](https://github.com/IntelRealSense/librealsense/pull/8875) - **[Linux/v4l]** Memory leak test sensor option.    
Add test for memory issue using valgrind and a fix.(DSO-16885)
* [#9035](https://github.com/IntelRealSense/librealsense/pull/9035) - **[3rd party]** change libjpeg-turbo branch to 'main' 
`master`... branch was renamed to `main`
(cherry picked from commit 9d58316dba78e93ad89aeeace5670c915be2435e))
* [#9021](https://github.com/IntelRealSense/librealsense/pull/9021) - **[Unity]** Modified editor script for Unity on Linux  
 Changed `BuildAllAssetBundles` method for use in Unity on Linux
* [#8876](https://github.com/IntelRealSense/librealsense/pull/8876) - **[SR300]** Fix SR3xx firmware image size and recovery checks.  
 Adjust recognized FW sizes for SR3xx.  
 Modify SR3xx recovery device condition for Win10 build 1909. (DSO-16985)
* [#8749](https://github.com/IntelRealSense/librealsense/pull/8749) - **[SR300]** Enabling LRS examples/tutorials on SR306  (DSO-16721)
* [#9003](https://github.com/IntelRealSense/librealsense/pull/9003) - **[L515]** Add Transmitter Frequency control
 Enable receiver sensitivity according to ambient light, bounded by the Receiver Gain control.  
 Fix removal of Max Useful Range and IR Reflectivity. (RS5-11278)  
* [#8921](https://github.com/IntelRealSense/librealsense/pull/8921) - **[L515]** Metadata display negative brightness and hue (DSO-14036)
* [#8952](https://github.com/IntelRealSense/librealsense/pull/8952) - **[L5xx]** Support for new options.  
- Create 0x0b68 hw_options class for options.
- Remove option RS2_OPTION_DIGITAL_GAIN not applicable for 0x0b68 .  
- Add option RS2_OPTION_RX_SENSITIVITY.  
- Remove all preset values (except custom) for now - until we will have declaration of presets for 0x0b68.  
- Remove options `RS2_OPTION_ENABLE_MAX_USABLE_RANGE` and `RS2_OPTION_ENABLE_IR_REFLECTIVITY` - for now until we will have declaration of them for 0x0b68.  
* [#8979](https://github.com/IntelRealSense/librealsense/pull/8979) - **[L515]** Remove AC.    
Backwards-compatibility is maintained at the level of the C runtime, but not the wrappers or C/C++ compilations:
`rs2_cah_trigger`, for example, was removed -- code using it will not compile.
Likewise, the options for TRIGGER and RESET are still there, but nothing supports them and so they have been removed from most wrappers.
Interfaces like `calibrated_sensor` are still there, but no longer in use.

* [#8896](https://github.com/IntelRealSense/librealsense/pull/8896) - **[CUDA]** Fix unpack from sr300 `CUDA` implementation  
This PR fixes a bug with incorrect depth image on the SR300 series cameras when librealsense is built with CUDA. Addresses [#8897](https://github.com/IntelRealSense/librealsense/issues/8897)].
The functions `unpack_z16_y8_from_sr300_inzi` and `unpack_z16_y16_from_sr300_inzi` use the source data pointer (`in`) - the first half of the data is bit-shifted and stored in `dest[1]`, the second half is directly copied to `dest[0]`.  
In the CPU version, the pointer is moved in a loop and after its execution it points to `in + count` - so the copying works as it should.  
In the `CUDA` version, the pointer is never moved, so bad data is copied to `dest[0]`, resulting in an incorrect depth image.  
Simply moving the pointer to the right place fixes the problem. ) contributed by [@andrusza2](https://github.com/andrusza2)
* [#8910](https://github.com/IntelRealSense/librealsense/pull/8910) - **[Core]** Signed Firmware image check before updating.  
Throw a informative exception message when trying to update signed firmware image if its corrupted or unsupported by the specific device. (DSO-16641)
* [#8946](https://github.com/IntelRealSense/librealsense/pull/8946) - **[D4xx]** Depth Units default value added  (DSO-16963)
* [#8950](https://github.com/IntelRealSense/librealsense/pull/8950) - **[D4xx]** Fix preset setting when not streaming  (DSO-15585)
* [#8913](https://github.com/IntelRealSense/librealsense/pull/8913) - **[Core]** Replace GVD queries with device update notification
 ..to verify device removal during DFU.    
Add a ~6 seconds maximum loop to verify the device is disconnected after the DFU command (replacing HWM calls)
This partly reverts PR [#8018](https://github.com/IntelRealSense/librealsense/issues/8018) which caused freezes after DFUs. (RS5-11058)
* [#8920](https://github.com/IntelRealSense/librealsense/pull/8920) - **[CMake]** Flag to control AVX for OpenVINO  
 Add LRS CMake flag `BUILD_WITH_CPU_EXTENSIONS` [default=ON]. Will affect LRS use of CPU extensions in the future.  
 If `BUILD_WITH_CPU_EXTENSIONS` == OFF => disable AVX support for OpenVINO.
* [#8912](https://github.com/IntelRealSense/librealsense/pull/8912) - **[Viewer]** SW update - support 3-field version.  
  DB file parsing will now support 3 fields version (as release versions are represented). i.e "version": "1.5.4".  
  Before the fix the regex supported 3 fields only with an ending period.
  i.e.  "version: 1.5.4."
* [#8867](https://github.com/IntelRealSense/librealsense/pull/8867) - **[Viewer]** Stop FW logs with button  (DSO-16977)
* [#8903](https://github.com/IntelRealSense/librealsense/pull/8903) - **[Viewer]** crash when close app (RS5-10427)
* [#8869](https://github.com/IntelRealSense/librealsense/pull/8869) - **[Python]** Add GIL release to enter_update_state() function
`enter_update_state()` can take ~2.5 seconds and need to release the GIL for other function to get CPU time
* [#8820](https://github.com/IntelRealSense/librealsense/pull/8820) - **[Core]** Protect illegal access on types.h floats**  (Fix CppCheck warning on types.h
i.e: src\types.h:585:24: warning: The address of local variable 'x' might be accessed at non-zero index. [objectIndex] return (&x)[i];) 
* [#8861](https://github.com/IntelRealSense/librealsense/pull/8861) - **[Android]** Android Camera app - Update settings UI to ExpandableListView
Fix an issue where not all settings items are accessible at landscape view (RS5-8610)
* [#8821](https://github.com/IntelRealSense/librealsense/pull/8821) - **[Core]** Realsense2 warnings fixed.   
* [#8856](https://github.com/IntelRealSense/librealsense/pull/8856) - **[Viewer]** Fixed frameset handling on viewer
Open the frameset on `viewer_model::handle_ready_frames` and save the frames on `last_frames`, so if one of the frames missing we will take the last frame saved.
(DSO-16419)
* [#8525](https://github.com/IntelRealSense/librealsense/pull/8525) - **[ImGui] Clamp slider rounding operations to keep ranged values**  
ImGUI floating point interpolations and rounding operations do not ensure min/max range on results.
The PR fixes it to avoid overriding user-defined min/max slider boundaries.
(DSO-15067)
* [#8805](https://github.com/IntelRealSense/librealsense/pull/8805) - **[Core]** Fix warnings in sensor.cpp.
* [#8822](https://github.com/IntelRealSense/librealsense/pull/8822) - **[CMake]** Make `BUILD_GRAPHICAL_EXAMPLES` depend on `BUILD_EXAMPLES`
* [#8817](https://github.com/IntelRealSense/librealsense/pull/8817) - **[Viewer]** Increase updates windows stability on multiple updates  (**Changes:**
- Fix Download & Install button missing on multiple update processes (DSO-16891)
- Use typedef to version to info map
- Inhibit updates popup when updates windows is on - even when only recommended available (Can occur after updating essential update)
- Break "check_for_updates" to several functions for simplicity.
* [#8802](https://github.com/IntelRealSense/librealsense/pull/8802) - **[CMake]** Add BUILD_TOOLS flag to CMake  
`BUILD_TOOLS` is `ON` by default!
NOTE: this may break some builds that have disabled `BUILD_EXAMPLES` (therefore tools weren't built) but that will now automatically get `BUILD_TOOLS` so their tools will build (perhaps with errors).
* [#8816](https://github.com/IntelRealSense/librealsense/pull/8816) - **[Viewer]** Inhibit checking for updates on playback device (Part of RS5-8604)
* [#8811](https://github.com/IntelRealSense/librealsense/pull/8811) - **[DQT]** Remove online updates
* [#8807](https://github.com/IntelRealSense/librealsense/pull/8807) - **[Viewer]** Recommended update pop up hot fix.  
Display updates window only on essential updates.  
If no essential updates, recommended updates will show as a pop up.  
The fix adds a version check for getting the SW/FW updates.   

### Documentation
* [#8986](https://github.com/IntelRealSense/librealsense/pull/8986) - **Update post-processing-filters.md**.  
I am not sure if it was supposed to be dec_filter or decimation_filter (same for spatial_filter) contributed by [@c-flew](https://github.com/c-flew)
* [#8935](https://github.com/IntelRealSense/librealsense/pull/8935) - **No more distinction for different Ubuntu required**  (=> Use bash for codename) contributed by [@tkazik](https://github.com/tkazik)

### Known issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block.
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized variable. (DSO-13700)
* [#4261](https://github.com/IntelRealSense/librealsense/issues/4261) - [T265] Add ability to open multiple devices from different processes.
* [#4518](https://github.com/IntelRealSense/librealsense/issues/4518) – [T265] Pose data produces `NaNs`. Can still occur in some cases. If detected, please attempt to make a raw data (images + IMU) recording using the [recorder tool](https://github.com/IntelRealSense/librealsense/tree/master/tools/recorder), and attach a link to it in the github issue, to assist our resolution.
* [#6009](https://github.com/IntelRealSense/librealsense/issues/6009) v2.33.1 does not compile with -DBUILDEASYLOGGINGPP=OFF
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* (DSO-13525) - [D400] 3D viewer moved when sliding the tare calibration sliders


## Release 2.45.0
Release Date: 2nd May 2021

### API Changes
N/A
### Bug Fixes and Enhancements
* [#8672](https://github.com/IntelRealSense/librealsense/pull/8672) - **Add recommended on-line updates**
  * The Viewer now checks (by default) online for SDK and/or FW recommended updates whenever a new device is detected (previously, it only checked for essential updates)
  * The "Install Recommended Firmware" button (press More next to your device) was replaced with "Check For Updates", which will also look online
  * If an update is available, a notification will be displayed (same as before)
  * NOTE: If the notification was previously dismissed, it may not show up! To reset this, go into settings and, under the General tab, "Restore Defaults"
* [#8925](https://github.com/IntelRealSense/librealsense/pull/8925) - **[D400] Fw version 5.12.13.50** 
   Fixes and stability improvements
* [#8790](https://github.com/IntelRealSense/librealsense/pull/8790) - **[D400] Make intercam_sync option read-only while streaming**  (DSO-16833)
* [#8791](https://github.com/IntelRealSense/librealsense/pull/8791) - **[L515] Brown-Conrady forward/backward distortion, deproject impl (GitHub #7335)**  (RS5-9995)
* [#8759](https://github.com/IntelRealSense/librealsense/pull/8759) - **[Core] Implementation of Brown-Conrady model on SSE , CUDA and GLSL**  
  * Implementation of Brown-Conrady transformation on `deproject_pixel_to_point_cuda()`.
  * Fix of Inverse-Brown-Conrady transformation on `deproject_pixel_to_point_cuda()`.
  * Implementation of Brown-Conrady transformation on `get_texture_map_sse()`.
  * Added unit tests for sse code (Inverse and Forward Brown-Conrady)..
  * Added unit tests for cuda code (Inverse and Forward Brown-Conrady).
  * Implementation of Brown-Conrady transformation on GLSL.
* [#8784](https://github.com/IntelRealSense/librealsense/pull/8784) - **[Core] Allow physical port retrieval from recovery devices**
* [#8787](https://github.com/IntelRealSense/librealsense/pull/8787) - **[Linux] Kernel patches for LTS 5.8 and SKU 0x0aa3 and 0x0b68**  
  * Add support for Ubuntu `focal`/5.8 kernel.  
  * Register PIDs for metadata support.  
  * Apply to LTS kernel patches 4.4-5.4.  
  * Fix kernel application for bionic legacy 5.0/5.3.
  * Apply Tegra patches for 32.2.3 and 32.5.1 releases.  
  * Minor refactoring and improvements in manual patching flow.  
    (DSO-16720, RS5-10737)
* [#8536](https://github.com/IntelRealSense/librealsense/pull/8536) - **[Viewer] Adjust error report for out of boundary in viewer**  (DSO-13749)
* [#8606](https://github.com/IntelRealSense/librealsense/pull/8606) - **[Core] SKU 0x0B5B - fine tunings in viewer**  (DSO-16440)
* [#8775](https://github.com/IntelRealSense/librealsense/pull/8775) - **[Windows] Catch exception on shutdown, release resources**  (DSO-15821)
* [#8713](https://github.com/IntelRealSense/librealsense/pull/8713) - **[SR300] Adding SR300 derivative into SDK core**  
   The following tools are functioning in SR300-like device :
    - rs-enumerate-devices
    - Realsense-viewer streaming
    - Realsense-viewer controls  
    (DSO-16719)
* [#8777](https://github.com/IntelRealSense/librealsense/pull/8777) - **[Python] update examples to python 3**  
Tests work using python 3 (does not include: box_dimensioner_multicam, ethernet_client_server, t265_wheel_odometry).  
(RS5-10433)
* [#8709](https://github.com/IntelRealSense/librealsense/pull/8709) - **[D400] Added genlock features to extend the option RS2_OPTION_INTER_CAM_SYNC**  
Replacing old pull request #8584 for adding genlock features to extend the option RS2_OPTION_INTER_CAM_SYN_MODE.  
(DSO-16833)
* [#8751](https://github.com/IntelRealSense/librealsense/pull/8751) - **[LRS-Net] Fix connecting to network device with realsense-viewer**  
There was changes introduced in Feb 15 in model-views.cpp that breaks connections to network devices from realsense-viewer. This PR fixes this.
Addresses [#8675](https://github.com/IntelRealSense/librealsense/issues/8675)
* [#8748](https://github.com/IntelRealSense/librealsense/pull/8748) - **[Viewer] Resize drop-down box size to fit the text**  (DSO-12348)
* [#8724](https://github.com/IntelRealSense/librealsense/pull/8724) - **[Viewer] Updated OCC part of the RS Viewer**  (DSO-16841.)
* [#8690](https://github.com/IntelRealSense/librealsense/pull/8690) - **[Core] Fix LibRS compilation with ELPP disabled**  (RS5-10758)
* [#8696](https://github.com/IntelRealSense/librealsense/pull/8696) - **[CI] Travis-CI - update OSX image  + decrease builds runtime**  (Travis-CI build fails when using MacOs < 10.13.
  * Update xcode_image to 12.2 (latest)
  * Remove CHECK_FOR_UPDATE flag for windows build to shorten build time
  * Remove UT run on Windows first build
  * Remove building UT on Linux build
* [#8693](https://github.com/IntelRealSense/librealsense/pull/8693) - **[Viewer] Fix DQT + Viewer crash when minimized when running on Debug**  (When running on Debug configuration, minimizing the DQT / Viewer windows cause the application to crash.
On this PR I add protection against illegal inputs to open-gl functions.  
(RS5-10796)
### Documentation
* [#8768](https://github.com/IntelRealSense/librealsense/pull/8768) - **[D400] OCC Documentation Update**  
Replaced one missing "OCC All" with "OCC Extended".  
(DSO-16841)

### Known issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block.
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized variable. (DSO-13700)
* [#4261](https://github.com/IntelRealSense/librealsense/issues/4261) - [T265] Add ability to open multiple devices from different processes.
* [#4518](https://github.com/IntelRealSense/librealsense/issues/4518) – [T265] Pose data produces `NaNs`. Can still occur in some cases. If detected, please attempt to make a raw data (images + IMU) recording using the [recorder tool](https://github.com/IntelRealSense/librealsense/tree/master/tools/recorder), and attach a link to it in the github issue, to assist our resolution.
* [#6009](https://github.com/IntelRealSense/librealsense/issues/6009) v2.33.1 does not compile with -DBUILDEASYLOGGINGPP=OFF
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* (DSO-13525) - [D400] 3D viewer moved when sliding the tare calibration sliders


## Release 2.44.0
Release Date: 1st Apr 2021

### API Changes
https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2440

### New Features
* [#8633](https://github.com/IntelRealSense/librealsense/pull/8633) - **[NodeJs]**
Support for NodeJs14.  
Fix Nodejs14 cameras autoconfiguration new glfw 

### Bug Fixes and Enhancements
* [#7997](https://github.com/IntelRealSense/librealsense/pull/7997) - **[D400] Added rs2_get_target_size_on_frame and updated python wrapper to use it.**  
API `rs2_get_target_size_on_frame` is created and exposed in the Python wrapper.
* [#8671](https://github.com/IntelRealSense/librealsense/pull/8671) - **[D400] Tare calibration fix**  
Fix issues in `if` statements  (DSO-16782).  
* [#8519](https://github.com/IntelRealSense/librealsense/pull/8519) - **[D400] Auto Exposure Limit : range value setting limitation**  
When setting Auto Exposure value outside the fixed limit, error will be printed to the log and AE value will remain as previous valid value (DSO-16596).
* [#8428](https://github.com/IntelRealSense/librealsense/pull/8428) - **[C#] Fix colorizer/Software Object bundle**  
Addresses [#8407](https://github.com/IntelRealSense/librealsense/issues/8407)
* [#8018](https://github.com/IntelRealSense/librealsense/pull/8018) - **[Core] FW update exception interception**  
When FW update runs, the device disconnects when it switches to DFU. 
Exception is thrown when FW update calls query devices while device still connected (not switch to DFU already.  
In this fix, after sending the command of switching device to DFU, I checked when device is disconnected then continue to next step of querying devices.
(DSO-16069)
* [#8489](https://github.com/IntelRealSense/librealsense/pull/8489) - **[Android]  Redesigning presets activity to dialog**  
(DSO-15649, DSO-15651)
* [#8658](https://github.com/IntelRealSense/librealsense/pull/8658) - **[Core] Add FW version and locked status to device string**  (RS5-10771)

* [#8561](https://github.com/IntelRealSense/librealsense/pull/8561) - **[T265]  Fix for race condition during pipeline stop.**  
This PR is effectively the suggested fix in the GH issue below. Thank you @ankyur.
https://github.com/IntelRealSense/librealsense/issues/7276 
I've validated the fix works using the following code:
    ```cpp
    // Reproduces T265 Hand on Exit.
    int main(int, char**)
    {
        constexpr std::chrono::seconds timeout{ 1 };
        while (true)
        {
            // Start
            rs2::config config;
            rs2::pipeline pipeline;

            std::cout << "Entering pipeline.start()" << std::endl;
            pipeline.start();
            std::cout << "Exiting pipeline.start()" << std::endl;

            std::cout << "Sleeping for 1 second..." << std::endl;
            std::this_thread::sleep_for(timeout);
            
            std::cout << "Entering pipeline.stop()" << std::endl;
            pipeline.stop();
            std::cout << "Exiting pipeline.stop()" << std::endl;
        }
        return 0;
    }
    ```
    Suspect this fix potentially addresses the following open T265 issues as well:
[#7553](https://github.com/IntelRealSense/librealsense/issues/7553),
[#5807](https://github.com/IntelRealSense/librealsense/issues/5807),
[#6272](https://github.com/IntelRealSense/librealsense/issues/6272),
[#7555](https://github.com/IntelRealSense/librealsense/issues/7555),
[#7750](https://github.com/IntelRealSense/librealsense/issues/7750)
* [#8594](https://github.com/IntelRealSense/librealsense/pull/8594) - **[Core] FW Update fortification**  
Adding size check for signed & unsigned firmware  (DSO-13524)
* [#8624](https://github.com/IntelRealSense/librealsense/pull/8624) - **[D400] Baseline computing corrected**  (DSO-16780)  

* [#8623](https://github.com/IntelRealSense/librealsense/pull/8623) - **[D400]**
Checking `supports` API before calling get or set to emitter option**  (DSO-16782) 
* [#8640](https://github.com/IntelRealSense/librealsense/pull/8640) - **[Core]**
Disable_stream() fix when running `enable_all_streams()`
Fix `disable_stream()` to actually stopping disabled streams from arriving.
Reported issue: [#3919](https://github.com/IntelRealSense/librealsense/issues/3919)
(DSO-13705)
* [#8625](https://github.com/IntelRealSense/librealsense/pull/8625) - **[Core]**
Fix deadlock on pipeline.stop() with playback device in non realtime  
  - Add `stop` to syncer to avoid deadlock in case of blocking enqueue.
(DSO-15157)
* [#8593](https://github.com/IntelRealSense/librealsense/pull/8593) - **[L515]** 
Keep USB power on when multiple HWM calls made.  
Reduce L515 device creation and sensor open/start calls time by keeping the USB power on when multiple access are made.
(RS5-8208)
* [#7880](https://github.com/IntelRealSense/librealsense/pull/7880) - **[MATLAB]**
Use case extension draft
* [#8600](https://github.com/IntelRealSense/librealsense/pull/8600) - **[NodeJs]** Make `RSPointCloud.mapTo` compatible with `rs_processing.hpp map_to``  
Related to [#5057](https://github.com/IntelRealSense/librealsense/issues/5057), [#6223](https://github.com/IntelRealSense/librealsense/issues/6223)
RSPointCloud.mapTo should repeat same behavior as rs_processing.hpp [map_to](https://github.com/IntelRealSense/librealsense/blob/master/include/librealsense2/hpp/rs_processing.hpp#L452) which was changed by [commit](https://github.com/IntelRealSense/librealsense/commit/6850677e0473bd01a2078ef6e0398a617539cf56#diff-0657358b7a274211507c5de54e211fe5915712beb08733d0c1dac831bbc32a5cL318), contributed by [@whsol](https://github.com/whsol)
* [#8555](https://github.com/IntelRealSense/librealsense/pull/8555) - **C# tutorial 1 fixes**  
Example fix: `cs-tutorial-1-depth`.  
Checked against cameras: D415, D435, SR300, L515 and USB2/USB3 .NET 4.8, dotnet 5.0, mono.
* [#8559](https://github.com/IntelRealSense/librealsense/pull/8559) - **[C#] tutorial 2 fixes**  
Example fix: `cs-tutorial-2-capture`.  
Checked against cameras: D415, D435, SR300, L515 and USB2/USB3 .NET 4.8, dotnet 5.0
* [#8560](https://github.com/IntelRealSense/librealsense/pull/8560) - **[C#] Tutorial 3 fixes**  
Example fix: `cs-tutorial-3-processing`.  
Verified for D415, D435, SR300, L515 and USB2/USB3 .NET 4.8, dotnet 5.0
* [#8552](https://github.com/IntelRealSense/librealsense/pull/8552) - **C# tutorial 4 fixes** 
Example fix: `cs-tutorial-4-software-dev`.  
Verified for D415, D435, SR300, L515 and USB2/USB3 .NET 4.8, dotnet 5.0.
* [#8576](https://github.com/IntelRealSense/librealsense/pull/8576) - **[Python] Update pybind11 version to 2.6.2**  
Apply fixes for python 3.8 + 3.9 support, align to the latest version. (RS5-10670)
* [#8575](https://github.com/IntelRealSense/librealsense/pull/8575) - **[Android] Fix record for Android 11**  (DSO-16751)
* [#8573](https://github.com/IntelRealSense/librealsense/pull/8573) - **[Core] Syncer to produce frameset (always)**  
Solve the playback with syncer bug added on [#8378](https://github.com/IntelRealSense/librealsense/issues/8378) and pipeline with single stream bug.
The old code expected syncer output to always be a frameset.
* [#8574](https://github.com/IntelRealSense/librealsense/pull/8574) - **[Python] Fix auto calibration python example**  (DSO-16768)
* [#8570](https://github.com/IntelRealSense/librealsense/pull/8570) - **[Python] Fix pyrealsense2 crash when importing other pybind11 created modules**  
Pybind's ABI is made up of compiler type among other things, but not compiler version -- so two modules with pybind compiled with VS 2019 and VS 2015 would generate the same ABI and pybind will try to share resources, crashing Python.
(see https://github.com/pybind/pybind11/issues/2898). The change forces a unique ABI version to be created and loaded that won't be shared with pybind instances. (RS5-10582)
* [#8553](https://github.com/IntelRealSense/librealsense/pull/8553) - **[Viewer] Depth color map ruler show wrong values**  
Fix Viewer depth units ruler's ranges on the right side (RS5-10658)

### Documentation
* [#8613](https://github.com/IntelRealSense/librealsense/pull/8613) - **[HDR] link correction**  
Changed Gif address to the Gif in the development branch instead of the previous (private github link))

### Known Issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block.
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized variable. (DSO-13700)
* [#4261](https://github.com/IntelRealSense/librealsense/issues/4261) - [T265] Add ability to open multiple devices from different processes.
* [#4518](https://github.com/IntelRealSense/librealsense/issues/4518) – [T265] Pose data produces `NaNs`. Can still occur in some cases. If detected, please attempt to make a raw data (images + IMU) recording using the [recorder tool](https://github.com/IntelRealSense/librealsense/tree/master/tools/recorder), and attach a link to it in the github issue, to assist our resolution.
* [#6009](https://github.com/IntelRealSense/librealsense/issues/6009) v2.33.1 does not compile with -DBUILDEASYLOGGINGPP=OFF
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* (DSO-13525) - [D400] 3D viewer moved when sliding the tare calibration sliders


## Release 2.43.0
Release Date: 21 Mar 2021

### Debian Distribution PPA Change - User Notification
The Debian Distribution PPA is being transferred and redirected to use new URL with Secure Access - **HTTPS** instead of ~~HTTP~~  
The new addresses shall start with `https://librealsense.intel.com`  
The users who use seamless Ubuntu update shall modify the server's list according to the [updated documentation](https://github.com/IntelRealSense/librealsense/blob/master/doc/distribution_linux.md#installing-the-packages).   
Relates to [#8414](https://github.com/IntelRealSense/librealsense/pull/8414)

### NVidia Jetson - User Notification
With the release of Jetpack 4.4.1 support for RealSense Devices D415,D435(i) has been integrated into the L4T kernel, thus the owners/developers for that platform (Jetson AGX/Nano) that use those devices do not need to apply kernel patches anymore in order to receive the extended functionality and features, such as metadata.

### API Changes
https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2430

### New Features
* [#8622](https://github.com/IntelRealSense/librealsense/pull/8622) - **[D400] Fw version v5.12.12.100**  
* [#8605](https://github.com/IntelRealSense/librealsense/pull/8605) - **[L515] FW version 1.5.5.0**  
* [#8405](https://github.com/IntelRealSense/librealsense/pull/8405) - **[Python] Generate wheels for Python 3.8 + 3.9**
Package classifiers updated to support python 3.8 + 3.9 (DSO-16485)  
* [#8366](https://github.com/IntelRealSense/librealsense/pull/8366) - **[NVidia JetPack]**
Adding support for Jetpack 4.4.1+4.5 (32.4.4 + 32.5)
  - Manage separate kernel patches for formats and metadata since L4T 4.4.1
  - Modify script to avoid redundant patches that have been introduced inL4T 4.4.1
  - Rename 4.9 patches into 4.4/4.4.1 to follow the JetPack baseline version
  - Minor adjustments to script to handle with Git/Kernel variants  
 Addresses [#8254](https://github.com/IntelRealSense/librealsense/issues/8254) ) (DSO-16259)
* [#8298](https://github.com/IntelRealSense/librealsense/pull/8298) - **[Android] Support for Android 11**  
Added changes needed by Android Studio and the gradle build system for supporting the latest version of the Android SDK (30).
The changes were tested to be working on a Pixel device with Android version 11

### Bug Fixes and Enhancements
* [#8473](https://github.com/IntelRealSense/librealsense/pull/8473) - **[Core] Video profile eq**  (video_stream_profile class is missing operator==
This cause 2 video_stream_profile objects to use the stream_profile::operator== instead which gives false results.  
(DSO-16565)
* [#8524](https://github.com/IntelRealSense/librealsense/pull/8524) - **[Core] Fix include in src/sensor.h**  
* [#8487](https://github.com/IntelRealSense/librealsense/pull/8487) - **[Unit-tests] FW update test**  
* [#8518](https://github.com/IntelRealSense/librealsense/pull/8518) - **[Unit-tests] Negative number of assertions**  
* [#8494](https://github.com/IntelRealSense/librealsense/pull/8494) - **[Unit-tests] Memory Leak  test fix**  
Fixed internal unit test : "Extrinsic memory leak detection"
Fix: accepting first arrived frame in each iteration regardless of its frame number 
(DSO-16625)
* [#8495](https://github.com/IntelRealSense/librealsense/pull/8495) - **[Android] FW Update message correction**  
Avoid suggesting the user to update the recommended fw if already there  (DSO-15522)
* [#8365](https://github.com/IntelRealSense/librealsense/pull/8365) - **[Core] Add support for SKU B5B**  (DSO-16440)
* [#8490](https://github.com/IntelRealSense/librealsense/pull/8490) - **[Unit-tests] Misc run-unit-tests attempts at better stability**   Including work in set-options
* [#8461](https://github.com/IntelRealSense/librealsense/pull/8461) - **[Core] Fix for IMU drops on playback**  
Addresses issue [#7908](https://github.com/IntelRealSense/librealsense/issues/7908).  
Adding log for manitenance (DSO-12379)
* [#8488](https://github.com/IntelRealSense/librealsense/pull/8488) - **[Windows] LRS examples from Windows installer does not build**
Split example.hpp ImGui related code into a new file example-imgui.hpp  
Fix examples includes  
Fix examples warnings (RS5-10360)  
* [#8463](https://github.com/IntelRealSense/librealsense/pull/8463) - **[3rd Party] Libcurl update - SSL support**  
Update libcurl third-party to version 7.75  
Build libcurl with SSL  
Change online versions DB link to https link  (DSO-16463)
* [#8056](https://github.com/IntelRealSense/librealsense/pull/8056) - **[L500] Add MUR and reflectivity unit tests**  
(RS5-9400)
* [#8462](https://github.com/IntelRealSense/librealsense/pull/8462) - **[Playback] Slider show milliseconds**  
(DSO-12379)
* [#8477](https://github.com/IntelRealSense/librealsense/pull/8477) - **[Unit-Tests] fixes**
Fix missing unit-tests test:device & better debug output  
Add rspy.log.debug_indent()  
Add rspy.test.print_separator()  
Fix some weird behavior with rspy.log color output   
[#8475](https://github.com/IntelRealSense/librealsense/pull/8475) - **[L500] Fixed AC-triggers test**  
* [#8468](https://github.com/IntelRealSense/librealsense/pull/8468) - **[Unit-Tests] Added tag directive**  
* [#8444](https://github.com/IntelRealSense/librealsense/pull/8444) - **[Unit-Tests]**
Fix run-unit-tests issue with finding acroname  
* [#8443](https://github.com/IntelRealSense/librealsense/pull/8443) - **[Unit-Tests]**
Added priority option to config  
* [#8440](https://github.com/IntelRealSense/librealsense/pull/8440) - **[Unit-Tests]**
Added configuration for c/cpp tests  
* [#7859](https://github.com/IntelRealSense/librealsense/pull/7859) - **[Unit-Tests]**
Added unit tests for hw-errors handling  
Error handling sanity, in and out of streaming
(RS5-9659)
* [#8406](https://github.com/IntelRealSense/librealsense/pull/8406) - **[Unit-Tests] fixes**
run-unit-tests.py no longer requires pyrealsense2.pyd  
* [#8432](https://github.com/IntelRealSense/librealsense/pull/8432) - **[Core] Change to C style cast**  
Fixes [#8431](https://github.com/IntelRealSense/librealsense/issues/8431) contributed by [@surban](https://github.com/surban)
* [#8410](https://github.com/IntelRealSense/librealsense/pull/8410) - **[Core] Log to output file instead of console**  
Adding option for printing the fw logs in an output file
* [#8414](https://github.com/IntelRealSense/librealsense/pull/8414) - **[Linux Distribution] Update Repository Server URL**  
Replacing URLs:  
  - ~~realsense-hw-public.s3.amazonaws.com~~  
  - ~~realsense-hw-publics3-eu-west-1.amazonaws.com~~  
with `librealsense.intel.com`
Installation tested on Ubuntu 16.04, 18.04, 20.04
(DSO-16482)
* [#8420](https://github.com/IntelRealSense/librealsense/pull/8420) - **[L500] Update MUR limits and resolution**  
(RS5-9580)
* [#8378](https://github.com/IntelRealSense/librealsense/pull/8378) - **[Core] Syncer Fix**  
Fix syncer bug causing wrong frame order:  
  - Fix l500 matcher hierarchy (remove timestamp_composite_matcher as root above device-generated matcher)
  - Add mutex to protect try_dequeue and calling to callback
  - Add log parameter to syncer (RS5-10200)

* [#8403](https://github.com/IntelRealSense/librealsense/pull/8403) - **[Unit-Tests]**  
Fix small issues with devices.py
* [#8387](https://github.com/IntelRealSense/librealsense/pull/8387) - **[Core] Use standard cast syntax to cast to float**  
This change was supposedly done to reduce / remove the number of warnings, but ended up breaking our Rust bindings to librealsense2 (bindgen is expecting an expression which isn't there). See [our realsense-rust fork](https://github.com/Tangram-Vision/realsense-rust).  
If you're looking for the specific error when building against the official 2.42 release.) contributed by [@ThatGeoGuy](https://github.com/ThatGeoGuy)
* [#6653](https://github.com/IntelRealSense/librealsense/pull/6653) - **[Linux] add patch scripts for debian 10 and linux vanilla**  
L515 working on Debian 10. contributed by [@andrewhodel](https://github.com/andrewhodel)

* [#7751](https://github.com/IntelRealSense/librealsense/pull/7751) - **[Unit-Tests]**  
Encapsulate the content of internal-tests-linux.cpp with ```#ifdef __linux__```   
* [#8380](https://github.com/IntelRealSense/librealsense/pull/8380) - **[Android] switch HTTP to HTTPS**  
...  in camera `app gradle.build`  (DSO-16528)
* [#8336](https://github.com/IntelRealSense/librealsense/pull/8336) - **[Unit-Tests] - HDR** 
Move HDR tests from live tests to dedicated folder** (DSO-16532)
* [#8370](https://github.com/IntelRealSense/librealsense/pull/8370) - **[Unit-Tests]**
Additional unit-testing fixes  
* [#8359](https://github.com/IntelRealSense/librealsense/pull/8359) - **[Unit-Tests]** 
  - Test:device acroname control (for py only)  
  - Separated log functionality into `rspy.log` module
  - Use `devices` module from run-unit-tests.py
  - Changed python subprocess invocation to `sys.executable`
  - HWM cmd `SET_AGE` -> `UNIT_AGE_SET`, to fit XML
* [#8328](https://github.com/IntelRealSense/librealsense/pull/8328) - **[Viewer]** OpenVINO filter fix **  
  - Disable OpenVINO filter while streaming single frames (when IMU is ON) and returning it back when IMU is disabled
  - Enable frameset streaming after IMU is disabled   
  Addresses [#8074](https://github.com/IntelRealSense/librealsense/issues/8074)   (DSO-15250)
* [#8351](https://github.com/IntelRealSense/librealsense/pull/8351) - **[Python Examples]**
Adjust python examples to support L515 device (RS5-10333)
* [#8338](https://github.com/IntelRealSense/librealsense/pull/8338) - **[L500] Mode check remove**  
Remove the sensor mode check on start stream on custom: profile resolution now trumps sensor_mode  
Added test to check it
* [#8335](https://github.com/IntelRealSense/librealsense/pull/8335) - **[Core] Add const to a couple of HW monitor APIs**
* [#8331](https://github.com/IntelRealSense/librealsense/pull/8331) - **[Tenssor Flow]**
Rename `camera_simulation.py` to `example5-denoised.py` in part 5)  
* [#8304](https://github.com/IntelRealSense/librealsense/pull/8304) - **[D400] DFU updates**
Version 107 added, version 106 corrected (DSO-16507)
* [#8285](https://github.com/IntelRealSense/librealsense/pull/8285) - **[Android] Fix Permisisons** 
Camera permission skipped in OS versions < 8 (DSO-16060)
* [#8282](https://github.com/IntelRealSense/librealsense/pull/8282) - **[3rd Party]Live555 version freeze**
* [#8297](https://github.com/IntelRealSense/librealsense/pull/8297) - **[Unit-Tests]**  
Fixed preset unit-test issues
* [#8214](https://github.com/IntelRealSense/librealsense/pull/8214) - **[Unit-Tests]**  
Pipe unit test for first frame delay issue  
Tested on D435i and L515, USB3 (DSO-16305)

### Documentation
* [#8286](https://github.com/IntelRealSense/librealsense/pull/8286) - **[Doc]**  Add Android releases artifactory link  (DSO-16496)

### Known Issues
* (RS5-10755) - [Android] Significant slowness when working with Viewer App and L515  
* (DSO-16751) - [Android] Apk record cannot work with Android 11
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block.
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized variable. (DSO-13700)
* [#4261](https://github.com/IntelRealSense/librealsense/issues/4261) - [T265] Add ability to open multiple devices from different processes.
* [#4518](https://github.com/IntelRealSense/librealsense/issues/4518) – [T265] Pose data produces `NaNs`. Can still occur in some cases. If detected, please attempt to make a raw data (images + IMU) recording using the [recorder tool](https://github.com/IntelRealSense/librealsense/tree/master/tools/recorder), and attach a link to it in the github issue, to assist our resolution.
* [#6009](https://github.com/IntelRealSense/librealsense/issues/6009) v2.33.1 does not compile with -DBUILDEASYLOGGINGPP=OFF
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* (DSO-13525) - [D400] 3D viewer moved when sliding the tare calibration sliders


## Release 2.42.0
Release Date: 14 Feb 2021

### API Changes
https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2420

### Android Releases - User Notification
The Android Distribution Center has been transferred from `bintray.com` to `jfrog` servers starting from Feb 1st.
The [previously available server](https://dl.bintray.com/intel-realsense/librealsense) is not accessible since Jan 24th.
The official releases of the Android apk and Java library can be browsed with `https://egiintel.jfrog.io/artifactory/librealsense/com/intel/realsense/librealsense`  
See [#8280](https://github.com/IntelRealSense/librealsense/pull/8280) for details.  
Note that the new server currently is hosting SDK releases v2.41.0+ only. Transfer of older releases is envisioned but not prioritized.

### New Features
* [#8350](https://github.com/IntelRealSense/librealsense/pull/8350) - **[L515] FW update v1.5.4.1**  (RS5-10444) 
* [#8345](https://github.com/IntelRealSense/librealsense/pull/8345) - **[D400] FW update v5.12.11.0**
   - Max time limit for IR sensors (DSO-12884)
   - Customer enhancements (DSO-15098).
* [#8036](https://github.com/IntelRealSense/librealsense/pull/8036) - **[UE] Unreal Engine wrapper is converted to UE4.26**  
This changes makes _unrealengine4_ wrapper to be build against Unreal Engine 4.26 and _introduces some breaking changes_ for engines less than v4.26 according to a new UE API.
* [#8281](https://github.com/IntelRealSense/librealsense/pull/8281) - **[API] Enable rolling log files**  
   Continues the work of @aseelegbaria ([#8212](https://github.com/IntelRealSense/librealsense/issues/8212)
   Log files can have a max size. upon reaching max size new file will be created and current file will be truncated.  
   (RS5-9271)
* [#8211](https://github.com/IntelRealSense/librealsense/pull/8211),[#8240](https://github.com/IntelRealSense/librealsense/pull/8240) - **[SDK] CMake Build transition to HTTPS**  
Librealsense Cmake Build requires downloading and bundling FW images from external server. Starting with v2.42.0 the actual downloads will be queried using Secure Access (https).  
The new resource URLs are retrofitted into CMake and the source tree documentation.  
(DSO-16368, DSO-16444)  
Fix FW download link url in Realsense-viewer (RS5-8513) 
* [#8220](https://github.com/IntelRealSense/librealsense/pull/8220) - **[D400] Auto-Exposure and Gain max limits for IR sensors.**  
The new APIs set the effective upper bound for Exposure and Gain values for Depth Auto-Exposure. This allows to control and limit the changes in FPS rate while enabling for AE auto-adjustments.  
Requires FW5.12.11.0 or later.
* [#8029](https://github.com/IntelRealSense/librealsense/pull/8029) - **[MacOS] IMU data processing via HIDAPI on MacOS**  
   Support of IMU accelerometer and Gyro data on MacOS.  
   s_hid_device class was modified to receive the IMU data via HIDAPI library functions on Mac OS.  
(RS5-9285, RS5-8852, possibly RS5-9121) by [@sokolov19830711](https://github.com/sokolov19830711)
* [#8063](https://github.com/IntelRealSense/librealsense/pull/8063) - **[Core] -Reset logger**
Stop and reset reset logger on demand to cope with huge log files. (RS5-9270)

### Bug Fixes and Enhancements

* [#8289](https://github.com/IntelRealSense/librealsense/pull/8289) - **[DQT] Fix L515 handling in Custom Mode **  
In custom mode we need to update the sensor with sensor mode before start.
* [#8072](https://github.com/IntelRealSense/librealsense/pull/8072) - **[Unity] using SafeHandle in Context class**  
Context class with CriticalFinalizerObject and CER implementation.  
(DSO-13740)
* [#8002](https://github.com/IntelRealSense/librealsense/pull/8002) - **[L515] Remove default preset**  (RS5-8999)
   1. **Remove default preset**
      -  on LRS start up we calculate if we are in one of the presets and if not we are on custom
      - when loading old Json file if the preset is default the preset will be custom
      - if user try to set preset default he will get an error.
   2. **Reading of defaults values** 
      - on new fw versions( >1.5.3.0) is by get_default command
      - on old fw versions its stay as it was - set_current -1 and than get_current gives the defaults values
   3. **update defaults values of hw options on the following cases**
      - on startup 
      - when changing resolution 
      - when changing gain
   4. **Flow of set preset**
      - set gain according to preset
      - read the default values 
      - set default values to currents
      - set laser according to preset

   NOTE: When starting to stream, librealsense may raise an exception:
   > sensor mode (VGA) with RS2_L500_VISUAL_PRESET_CUSTOM is incompatible with the requested profile resolution (XGA)

   This is caused when the visual preset is CUSTOM and the value in RS2_OPTION_SENSOR_MODE does not match the resolution from the profile. Either make 
   sure the preset is one of the predefined ones or set the SENSOR_MODE to match the profile.
   This exception has since been removed, see PR [#8338](https://github.com/IntelRealSense/librealsense/pull/8338).
* [#8261](https://github.com/IntelRealSense/librealsense/pull/8261) - **[Android] Remove (L515) Confidence stream from settings**  (RS5-8989)
* [#8262](https://github.com/IntelRealSense/librealsense/pull/8262) - **[L515] Deprecate Zero-Order option** (RS5-10182) 
* [#8252](https://github.com/IntelRealSense/librealsense/pull/8252) - **[Core] Fix GCC 5 error/warnings in HDR demo**  
* [#8234](https://github.com/IntelRealSense/librealsense/pull/8234) - **[Linux] Revert "Switch v4l to use memory-mapped files instead of userptr."**  
   Use of mmap leads to sporadic segfault on stream close/frames release.  
   Switch back to user-allocated buffers for v4l data exchange.  
   Addresses [#8154](https://github.com/IntelRealSense/librealsense/issues/8154).  
   (DSO-16453)
* [#8245](https://github.com/IntelRealSense/librealsense/pull/8245) - **[Live555] Fix build_with_network CMake option and other workarounds**  
   - liveMedia update - liveMedia has been upgraded by live555 in 2021-jan-21.   Available as an archive from server only.
   - A workaround for wrong default value about RS2_OPTION_POWER_LINE_FREQUENCY.
   - A workaround for bad parameter when create frame object in Y8 format with network mode.
   - add 'add_to' method for net_device in python wrapper.
* [#8048](https://github.com/IntelRealSense/librealsense/pull/8048) - **[EasyLogging] - add asynchronous handling (Linux)**  
   Handling log messages asynchronously to reduce latencies.  
   ( DSO-16295)
* [#8180](https://github.com/IntelRealSense/librealsense/pull/8180) - **[L515] Frames Filter unit-tests**
   Check that IR frames do not arrive to the user callback if not specifically asked for.  
   (RS5-10249)
* [#8206](https://github.com/IntelRealSense/librealsense/pull/8206) - **[Viewer] Fix FW update exception showed after entering update state**  
   On FW update window we see a recoverable exception string during the FW update process. This PR fixes this behaviour.
   Actions taken:  
   1. Add delay between sending the device command to enter update state and query devices (if we do it too fast we still get the device before it was able to disconnect)
   2. Replace FW update UI exception with log exception  
   (RS5-7900)
* [#8176](https://github.com/IntelRealSense/librealsense/pull/8176) - **[Python] Integrate pybind11 V2.6.1 + replace pybind11 files with clone action**  
   - Clone pybind11  instead of keeping pybind files inside librealsense2 repo  
   - Update pybind version 2.2.1 -> 2.6.1  
   (RS5-9118)
* [#8159](https://github.com/IntelRealSense/librealsense/pull/8159) - **[Examples] HDR demo overhaul**
   - Review and refactoring both in Documentation and in Code.
   - Handle Mosaic View to present stacked frames.
   - Add HDR-specific controls
* [#7661](https://github.com/IntelRealSense/librealsense/pull/7661) - **[L500] Age-update implemented for projection**  (The age is now updated in the DSM on setup.
When AC is run the Age and last AC fields in DSM are updated, the fields that involve the MC are not changed.
* [#8182](https://github.com/IntelRealSense/librealsense/pull/8182) - **Unit-tests fixes**  
   Change the default directory for test logs to build/unit-tests instead of build.  
   Remove from the printing of the call stack when check fails the printing of the calls to the functions inside `test.py`
* [#8142](https://github.com/IntelRealSense/librealsense/pull/8142) - **[Core] Memory Leaks fixes**  (Fixes some memory leaks:
   - In the Linux backend, the deleter of a `std::unique_ptr` didn't call `delete`;
   - The `hdr_config` type and its sensor were holding mutual shared pointers to each other, causing them to not be deleted;
   - In `synthetic-stream.cpp`, the `on_set` method is called on a `ptr_option`, which stores the function passed to that method. However, the lambda passed captures the shared pointer, giving another shared pointer loop.) contributed by [@Rheel17](https://github.com/Rheel17)
* [#8066](https://github.com/IntelRealSense/librealsense/pull/8066) - **[Examples] - rs-kinfu**
   Place OMP pragma before for statement in rs-kinfu app.   Fixes [#5776](https://github.com/IntelRealSense/librealsense/issues/5776). Contributed by [@PeterBowman](https://github.com/PeterBowman)
* [#8084](https://github.com/IntelRealSense/librealsense/pull/8084) - **[Open3D] wrapper examples and tutorial**  
This PR adds a tutorial and some examples for using RealSense devices through the new Open3D wrapper. contributed by [@ssheorey](https://github.com/ssheorey)
* [#8162](https://github.com/IntelRealSense/librealsense/pull/8162) - **[Unit-Tests] Add run-unit-tests delay after acroname reset; add --stdout, --asis**
* [#8145](https://github.com/IntelRealSense/librealsense/pull/8145) - **[Unit-Tests] - Acroname controls in unit-tests**  Disable unit-tests in Travis Linux static build -- the Python version there misbehaved with ModuleNotFoundError and it was to be removed eventually anyway.
* [#8151](https://github.com/IntelRealSense/librealsense/pull/8151) - **[Core] fix realsense2-compression & realsense2-net plugin**
Follow up on [#8141](https://github.com/IntelRealSense/librealsense/pull/8141)  
* [#8141](https://github.com/IntelRealSense/librealsense/pull/8141) - **[Core] amendments**
   `src` removed from include directories of projects that do not need it.
* [#8127](https://github.com/IntelRealSense/librealsense/pull/8127) - **[D400] Detect IMU type (BMI-085 or BMI-055) regardless of device type.**
   - This PR allows the device to set the correct accel frame rate based on the IMU type, instead of the device type.  
   - Requires FW 05.12.10.0 or later.
   - `rs-imu-calibration.py` was tested using this PR in Ubuntu 16.04.  
(DSO-16367)
* [#8135](https://github.com/IntelRealSense/librealsense/pull/8135) - **[Unity] Add log creation**
 Add a `-logFile` parameter to unity export package command for investigation failures
* [#8105](https://github.com/IntelRealSense/librealsense/pull/8105) - **[Unit-Tests] updates**  (RS5-9997)
* [#8133](https://github.com/IntelRealSense/librealsense/pull/8133) - **[Examples] Correcting compression labels in rosbag-inspector tool**  (Compression labels displayed in the tool are swapped) contributed by [@matbb](https://github.com/matbb)
* [#8131](https://github.com/IntelRealSense/librealsense/pull/8131) - **[Python] python subprocess invocation using same python exe**
* [#8061](https://github.com/IntelRealSense/librealsense/pull/8061) - **[L515] IR Reflectivity - hide until value stabilized**  (RS5-9613)
* [#8109](https://github.com/IntelRealSense/librealsense/pull/8109) - **[SDK] REmove redundant Include directories from CMake**
   There was a misuse of the include_directories function where the first argument was a project name. This function should only get the wanted directories as parameters.
   (RS5-9747)
* [#8087](https://github.com/IntelRealSense/librealsense/pull/8087) - **[L515] correct display of advanced controls + UI setting when switching devices**  
   - Fix querying advanced controls with the wrong sensor mode.
   - Fix restoring UI controls from different device
   (RS5-9466)
* [#8073](https://github.com/IntelRealSense/librealsense/pull/8073) - **[Core] Retrofit changes from v2.41 into Development**
* [#8071](https://github.com/IntelRealSense/librealsense/pull/8071) - **[Core] Fix GCC 7.0 compilation error**
* [#8064](https://github.com/IntelRealSense/librealsense/pull/8064) - **[Unit-Tests] enhancements**
   - set-option unit-test fix & bug fix in test.info
   - abort test in case of exception   
   (RS5-9850)
* [#7781](https://github.com/IntelRealSense/librealsense/pull/7781) - **[Examples] Wrap Viewer/DQT error pop up text**
   The error pop up text on Viewer and DQT application is not wrapped, It use a ImGui::InputTextMultiline because the input text is dynamic.
   For static text we can use ImGui API - "ImGui::PushTextWrapPos / ImGui::PopTextWrapPos ".
   (RS5-9465)
* [#8050](https://github.com/IntelRealSense/librealsense/pull/8050) - **[L515] IR Reflectivity - move setting**  
   IR reflectivity option moved to configuration section at the DQT application.  
   (RS5-9612)
* [#8052](https://github.com/IntelRealSense/librealsense/pull/8052) - **[CI] Revert change on Linux parallel build**  
   - Travis CI currently allow VM with 2 cores.
   - Using -j$(($(nproc)-1)) disabled the parallel build on Linux builds.This PR revert this change
* [#8051](https://github.com/IntelRealSense/librealsense/pull/8051) - **[Unit-Tests] fix**  
   Fix unit-test failure and add info for the test. (RS5-9850)
* [#8023](https://github.com/IntelRealSense/librealsense/pull/8023) - **[PyBackend] fix**  
   Deadlock occurs when HID callback invokes from multi-threads
* [#8041](https://github.com/IntelRealSense/librealsense/pull/8041) - **[Unit-Tests] - test.info** (RS5-9754)
* [#7895](https://github.com/IntelRealSense/librealsense/pull/7895) - **[Unit-Tests] serializable_device preset bug fix & unit-test**  
   Added a test for serializable device json serialize and load
   Also changed load_json in l500-serializable so it works with the presets.  (RS5-9630)
* [#8016](https://github.com/IntelRealSense/librealsense/pull/8016) - **[Unit-Tests] Fixed test-FG - remove 1280x720 resolution**  
   Remove test "streaming FG 1280x720". FW support only 800x600 resolution on FG.  (RS5-9894)
* [#8000](https://github.com/IntelRealSense/librealsense/pull/8000) - **[Unit-Tests] frame drops after set_option**  Unit-tests to check if there is any frame drops after set_option in Windows and Linux. (RS5-9850)

### Documentation
* [#8280](https://github.com/IntelRealSense/librealsense/pull/8280) - **[Android] repo updated in java_example and native_example** (DSO-16496)
* [#8238](https://github.com/IntelRealSense/librealsense/pull/8238) - **Tensorflow fix**  Fixed URL in part 3.

### Known Issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block.
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized variable. (DSO-13700)
* [#4261](https://github.com/IntelRealSense/librealsense/issues/4261) - [T265] Add ability to open multiple devices from different processes.
* [#4518](https://github.com/IntelRealSense/librealsense/issues/4518) – [T265] Pose data produces `NaNs`. Can still occur in some cases. If detected, please attempt to make a raw data (images + IMU) recording using the [recorder tool](https://github.com/IntelRealSense/librealsense/tree/master/tools/recorder), and attach a link to it in the github issue, to assist our resolution.
* [#6009](https://github.com/IntelRealSense/librealsense/issues/6009) v2.33.1 does not compile with -DBUILDEASYLOGGINGPP=OFF
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* (DSO-13525) - [D400] 3D viewer moved when sliding the tare calibration sliders


## Release 2.41.0
Release Date: 27 Dec 2020

### API Changes
https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2410


### New Features
* [#7802](https://github.com/IntelRealSense/librealsense/pull/7802) - **[D400] FW Update - 5.12.10.0**
    - Bug fixes and improvements
* [#7808](https://github.com/IntelRealSense/librealsense/pull/7808) - **[L515] FW Update - 1.5.3.0**
    - Bug fixes
* [#7851](https://github.com/IntelRealSense/librealsense/pull/7851) - **[OpenVINO]** Compatibility to both 2019/2020 versions of the openVINO was added.
  Compatibility for 2019 and 2020 versions of the openVINO for the openVINO wrapper examles and realsense-viewer has been added. Addresses [6127](https://github.com/IntelRealSense/librealsense/issues/6127).  

### Bug Fixes and Enhancements
* [#7983](https://github.com/IntelRealSense/librealsense/pull/7983) - **[Linux]** Kernel 5 improvements:  
  Changes in v4l backend implementation applicable for Kernels 4.16+ only:
  - v4l backend - use multiplexing to handle metadata and video payloads
  - Modify UVC_URBs from 5 to 16 to mitigate frame drops in uvcvideo
  - Minor enhancement in manual patching script
  - Frame drops unit-test refactoring  
* [#8005](https://github.com/IntelRealSense/librealsense/pull/8005) - **[Linux]** Fix for video buffers guard
* [#7993](https://github.com/IntelRealSense/librealsense/pull/7993) - **[Network Device]** Update rs-server to the latest Live555.
* [#7977](https://github.com/IntelRealSense/librealsense/pull/7977) - **[Android]** Enabled advanced mode is not supported  
  Display the advanced mode option only when it's actually supported. (RS5-8619)
* [#7911](https://github.com/IntelRealSense/librealsense/pull/7911) - **[D400]** Emitter On /Off and Emitter Always On enabling together avoided (DSO-16064)
* [#7938](https://github.com/IntelRealSense/librealsense/pull/7938) - **[D400]** On-Chip Calibration  - Focal Length Enhancements  
* [#7915](https://github.com/IntelRealSense/librealsense/pull/7915) - **[D400]** On-Chip Calibration support for csharp wrapper
  Add on-chip calibration call in csharp wrapper.) contributed by [@mengyui](https://github.com/mengyui)
* [#7535](https://github.com/IntelRealSense/librealsense/pull/7535) - **[D400]** On-Chip Calibration - Focal Length calibration and merged Tare Ground Truth.
  Added on-chip focal length calibration and merged tare ground truth
* [#7872](https://github.com/IntelRealSense/librealsense/pull/7872) - **[L515]** IR Reflectivity report at 15% resolution  
  IR Reflectivity resolution changes to 15% increments.
  Add `stabilized_value` utility class and use it to smooth low-rate changes to the output value. (RS5-9611)  
* [#7759](https://github.com/IntelRealSense/librealsense/pull/7759) **[SDK]** Convert frame to frameset in post processing filters. Make allocations consistent for all processing blocks. Addresses [#7584](https://github.com/IntelRealSense/librealsense/issues/7584) (DSO-15250)
* [#7339](https://github.com/IntelRealSense/librealsense/pull/7339) - **[Linux]** Update packages error.  
  A solution to issue [#5092](https://github.com/IntelRealSense/librealsense/issues/5092).   
  error : E: Unable to locate package libdrm-amdgpu1-dbg
  I ran `apt-cache search libdrm-amdgpu1-dbg` and it returned libdrm-amdgpu1-dbgsym and libdrm-amdgpu1.  
  Contributed by [@bestaps](https://github.com/bestaps)
* [#7774](https://github.com/IntelRealSense/librealsense/pull/7774) - **[SDK]** input to `try_parse` ROS record method updated** (DSO-16068)  
* [#7931](https://github.com/IntelRealSense/librealsense/pull/7931) - **[MacOS]** IMU was temporarily switched off on Mac OS  Processing of IMU data was temporarily switched off in case of Mac OS usage to prevent the viewer and examples applications crash. Contributed by [@sokolov19830711](https://github.com/sokolov19830711)
* [#7878](https://github.com/IntelRealSense/librealsense/pull/7878) - **[PCL]** Fixes  
  Introduce cmake flag.  
  Add try-catch for main function of pcl-color example.
  Contributed by [@manson](https://github.com/manson)
* [#7876](https://github.com/IntelRealSense/librealsense/pull/7876) - **[PCL]** Fix wrapper building
  Searching procedure for headers and libs of the GLFW libriary was modified for using with the internal version of GLFW. Contributed by [@sokolov19830711](https://github.com/sokolov19830711)
* [#7885](https://github.com/IntelRealSense/librealsense/pull/7885) - **[L515]** Fix error during AC when alt-IR isn't available
* [#7887](https://github.com/IntelRealSense/librealsense/pull/7887) - **[OpenCV]** rotate point cloud example was added. Contributed by [@Allius27](https://github.com/Allius27)
* [#7881](https://github.com/IntelRealSense/librealsense/pull/7881) - **[libusb]** Work-around for libusb breaking changes in Master branch (v1.0.24)
* [#7836](https://github.com/IntelRealSense/librealsense/pull/7836) - **[Python]** Add `run-unit-tests.py` ability to run tests by name  
* [#7842](https://github.com/IntelRealSense/librealsense/pull/7842) - **[Network Device]** Fix for the API usage removed bythe new version of Live555. 
* [#7854](https://github.com/IntelRealSense/librealsense/pull/7854) - **[L515]** Parse additional HW error types.  
  Added additional "Fatal Errors" 18-24. (RS5-9443)
* [#7741](https://github.com/IntelRealSense/librealsense/pull/7741) - **[L515]** Added unitests for alt-IR:
    - Sanity test.
    - Check that AC fails if AltIR was enable before stream start.
    - Check that AC fails if AltIR was enable after stream start.)(RS5-9423)
* [#7812](https://github.com/IntelRealSense/librealsense/pull/7812) - **[Python]** pybind linux - Fix missing debug symbols in RelWithDebug mode.
  The output binary .so file of building pyrealsense2 on Linux and on configuration RelWithDebInfo do not contain debug symbols.
  It was caused due to a known issue with pybind11 version 2.2.1 that is used in librealsense SDK.
  The issue was fixed at [PR1892](https://github.com/pybind/pybind11/pull/1892/files) at pybind11 repo.  
  Linux python 2.7 wheel size before this changes is : ~10 [MB]  
  Linux python 2.7 wheel size after this changes is : ~70 [MB]  
* [#7835](https://github.com/IntelRealSense/librealsense/pull/7835) - **[Python]** stream_profile stream/format output now uses to_string()  
* [#7794](https://github.com/IntelRealSense/librealsense/pull/7794) - **[Unity]** For the L515 device, disabled processing blocks in the `PointCloudProcessingBlocks` scene, except for Temporaral Filter) contributed by [@SergeySPF](https://github.com/SergeySPF)
* [#7840](https://github.com/IntelRealSense/librealsense/pull/7840) - **Fix get_stream_profiles()**
* [#7833](https://github.com/IntelRealSense/librealsense/pull/7833) - **[Java]** Download firmware image file for l500  
  Add missing enum to Java wrapper (RS5-9606)
* [#7832](https://github.com/IntelRealSense/librealsense/pull/7832) - **[Git]** fix merge issue  
  Fix merge from `master` issue of duplicate code
* [#7818](https://github.com/IntelRealSense/librealsense/pull/7818) - **[Git]** Retrofit Development branch with 2.40.0 changes  
* [#7796](https://github.com/IntelRealSense/librealsense/pull/7796) - **L500]** Support for `FG` with debug_stream_sensor    Added API function `get_debug_stream_profiles()` that returns the list of debug formats  (FG).
  The debug format will not be available through the `sensor::get_stream_profiles()` API.
  Added support for python wrapper.
  Added unit tests on python.

### Known Issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block.
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized variable. (DSO-13700)
* [#4261](https://github.com/IntelRealSense/librealsense/issues/4261) - [T265] Add ability to open multiple devices from different processes.
* [#4518](https://github.com/IntelRealSense/librealsense/issues/4518) – [T265] Pose data produces `NaNs`. Can still occur in some cases. If detected, please attempt to make a raw data (images + IMU) recording using the [recorder tool](https://github.com/IntelRealSense/librealsense/tree/master/tools/recorder), and attach a link to it in the github issue, to assist our resolution.
* [#6009](https://github.com/IntelRealSense/librealsense/issues/6009) v2.33.1 does not compile with -DBUILDEASYLOGGINGPP=OFF
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* (DSO-13525) - [D400] 3D viewer moved when sliding the tare calibration sliders
* (RS5-7374) - [L515] Error after performing HW reset
* (DSO-15118) - [D400] Viewer is closed forcibly with cycling start/stop streaming in 3D view.

## Release 2.40.0
Release Date: 18 Nov 2020

**Ubuntu 20 LTS and Ubuntu 18 LTS with kernel 5 known issue**:  
We encountered high level of frame drops with several consecutive, compared to Ubuntu16/Kernel 4.  
Streaming Librealsense with Kernel 5 results in a higher frame drop rate that is intensified by consecutive frame drops, mostly between 2-4 frames in a row, reaching 7 frames in certain cases.  
The recurrence rate at which the drops appear is affected by CPU and resources utilization:  
-	For streaming Depth at VGA resolution, the frame drops may occur in intervals of 5-30 min.  
-	Streaming Depth+IR+RGB+IMU the frame drops will appear within 1-3 minutes.  
-	IMU-enabled devices, such as D435i, D455 and L515 encounter higher rate of frame drops compared to D415 and D435.  

Kernel 5 is currently the backbone of Ubuntu 18 and Ubuntu20, thus the issue would affect most users of those versions.  
Note that the frame drops issue is confirmed in Ubuntu LTS 18 and 20, and it does not affect Ubuntu16 with LTS kernel 4.4.  

<ins>Mitigation Plan and Alternatives</ins>  
Realsense support team is working to resolve the issue on SDK and OS levels, and while the investigation is ongoing, one recommended method to mitigate this is by compiling and running Librealsense SDK with RSUSB backend - 
(`cmake .. -DFORCE_RSUSB_BACKEND`).  
The main limitation of RSUSB backend is that it is not suited for multi-cam scenarios. But whenever applicable - using RSUSB backend allows to bypass the Video4Linux kernel APIs and communicate with the device using generic USB driver.  
Switching to RSUSB backend provides a mitigating for sequential frame drops.  

### API Changes
https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2400


### New Features
* [#7802](https://github.com/IntelRealSense/librealsense/pull/7802) - **[D400] FW Update - 5.12.9.0**
    - Bug fixes
    - Suppportfor DFU counter reset
* [#7808](https://github.com/IntelRealSense/librealsense/pull/7808) - **[L515] FW Update - 1.5.2.0**
    - Bug fixes
    - Alternate IR option

* [#7675](https://github.com/IntelRealSense/librealsense/pull/7675) - **[L500] Add reflectivity estimator to Depth Quality Tool**  
    Calculates and provides LiDAR reflectivity estimation.
    - Add "RS2_NOISE_ESTIMATION" option  
    - Add "RS2_ENABLE_IR_REFLECTIVITY" option  
    - Add reflectivity algorithm class  
    - Add display at the DQT application  
    - Current calculations for display occurs at frame rate  
    _Note: This PR does not contain all logical conditions for activating IR Reflectivity_
    (RS5-9263)  
* [#7672](https://github.com/IntelRealSense/librealsense/pull/7672) - **[L500] Add Alternate IR**  
    Requires firmware_version 1.5.2.0+. (RS5-9266);

* [#7637](https://github.com/IntelRealSense/librealsense/pull/7637) - **[L500] Add Max Usable Range feature**   
    Max Usable Range calculates the maximum range of the camera given the amount of ambient light in the scene.
    - Added to Viewer + LibRS API (not DQT)
    - New option : RS2_OPTION_MAX_USABLE_RANGE
    - New LibRS API function "rs2_get_max_usable_range"
    - Add sensor extension "RS2_EXTENSION_MAX_USABLE_RANGE_SENSOR"
    - Wrappers handling (tested on C#, Python, C++)
    - Requires FW version >= 1.5.0.0 (added NEST (Noise-Estimation) value to temperatures)  
    (RS5-9263)   

* [#7692](https://github.com/IntelRealSense/librealsense/pull/7692) - **Ubuntu 20 (Focal) Support**  
    Alpha level - see above note.  
    Update the patches list with Ubuntu Focal configuration. (DSO-16026)
* [#7518](https://github.com/IntelRealSense/librealsense/pull/7518) - **Add support for Jetson Xavier**  
    Adding kernel patches for V4L driver for Jetson with Ubuntu18, tested with L4T version 4.2.3, 4.3 and 4.4 Jetson installation guide update.  
    (DSO-15132)
* [#7470](https://github.com/IntelRealSense/librealsense/pull/7470) - **Tensorflow Examples**  (DSO-15262)


### Bug Fixes and Enhancements
* [#7706](https://github.com/IntelRealSense/librealsense/pull/7706) - **[L500] Add Reflectivity option restrictions**
    - When changing resolution (sensor mode) during streaming and Enable IR Reflectivity on , IR reflectivity will be disabled (With no error pop up window)
    - ROI restriction is not addressed on this PR  
    (RS5-8358)  
* [#7724](https://github.com/IntelRealSense/librealsense/pull/7724) - **[L500] Disable AC if Alt IR is on.** Throw exception when "Alt IR" is on and user asks for AC.
* [#7718](https://github.com/IntelRealSense/librealsense/pull/7718) - **Trim newlines utility function**  
* [#7635](https://github.com/IntelRealSense/librealsense/pull/7635) - **Udev rules power down**  (On kernel 5.x.x.x there is an issue with iio sensors - some of them does not reduce power consumption after connection.
    This udev-rules correction disables the sensor after it's initial connection.
    (DSO-15863)  
* [#7548](https://github.com/IntelRealSense/librealsense/pull/7548) - **Update rsutil.h**  (In order to remove a warning generated by Rust bindgen) contributed by [@neilyoung](https://github.com/neilyoung)  
* [#7693](https://github.com/IntelRealSense/librealsense/pull/7693) - **Fix Platform Camera streaming  and controls**  
    - Reenable Webcam streaming, with YUYV/2 and MJPEG formats.  
    - Present USB type field.  
    - Extract UVC Header timestamp (metadata) when applicable.  
* [#7687](https://github.com/IntelRealSense/librealsense/pull/7687) - **Fix `Resource deadlock avoided` exception on thread join**  (It's illegal to call **_thread->join();** in capture_loop().
    Setting **_is_capturing = false** breaks to outer while loop, and the thread should be terminated.) contributed by [@Domos](https://github.com/Domos)
 * [#7689](https://github.com/IntelRealSense/librealsense/pull/7689) - **Colorizer - fix division by zero**  (RS5-8689)  
* [#7698](https://github.com/IntelRealSense/librealsense/pull/7698) - **[Viewer] Fix out-of order resources deallocation**  Continued from [#7647](https://github.com/IntelRealSense/librealsense/issues/7647). (DSO-15827)  
* [#7700](https://github.com/IntelRealSense/librealsense/pull/7700) - **[Viewer] Fix playback Resuming in 3D viewer**  (Fixed :
    - Resuming of playback  and 3D viewer are now coupled , when resuming playback - 3D automatically promoted from pause to to play.
    - Stop uploading texture when playback is running and 3D viewer is resumed.  
    (DSO-12584)
* [#7673](https://github.com/IntelRealSense/librealsense/pull/7673) -  **[D400] Skip enabling HDR if already enabled**  (DSO-15909)  
* [#7697](https://github.com/IntelRealSense/librealsense/pull/7697) - **[Java] Fix syntax error on Option.java.**
* [#7674](https://github.com/IntelRealSense/librealsense/pull/7674) - **ROS dependencies update**
    Remove some dependencies to be build dependencies. (DSO-15133)
* [#7558](https://github.com/IntelRealSense/librealsense/pull/7558) - **[D400] HDR-merge and sequence-id-filter proc blocks
    Also add ROS recorder read/write (DSO-15980)  
* [#7619](https://github.com/IntelRealSense/librealsense/pull/7619) - **Fw logger tool adding sleep**  
    Adding a sleep between each fw log polling. Default value is hardcoded in the tool's code. Can be passed from command line with -s <sleep_in_ms> flag
    (DSO-15939)  
* [#7645](https://github.com/IntelRealSense/librealsense/pull/7645) - **Fix colorizer - Get depth units from depth_sensor API.**  
    Get depth units from depth_sensor API instead of disparity_info to support also l500.
[[#7089](https://github.com/IntelRealSense/librealsense/issues/7089)](https://github.com/IntelRealSense/librealsense/issues/7089)  
    Disabling histogram equalization leads depth map to void. (RS5-8868)  
* [#7668](https://github.com/IntelRealSense/librealsense/pull/7668) - **[Python] Fix Python API backwards compatibility for "lld_temperature"**  
    Python API to support both lld_temperature and ldd_temperature. (RS5-9334)  
* [#7543](https://github.com/IntelRealSense/librealsense/pull/7543) - **[D400] HDR live tests update** 
    Checking lrs version, FW support options and metadata available.  
* [#7649](https://github.com/IntelRealSense/librealsense/pull/7649) - **[Viewer] Improve IR stream footer text.**  
    Remove the equal mark from IR stream footer text on mouse picking. (DSO-15449)
* [#7301](https://github.com/IntelRealSense/librealsense/pull/7301) - **[MacOS] define SQLITE_HAVE_ISNAN so xcode 10 will compile**  
    When trying to build librealsense using xcode 10 on macOS 10.14 I ran into the following build error: "SQLite will not work correctly with the -ffast-math option of GCC.   
    The following stackoverflow post suggests defining SQLITE_HAVE_ISNAN in order to fix that error:  https://stackoverflow.com/questions/48917320/getting-gcc-error-when-using-sqlite-and-fast-math-sqlite-will-not-work-correct .  
    contributed by [@robtherich](https://github.com/robtherich)
* [#7573](https://github.com/IntelRealSense/librealsense/pull/7573) - **[NodeJS] Resolve errors and deprecated warnings when build wrapper on nodejs 12**  
    - Updated jsdoc to 3.6.6 to support nodejs 12, but it drop compatibility with nodejs 6.  
    - Update nan for latest version.  
    contributed by [@whsol](https://github.com/whsol)
* [#7648](https://github.com/IntelRealSense/librealsense/pull/7648) - **[Viewer] Remove dots from some of the controls tool tip.**  (DSO-15513)  
* [#7650](https://github.com/IntelRealSense/librealsense/pull/7650) - **[L500] Spelling fix for LDD option**  
    Fixed spelling mistake of LDD Temperature option on get_string(),   The enum value cannot be changed due to backward compatibility. (RS5-8371 )  
* [#7653](https://github.com/IntelRealSense/librealsense/pull/7653) - **Improve the description of alternating_emitter_option.**  (DSO-15512)
* [#7367](https://github.com/IntelRealSense/librealsense/pull/7367) - **rs-convert tool supports extracting frame\time ranges** (DSO-11742) 
* [#7613](https://github.com/IntelRealSense/librealsense/pull/7613) - **[cmake] Add MULTITHREADED/APARTMENTTHREADED selector flag.**  
    Sets the param passed to CoInitializeEx call (Windows only) (DSO-15197)  
* [#7625](https://github.com/IntelRealSense/librealsense/pull/7625) - **[Unity] Adapting wrapper to Unity 2018-2020**  
    - Deleted GUI Layer from some scenes  
    - Added AR Background Resitrictions for  prevent using old classes in Unity 2020)  
    contributed by [@SergeySPF](https://github.com/SergeySPF)  
* [#7632](https://github.com/IntelRealSense/librealsense/pull/7632) - **[NodeJS] Add missing enums to nodejs wrapper and python script for check enums**  
    contributed by [@whsol](https://github.com/whsol)  
* [#7604](https://github.com/IntelRealSense/librealsense/pull/7604) - **Handle exception in uvc_sensor::acquire_power**  
    Reduce the reference counter in case that exception throws in acquire_power (RS5-9071)  
* [#7627](https://github.com/IntelRealSense/librealsense/pull/7627) - **Filter only intel product line on DQT tool**  
    DQT recognizes platform camera when no Realsense  device connected. (DSO-15662)  
* [#7600](https://github.com/IntelRealSense/librealsense/pull/7600) - **[L515] - Add temperature fetcher thread**  
    Periodic read of temperatures and noise estimation values once depth sensor is on.  
    Precondition for reflectivity tool + max range  
    *Description:*
    - A new temperature fetcher thread is created on depth sensor start and closed on stop.
    - All of temperature clients get it from a "get_temperatures" new function 
    - get_temperatures function return the protected fetcher values or read directly from the FW is no fresh values from the thread
    - N-Est values are exposed only with FW ver 1.5.0.0+
    (RS5-9263)  
* [#7610](https://github.com/IntelRealSense/librealsense/pull/7610) - **[Viewer] FW update error popup fix**  (DSO-15557)  
* [#7563](https://github.com/IntelRealSense/librealsense/pull/7563) - **[L515] Add digital gain option that replaces ambient light option** 
    * Add digital gain option at same option value as ambient light.  
    * Add comment on LRS API option ambient light that it is deprecated.  
    * Replace ambient light option registration with digital gain option (same value for both) from L515 depth sensor  
    * Add it to all wrappers  
    * Add special case for python wrapper for dealing with 2 options with the same value  
    * no ambient == high gain  
    (RS5-9152)
* [#7598](https://github.com/IntelRealSense/librealsense/pull/7598) - **Fix humidity option compilation**  (Fix for PR [7474](https://github.com/IntelRealSense/librealsense/pull/7474) compilation issue)
* [#7474](https://github.com/IntelRealSense/librealsense/pull/7474) - **Add description to l500-options**  (RS5-8665)
* [#7572](https://github.com/IntelRealSense/librealsense/pull/7572) - **Added default ctor and initialized valid**  
    Added a default constructor for thermal_calibration_table class, and in non-default constructor added an initialization of _header.valid.
* [#7554](https://github.com/IntelRealSense/librealsense/pull/7554) - **Enable BUILD_EASYLOGGINGPP=OFF**  contributed by [@hsuys](https://github.com/hsuys)
* [#7452](https://github.com/IntelRealSense/librealsense/pull/7452) - **Add time utility classes + UT**  
    * Add a space for common utility classes.  
    * Take timer helper functions from rendering.h , refactor + add functionality and place on timer.h utility file  
    * Replace all time functions usage with new classes  
    * Add unit tests for 3 new classes)  
* [#7551](https://github.com/IntelRealSense/librealsense/pull/7551) - **[L500] Pick K_th fixes**  
* [#7542](https://github.com/IntelRealSense/librealsense/pull/7542) - **[HDR] Return nullptr instead of throwing exception**
* [#7437](https://github.com/IntelRealSense/librealsense/pull/7437) - **Identify L515 USB2 DFU mode**  
    L515 Units with old payload 0 on the device connected with USB2 is identify as D4xx device on DFU mode.
    This PR use the DFU version to determine is the DFU device is L515 or D4xx.
    (RS5-8812)
* [#7493](https://github.com/IntelRealSense/librealsense/pull/7493) - **Fix Viewer GUI freeze due to DFU and SW update pop ups interference**  
    Both processes try to open a popup modal from the same popup tree level, which
is not supported in ImGui.  
    - Handle error message during SW update process (Do not allow pop up from inside a pop up).  
    - Display SW update popup only when no other expended notification is being displayed.  
    (RS5-8934)  
* [#7500](https://github.com/IntelRealSense/librealsense/pull/7500) - **Fix sporadic spinning bug in rs-motion.cpp**
    If the first gyro frame is received before the first accel frame the "first" flag is cleared and the timestamp value is incorrect. This separates the flags to be method specific.  
    Addresses [#4915](https://github.com/IntelRealSense/librealsense/issues/4915))   
* [#7512](https://github.com/IntelRealSense/librealsense/pull/7512) - **Fix error poller method.**  
    - Support graceful sensors closure on switching to DFU mode.  
    - Minor formatting and wording corrections.  
    (RS5-8661)  
* [#7522](https://github.com/IntelRealSense/librealsense/pull/7522) - **[HDR]  Enhancements**  
    Replace exceptions with log writing when accessing affected controls while in HDR mode.  
    Adding HDR unit-tests suite.  
    (DSO-15603), (DSO-15592)
* [#7514](https://github.com/IntelRealSense/librealsense/pull/7514) - **[L500] Fix GlobalTimer**  
    Fix FW clock readings. Addresses [#7508](https://github.com/IntelRealSense/librealsense/issues/7508) (RS5-9117)  
* [#7023](https://github.com/IntelRealSense/librealsense/pull/7023) - **Fix warning C4005 macro redefinition**  
    ex: warning C4005: 'WIN32_LEAN_AND_MEAN': macro redefinition. contributed by [@chadbramwell](https://github.com/chadbramwell)  
* [#7515](https://github.com/IntelRealSense/librealsense/pull/7515) - **Fix black pixel at upper left corner** 
    Remove setting upper left pixel on viewer to 0,0,0 (Black) (RS5-8960)
* [#7501](https://github.com/IntelRealSense/librealsense/pull/7501) - **[Python] Add D455 product id to python-rs400-advanced-mode-example.py**  
    Even though a hardcoded list of magic numbers is a bad practice, the example should work with the new D455 sensor. contributed by [@Petrox](https://github.com/Petrox)
* [#7440](https://github.com/IntelRealSense/librealsense/pull/7440) - **[Viewer] make snapshots for IMU and pose frames.**  (RS5-8691)  
* [#7490](https://github.com/IntelRealSense/librealsense/pull/7490) - **Add L515 humidity temperature option**  (RS5-9069)
* [#7468](https://github.com/IntelRealSense/librealsense/pull/7468) - **Fix depth stream freeze in post-processing**
    Use streams_origin to find out the origin ID of stream in case the frame is after some post-processing.  
* [#7451](https://github.com/IntelRealSense/librealsense/pull/7451) - **Fix stuck depth scene in texture mapping** (RS5-8690)  
 [#7413](https://github.com/IntelRealSense/librealsense/pull/7413) - **Improve the message at the end of the recording** (RS5-8698) 
* [#7297](https://github.com/IntelRealSense/librealsense/pull/7297) - **[L500] Krgb-thermal support**  
    Reading k-thermal table from FW and use it to correct k-rgb according to current humidity temp, the k-rgb that written to FW is without the thermal correct ,but user will get the corrected one.  
* [#7438](https://github.com/IntelRealSense/librealsense/pull/7438) - **added CMake error when BUILD_WITH_TM2 is on but IMPORT_DEPTH_CAM_FW is off**  (RS5-8833)  
* [#7412](https://github.com/IntelRealSense/librealsense/pull/7412) - **Improve error message**  
    Improve the error message in custom preset when sensor_mode is incompatible with requested resolution. (RS5-8797 )  

 ### Documentation
* [#7725](https://github.com/IntelRealSense/librealsense/pull/7725) - **update installation.md for Udev rules power down**  
    add "at" installation to installation.md
* [#7061](https://github.com/IntelRealSense/librealsense/pull/7061) - **Document the order of the coefficients in rs2_intrinsics::coeffs**  
    I could not find the order of the distortion coefficients in the Realsense documentation so I am adding them to it. I am assuming that they follow the order that is used in OpenCV for its distortion coefficients. contributed by [@foohyfooh](https://github.com/foohyfooh)
* [#7329](https://github.com/IntelRealSense/librealsense/pull/7329) - **Fix documentation in C API examples**  
    Fix some comments in the documentation of C API examples. contributed by [@gsaponaro](https://github.com/gsaponaro)  
* [#7382](https://github.com/IntelRealSense/librealsense/pull/7382) - **Fixed Typos in Documentation**  contributed by [@harshmittal2210](https://github.com/harshmittal2210)


### Known Issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block.
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized variable. (DSO-13700)
* [#4261](https://github.com/IntelRealSense/librealsense/issues/4261) - [T265] Add ability to open multiple devices from different processes.
* [#4518](https://github.com/IntelRealSense/librealsense/issues/4518) – [T265] Pose data produces `NaNs`. Can still occur in some cases. If detected, please attempt to make a raw data (images + IMU) recording using the [recorder tool](https://github.com/IntelRealSense/librealsense/tree/master/tools/recorder), and attach a link to it in the github issue, to assist our resolution.
* [#6009](https://github.com/IntelRealSense/librealsense/issues/6009) v2.33.1 does not compile with -DBUILDEASYLOGGINGPP=OFF
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* (DSO-13525) - [D400] 3D viewer moved when sliding the tare calibration sliders
* (RS5-7374) - [L515] Error after performing HW reset
* (DSO-15118) - [D400] Viewer is closed forcibly with cycling start/stop streaming in 3D view.
* (DSO-15250) - [Viewer] with OpenVINO stops the RGB stream when IMU is activated


## Release 2.39.0
Release Date: 1 Oct 2020

### API Changes
https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2390


### New Features
* [#7453](https://github.com/IntelRealSense/librealsense/pull/7453) - **D4xx Firmware - v5.12.8.200** 
  - **Important: the firmware is intended to be used with SDK v2.39.0(+) and is not compatible with the previous Librealsense releases**
  - Add RGB stream from Left IR imager (D455).
  - HDR Functionality for D400 Depth sensors
* [#7466](https://github.com/IntelRealSense/librealsense/pull/7466) - **L515 Firmware -  v1.5.1.3**  
  - Support for USB2
  - Support QVGA resolution
  - Controls and stability improvements

* [#6916](https://github.com/IntelRealSense/librealsense/pull/6916) - **[D455] Add synthetic RGB from left imager**
* [#7205](https://github.com/IntelRealSense/librealsense/pull/7205) - **[Android] Enable USB weak host workaround to improve android stability**  
Enable L515 android stability workaround. Current USB buffer settings in device require large number of small USB transactions from device to USB host controller, some hosts with relatively weak host cannot handle this many transactions quickly, results in stability issues, camera disappear from os enumeration or one of the streams dies during stream, etc. This workaround uses larger buffer settings to reduce the number of transaction and improve performance and stability.
Workaround is enabled by default for all android platforms. Requires additional firmware command support in new firmware release.
(RS5-8011)
* [#7354](https://github.com/IntelRealSense/librealsense/pull/7354) - **Depth HDR Functionality (Alpha Release)** 
A new Depth enhancement feature implemented in both Host and the Device Firmware that allows to improve Depth data in adverse light conditions by merging data from consecutive frames.
At its core the feature allows to configure and run Depth and IR stream with per-frame specified exposure and gain values (sequence of 2 frames). Dedicated metadata attributes allow to associate the arriving frames with the configuration established by user.  
When arrived on host a specially-designed `Merge` processing block shall be utilized to fuse depth data from the incoming frames.
Additionally a `Filtered Id` processing block is provided that allows to forward Depth and IR frames that correspond to even/odd frames of the sequence.  
The feature is integrated with `Realsense-Viewer` application via a set of dedicated controls and post-processing blocks.  
It is an Alpha release, and will be followed by a White Paper for in-depth presentation and usage guidance.  
(DSO-15603)  
Complementary PRs:   
    * [#7257](https://github.com/IntelRealSense/librealsense/pull/7257) - **[HDR] Split/Merge processing blocks infrastructure**  
    * [#7354](https://github.com/IntelRealSense/librealsense/pull/7354) - **[HDR] Support for sub-preset configuration**
    * [#7397](https://github.com/IntelRealSense/librealsense/pull/7397) - **[HDR] Adjust metadata names**  
    * [#7404](https://github.com/IntelRealSense/librealsense/pull/7404) - **[HDR] Update metadata sequence id**  
* [#7385](https://github.com/IntelRealSense/librealsense/pull/7385) - **[Network-device] support for D455 and L515, update list of supported streaming configurations**

### Bug Fixes and Enhancements
* [#7409](https://github.com/IntelRealSense/librealsense/pull/7409) - **[Python] Reverting the parametrized `get_depth_frame`**
* [#7403](https://github.com/IntelRealSense/librealsense/pull/7403) - **[Windows] Fix crash on stop sensors**  Use auto reset event instead of manual event (RS5-8921)
* [#7368](https://github.com/IntelRealSense/librealsense/pull/7368) - **[Android] Correct  the order in the extensions list**  Change in `rs_extension` had been done while the new enums were added out of order.
* [#6564](https://github.com/IntelRealSense/librealsense/pull/6564) - **[Core] Min Distance can't get higher value than the Max Distance**   Affects Threshold Filter and Depth Visualization (DSO-12346, DSO-12163)
* [#7344](https://github.com/IntelRealSense/librealsense/pull/7344) - **[L515] Disable unsigned FW update for unlocked units**
* [#7330](https://github.com/IntelRealSense/librealsense/pull/7330) - **Refactor error polling with shared/weak ptr instead of unique_ptr.**  (Switch to shared/weak ptr model for active object handler. Follow up on [#7272](https://github.com/IntelRealSense/librealsense/issues/7272). (DSO-15542)
* [#7334](https://github.com/IntelRealSense/librealsense/pull/7334) - **[L515] Fix DFU crash**  DFU update on L515 is generating an exception and fails the update process. The Asic serial number structure retrieved from the DFU FW has reversed bytes order then expected. This PR address this issue and treat the data at the real FW implementation order.
(RS5-8843)
* [#7323](https://github.com/IntelRealSense/librealsense/pull/7323) - **[Viewer] Fix SW update crash**  (When connecting a device, the SW Update checks whether a new version is available.
  - Use a weak ptr for the SW update detached thread
  - Reduce the DB download tried to 1 (still 5 seconds timeout)
(RS5-8694)
* [#7325](https://github.com/IntelRealSense/librealsense/pull/7325) - **[L515] Remove SW limitation of noise filter range**  (Previous SW min limit=2 for noise filter option) 
* [#7272](https://github.com/IntelRealSense/librealsense/pull/7272) - **Stability enhancemets** Follow up on [#7330](https://github.com/IntelRealSense/librealsense/pull/7330).
* [#7246](https://github.com/IntelRealSense/librealsense/pull/7246) - **[L515] Callback receives depth+infrared when only depth is requested**  (Add test: "test-depth-only" - checks that if depth is the only profile specified when opening a sensor, the callback given at sensor.start() will be called for depth frames only)
* [#7294](https://github.com/IntelRealSense/librealsense/pull/7294) - **[Viewer] SW Updates - Fix UI issue + allow use of local DB file**  
  - Overcome `ImGui` issue that cause an error pop_up closing the updates window. (always force open it unless no need)
 -Allow local version's DB file use. (Treat "file://" prefix as local file path)
* [#7269](https://github.com/IntelRealSense/librealsense/pull/7269) - **[L515] Filter IR frames if were not requested by the user**  (RS5-8734)
* [#7203](https://github.com/IntelRealSense/librealsense/pull/7203) - **[L515] Fix GVD fields for S.N. & lock statuses**  (RS5-8550)
* [#7293](https://github.com/IntelRealSense/librealsense/pull/7293) - **[L515] Fix crash when connecting L515 device to the Viewer application**  
   - Addresses issue caused on [PR 7126](https://github.com/IntelRealSense/librealsense/pull/7126/files) caused by a virtual function call from inside L515 ctor -- replaced with a member variable.
* [#7278](https://github.com/IntelRealSense/librealsense/pull/7278) - **Report reason for stream stop (HW errors)**  (RS5-7657)
* [#7259](https://github.com/IntelRealSense/librealsense/pull/7259) - **[Linux] named_mutex: release if exception occurs while locking.**  (Addresses deadlock in viewer when unplugging camera while streaming and then plugging it back
 Add to unit-tests (multicam_streaming test) - test streaming of depth only due to known issues with alternate streaming of color. (DSO-15623)
* [#7248](https://github.com/IntelRealSense/librealsense/pull/7248) - **[Tools] Print FW logs with host clock**  () contributed by [@ashrafabuesba](https://github.com/ashrafabuesba)
* [#7241](https://github.com/IntelRealSense/librealsense/pull/7241) - **Bundled firmware update no longer fail if backup step fail**  (The flash backup was a mandatory step at the firmware update. At this PR we allow to proceed with the update even if the backup step fail. (DSO-15599)
* [#7213](https://github.com/IntelRealSense/librealsense/pull/7213) - **Fix a wrong argument in "CreateEvent" call** contributed by [@libdavid](https://github.com/libdavid)
* [#7238](https://github.com/IntelRealSense/librealsense/pull/7238) - **Add rs2_cah_trigger_to_string to realsense.def** 
* [#6304](https://github.com/IntelRealSense/librealsense/pull/6304) - **BUILD_EASYLOGGING = OFF **  (compiles with -DBUILD_EASYLOGGINGPP=OFF and unit_tests pass. Addresses [#6009](https://github.com/IntelRealSense/librealsense/issues/6009) 
* [#7207](https://github.com/IntelRealSense/librealsense/pull/7207) - **[Viewer] Fix crash on Fw update**  enhancement over PR7165. (DSO-15554)
* [#7186](https://github.com/IntelRealSense/librealsense/pull/7186) - **[Viewer] Prevent imGui state corruption in Viewer**  by encapsulating exception handling. (DSO-15542)
* [#7184](https://github.com/IntelRealSense/librealsense/pull/7184) - **Fix SW update links**  (The SW Update links filling were mistakenly comment out on a LLVM unused variable warning fix This PR undo it and removed the unused variables.)
* [#7158](https://github.com/IntelRealSense/librealsense/pull/7158) - **[L515] Remove depth invalidation option** 
* [#7165](https://github.com/IntelRealSense/librealsense/pull/7165) - **[Viewer] Disable click on fw update buttons when streaming**  (DSO-15554)
* [#7171](https://github.com/IntelRealSense/librealsense/pull/7171) - **Uncomment "message" call**  (Call to message(STATUS "Checking internet connection...") was commented.) contributed by [@libdavid](https://github.com/libdavid)
* [#7161](https://github.com/IntelRealSense/librealsense/pull/7161) - **Fix backward button issue with viewer.**  (the function rs2_playback_seek behind the playback::seek function can't receive negative timestamps. (DSO-15533)
* [#7164](https://github.com/IntelRealSense/librealsense/pull/7164) - **Add OpenCV license to NOTICE** 
* [#7147](https://github.com/IntelRealSense/librealsense/pull/7147) - **[L515] Fix CAH unit-tests file separators in Linux**
* [#7148](https://github.com/IntelRealSense/librealsense/pull/7148) - **added 'using std::abs' in types.h to avoid Linux bug**  (This fixes a bug where compilation occurs on Ubuntu 16, where GCC decides to use abs(int) and this obviously causes bad results. The global ::abs() should really never be used, but I found no good way of removing it.
(RS5-8641)
* [#7133](https://github.com/IntelRealSense/librealsense/pull/7133) - **[Viewer] Fix ctrl+Num keys not rendering on viewer terminal**  
  - Viewer application does not support special keys like '_' (all of shift + Num keys) as inputs. The code that was removed on this PR is the reason. It was inserted as part of the terminal feature to allow copy-paste capabillity.
* [#7131](https://github.com/IntelRealSense/librealsense/pull/7131) - **Occlusion bug fix**  Addresses [#6990](https://github.com/IntelRealSense/librealsense/issues/6990). (RS5-8522)
* [#7126](https://github.com/IntelRealSense/librealsense/pull/7126) - **[L515] Update tagged profiles for USB2**  
  - Remove raw resolutions processing blocks
  - Update L515 tagged profiles to support USB2 resolutions
  - Remove viewer resolutions + FPS constrains for USB 2 (behavior stayed the same due to tagged profiles)
(RS5-8384)
* [#7097](https://github.com/IntelRealSense/librealsense/pull/7097) - **Fix sensor mode exception on QVGA resolution (USB2)**
  - Sensor default mode change from XGA to [USB3 -> VGA], [USB2 -> QVGA]. (RS5-8352)
* [#7105](https://github.com/IntelRealSense/librealsense/pull/7105) - **[L515] Fixed bug on manual trigger:**  (movement_from_last_success is always true in manual trigger.) 
* [#7094](https://github.com/IntelRealSense/librealsense/pull/7094) - **[L515] Handle dynamic number of intrinsic tables for USB2 support**  On L515 device we get 2 depth resolutions on USB3 and 1 on USB2.
This PR replace sharing the FW raw data vector with a physical preallocated full size buffer (Support up to 5 resolutions)
Same change apply for depth and color intrinsic tables handling.
* [#7072](https://github.com/IntelRealSense/librealsense/pull/7072) - **[L515] Set default values for color sensor controls for calibration**  (For better results the CAH process require default values to some of the RGB sensor controls.
When the CAH process need to open the color sensor, it will check and set the sensor controls to it's default values if needed.
When the CAH need to stop the color sensor or if the user will open the sensor while CAH opened it, it will restore the values to it's previous ones if needed.
)
* [#7092](https://github.com/IntelRealSense/librealsense/pull/7092) - **Call to callback from optimize() and optimize_p(), also on non debug mode.**  (Call to callback from optimize() and optimize_p() also on non debug mode, to enable stop in time if needed)
* [#7075](https://github.com/IntelRealSense/librealsense/pull/7075) - **Small optimization in CAH plus removal of two annoying repetitive messages in Viewer**
* [#7074](https://github.com/IntelRealSense/librealsense/pull/7074) - **[L515] New CAH limiters & handling per VAL request**
* [#7076](https://github.com/IntelRealSense/librealsense/pull/7076) - **[Python] Fix GIL locks in calibrated_sensor APIs which can cause frame drops**
* [#7071](https://github.com/IntelRealSense/librealsense/pull/7071) - **[L515] Disable auto CAH by default and update bundled L515 FW version to 1.4.1.2**

### Known Issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block.
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized variable. (DSO-13700)
* [#4261](https://github.com/IntelRealSense/librealsense/issues/4261) - [T265] Add ability to open multiple devices from different processes.
* [#4518](https://github.com/IntelRealSense/librealsense/issues/4518) – [T265] Pose data produces `NaNs`. Can still occur in some cases. If detected, please attempt to make a raw data (images + IMU) recording using the [recorder tool](https://github.com/IntelRealSense/librealsense/tree/master/tools/recorder), and attach a link to it in the github issue, to assist our resolution.
* [#6009](https://github.com/IntelRealSense/librealsense/issues/6009) v2.33.1 does not compile with -DBUILDEASYLOGGINGPP=OFF
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* (DSO-13525) - [D400] 3D viewer moved when sliding the tare calibration sliders
* (RS5-7374) - [L515] Error after performing HW reset
* (DSO-15118) - [D400] Viewer is closed forcibly with cycling start/stop streaming in 3D view.
* (DSO-15250) - [Viewer] with OpenVINO stops the RGB stream when IMU is activated


## Release 2.38.1
Release Date: 27 Aug 2020

### API Changes
https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2381

### New Features
* [#6970](https://github.com/IntelRealSense/librealsense/pull/6970) - **D4xx Firmware - v5.12.7.100**
* [#6997](https://github.com/IntelRealSense/librealsense/pull/6997) - **L515 Firmware -  v1.5.0.0**  

  - Controls and stability improvements
  

### Bug Fixes and Enhancements
* [#6150](https://github.com/IntelRealSense/librealsense/pull/6150) - **global_timestamp_reader: no blocking.**.  Prevent calls for get_device_time_ms from frame thread. Continue system_time-hw_time equation through time loop.
* [#7017](https://github.com/IntelRealSense/librealsense/pull/7017) - **Fix reporting distance with SW device**  (DSO-15441)
* [#6933](https://github.com/IntelRealSense/librealsense/pull/6933) - **Release CComPtr cause to access violation** 
* [#6926](https://github.com/IntelRealSense/librealsense/pull/6926) - **Fw logger bug fix**.  Fixing timestamp in new fw logger message (DSO-15394) 
* [#6945](https://github.com/IntelRealSense/librealsense/pull/6945) - **Android stability fixes** -  Camera app performance/stability issues:
1. fix pipe.stop() issues
   a) problem: null pointer segfault
       cause: thread synchronization in dispatcher
       fix: add additional lock in invoke_and_wait.
  b) problem: invalid address segfault in uvc_streamer, active_object, dispatcher flows
      cause: double release of memory in destructors due to synchronization issue in watchdog, active_object, dispatcher, and primarily uvc_streamer.
      fix: add additional state check and synchronization.
  c) problem: invalid address segfault in pipeline flows
     cause: double release of memory in pipeline destructor to stop pipeline twice, the extra stop come from JNI code when handle is deleted and invokes destructor which tries to run stop() again.
     fix: do shared pointer reset when profile active (also this is when they are initialized).
   d) problem: invalid address segfault at various points depends on workload
      cause: uvc_streamer shared pointers cleared at the wrong time, not cleared when stream stopped, so it tries to stop the streamer a second time during destruction
      fix: destroy uvc_streamers after stream is stopped and no active profiles
   e) problem: invalid address segfault in device info destructor through JNI stack
       cause: appears to be double releasing as JNI stack references the pointer
       fix: avoid delete in JNI, need to confirm it's released on native stack.
2. problem: invalid address segfault related to stream profiles
   cause: profile data array copy in JNI, jlong is 64-bit but pointer could be 32-bit, should not xcopy the entire buffer
   fix: copy array elements
3. problem: invalid address segfault related to query sensors
   cause: sensor data array copy in JNI, jlong is 64-bit but pointer could be 32-bit, should not xcopy the entire buffer
   fix: copy array elements
4. problem: camera app performance degradation and stability issues after repeated device disconnect/connect
   app cannot recover if device disconnected in middle of starting streaming
   cause: activity operations executed multiple times due to activity instances and sequence issue
   fix: create single activity instance, manage its creation and destruction, and correct sequence.  
(RS5-8219, DSO-15293, DSO-15294, DSO-15358, DS5U-4588)
* [#7008](https://github.com/IntelRealSense/librealsense/pull/7008) - **Query Projector capability from FW**  (DSO-15453)
* [#7000](https://github.com/IntelRealSense/librealsense/pull/7000) - **Increase logs queue size on viewer**  (The current queue is at default size of 10, that means that at a viewer cycle when the logs queue is full after 10 logs the output window will miss logs.
This PR increase the logs to 100 in order to display a peek of logs.) 
* [#6567](https://github.com/IntelRealSense/librealsense/pull/6567) - **Viewer starts without Documents directory**.  Fixes [#5707](https://github.com/IntelRealSense/librealsense/issues/5707)
(DSO-13589) 
* [#6950](https://github.com/IntelRealSense/librealsense/pull/6950) - **Fix viewer error while trying to replay a bag file with confidence stream**  
* [#6929](https://github.com/IntelRealSense/librealsense/pull/6929) - **Remove dependency on fts.h**  (`fts.h` is included in these two backend-v4l2 files but not actually used.) contributed by [@dbolkensteyn](https://github.com/dbolkensteyn)
* [#6954](https://github.com/IntelRealSense/librealsense/pull/6954) - **Fix extrinsics related log_warnings while recording on L515**  (Fix a log warning when recording RGB / Confidence streams on L515 devices) 
* [#6998](https://github.com/IntelRealSense/librealsense/pull/6998) - **Set L500 Depth Invalidation flag to false by default**  
* [#6999](https://github.com/IntelRealSense/librealsense/pull/6999) - **Remove Y8I conditional invocation for USB2**  (DSO-15468)
* [#7007](https://github.com/IntelRealSense/librealsense/pull/7007) - **Add missing string to rs_sensor_mode enum**
* [#7046](https://github.com/IntelRealSense/librealsense/pull/7046) - **Always open IR stream with depth on L515**
* [#7004](https://github.com/IntelRealSense/librealsense/pull/7004) - **Disable CAH buttons on viewer device menu** 


### Documentation
* [#7034](https://github.com/IntelRealSense/librealsense/pull/7034) - **Link fix**  () contributed by [@fburak](https://github.com/fburak)
* [#7011](https://github.com/IntelRealSense/librealsense/pull/7011) - **Update imu calibration white paper public web link**  (The latest IMU white paper refresh was published to a different web link than previously documented in read.md. This change is to update the link. No impact to calibration script or user.)


## Pre-Release 2.37.0
Release Date: 27 Jul 2020

* [#6872](https://github.com/IntelRealSense/librealsense/pull/6872) - *Adjusting Unity package to work with L515 out of the box* (This PR is introducing couple of small changes with regards to IR stream indexing to ensure Unity sample works with both D400 and L500 families of cameras. Following-up on [#6579](https://github.com/IntelRealSense/librealsense/issues/6579) and [#6859](https://github.com/IntelRealSense/librealsense/issues/6859) 
* [#6891](https://github.com/IntelRealSense/librealsense/pull/6891) - *Adjust windows cmake build to v3.6+*
* [#6848](https://github.com/IntelRealSense/librealsense/pull/6848) - *pointcloud::calculate add const specifier* contributed by [@ohadmen](https://github.com/ohadmen)
* [#6884](https://github.com/IntelRealSense/librealsense/pull/6884) - *Pyrealsense2-net wrapper* (follow-up on https://github.com/IntelRealSense/librealsense/pull/6171 by @lramati. Adds `pyrealsense2-net` wrapper for `realsense2-net` extension module)
* [#6889](https://github.com/IntelRealSense/librealsense/pull/6889) - *Re-enable Thermal Compensation loop* (Requires FW v5.12.7+
Tracked on:DSO-14980)
* [#6645](https://github.com/IntelRealSense/librealsense/pull/6645) - *Fix rs-tracking-and-depth with T265* contributed by [@pjessesco](https://github.com/pjessesco)
* [#5800](https://github.com/IntelRealSense/librealsense/pull/5800) - Fix a bug concerning connection to multiple cameras by several threads or processes with V4L2 backend (Linux). Tracked on: DSO-13745
* [#6880](https://github.com/IntelRealSense/librealsense/pull/6880) - *Adding GPIOs state to metadata* (Field added in metadata in order to support GPIO Input Data. Triggered by jira ticket: DSO-15379)
* [#6793](https://github.com/IntelRealSense/librealsense/pull/6793) - *Controls emitter options list changed to dynamic* (Triggered by jira ticket: DSO-15273. In android camera application, the relevant values for the emitter options are now dynamic. This implements also the requirements due to jira ticket: RS5-8168)
* [#6818](https://github.com/IntelRealSense/librealsense/pull/6818) - Android Wrapper - Projection API functions added. Triggered by jira ticket: DSO-15199
* [#6858](https://github.com/IntelRealSense/librealsense/pull/6858) - *Improved Error Handling for SDK tools* (On Windows, the Viewer & DQT could silently fail in certain situations, now all errors will be reported with an option to submit as a new GitHub ticket)
* [#6786](https://github.com/IntelRealSense/librealsense/pull/6786) - *Fix the return value of poll_for_frames in MATLAB wrapper*  contributed by [@mengyui](https://github.com/mengyui)
* [#6809](https://github.com/IntelRealSense/librealsense/pull/6809) - *remove rs_terminal_parser.h - obsolete file*
* [#6779](https://github.com/IntelRealSense/librealsense/pull/6779) - *Auto-exposure Priority Mode tooltip text corrected* (Tooltip text corrected so that the user would understand the implications of turning ON/OFF the Auto Exposure Priority button in realsense viewer.
Tracked on: DSO-13683)

## Release 2.36.0
Release Date: 9 Jul 2020

### API Changes
https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2360


### New Features
* [#6782](https://github.com/IntelRealSense/librealsense/pull/6782) - **New 4xx FW v5.12.6.0** - Stability and performance enhancements
* [#6694](https://github.com/IntelRealSense/librealsense/pull/6694) - **New L515 FW v1.4.1.2** - Stability and performance enhancements  
  - * (RS5-6586) - [L515] Corrupted Depth and IR  
* [#6679](https://github.com/IntelRealSense/librealsense/pull/6679),[#6623](https://github.com/IntelRealSense/librealsense/pull/6623),[#6666](https://github.com/IntelRealSense/librealsense/pull/6666),[#6743](https://github.com/IntelRealSense/librealsense/pull/6743) - **RealSense-Viewer Enhancements**  
  - Firmware logs window to streamline profyling and debugging (DSO-14959)
  - Terminal for Firmware commands and calibration data window (DSO-15212)
* [#6587](https://github.com/IntelRealSense/librealsense/pull/6587) - **[L515] Add IMU Calibration and Motion Correction support**
  - Support L515 IMU calibration and motion correction
  - Updated L515 extrinsic
  - Keep lazy design, use default intrinsic in case valid calibration data is not available on device
  - Calibration instruction aligned for all D400 and L515 devices with IMU  
(RS5-7834)
* [#6594](https://github.com/IntelRealSense/librealsense/pull/6594) - **Android intrisics extrinsics added to wrapper**  (Adding Intrisic and Extrinsic libRS API to the Android wrapper)  
  - rs2_get_extrinsics
  - rs2_get_video_stream_intrinsics
  - rs2_get_motion_intrinsics  
Resolves [#4580](https://github.com/IntelRealSense/librealsense/issues/4580) (DSO-14957) 

### Bug Fixes and Improvements
* [#6680](https://github.com/IntelRealSense/librealsense/pull/6680) - **[C#] Add L500 preset enum**   -Add enum similar to `Rs400VisualPreset` to easily set `Option.VisualPreset` for L500 devices. contributed by [@jangernert](https://github.com/jangernert)
 
* [#6615](https://github.com/IntelRealSense/librealsense/pull/6615) - **[C#] fix L500 intrinsic initialization**. Addresses [#6609](https://github.com/IntelRealSense/librealsense/issues/6609) contributed by [@jangernert](https://github.com/jangernert)
* [#6709](https://github.com/IntelRealSense/librealsense/pull/6709) - **[Linux] Update patch-arch.sh**   - Fix for on Manjaro Linux. Extending the patch for Depth Metadata. contributed by [@puzzlepaint](https://github.com/puzzlepaint)
* [#6727](https://github.com/IntelRealSense/librealsense/pull/6727) - **[L515] NUM_OF_DEPTH_RESOLUTIONS reverted."**  (return NUM_OF_DEPTH_RESOLUTIONS to 2 for backward compatibility. 
* [#6722](https://github.com/IntelRealSense/librealsense/pull/6722) - **[MacOS] Fix imGui Font, habdle LLVM warnings**:
  - Adding new imGui font results in artifact similar to [#4558](https://github.com/IntelRealSense/librealsense/issues/4558);  
  - remove unused variables; 
  - re-order ctor init lists to rectify potential out-of order value initialization
* [#6723](https://github.com/IntelRealSense/librealsense/pull/6723) - **[L515] Resolutions alignment and Algo recording**
  - Update the num of resolutions.
  - Separate the dir of  algo recording from files names
* [#6700](https://github.com/IntelRealSense/librealsense/pull/6700) - **[L515] Fix Confidence Stream handling**  (Enabling confidence stream causes missing depth frames)
* [#6699](https://github.com/IntelRealSense/librealsense/pull/6699), [#6693](https://github.com/IntelRealSense/librealsense/pull/6693) - **[Android] WaitForFrames timeout adjustment for L515**  (switched to 5sec default)
* [#6622](https://github.com/IntelRealSense/librealsense/pull/6622) - **MSVC screening** 
  - static casts; unused variables; excessive #define usage
* [#6668](https://github.com/IntelRealSense/librealsense/pull/6668) - **[L515] Provision for additional streaming profiles** - USB2 mode.  (RS5-7992)
* [#6644](https://github.com/IntelRealSense/librealsense/pull/6644) - **[Unity] Fix the value of RS2_OPTION_FILTER_MAGNITUDE**  contributed by [@mengyui](https://github.com/mengyui)
* [#6654](https://github.com/IntelRealSense/librealsense/pull/6654) - **Linux Updates and Fixes**
  - Fix streaming CNF4 with kernel 4.19+
  - Add patches for kernel 5.4 LTS (Bionic)
  - Retrofit FG/INZC/PAIR/Z16H FourCC into patches to eliminate irrelevant warnings.
  - Patches script - fix Ubuntu tag selection to filter 4-digit kernel versions.
  - Fix patches for the deprecated Bionic/4.18 branch.  
(RS5-8037)
* [#6672](https://github.com/IntelRealSense/librealsense/pull/6672) - **[Android] fix stability issue on strea.stop**  (fix invoke_and_wait regression (caused by https://github.com/IntelRealSense/librealsense/pull/6203).  
(RS5-7999), (DSO-15084)
* [#6220](https://github.com/IntelRealSense/librealsense/pull/6220) - **[Python] Move bindings into separate cmake target**  (* Move python bindings into a separate cmake target so we can install them separately. Also install the python bindings into python's sitearch, not in `/usr/lib64` (or `/usr/local/lib64`)  
Resolves [#6124](https://github.com/IntelRealSense/librealsense/issues/6124). contributed by [@morxa](https://github.com/morxa)
* [#6617](https://github.com/IntelRealSense/librealsense/pull/6617) - **[L500] Block return to Default Preset + cancel max_range on stream start**  (**System requirement** - Trying to return to visual preset "Default" should generate an error on the viewer + exception on the API. (RS5-7898)
* [#6639](https://github.com/IntelRealSense/librealsense/pull/6639) - **[Viewer] Fix UI crash when disabling measurement**  (On the viewer/depth quality tool if a user pressed on "Measure" button while a measurement is active, the application crashed.
* [#6629](https://github.com/IntelRealSense/librealsense/pull/6629) - **[SDK Core] Hexify helper method name in test changed to char2hex**  (Same name of helper function in global namespace lead to failure in linkage.)
* [#6625](https://github.com/IntelRealSense/librealsense/pull/6625) - **[L515] UI Adjustments**  Cosmetic changes for l515 (RS5-7661)
* [#6621](https://github.com/IntelRealSense/librealsense/pull/6621) - **[Viewer]: Fix Depth ROI button for D4xx**  (DSO-15009)
* [#6593](https://github.com/IntelRealSense/librealsense/pull/6593) - **Add capability to override the official SW update server url** - Debugging Capability
* [#6581](https://github.com/IntelRealSense/librealsense/pull/6581) - **[CUDA] Fix broken compilation**  (Fix gcc-pedantic remarks). Resolves [#6573](https://github.com/IntelRealSense/librealsense/issues/6573) (DSO-15134)
* [#6405](https://github.com/IntelRealSense/librealsense/pull/6405) - **[Core SDK] Move to Catch2 version 2.12.1**  (Only affects unit-testing)  
  - Catch2 changes the way we do approximate-equals comparisons, and we now define our own approx() (rather than the Catch Approx()) for customizability.
  - Also fixed some warnings.
* [#6595](https://github.com/IntelRealSense/librealsense/pull/6595) - **[CI] Travis update nodeJs version to  10.15.3**  (regression on travis-cs nodeJs build)
  - Mocha latest verison no longer support NodeJs version 6 https://github.com/mochajs/mocha/releases [#4164](https://github.com/IntelRealSense/librealsense/issues/4164):
  - Mocha v8.0.0 now requires Node.js v10.0.0 or newer. Mocha no longer supports the Node.js v8.x line ("Carbon"), which entered End-of-Life at the end of 2019)

## Documentation
* [#6747](https://github.com/IntelRealSense/librealsense/pull/6747) - **monitor the dmesg change**   contributed by [@wwppyy](https://github.com/wwppyy)
* [#6678](https://github.com/IntelRealSense/librealsense/pull/6678) - **Fixed "depth" typo**  (L110: depht -> depth) contributed by [@AndreiCostinescu](https://github.com/AndreiCostinescu)
* [#6592](https://github.com/IntelRealSense/librealsense/pull/6592) - **Fixed typo in examples/cmake/readme.md**  (applicatoin -> application) contributed by [@AndreiCostinescu](https://github.com/AndreiCostinescu)

### Known Issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block.
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized variable. (DSO-13700)
* [#4261](https://github.com/IntelRealSense/librealsense/issues/4261) - [T265] Add ability to open multiple devices from different processes.
* [#4518](https://github.com/IntelRealSense/librealsense/issues/4518) – [T265] Pose data produces `NaNs`. Can still occur in some cases. If detected, please attempt to make a raw data (images + IMU) recording using the [recorder tool](https://github.com/IntelRealSense/librealsense/tree/master/tools/recorder), and attach a link to it in the github issue, to assist our resolution.
* [#6009](https://github.com/IntelRealSense/librealsense/issues/6009) v2.33.1 does not compile with -DBUILDEASYLOGGINGPP=OFF
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* (DSO-13525) - [D400] 3D viewer moved when sliding the tare calibration sliders
* (RS5-7374) - [L515] Error after performing HW reset
* (DSO-15118) - [D400] Viewer is closed forcibly with cycling start/stop streaming in 3D view.
* (DSO-15250) - [Viewer] with OpenVINO stops the RGB stream when IMU is activated
* (RS5-8167) - [Viewer] Firmware Update process may generate unnecessary warning/popup message that must be clicked to proceeds
* (RS5-8011) - [Android] L515 looses connection under stress streaming.

## Release 2.35.2
Release Date: 10 Jun 2020

### API Changes
N/A

### New Features
* [#6487](https://github.com/IntelRealSense/librealsense/pull/6487) - **Introduce L515 Depth Camera** official support. 
* [#6480](https://github.com/IntelRealSense/librealsense/pull/6480) - **L515**: Firmware version 1.4.1.0.  
   - Fix Horizontal shift of depth map. 
   - Recommended firmware with this release.  
* [#6423](https://github.com/IntelRealSense/librealsense/pull/6423) - **Viewer** 3D view enhancements:
   - 3D interactive measurements
   - Shading with Diffuse Light mode
   - Updated UI
* [#6457](https://github.com/IntelRealSense/librealsense/pull/6457), [#6548](https://github.com/IntelRealSense/librealsense/pull/6548) - **Viewer**: Add check-for-updates capability to streamline SW updates. The feature would provide notification in case of software changes/new version releases (RS5-7674).


### Bug Fixes and Improvements
* [#6488](https://github.com/IntelRealSense/librealsense/pull/6488) - **Linux**: Remove libusb warning on device removal.
* [#6484](https://github.com/IntelRealSense/librealsense/pull/6484) - **Core**: fix memory leak in occlusion-filter. (RS5-7793).
* [#6467](https://github.com/IntelRealSense/librealsense/pull/6467) - **Core**: Fix Multi-camera behavior with RSUSB backend. Follow up on [5615](https://github.com/IntelRealSense/librealsense/pull/5615).  Should address [#5614](https://github.com/IntelRealSense/librealsense/issues/5614), [#5935](https://github.com/IntelRealSense/librealsense/issues/5935), [#6084](https://github.com/IntelRealSense/librealsense/issues/6084).
* [#6487](https://github.com/IntelRealSense/librealsense/pull/6487) - **L515** Fixes and enhancements:
   - PID update + enable metadata support.
   - Remove unnecessary froms from `validator` (RS5-7628)
   - Fix memory leak in `validator` (RS5-7715)   
   - Fix playback crash (RS5-7726)  
* [#6492](https://github.com/IntelRealSense/librealsense/pull/6492) - **L515**: Viewer to include recommended FW for the said SKU (RS5-7866).
* [#6492](https://github.com/IntelRealSense/librealsense/pull/6492) - **L515**: Adjust projector power default mode  (RS5-7780).
* [#6500](https://github.com/IntelRealSense/librealsense/pull/6500) - **L515**: IMU Calibration script to support said SKU. (RS5-7793).
* [#6503](https://github.com/IntelRealSense/librealsense/pull/6503) - **Linux**: Adjust fix for build with GCC 5.3.
* [#6539](https://github.com/IntelRealSense/librealsense/pull/6539) - **Libcurl**: Fix for non-default build configurations.

### Known Issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block.
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized variable. (DSO-13700)
* [#4261](https://github.com/IntelRealSense/librealsense/issues/4261) - [T265] Add ability to open multiple devices from different processes.
* [#4518](https://github.com/IntelRealSense/librealsense/issues/4518) – [T265] Pose data produces `NaNs`. Can still occur in some cases. If detected, please attempt to make a raw data (images + IMU) recording using the [recorder tool](https://github.com/IntelRealSense/librealsense/tree/master/tools/recorder), and attach a link to it in the github issue, to assist our resolution.
* [#6009](https://github.com/IntelRealSense/librealsense/issues/6009) v2.33.1 does not compile with -DBUILDEASYLOGGINGPP=OFF
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* (DSO-13525) - [D400] 3D viewer moved when sliding the tare calibration sliders
* (RS5-7374) - [L515] Error after performing HW reset
* (RS5-6586) - [L515] Corrupted Depth and IR  
* (DSO-15118) - [D400] Viewer is closed forcibly with cycling start/stop streaming in 3D view.

## Release 2.35.0
Release Date: 31 May 2020

### API Changes
https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2350

### New Features
* [#6480](https://github.com/IntelRealSense/librealsense/pull/6480) - **D4xx**: Firmware version 5.12.5.0.
* [#6176](https://github.com/IntelRealSense/librealsense/pull/6176) - **T265**: Firmware version 0.2.0.951.
   - Make initial yaw consistent when starting forward, backward, up and down.
   - Fix intermittent descriptor/imu corruption
* [#6205](https://github.com/IntelRealSense/librealsense/pull/6205) - **Extended cam sync mode to support full slave and genlock modes**

### Bug Fixes and Improvements
* [#6470](https://github.com/IntelRealSense/librealsense/pull/6470) - **Android**: Detach event was generated instead of Attach event. Addresses [#6452](https://github.com/IntelRealSense/librealsense/issues/6452), (DSO-15065,DSO-14493).
* [#6318](https://github.com/IntelRealSense/librealsense/pull/6318) - **MATLAB**: Add missing mapping for depth_frame::get_units()  contributed by [@mengyui](https://github.com/mengyui).
* [#6321](https://github.com/IntelRealSense/librealsense/pull/6321) - **Linux**: Fix aarch64/arm detection. Depending on the compiler, `-dumpmachine` can output any `aarch64-*` or `arm-*`, such as `aarch64-suse-linux` or `arm-suse-linux-gnueabi`, not only `aarch64-linux-gnu` or `arm-linux-gnueabihf`.) contributed by [@ggardet](https://github.com/ggardet).
* [#6434](https://github.com/IntelRealSense/librealsense/pull/6434) - **DQT**: UI fixes and enhancements (DSO-14283).
   - Fix UI units.
   - Log Plane Fit RMS % error metric) 
* [#6429](https://github.com/IntelRealSense/librealsense/pull/6429) - **Support FW Flash scheme 106**  (Required for FW v5.12.4.+).
* [#6308](https://github.com/IntelRealSense/librealsense/pull/6308) - **rs-convert** rework 
   - Switch to sensor callback API (except for ply export)
   - Assign file names based on frame timestamp (instead of frame number)
   - Generate metadata text file per frame
* [#6427](https://github.com/IntelRealSense/librealsense/pull/6427) - **Fix dead-lock on multi-threaded log invocations**  Addresses [#6231](https://github.com/IntelRealSense/librealsense/issues/6231), [#6393](https://github.com/IntelRealSense/librealsense/issues/6393) (DSO-15016).
* [#6420](https://github.com/IntelRealSense/librealsense/pull/6420) - **LRS Network Extensions**: Live555 3rd party compilation fix.
* [#6417](https://github.com/IntelRealSense/librealsense/pull/6417) - **Fix Memory Leak in frame-validator PB** (RS5-7715)
* [#6375](https://github.com/IntelRealSense/librealsense/pull/6375) - **Fix Syncer Memory Leak**  (Fix for [#6337](https://github.com/IntelRealSense/librealsense/issues/6337) contributed by [@MojamojaK](https://github.com/MojamojaK)
* [#6350](https://github.com/IntelRealSense/librealsense/pull/6350) - **Thermal Compensation** option provisioning (DSO-14980).
* [#6355](https://github.com/IntelRealSense/librealsense/pull/6355) - **Manual gain setting to override AE** (changing gain manually with Auto-Exposure on should disable AE instead of error), addresses [#5952](https://github.com/IntelRealSense/librealsense/issues/5952) 
(DSO- 14638).
* [#6328](https://github.com/IntelRealSense/librealsense/pull/6328), [#6188](https://github.com/IntelRealSense/librealsense/pull/6188) - **LRS Network Extensions**: Hot Fixes.
* [#6315](https://github.com/IntelRealSense/librealsense/pull/6315) - **Disable creation of unnecessary log files** Configure filename for logging ONLY if it was requested, in which case `minimum_file_severity` is different than `RS2_LOG_SEVERITY_NONE`.) contributed by [@AndrejOrsula](https://github.com/AndrejOrsula).
* [#6299](https://github.com/IntelRealSense/librealsense/pull/6299) - **GlSL occlusion**  (implement occlusion removal on gpu with glsl.
* [#6305](https://github.com/IntelRealSense/librealsense/pull/6305) - **Add notification for L500 corrupted frames**.
* [#6143](https://github.com/IntelRealSense/librealsense/pull/6143) - **Android**: Fix YUYV preview for the RGB stream in camera app.
* [#6288](https://github.com/IntelRealSense/librealsense/pull/6288) - **CMake** Set depth invalidation enabled by default.
* [#6290](https://github.com/IntelRealSense/librealsense/pull/6290) - **Win metadata script** fix
(DSO-14805, DSO-14653).
* [#6203](https://github.com/IntelRealSense/librealsense/pull/6203) - **Fix RSUSB messaging takes 20 msec**  (fix signalling in concurrency.h invoke_and_wait function. Was waiting until timeout and missing the signal, Addresses [#6206](https://github.com/IntelRealSense/librealsense/issues/6206).
* [#6198](https://github.com/IntelRealSense/librealsense/pull/6198) - **Python: add log_to_callback** add rs2::log_message, rs2::log_to_callback() and rs2::log() to python wrapper.


### Documentation
* [#6450](https://github.com/IntelRealSense/librealsense/pull/6450) - **Update rs_frame.hpp**  (Clarify get_timestamp()'s behavior.) contributed by [@krazycoder2k](https://github.com/krazycoder2k).
* [#6263](https://github.com/IntelRealSense/librealsense/pull/6263) - **Fixes Typos**  () contributed by [@himanshugarg](https://github.com/himanshugarg).
* [#6200](https://github.com/IntelRealSense/librealsense/pull/6200) - **Fixed minor comment typo**. Resolves  [#5872](https://github.com/IntelRealSense/librealsense/issues/5872) contributed by [@krazycoder2k](https://github.com/krazycoder2k).


### Known Issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block.
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized variable. (DSO-13700)
* [#4261](https://github.com/IntelRealSense/librealsense/issues/4261) - [T265] Add ability to open multiple devices from different processes.
* [#4518](https://github.com/IntelRealSense/librealsense/issues/4518) – [T265] Pose data produces `NaNs`. Can still occur in some cases. If detected, please attempt to make a raw data (images + IMU) recording using the [recorder tool](https://github.com/IntelRealSense/librealsense/tree/master/tools/recorder), and attach a link to it in the github issue, to assist our resolution.
* [#6009](https://github.com/IntelRealSense/librealsense/issues/6009) v2.33.1 does not compile with -DBUILDEASYLOGGINGPP=OFF
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* (DSO-13524) - Viewer crash when running Update Unsigned FW with signed FW image (unlocked units only)
* (DSO-13525) - 3D viewer moved when sliding the tare calibration sliders

## Release 2.34.0
Release Date: 31 Mar 2020

### API Changes
https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2340

### New Features and Enhancements
* [#6136](https://github.com/IntelRealSense/librealsense/pull/6136) - **RealSense Device over Ethernet**  (from [#5999](https://github.com/IntelRealSense/librealsense/pull/5999)) (CAMOE-11) *Alpha*
  - `rs-server` tool is a stand-alone application for streaming Depth and RGB sensors. Currently the server provides partial functionality for D415 and D435 cameras, on Linux only. 
  - A new extension module `realsense2-net` was introduced to encapsulate depth camera streaming and management over network. Configured with `BUILD_NETWORK_DEVICE` Cmake flag, currently limited to Linux and Windows OS. 
* [#6026](https://github.com/IntelRealSense/librealsense/pull/6026) - **OpenVINO Face Detection** toolkit is integrated into `realsense-viewer` (DSO-13910) - Requires Windows SDK Installer, not included in the standalone viewer. 
* [#6097](https://github.com/IntelRealSense/librealsense/pull/6097) - **On-Chip Calibration (OCC) fine-tuning** (DSO-14650):  
  - OCC health check error for values <0  is now based on error magnitude.
  - OCC More Options, White Wall is set as default for D415.
  - Tare-cal, The Avg step default values when you hover on the name are now compatible with the number shown (20).
  - Tare is set to last value the user typed.
  - Error messages is shifted slightly to the left and the box is bigger.
  - Progress bar during Tare is updated.
* [#6118](https://github.com/IntelRealSense/librealsense/pull/6118) - [Software Device] **Add API to break circular dependency with Active Object**  
  - The new `rs2_software_sensor_detach` function should be called on all instances of `rs2::software_sensor` stored inside the Active Object.
* [#6000](https://github.com/IntelRealSense/librealsense/pull/6000) - [L500] **Add HW Sync Enable control** (RS5-6978)
* [#6109](https://github.com/IntelRealSense/librealsense/pull/6109) - [rs-fw-logger] **Parse parametric floating point input** (RS5-7008)
* [#6098](https://github.com/IntelRealSense/librealsense/pull/6098) - [CMake] **Make firmware URL overrideable**  (as proposed in [5114](https://github.com/IntelRealSense/librealsense/issues/5114) by [@mikepurvis](https://github.com/mikepurvis))  
  - Permits the firmware binaries to be mirrored on-site for more restrictive CI environments that do not permit external network access at build time.  
* [#5996](https://github.com/IntelRealSense/librealsense/pull/5996) - [API] **Add depth_frame::get_units()**  convenience API  
* [#5997](https://github.com/IntelRealSense/librealsense/pull/5997) - [SR305] **Fix device designation** (DSO-14170) 
* [#6006](https://github.com/IntelRealSense/librealsense/pull/6006) - [D400] **Added emmitter always on option** (DSO-14265)  
* [#5879](https://github.com/IntelRealSense/librealsense/pull/5879) - [Core] **Establish 0x0B5B, 0xB5C SKUs** (DSO-14577)  
* [#5961](https://github.com/IntelRealSense/librealsense/pull/5961) - [API] Add **rs2_allocate_synthetic_motion_frame** (DSO-14645)  

 ### Bug Fixes and Improvements
* [#6161](https://github.com/IntelRealSense/librealsense/pull/6161) - [Video4Linux] **Fix broken activation of custom deleter in RAII**.  Ensure custom deleter is called by passing non-null pointer in ctor. Contributed by [@anmelleSlamcore ](https://github.com/anmelleSlamcore).
* [#6149](https://github.com/IntelRealSense/librealsense/pull/6149) - [Software Device] **invoke pixels deleter**.  Make sure frame deleter is called even if no streaming is active.
* [#6151](https://github.com/IntelRealSense/librealsense/pull/6151) - [Python] **revert save_to_ply options to properties** (using properties with getter functions fixes static compilation)
* [#6137](https://github.com/IntelRealSense/librealsense/pull/6137) - [EasyLogging] **Fix logger activation** (Fix typo)
* [#6133](https://github.com/IntelRealSense/librealsense/pull/6133) - [MSVC] **Visual Studio 2019 compilation fix**
* [#6040](https://github.com/IntelRealSense/librealsense/pull/6040) - [RaspberryPi4] **Update installation script for Raspbian Buster**
* [#6043](https://github.com/IntelRealSense/librealsense/pull/6043) - [Global Time] **Handle get_device_time delay**  (occasionally the may takes up to ~250 ms resulting in frame drops).
* [#6037](https://github.com/IntelRealSense/librealsense/pull/6037) - [Metadata] **Fix time domain handling** (DSO-13182, DSO-14314) 
  - (Track and update metadata presence continuously to handle cases where v4l driver produces zeros
* [#6083](https://github.com/IntelRealSense/librealsense/pull/6083) - **Fix 100% CPU usage when using libusb**  
  - Replace broken dispatcher with a thread to fix 100% CPU usage. Addresses [#5783](https://github.com/IntelRealSense/librealsense/issues/5783), [#6062](https://github.com/IntelRealSense/librealsense/issues/6062).
* [#6029](https://github.com/IntelRealSense/librealsense/pull/6029) - [D435i] **Fix imu motion correction**  
  - Motion Correction is calculated and to be applied in the same CS as the depth sensor
* [#6022](https://github.com/IntelRealSense/librealsense/pull/6022) - **Odroid kernel patch scripts update** contributed by [@chlakshminarayana](https://github.com/chlakshminarayana)
* [#5991](https://github.com/IntelRealSense/librealsense/pull/5991) - [Android]**Fix for USB disconnect-recovery flow** (DSO-14132)
* [#6064](https://github.com/IntelRealSense/librealsense/pull/6064) - []API **Fix Emitter_On option** (DSO-14265)
* [#6044](https://github.com/IntelRealSense/librealsense/pull/6044) - [Matlab] **Fix broken include in wrapper**
* [#6021](https://github.com/IntelRealSense/librealsense/pull/6021) - [Python] **Add missing cpp file into CMakeLists**
* [#5922](https://github.com/IntelRealSense/librealsense/pull/5922) - [C++] **Dynamic cast compatibility** contributed by [@militaryCoder](https://github.com/militaryCoder)
* [#5970](https://github.com/IntelRealSense/librealsense/pull/5970) - [Matlab] **Matlab bindings fixes**  (Address [#5906](https://github.com/IntelRealSense/librealsense/issues/5906), [#4453](https://github.com/IntelRealSense/librealsense/issues/4453), [#5236](https://github.com/IntelRealSense/librealsense/issues/5236) (DSO-14313)
* [#5977](https://github.com/IntelRealSense/librealsense/pull/5977) - [Metadata] **Minor fixes and enhancements**  
  - Fix the behaviour of "Enable Metadata" button on Windows
  - Add horizontal and vertical FOV to `rs-enumerate-devices`
* [#5988](https://github.com/IntelRealSense/librealsense/pull/5988) - [T265] **Fix default USB permissions for T265 (Android)**  contributed by [@smartynenko](https://github.com/smartynenko)
* [#5622](https://github.com/IntelRealSense/librealsense/pull/5622) - [C#] **C# Wrapper Reference Counting for Sensor/Device**.  Fixes [#5369](https://github.com/IntelRealSense/librealsense/issues/5369), contributed by [@JBBee](https://github.com/JBBee)
* [#5333](https://github.com/IntelRealSense/librealsense/pull/5333) - [Software Device] **Enhancement**  Fix python wrapper FW update callback (DSO-14348)
* [#5579](https://github.com/IntelRealSense/librealsense/pull/5579) - [T265] **Updated T parameters in rotated camera odometry example.** contributed by [@krazycoder2k](https://github.com/krazycoder2k)
* [#5483](https://github.com/IntelRealSense/librealsense/pull/5483) - [ROS] **remove rosdep 'linux-headers-generic'**  contributed by [@christian-rauch](https://github.com/christian-rauch)
* [#5665](https://github.com/IntelRealSense/librealsense/pull/5665) - [C++] **notification registration route to base sensor**.  Fixes [#5479](https://github.com/IntelRealSense/librealsense/issues/5479)  
* [#5853](https://github.com/IntelRealSense/librealsense/pull/5853) - [C#] **Add the missing  distortion fisheye model**  contributed by [@mengyui](https://github.com/mengyui)  
* [#5910](https://github.com/IntelRealSense/librealsense/pull/5910) - [Android] **Handle Resources deallocation**  
* [#5921](https://github.com/IntelRealSense/librealsense/pull/5921) - [Core] **Downgrade UVC warning messages**  
* [#5929](https://github.com/IntelRealSense/librealsense/pull/5929) - [Core] **Typo in `rs-ar-advanced.cpp`**  contributed by [@pjessesco](https://github.com/pjessesco)


### Documentation
* [#6158](https://github.com/IntelRealSense/librealsense/pull/6158) - **Depth cameras support matrix**  migrated to https://dev.intelrealsense.com/docs/sdk-knowledge-base

### Known Issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block.
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized variable. (DSO-13700)
* [#4261](https://github.com/IntelRealSense/librealsense/issues/4261) - [T265] Add ability to open multiple devices from different processes.
* [#4518](https://github.com/IntelRealSense/librealsense/issues/4518) – [T265] Pose data produces `NaNs`. Can still occur in some cases. If detected, please attempt to make a raw data (images + IMU) recording using the [recorder tool](https://github.com/IntelRealSense/librealsense/tree/master/tools/recorder), and attach a link to it in the github issue, to assist our resolution.
* [#6009](https://github.com/IntelRealSense/librealsense/issues/6009) v2.33.1 does not compile with -DBUILDEASYLOGGINGPP=OFF
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* (DSO-13524) - Viewer crash when running Update Unsigned FW with signed FW image (unlocked units only)
* (DSO-13525) - 3D viewer moved when sliding the tare calibration sliders


## Release 2.33.1
Release Date: 1 Mar 2020

### Enhancements

* [#5946](https://github.com/IntelRealSense/librealsense/pull/5946) - **FW 5.12.3.0 for D4xx Devices**  Stability and Power Management fixes and enhancements

* [#5741](https://github.com/IntelRealSense/librealsense/pull/5741) - **Visual presets and new options for L500** (RS5-6455):  
        **Options:**  
        - confidence,  
        - post_processing_sharpness ,  
        - pre_processing_sharpness ,  
        - noise_filtering ,
        - avalanche_photo_diode,  
        - laser_gain,  
        - min_distance,
        - invalidation_bypass,  
        - ambient_light  
        **Presets:**  
        - RS2_L500_VISUAL_PRESET_NO_AMBIENT,  
        - RS2_L500_VISUAL_PRESET_LOW_AMBIENT,  
        - RS2_L500_VISUAL_PRESET_MAX_RANGE,  
        - RS2_L500_VISUAL_PRESET_SHORT_RANGE  
        **Serialization support:**  
        - save and load options/presets with JSON.

* [#5584](https://github.com/IntelRealSense/librealsense/pull/5584) - **log_to_callback + unit-tests**:  
Fixes OpenVino test-cases in static (non-shared) builds.  
Fixes some of the warnings repeatedly given (from headers) during compilation.  
Travis "Linux - cpp" build is now "Linux - cpp - static" and uses static (non-shared; BUILD_SHARED_LIBS is off) compilation.  
Added unit-testing mechanism:
  - Any test-\*.cpp (and, in future, test-*.py, etc.) under unit-tests/ is a unit-test
  - Tests can be nested inside directories
  - Each test creates its own project (in Visual Studio under Unit-Tests, with same nested structure)   and thus its own executable
  - A script (unit-tests/run-unit-tests.py) runs all unit-tests and exits with status 1 or 0
  - Unit-tests can be specifically shared or static
- Unit-tests are now run in Travis, in both shared and static builds
- BUILD_SHARED_LIBS is now a compiler definition, too
- Added rs2::log_to_callback, rs2_log_to_callback, rs2_log_to_callback_cpp
  - Minimum level can be set for a callback (e.g., warnings and above)
  - Added RS2_LOG_SEVERITY_ALL, same as _DEBUG, so it's easier to say "log all messages"
  - 21 unit-tests under unit-tests/log/

* [#5751](https://github.com/IntelRealSense/librealsense/pull/5751) - **Linux Kernel 5 and 5.3 fixes and enhancements** (DSO-14299):  
 - Fixes:
    - UVC and metadata node matching shall use non-lexicographic sort in v4l. Applicable for kernels 4.16+.  
   - Enforce Video/Metadata sync by using two-stage blocking calls. This replaces I/O multiplexing to ensure the pairing of video and meta nodes payloads.  
   - Check libusb return value to prevent null de-referencing and segfault.  
   - Enforce Metadata  polling is performed when invalid video buffer is encountered to ensure Video/Metadata node sync.
   - Fix hardware fps calculation when the frame number being reset internally by the FW, such as in `SCP Overflow` case.  
 - Enhancements:
   - Make unified kernel patches for LTS v5.0 and v5.3.
   - Re-enable unattended patch by overriding `patch --merge` inconsistency with try/apply. Github [#5710](https://github.com/IntelRealSense/librealsense/issues/5710)
   - Adjust metadata validation heuristics in SKU-neutral manner to allow for D400, SR300 and L500 md payloads.
   - Check  Metadata/Video nodes buffers synchronization by comparing the kernel sequence buffers
- Addresses [#5319](https://github.com/IntelRealSense/librealsense/issues/5319), [#4543](https://github.com/IntelRealSense/librealsense/issues/4543). Relevant for the following ROS issues [1024](https://github.com/IntelRealSense/realsense-ros/issues/1024), [1047](https://github.com/IntelRealSense/realsense-ros/issues/1047), [1060](https://github.com/IntelRealSense/realsense-ros/issues/1060), [492](https://github.com/IntelRealSense/realsense-ros/issues/492).

* [LabVIEW Package](https://realsense-hw-public.s3-eu-west-1.amazonaws.com/Releases/RS4xx/Windows/labview_2_33_0.zip) ** update with SDK v2.33.0**  

* [#5796](https://github.com/IntelRealSense/librealsense/pull/5796) - **Syncer to bypass IMU frames for L500** (RS5-5558):  
  - Added composite_identity_matcher that passes frames to callback without synchronize them.

* [#5792](https://github.com/IntelRealSense/librealsense/pull/5792) - **Android streamer start time increase fix** (DS5U-4538, DSO-14302):
   -   (Autoclosable try added for getting device into Updatable - bug was only in the Android wrapper.

* [#5789](https://github.com/IntelRealSense/librealsense/pull/5789) - **Android multicam fix** (DSO-14133):
  - Start/stop multicam application fixed

* [#5780](https://github.com/IntelRealSense/librealsense/pull/5780) - **RS2_OPTION_FREEFALL_DETECTION_ENABLED** (RS5-6461):
  - When an L500 camera experiences free-fall (is dropped) a safety mechanism is triggered and turns off the depth sensor to protect moving parts inside the camera.

* [#5698](https://github.com/IntelRealSense/librealsense/pull/5698) - **#4297 Multicamera IMU data mix up** (DSO-13711):  
  - HID devices now use the unique-ID assigned to their parent node (which is the USB node), letting them be properly associated with the proper composite device.  
  - Multiple HID cameras should be identified correctly.
  - Addresses [#4297](https://github.com/IntelRealSense/librealsense/issues/4297)

* [#5801](https://github.com/IntelRealSense/librealsense/pull/5801) - **rs-fw-logger - regression fix** (DSO-14405) 

* [#5716](https://github.com/IntelRealSense/librealsense/pull/5716) - **rs-enumerate-devices fixes and enhancements** (DSO-14190):  
 Revert `-s` option to invoke a one-line description per device.  
 Add `-S` to provide the device node data (used to be `-s`)  
 Addresses [#5423](https://github.com/IntelRealSense/librealsense/issues/5423)  
 
* [#5768](https://github.com/IntelRealSense/librealsense/pull/5768) - **On-Chip and Tare Calibration Fixes**:  
Minor fixes and quality of life improvements for OCC and Tare features
  
* [#5791](https://github.com/IntelRealSense/librealsense/pull/5791) - **Disable all L500 depth processing blocks except for the Temporal Gilter**

* [#5785](https://github.com/IntelRealSense/librealsense/pull/5785) - **Fix typo**  (apriltag_pose_destory -> apriltag_pose_destroy) contributed by [@pjessesco](https://github.com/pjessesco)
* [#5645](https://github.com/IntelRealSense/librealsense/pull/5645) - **T265 Serial Number Compatability**:  
  Keep the serial number of the T265 compatible with the `libtm` format (`8XXXXXXXXXXX` rather than `00008XXXXXXXXXXX`).  By [@radfordi](https://github.com/radfordi)
* [#5889](https://github.com/IntelRealSense/librealsense/pull/5889) - **Fix memory leak** in v4l2 backend with kernel 4.16+. 
* [#5923](https://github.com/IntelRealSense/librealsense/pull/5923) - **License for  Windows INF file** update.  Addresses [#5809](https://github.com/IntelRealSense/librealsense/issues/5809)


**Community Contribution**:  
* [#5750](https://github.com/IntelRealSense/librealsense/pull/5750) - **Ill-defined for loop fix**. by [@JTrantow](https://github.com/JTrantow)
* [#5769](https://github.com/IntelRealSense/librealsense/pull/5769) - **Update camera renderer**:  
 Make SR305 and L500 appear correctly in the 3D view.

 * [#5623](https://github.com/IntelRealSense/librealsense/pull/5623) - **One Viewer Context**:  
Avoid creating multiple unused `rs2::context`s.
* [#5747](https://github.com/IntelRealSense/librealsense/pull/5747) - **Remove the std::move() on const. See Issue5746.**:  
  Resolves [#5746](https://github.com/IntelRealSense/librealsense/issues/5746)) contributed by [@JTrantow](https://github.com/JTrantow)
* [#5702](https://github.com/IntelRealSense/librealsense/pull/5702) - **Fix for conversion from RGB to BGR in the OpenCV wrapper helpers file**:  
  Related to [#5701](https://github.com/IntelRealSense/librealsense/issues/5701).  
  Without converting to a seperate `cv::Mat` object, the conversion would not happen on my machine. This seems to be the case for other users, see [this example.](https://answers.opencv.org/question/199603/problem-with-intel-realsense-sdk-20-and-cvcvtcolor/)). Contributed by [@cedriclmenard](https://github.com/cedriclmenard)
* [#5724](https://github.com/IntelRealSense/librealsense/pull/5724) - **Fix typos**.  Contributed by [@ruffsl](https://github.com/ruffsl)
* [#5734](https://github.com/IntelRealSense/librealsense/pull/5734) - **Fix what appears to be a copy/paste redundant code problem**:  
Addresses [#5733](https://github.com/IntelRealSense/librealsense/issues/5733) - Change second conditional to test values_ir.size(). By [@JTrantow](https://github.com/JTrantow)


## Release 2.32.1
Release Date: 23 Jan 2020

### Enhancements

[#5678](https://github.com/IntelRealSense/librealsense/pull/5678) - **Enable metadata with a single click on Windows**
UVC frame metadata on Windows is causing a lot of questions and confusion, adding a single click solution that would be applicable from within the Viewer.  contributed by [@dorodnic](https://github.com/dorodnic)

[#5700](https://github.com/IntelRealSense/librealsense/pull/5700) - **Upgrade D4XX firmware to 5.12.2.100** 
- Maintenance update: performance and stability improvements

[#5687](https://github.com/IntelRealSense/librealsense/pull/5687) - **Upgrade T265 firmware to 0.2.0.926** 
- Fix map load hangs
- Fix USB serial number (remove trailing zeros)
- Support for `remove_static_node`
- Fix map export hangs based on map sizes ([#5394](https://github.com/IntelRealSense/librealsense/issues/5394))
- Fix immediate NaNs based due to a specific initial condition
- Fix export without import or start
- Allow export of imported but not started maps

[#5213](https://github.com/IntelRealSense/librealsense/pull/5213) - **Replace libtm with direct communication with T265**  This PR removes libtm completely and makes T265 a first class driver in librealsense. In more detail, it:
- Removes libtm and remnants of older products and directly communicates with T265
- Uses the librealsense usb abstraction for booting T265 and communicating with it
- Unifies firmware download into common/fw
- Fixes many issues on macOS
- Fixes multi-process issues caused by libtm greedily claiming the device
- Adds hardware reset support
- Allows export of map while running
- Logs an error when the user tries to stream video data over USB 2 connections
- Fixes the pipeline resolution bug mentioned in [#4506](https://github.com/IntelRealSense/librealsense/issues/4506)

Contributed by [@bfulkers-i](https://github.com/bfulkers-i)

[#5356](https://github.com/IntelRealSense/librealsense/pull/5356) - **T2XX sample: new `ar-advanced` sample to show map import/export and get/set static node APIs**  
* Import localization map from raw data file before starting T2xx tracking.
* Export localization map to raw data file after T2xx tracking ended.
* Command line options to specify raw data file paths for the above map import/export.
* Set up a callback to receive relocalization event from the device and action after the event.
* Get static node from imported localization map after relocalization event detected.
* Set static node before localization map exported.

Contributed by [@honpong](https://github.com/honpong)

[#5468](https://github.com/IntelRealSense/librealsense/pull/5468) - **Add sensor extensions** 
Add color_sensor, motion_sensor and fisheye_sensor extensions.
Enable the option to `dev.first<rs2::color_sensor>()` or `sensor.is<rs2::color_sensor>()` etc. Contributed by [@doronhi](https://github.com/doronhi)

[#5529](https://github.com/IntelRealSense/librealsense/pull/5529) - **.NET VideoStreamProfile.Clone added**  
Cloning a profile is sometimes needed when writing custom processing blocks. Contributed by [@ogoshen](https://github.com/ogoshen)

[#5419](https://github.com/IntelRealSense/librealsense/pull/5419) - **Merge Android development branch** 
Adds the following features:
- Terminal
- FW Logger
- FW Backup
- Extend stream stats
- Basic Controls tab - Emitter mode only for now

Fixes issues:
- Pipeline memory leak
- Recordings functionality
- Permissions issues

### Bug Fixes

[#5691](https://github.com/IntelRealSense/librealsense/pull/5691) - **Fix possible bug in device-hid detection**
Comparison to capacity(), which can be greater than size(), can lead to wrong logic contributed by [@maloel](https://github.com/maloel)

[#5639](https://github.com/IntelRealSense/librealsense/pull/5639) - **T265 time sync: filter outliers**
Global timestamp will show deviation due to global timestamp base adjustment every 500ms, contributed by [@cchen6](https://github.com/cchen6)

[#5597](https://github.com/IntelRealSense/librealsense/pull/5597) - **VAAPI HEVC Main10 hardware depth encoding example** contributed by [@bmegli](https://github.com/bmegli)

[#5586](https://github.com/IntelRealSense/librealsense/pull/5586) - **Add support for POWER9 CPUs on Linux**
Tested on a Raptor Computing Blackbird motherboard with an IBM POWER9 CPU, running Void Linux PPC. Contributed by [@AlbertoGP](https://github.com/AlbertoGP)

[#5626](https://github.com/IntelRealSense/librealsense/pull/5626) - **Adding links to community projects**
* Linking to [Raspberry Pi Handheld 3D Scanner](https://eleccelerator.com/pi-handheld-3d-scanner/) blog-post by @frank26080115
* Linking to  [Erwhi Hedgehog](https://gbr1.github.io/erwhi_hedgehog.html) by @gbr1

[#5589](https://github.com/IntelRealSense/librealsense/pull/5589) - **L500 duplicated IR frame fix**  
IR frame was processed from both IR-id and from zero order processing blocks.
Now IR frame is not passed-through by default when zero order is disabled
Tracked-on: RS5-6159

[#5677](https://github.com/IntelRealSense/librealsense/pull/5677) - **Prevent UVC frame overflow with v4l**  Add heuristic to drop UVC overflow frames with v4l kernel prior to v4.16
Handle GCC pedantic
Tracked on: DSO-14370

[#5627](https://github.com/IntelRealSense/librealsense/pull/5627) - **Remove Accelerometer 50Hz profile**  (Tracked-on: RS5-5473)

[#5636](https://github.com/IntelRealSense/librealsense/pull/5636) - **Fix for memory overrun when parsing metadata** 
Trim frame size when the metadata is present but invalid
Add heuristics to validate metadata existence. (assume d4xx format)
Tracked on : DSO-14292

[#5637](https://github.com/IntelRealSense/librealsense/pull/5637) - **Include stddef in usbhost.h** 
`usbhost.h` uses `size_t`, which is defined in `stddef.h`. This change includes `stddef.h` header. Contributed by [@jonberling](https://github.com/jonberling)

[#5534](https://github.com/IntelRealSense/librealsense/pull/5534) - **Fix motion and motion correction routines**  
- Fix reported HID frame size required for unpacker
- Motion correction and handling:
- Query motion correction option at run-time
- Assign the intrinsic data to the processing block and switch to cached data
- Fix data flow management
- Move exception handling to initialization
- Assign default MM axis rotation for unsupported SKUs
Addresses  [#5515](https://github.com/IntelRealSense/librealsense/issues/5515), possibly [#5496](https://github.com/IntelRealSense/librealsense/issues/5496), [#5498](https://github.com/IntelRealSense/librealsense/issues/5498)) contributed by [@ev-mp](https://github.com/ev-mp)

[#5574](https://github.com/IntelRealSense/librealsense/pull/5574) - **Added action_delayer and remove gamma correction control.**  
1. Added action_delayer to enable delayed action
2. Remove gamma correction control from RGB sensor

[#5264](https://github.com/IntelRealSense/librealsense/pull/5264) - **Update installation_osx.md**  (Make it work on Catalina) contributed by [@neilyoung](https://github.com/neilyoung)

[#5155](https://github.com/IntelRealSense/librealsense/pull/5155) - **update pyglet to 1.4.x** 
The pyglet class pyglet.clock.ClockDisplay has removed in 1.4.x. Use pyglet.window.FPSDisplay instead. Addresses [#255](https://github.com/los-cocos/cocos/issues/255). Apply the changes from [#3887](https://github.com/IntelRealSense/librealsense/issues/3887)
Contributed by [@mengyui](https://github.com/mengyui)

[#5387](https://github.com/IntelRealSense/librealsense/pull/5387) - **polling_device_watcher: fix race between construction and start**
Device watchers do not notify for devices that already exist when they are constructed (e.g. `win_event_device_watcher`), but polling_device_watcher was also not notifying if devices were added between construction and start. This aligns polling_device_watcher with `win_event_device_watcher`.
Contributed by [@bfulkers-i](https://github.com/bfulkers-i)

[#5444](https://github.com/IntelRealSense/librealsense/pull/5444) - **CMake: unix_config, set CMAKE_POSITION_INDEPENDENT_CODE**  
And remove manually set `fPIC`. `CMAKE_POSITION_INDEPENDENT_CODE` properly sets `fPIC` for libraries and `fPIE` for executables.
Contributed by [@jirislaby](https://github.com/jirislaby)

[#5443](https://github.com/IntelRealSense/librealsense/pull/5443) - **common: fw-update-helper, don't compare addresses**  
`create_default_fw_table` tries to compare `""` with another string. But both are addresses. Use `strlen` in that case. Issue: [#5525](https://github.com/IntelRealSense/librealsense/issues/5525), Contributed by [@tylerjw](https://github.com/tylerjw)

[#5521](https://github.com/IntelRealSense/librealsense/pull/5521) - **Fix extrinsics commutativity**  
Manually create a new stream profile for every clone needed by the sensor while initializing the stream profiles, instead of cloning via the raw profile. Cloning the profiles caused extrinsic registration with incomplete profiles.
Addresses [#5451](https://github.com/IntelRealSense/librealsense/issues/5451)

[#5522](https://github.com/IntelRealSense/librealsense/pull/5522) - **Fix recording crash** 
Recording unsubscribe signal was not called properly, causing sensor hooks to be called even after the record device object was destroyed.

[#5421](https://github.com/IntelRealSense/librealsense/pull/5421) - **Typo removed**  
Contributed by [@neilyoung](https://github.com/neilyoung)


## Release 2.31.0
Release Date: 9 Dec 2019

### API Changes
TBD

### Bug-fixes and Enhancements

* [#5402](https://github.com/IntelRealSense/librealsense/pull/5402) - *Promote recommended D400 firmware to 5.12.1.0* contributed by [@ev-mp](https://github.com/ev-mp)
* [#5360](https://github.com/IntelRealSense/librealsense/pull/5360) - *Add rs-dnn-vino sample* (rs-dnn-vino
This example demonstrates OpenVINO™ toolkit integration with object detection, using
basic depth information to approximate distance) contributed by [@maloel](https://github.com/maloel)
* [#5377](https://github.com/IntelRealSense/librealsense/pull/5377) - *Change default RGB resolution* (Default change to HD 30) contributed by [@arilowen](https://github.com/arilowen)
* [#5386](https://github.com/IntelRealSense/librealsense/pull/5386) - *Warning fixes* (Fixes a few minor warnings encountered on macOS) contributed by [@bfulkers-i](https://github.com/bfulkers-i)
* [#5399](https://github.com/IntelRealSense/librealsense/pull/5399) - *Fix xhci_build flags handling* (Limit the xhci_patch to LTS v4.4 only, improve robustness. The bug fix resolved in v4.18 is being actively back-ported by Ubuntu, e.g 4.4.0-170. Assume that this may occur for other kernel branches as well) contributed by [@ev-mp](https://github.com/ev-mp)
* [#5380](https://github.com/IntelRealSense/librealsense/pull/5380) - *fix openni2 wrapper convertDepthToColorCoordinates* (Оnly intrinsics are used in the function `Rs2Stream::convertDepthToColorCoordinates`, which leads to a small error due to the fact that the depth and color cameras are offset relative to each other) contributed by [@DarkCon](https://github.com/DarkCon)
* [#5325](https://github.com/IntelRealSense/librealsense/pull/5325) - *Update t265_rpy.py* (Reverting regression regarding negative signs introduced with last commit to this PR. The negation of z and y is required in order to obtain similarity to the previous sample code (used in the ArduPilot project)) contributed by [@neilyoung](https://github.com/neilyoung)
* [#5208](https://github.com/IntelRealSense/librealsense/pull/5208) - *Option to preserve T265 maps in memory for subsequent starts* contributed by [@radfordi](https://github.com/radfordi)
* [#5370](https://github.com/IntelRealSense/librealsense/pull/5370) - *Update tinyfiledialog to v3.4.1 and set map extension* (The version of tinyfiledialog we have doesn't allow by extension file selection on macOS (see [#5245](https://github.com/IntelRealSense/librealsense/issues/5245) and [#5196](https://github.com/IntelRealSense/librealsense/issues/5196)). Even the newest version doesn't seem to allow `*.*` selection, so this PR introduces the `.map` extension of T265 localization maps) contributed by [@bfulkers-i](https://github.com/bfulkers-i)
* [#5283](https://github.com/IntelRealSense/librealsense/pull/5283) - *Openvino sample* (This example demonstrates OpenVINO™ toolkit integration with facial detection, using
basic depth information to approximate distance) contributed by [@maloel](https://github.com/maloel)
* [#5346](https://github.com/IntelRealSense/librealsense/pull/5346) - *Added API for depth auto calibration.* (Added auto_calibrated_device extension for on-chip (plane fit RMS) and tare calibration (absolute distance). Use this API in viewer and added example on python for auto calibration. Fixed viewer to put the default resolution on top and fixed default resolution in D435) contributed by [@aangerma](https://github.com/aangerma)
* [#5344](https://github.com/IntelRealSense/librealsense/pull/5344) - *Android add pipeline profile partial support* (Only get device function is supported for now) contributed by [@arilowen](https://github.com/arilowen)
* [#5352](https://github.com/IntelRealSense/librealsense/pull/5352) - *Version compatibility enhancement*. Relax version compatibility constrain from strict identity to the standardized convention: 
  1. The versions are compatible when the majors align and the version minors hold (lib_ver >= exe_ver) 
  2. Patch number version does not affect compatibility. 
* [#5353](https://github.com/IntelRealSense/librealsense/pull/5353) - *Synchronize D400 devices using timestamp by default, and not frame counter* contributed by [@arilowen](https://github.com/arilowen)
* [#5234](https://github.com/IntelRealSense/librealsense/pull/5234) - *Upgrade T265 firmware to 0.2.0.879*  contributed by [@radfordi](https://github.com/radfordi)
  1. Double map size. 
  2. Fix loading maps with a previously loaded map that hadn't relocalized [#4593](https://github.com/IntelRealSense/librealsense/issues/4593) 
  3. Relocalization significantly improved for areas up to 50 sq. m: testing indicates fast (< 5 seconds) relocalization for typical environments and usage with non-static T265 device.
  4. Addressing [#4593](https://github.com/IntelRealSense/librealsense/issues/4593) – [T265] Map corruption after repeated export and import
* [#4275](https://github.com/IntelRealSense/librealsense/pull/4275) - *Fixed inconsistent return type* (Fixes the following error (GNU Make 4.2.1): `error: inconsistent types 'bool' and 'int' deduced for lambda return type`) contributed by [@battlecry231](https://github.com/battlecry231)
* [#5331](https://github.com/IntelRealSense/librealsense/pull/5331) - *Fix global timestamp domain query crash* (DSO-13980 - Add missing global timestamp case to timestamp domain enum.) contributed by [@arilowen](https://github.com/arilowen)
* [#5334](https://github.com/IntelRealSense/librealsense/pull/5334) - *Fix a minor bug in sorting streams* (Assumes the ordering should be by format first, then index, then
stream number.) contributed by [@bfulkers-i](https://github.com/bfulkers-i)
* [#5336](https://github.com/IntelRealSense/librealsense/pull/5336) - *Fix save_to_ply symbols in python* (The core issue is that `static const` members are not instantiated anywhere python can access, so taking a reference to them doesn't work. Fix that by creating lambdas that return the value) contributed by [@bfulkers-i](https://github.com/bfulkers-i)
* [#5337](https://github.com/IntelRealSense/librealsense/pull/5337) - *Add __version__ to python wrapper*  contributed by [@bfulkers-i](https://github.com/bfulkers-i)
* [#5312](https://github.com/IntelRealSense/librealsense/pull/5312) - *Android multicam example* (- GLSurface cleans its view before each frame arrives. a. Extend wrapper API. b. Add support for multi cameras. c. Add multicamera example) contributed by [@arilowen](https://github.com/arilowen)
* [#5170](https://github.com/IntelRealSense/librealsense/pull/5170) - *.NET Development* (fix for [#5054](https://github.com/IntelRealSense/librealsense/issues/5054), [#5071](https://github.com/IntelRealSense/librealsense/issues/5071). Revert some c# sensor API changes back to non-generic) contributed by [@ogoshen](https://github.com/ogoshen)
* [#5200](https://github.com/IntelRealSense/librealsense/pull/5200) - *Add new functions to Python wrapper, update some more documentation* (Addresses [#5173](https://github.com/IntelRealSense/librealsense/issues/5173)) contributed by [@lramati](https://github.com/lramati)
* [#4720](https://github.com/IntelRealSense/librealsense/pull/4720) - *Automatically set supported profiles in viewer* (Added functionality to the realsense viewer: when the user picks a value (resolution / fps / format / stream) that isn't compatible with the current configuration, a new configuration that supports the chosen value is set) contributed by [@AnnaRomanov](https://github.com/AnnaRomanov)
* [#4396](https://github.com/IntelRealSense/librealsense/pull/4396) - *Smooth GlobalTimeStamp's corrections.* (prevents cases where, due to jumps in sampling, global timestamps difference is much greater then the original hw timestamp difference) contributed by [@doronhi](https://github.com/doronhi)
* [#5198](https://github.com/IntelRealSense/librealsense/pull/5198) - *Alternative, possibly simpler approach for obtaining pitch, roll and yaw from T265 pose in python* contributed by [@neilyoung](https://github.com/neilyoung)
* [#5244](https://github.com/IntelRealSense/librealsense/pull/5244) - *Wheeled Odometry Calibration Setup Examples* (Added basic wheeled odometry description / sample drawings/json to T265 documentation. ) contributed by [@krazycoder2k](https://github.com/krazycoder2k)
* [#5278](https://github.com/IntelRealSense/librealsense/pull/5278) - *Fix SR300 enumeration on linux* (DSO-13973, 
[#5230](https://github.com/IntelRealSense/librealsense/issues/5230), [#5233](https://github.com/IntelRealSense/librealsense/issues/5233), [#5219](https://github.com/IntelRealSense/librealsense/issues/5219)) contributed by [@matkatz](https://github.com/matkatz)
* [#5053](https://github.com/IntelRealSense/librealsense/pull/5053) - *Adding Support-Matrix and updating Jetson docs*. This PR introduces two new documentation enhancements: 
  1. Rewrite of [installation_jetson.md](https://github.com/dorodnic/librealsense/blob/jetson_doc/doc/installation_jetson.md) 
  2. Addition of [support-matrix.md](https://github.com/dorodnic/librealsense/blob/jetson_doc/doc/support-matrix.md) trying to capture the big picture of what features are available on which platforms)
* [#5111](https://github.com/IntelRealSense/librealsense/pull/5111) - *Sensor refactoring* (DSO-13626) 
  contributed by [@arilowen](https://github.com/arilowen)
  - Add Synthetic Sensor class.
  - Replace unpackers with processing blocks.
  - Add composite processing blocks
* [#5110](https://github.com/IntelRealSense/librealsense/pull/5110) - *Enhanced PLY exporter* contributed by [@AnnaRomanov](https://github.com/AnnaRomanov)
* [#5205](https://github.com/IntelRealSense/librealsense/pull/5205) - *Kernel patch adjustment for 4.4 branch* (The patch provides a new option to incorporate additional 4.13-upstreamed patches for usbcore and xhci-hcd modules on top of the basic patches suite. The additional patch is a retrofit of [patchwork.kernel.org/patch/11095737](https://patchwork.kernel.org/patch/11095737/). The patches allow to avoid sporadic errors reproduced during stress start/stop tests that can affect certain SKUs. Tracked on: RS-5440) contributed by [@ev-mp](https://github.com/ev-mp)
* [#5204](https://github.com/IntelRealSense/librealsense/pull/5204) - *Zero the high 32 bit of 64 bit imu timestamp.* contributed by [@aangerma](https://github.com/aangerma)
* [#5202](https://github.com/IntelRealSense/librealsense/pull/5202) - *Disable GLOBAL_TIME option* (Disable global timestamp by default for specific SKU) contributed by [@aangerma](https://github.com/aangerma)
* [#5179](https://github.com/IntelRealSense/librealsense/pull/5179) - *Include USB Host license* contributed by [@matkatz](https://github.com/matkatz)
* [#5190](https://github.com/IntelRealSense/librealsense/pull/5190) - *Fix link in t265.md* contributed by [@levingerdes](https://github.com/levingerdes)
* [#5306](https://github.com/IntelRealSense/librealsense/pull/5306) - *UE4 wrapper 4.24 update and Linux build* contributed by [@gaborpapp](https://github.com/gaborpapp)

## Release 2.30.0
Release Date: 4 Nov 2019

### API Changes
[link](https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2300)

### New Features & Improvements
* [#4975](https://github.com/IntelRealSense/librealsense/pull/4975) - New Cross-platform user-space implementation for supported USB protocols named `rsusb`. The refactored classes replace the multiplicity of UVC device per-platform implementations (Windows, Unix, Android) with a single cross-platform code infrastructure.
  In order to support the UVC device requirements, some modification were introduced into `rsusb` API, mainly adding asynchronous API to the USB messenger.
  Core Features:
  - Asynchronous API added to `rsusb` (USB request).
  - Multiple implementations of libuvc (Win7 / Linux / Android) replaced with single implementation
  - HID device modified to work with the new asynchronous API.
  Impact:
  - A new CMake option named `FORCE_RSUSB_BACKEND` added replaces the `FORCE_LIBUVC` and `FORCE_WINUSB_UVC` flags are now marked as deprecated. 
  - An update to WinUSB driver was implemented. **Important** - The new driver must be installed in order to use librealsense 2.30.0 or newer SDK version on Windows 7. Installation of the new driver is performed via [librealsense Windows 7 installer](https://github.com/IntelRealSense/librealsense/releases/download/v2.30.0/Intel.RealSense.SDK-WIN7-2.30.0.1174.exe).

* [#5169](https://github.com/IntelRealSense/librealsense/pull/5169) - [T265] Firmware Upgrade to 0.2.0.857:
   - Numerical stability improvements in various NaN pose scenarios, can have positive effect on issues [#4518](https://github.com/IntelRealSense/librealsense/issues/4518), [#5101](https://github.com/IntelRealSense/librealsense/issues/5101), [realsense-ros #955](https://github.com/IntelRealSense/realsense-ros/issues/955)
   - Minor relocalization improvements (including better cross-device map compatibility), toward upcoming greater relocalization update.
* [#4936 ](https://github.com/IntelRealSense/librealsense/pull/4936 ) -	[T265] New notification category.(RS2_NOTIFICATION_CATEGORY_POSE_RELOCALIZATION), produced on first relocalization to an imported map
* [#5109](https://github.com/IntelRealSense/librealsense/pull/5109) - [T265] display USB port chain in physical port info. by @BriceRenaudeau
* [#4966](https://github.com/IntelRealSense/librealsense/pull/4966) - New face detection and depth-enabled anti-spoofing demo exhibiting machine learning algorithms with [DLIB](http://dlib.net/) toolkit (DSO-13630).
* [#4889](https://github.com/IntelRealSense/librealsense/pull/4889) - [T265] Adding Wheel Odometry python sample.
* [#4953](https://github.com/IntelRealSense/librealsense/pull/4953) - A link to community project that builds Android 
application with librealsense. @cabelo 
* [#4907](https://github.com/IntelRealSense/librealsense/pull/4907) - [Realsense-Viewer] User notification improvement when the rendering format is not supported.

### Bug Fixes
* [#5157](https://github.com/IntelRealSense/librealsense/pull/5157) - Prevent hex formatting contamination.
* [#5106](https://github.com/IntelRealSense/librealsense/pull/5106) - Acquire depth units from intrinsic. (RS5-5486)
* [#5077](https://github.com/IntelRealSense/librealsense/pull/5077) - On-chip calibration crash fix
* [#5066](https://github.com/IntelRealSense/librealsense/pull/5066) - [rosbag-inspector] Crash fix. (DSO-13665, DSO-13562). Addresses [#4704](https://github.com/IntelRealSense/librealsense/issues/4704), [#4932](https://github.com/IntelRealSense/librealsense/issues/4932)
* [#5065](https://github.com/IntelRealSense/librealsense/pull/5065) - Memory leak in hid sensor (DSO-13080, DSO-13712, DSO-13639). Fixes [#4332](https://github.com/IntelRealSense/librealsense/issues/4332)
* [#5025](https://github.com/IntelRealSense/librealsense/pull/5025) - Raspbian Buster build fix. Fix IMU streams handling. Addresses [#4986](https://github.com/IntelRealSense/librealsense/issues/4986), [#4979](https://github.com/IntelRealSense/librealsense/issues/4979), [#4950](https://github.com/IntelRealSense/librealsense/issues/4950), [#4818](https://github.com/IntelRealSense/librealsense/issues/4818).
* [#5028](https://github.com/IntelRealSense/librealsense/pull/5028) - [Realsense-Viewer] Configuration file default path with white spaces was not handled properly (DSO-13701). Fixes [#3779](https://github.com/IntelRealSense/librealsense/issues/3779)
* [#4987](https://github.com/IntelRealSense/librealsense/pull/4987) - Rename `foreach` to `foreach_rs` to avoid namespace collisions with QT's "foreach" macro. [#4461](https://github.com/IntelRealSense/librealsense/issues/4461). Proposed by @cgpadwick's
* [#4981](https://github.com/IntelRealSense/librealsense/pull/4981) - Fix min Z offset for disparity domain colorization mode by @TetsuriSonoda
* [#4967](https://github.com/IntelRealSense/librealsense/pull/4967) - [Depth Quality Tool/Viewer] Metrics record fixes and 
improvements. Fixes [#4913](https://github.com/IntelRealSense/librealsense/issues/4913), [#4948](https://github.com/IntelRealSense/librealsense/issues/4948)
* [#4945](https://github.com/IntelRealSense/librealsense/pull/4945) - [rs-ar-basic] Fix extrinsic pose to camera transformation
* [#4914](https://github.com/IntelRealSense/librealsense/pull/4914) - Fix White Balance control for Rolling shutter sensor.
* [#4910](https://github.com/IntelRealSense/librealsense/pull/4910) - Robustness improvement: `get_distance` to verify user-provided pixel indexes. [#4877](https://github.com/IntelRealSense/librealsense/issues/4877)

### Known Issues
* [#2809](https://github.com/IntelRealSense/librealsense/issues/2809) - Advanced C# examples bug
* [#2356](https://github.com/IntelRealSense/librealsense/issues/2356) - [Python] missing python example of alignment with post-processing. (DSO-10681)
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block.
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized variable.
* [#4297](https://github.com/IntelRealSense/librealsense/issues/4297) - [Windows] Multi-camera IMU Mix up. (DSO-13711)
* [#4261](https://github.com/IntelRealSense/librealsense/issues/4261) - [T265] Add ability to open multiple devices from different processes.
* [#4505](https://github.com/IntelRealSense/librealsense/issues/4505). - Global Timestamps wrong after long use. (DSO-13418)
* [#4518](https://github.com/IntelRealSense/librealsense/issues/4518) – [T265] Pose data produces `NaNs`. Can still occur in some cases. If detected, please attempt to make a raw data (images + IMU) recording using the [recorder tool](https://github.com/IntelRealSense/librealsense/tree/master/tools/recorder), and attach a link to it in the github issue, to assist our resolution.
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* (DSO-9065)  - Frame Drops when changing depth controls while depth streaming.
* (DSO-12940) - IMU jitters and drops events
* (DSO-12942) - Global Timestamp: first 15 seconds of frames timestamps are unstable.
* (DSO-13524) - Viewer crash when running Update Unsigned FW with signed FW image (unlocked units only)
* (DSO-13525) - 3D viewer moved when sliding the tare calibration sliders
* (DSO-13539) - [Android] Camera disconnected after streaming some duration with Android Camera Sample
* (DSO-13541) - On-Chip Calibration stuck at 0% when in USB2 mode
* (DSO-13072) - Firmware Update on Linux - backup procedure may takes up to two minutes 
* (DSO-13078) - Firmware Update with `rs-fw-update` tool. The firmware update process may fail when additional librealsense application runs in background. Make sure to close any librealsense-based application during the Firmware Update routine

## Release 2.29.0
Release Date: 26 Sep 2019

### API Changes
[link](https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2290)

### Bug Fixes
* [#4886](https://github.com/IntelRealSense/librealsense/pull/4886) - [Pcl] Fix typo in documentation by @fburakk. 
* [#4909](https://github.com/IntelRealSense/librealsense/pull/4909) - Fixed bug on record-playback. (RS5-3373)
* [#4925](https://github.com/IntelRealSense/librealsense/pull/4925) - [Realsense-Viewer] Fix Tare-Calibration UI. (DSO-13537) 
* [#4927](https://github.com/IntelRealSense/librealsense/pull/4927) - Update doxygen of pose_sensor.


### Known Issues
* Firmware Update with `rs-fw-update` tool. The firmware update process may fail when additional librealsense application runs in background. Make sure to close any librealsense-based application during the Firmware Update routine (DSO-13078)
* Firmware Update on Linux - backup procedure may takes up to two minutes (DSO-13072)
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block
* Frame Drops when changing depth controls while depth streaming. (DSO-9065)
* [#2809](https://github.com/IntelRealSense/librealsense/issues/2809) - Advanced C# examples bug
* (Python) [#2356](https://github.com/IntelRealSense/librealsense/issues/2356) / DSO-10681 - missing python example of alignment with post-processing
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized value(s)
* Global Timestamp: first 15 seconds of frames timestamps are unstable (DSO-12942)
* IMU jitter and drops events [LRS] regression (DSO-12940)
* (DSO-13541) - On-Chip Calibration stuck at 0% when in USB2 mode
* (DSO-13539) - [Android] Camera disconnected after streaming some duration with Android Camera Sample
* (DSO-13525) - 3D viewer moved when sliding the tare calibration sliders
* (DSO-13524) - Viewer crash when running Update Unsigned FW with signed FW image (unlocked units only)
* (DSO-13418) - Global Timestamps wrong after long use [#4505](https://github.com/IntelRealSense/librealsense/issues/4505).

## Release 2.28.1
Release Date: 22 Sep 2019

### API Changes
No API changes - a maintenance release. 

### New Features & Improvements
* [#4808](https://github.com/IntelRealSense/librealsense/pull/4786) - [Realsense-Viewer] Set Decimation off by default for 
RGB streams (DSO-13570)
* [#4826](https://github.com/IntelRealSense/librealsense/pull/4826) - [rs-fw-logger] support extended XML scheme for dedicated SKUs.(RS5-5315)
* [#4836](https://github.com/IntelRealSense/librealsense/pull/4836) - [Android] Add sensor ROI support to the wrapper. (DSO-13473)
* [#4863](https://github.com/IntelRealSense/librealsense/pull/4863) - Change Depth Invalidation default to false.
* [#4773](https://github.com/IntelRealSense/librealsense/pull/4773) - Allow `frame_queue` to automatically keep frames
instead of requesting the user code to call `frame.keep()` explicitly for each required frame.


### Bug Fixes
* [#4808](https://github.com/IntelRealSense/librealsense/pull/4808) - [Python] Fix A-factor access in the wrapper. Addresses [#4807](https://github.com/IntelRealSense/librealsense/issues/4807)
* [#4809](https://github.com/IntelRealSense/librealsense/pull/4809) - Fixes for `save_single_frameset`. Related to [#4801](https://github.com/IntelRealSense/librealsense/issues/4801), [#4020](https://github.com/IntelRealSense/librealsense/issues/4020), [#3704](https://github.com/IntelRealSense/librealsense/issues/3704), [#3671](https://github.com/IntelRealSense/librealsense/issues/3671), [#2588](https://github.com/IntelRealSense/librealsense/issues/2588).
* [#4830](https://github.com/IntelRealSense/librealsense/pull/4830) - Wheel odometry unit-test fix.
* [#4851](https://github.com/IntelRealSense/librealsense/pull/4851) - [Python] Fix typo in `align-depth2color.py` example by  @BenDavisson.
* [#4866](https://github.com/IntelRealSense/librealsense/pull/4866) - Fix RGB distortion coefficients assignment (RS5-5354).
* [#4867](https://github.com/IntelRealSense/librealsense/pull/4867) - On-Chip Calibration adjustments


### Known Issues
* Firmware Update - owners of SR300 device shall not use firmware versions v3.27.xx as it overrides camera's product id (PID) and may render it non-recognizable with the client's code, especially with the legacy versions.
A new compatibility enforcement policy that shall prevents accidental upgrades will be integrated in future releases.
* Firmware Update with `rs-fw-update` tool. The firmware update process may fail when additional librealsense application runs in background. Make sure to close any librealsense-based application during the Firmware Update routine (DSO-13078)
* Firmware Update on Linux - backup procedure may takes up to two minutes (DSO-13072)
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block
* Frame Drops when changing depth controls while depth streaming. (DSO-9065)
* [#2809](https://github.com/IntelRealSense/librealsense/issues/2809) - Advanced C# examples bug
* (Python) [#2356](https://github.com/IntelRealSense/librealsense/issues/2356) / DSO-10681 - missing python example of alignment with post-processing
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized value(s)
* Global Timestamp: first 15 seconds of frames timestamps are unstable (DSO-12942)
* IMU jitter and drops events [LRS] regression (DSO-12940)
* (DSO-13541) - On-Chip Calibration stuck at 0% when in USB2 mode
* (DSO-13540) - On-Chip Calibration with D415 and blank wall target may return "Calibration didn't converge! (EDGE_TO_CLOSE) please retry in different lighting conditions". (Note: w/a is using textured target)
* (DSO-13539) - [Android] Camera disconnected after streaming some duration with Android Camera Sample
* (DSO-13525) - 3D viewer moved when sliding the tare calibration sliders
* (DSO-13524) - Viewer crash when running Update Unsigned FW with signed FW image (unlocked units only)
* (DSO-13418) - Global Timestamps wrong after long use [#4505](https://github.com/IntelRealSense/librealsense/issues/4505).

## Release 2.28.0
Release Date: 5 Sep 2019

### API Changes
N/A

### New Features & Improvements
* [#4786](https://github.com/IntelRealSense/librealsense/pull/4786) - [Realsense-Viewer] Set Decimation off by default for 
RGB streams (DSO-13570)
* [#4749](https://github.com/IntelRealSense/librealsense/pull/4749) - [Python] Code refactoring, minor fixes

### Bug Fixes
* [#4788](https://github.com/IntelRealSense/librealsense/pull/4788) - Fix Frame Validation (RS5-5244)
* [#4787](https://github.com/IntelRealSense/librealsense/pull/4787) - Fix Jupyter Notebook. #4142, #4579
* [#4785](https://github.com/IntelRealSense/librealsense/pull/4785) - Fix a potential out-of-order initialization for A-factor (DSO-13560)
* [#4774](https://github.com/IntelRealSense/librealsense/pull/4774) - [Kernel Patches] Adjust Ubuntu LTS track branches for kernel sources
* [#4772](https://github.com/IntelRealSense/librealsense/pull/4772) - [Realsense-Viewer] Fixing basic threading issues within on-chip calibration UI.
* [#4759](https://github.com/IntelRealSense/librealsense/pull/4759) - MSVC2019 compilation fix by @UnaNancyOwen
* [#4758](https://github.com/IntelRealSense/librealsense/pull/4758) - Fix deadlock on pipeline.stop() with playback device in realtime=false mode
* [#4752](https://github.com/IntelRealSense/librealsense/pull/4752) - [Android] Streaming got stuck with camera APP (DSO-13453)


### Known Issues
* Firmware Update - owners of SR300 device shall not use firmware versions v3.27.xx as it overrides camera's product id (PID) and may render it non-recognizable with the client's code, especially with the legacy versions.
A new compatibility enforcement policy that shall prevents accidental upgrades will be integrated in future releases.
* Firmware Update with `rs-fw-update` tool. The firmware update process may fail when additional librealsense application runs in background. Make sure to close any librealsense-based application during the Firmware Update routine (DSO-13078)
* Firmware Update on Linux - backup procedure may takes up to two minutes (DSO-13072)
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block
* Frame Drops when changing depth controls while depth streaming. (DSO-9065)
* [#2809](https://github.com/IntelRealSense/librealsense/issues/2809) - Advanced C# examples bug
* (Python) [#2356](https://github.com/IntelRealSense/librealsense/issues/2356) / DSO-10681 - missing python example of alignment with post-processing
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized value(s)
* Global Timestamp: first 15 seconds of frames timestamps are unstable (DSO-12942)
* IMU jitter and drops events [LRS] regression (DSO-12940)
* (DSO-13541) - On-Chip Calibration stuck at 0% when in USB2 mode
* (DSO-13540) - On-Chip Calibration with D415 and blank wall target may return "Calibration didn't converge! (EDGE_TO_CLOSE) please retry in different lighting conditions". (Note: w/a is using textured target)
* (DSO-13539) - [Android] Camera disconnected after streaming some duration with Android Camera Sample
* (DSO-13525) - 3D viewer moved when sliding the tare calibration sliders
* (DSO-13524) - Viewer crash when running Update Unsigned FW with signed FW image (unlocked units only)

## Release 2.27.0
Release Date: 29 Aug 2019

### API Changes
[link](https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2270)

### New Features & Improvements

* **T265 Calibration APIs**:
  * [#4650](https://github.com/IntelRealSense/librealsense/pull/4650) - write calibration APIs
  * [#4717](https://github.com/IntelRealSense/librealsense/pull/4717) - updating default T265 firmware to 0.1.0.279
     * Expose the ability to modify the T265 calibration (w/ #4650)
     * T265 tracking performance improvement
     * Fixes #4700 T265 load/start/stop/load/start failure
  * [#4739](https://github.com/IntelRealSense/librealsense/pull/4739) and [#4685](https://github.com/IntelRealSense/librealsense/pull/4685) - documentation enhancements
* **New OpenCV example** - [#4733](https://github.com/IntelRealSense/librealsense/pull/4733) (Implementing ["Depth Map Improvements for Stereo-based
Depth Cameras on Drones"](http://www.qwrt.de/pdf/Depth_Map_Improvements_for_Stereo_based_Depth_Cameras_on_Drones.pdf))
* [#4721](https://github.com/IntelRealSense/librealsense/pull/4721) - Fix format-security warnings (contributed by [@morxa](https://github.com/morxa))
* [#4706](https://github.com/IntelRealSense/librealsense/pull/4706) - [Viewer] Enable step forward and backward during playback

### Bug Fixes
* **On-Chip Calibration** enhancements ([#4735](https://github.com/IntelRealSense/librealsense/pull/4735)) - 
  * Integration with firmware 5.11.14 (pre-release)
  * Unlocking the feature for D435i (DSO-13538)
  * Fixing Tare calibration units (DSO-13537)
* [#4731](https://github.com/IntelRealSense/librealsense/pull/4731) - Fixing Java example (addressing issues [#4701](https://github.com/IntelRealSense/librealsense/issues/4701) and [#4693](https://github.com/IntelRealSense/librealsense/issues/4693))
* [#4729](https://github.com/IntelRealSense/librealsense/pull/4729) - Fix Ubuntu 32bit compilation (addressing [#3448](https://github.com/IntelRealSense/librealsense/issues/3448) / DSO-12283)
* [#4698](https://github.com/IntelRealSense/librealsense/pull/4698) - [Android] GLFrame resource release (addressing DSO-13120)

### Known Issues
* Firmware Update - owners of SR300 device shall not use firmware versions v3.27.xx as it overrides camera's product id (PID) and may render it non-recognizable with the client's code, especially with the legacy versions.
A new compatibility enforcement policy that shall prevents accidental upgrades will be integrated in future releases.
* Firmware Update with `rs-fw-update` tool. The firmware update process may fail when additional librealsense application runs in background. Make sure to close any librealsense-based application during the Firmware Update routine (DSO-13078)
* Firmware Update on Linux - backup procedure may takes up to two minutes (DSO-13072)
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block
* Frame Drops when changing depth controls while depth streaming. (DSO-9065)
* [#2472](https://github.com/IntelRealSense/librealsense/issues/2472) - Application hangs when trying to close file replay pipeline (DSO-10749)
* [#2809](https://github.com/IntelRealSense/librealsense/issues/2809) - Advanced C# examples bug
* (Python) [#2356](https://github.com/IntelRealSense/librealsense/issues/2356) / DSO-10681 - missing python example of alignment with post-processing
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized value(s)
* Global Timestamp: first 15 seconds of frames timestamps are unstable (DSO-12942)
* IMU jitter and drops events [LRS] regression (DSO-12940)
* (DSO-13541) - On-Chip Calibration stuck at 0% when in USB2 mode
* (DSO-13540) - On-Chip Calibration with D415 and blank wall target may return "Calibration didn't converge! (EDGE_TO_CLOSE) please retry in different lighting conditions". (Note: w/a is using textured target)
* (DSO-13539) - [Android] Camera disconnected after streaming some duration with Android Camera Sample
* (DSO-13525) - 3D viewer moved when sliding the tare calibration sliders
* (DSO-13524) - Viewer crash when running Update Unsigned FW with signed FW image (unlocked units only)

## Release 2.26.0
Release Date: 21 Aug 2019

### API Changes
[link](https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2260)

### New Features & Improvements

* [#4658](https://github.com/IntelRealSense/librealsense/pull/4658) - D400 On-Chip Auto-calibration. Allows to recalibrate and therefore, improve depth quality using SDK-provided tools : `realsense-viewer` and `rs-depth-quality`. Note that this capability relies on FW version 5.13.x+ which is not yet publicly available at this stage. Follow the above link for more details.
* [#4605](https://github.com/IntelRealSense/librealsense/pull/4605) - Cross platform IMU HID device. This PR brings a cross platform HID device (IMU), implemented above rsusb interface.
435i IMU streaming is now enabled on Android platform. It is also available on non-X86 platforms, such as Arm with libuvc backend (e.g. Jetson TX2 amd Nano). (DSO-11944)
* [#4433](https://github.com/IntelRealSense/librealsense/pull/4433) - Add support for Firmware Update using non-signed images
* [#4492](https://github.com/IntelRealSense/librealsense/pull/4492) - Python: an Ethernet client-server example code for Realsense devices. Requires python 2.7.

### Enhancements

* [#4677](https://github.com/IntelRealSense/librealsense/pull/4677) - Pointcloud 3D-view in Android. Addresses [#4601](https://github.com/IntelRealSense/librealsense/issues/4601)
* [#4673](https://github.com/IntelRealSense/librealsense/pull/4673) - T265 Firmware Upgrade to version 0.1.0.242 to improve start and stop reliability, and overall stability, can have a positive effect on [#4592](https://github.com/IntelRealSense/librealsense/issues/4592), [#4518](https://github.com/IntelRealSense/librealsense/issues/4518).
* [#4666](https://github.com/IntelRealSense/librealsense/pull/4666) - Synchronize wrappers options with the core SDK. (matlab, node.js, python) [#4288](https://github.com/IntelRealSense/librealsense/issues/4288)
* [#4645](https://github.com/IntelRealSense/librealsense/pull/4645) - T265: auxillary function to print out motion intrinsic data
* [#4638](https://github.com/IntelRealSense/librealsense/pull/4645) - T265 Documentation: fix python examples link
* [#4622](https://github.com/IntelRealSense/librealsense/pull/4622) - Support for MJPEG stream for selected SKUs.
* [#4620](https://github.com/IntelRealSense/librealsense/pull/4620) - Community Project: realsense with cyber update, by @mickeyouyou
* [#4618](https://github.com/IntelRealSense/librealsense/pull/4618) - Windows7 : Drivers update to include support for the new and provisional model types, DFU mode. Digital certificate update as of Aug 2019.
* [#4602](https://github.com/IntelRealSense/librealsense/pull/4602)  - T265: Increase internal queue size to mitigate frame drops
* [#4583](https://github.com/IntelRealSense/librealsense/pull/4583)  - Android: SR300 firmware update improvements. (DSO-13360)
* [#4587](https://github.com/IntelRealSense/librealsense/pull/4587)  - D400: fix IMU data rate configuration. (DSO-13325)
* [#4582](https://github.com/IntelRealSense/librealsense/pull/4582)  - CMake enhancement, addresses [#3024](https://github.com/IntelRealSense/librealsense/issues/3024)
* [#4546](https://github.com/IntelRealSense/librealsense/pull/4546) - Python: Add syncer support for sensor start method. (DSO-13062)
* [#4545](https://github.com/IntelRealSense/librealsense/pull/4545) - `rs-enumerate-devices` robustness enhancement
* [#4537](https://github.com/IntelRealSense/librealsense/pull/4537) - Conform to ROS name convention. [#1161](https://github.com/IntelRealSense/librealsense/issues/1161)
* [#4248](https://github.com/IntelRealSense/librealsense/pull/4248) - Windows7 stability fixes and enhancements

### Bug Fixes
* [#4678](https://github.com/IntelRealSense/librealsense/pull/4678) - Fix Out-of-Memory error - Android UVC. Addresses [#3612](https://github.com/IntelRealSense/librealsense/issues/3612), [#4091](https://github.com/IntelRealSense/librealsense/issues/4091), [#4215](https://github.com/IntelRealSense/librealsense/issues/4215)
* [#4631](https://github.com/IntelRealSense/librealsense/pull/4631) - Fix default stream index for synthetic stream
* [#4616](https://github.com/IntelRealSense/librealsense/pull/4616) - Fix matlab method syntax in points.m by @dpiskas
dpiskas
* [#4596](https://github.com/IntelRealSense/librealsense/pull/4596) - Fix memory leak in `rs-multi-cam` example
* [#4562](https://github.com/IntelRealSense/librealsense/pull/4562) - DQT: fix plane and metric annotations when stream is paused. (RS5-3386)
* [#4561](https://github.com/IntelRealSense/librealsense/pull/4561) - CMake: fix offline build with no FW image available
* [#4560](https://github.com/IntelRealSense/librealsense/pull/4560) - Viewer - fix unresponsive UI on errors. (DSO-13146)
* [#4535](https://github.com/IntelRealSense/librealsense/pull/4535) - Remove "Depth Visualization" tab under unrelated sensors in viewer. (DSO-12361)
* [#4531](https://github.com/IntelRealSense/librealsense/pull/4531) - Use full GPG Key IDs for installing in linux by @miguelprada
* [#4507](https://github.com/IntelRealSense/librealsense/pull/4507) - T265: Use the origin of the Pose frame as the baseline for sensor extrinsic


### Known Issues
* Firmware Update - owners of SR300 device shall not use firmware versions v3.27.xx as it overrides camera's product id (PID) and may render it non-recognizable with the client's code, especially with the legacy versions.
A new compatibility enforcement policy that shall prevents accidental upgrades will be integrated in future releases.
* Firmware Update with `rs-fw-update` tool. The firmware update process may fail when additional librealsense application runs in background. Make sure to close any librealsense-based application during the Firmware Update routine (DSO-13078)
* Firmware Update on Linux - backup procedure may takes up to two minutes (DSO-13072)
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block
* [MacOS] File-Open / File-Save dialogs are not available in the Viewer / DQT, preventing import of custom presets #3781.(DSO-9162)
* Frame Drops when changing depth controls while depth streaming. (DSO-9065)
* [#2472](https://github.com/IntelRealSense/librealsense/issues/2472) - Application hangs when trying to close file replay pipeline (DSO-10749)
* [#2809](https://github.com/IntelRealSense/librealsense/issues/2809) - Advanced C# examples bug
* (Python) [#2356](https://github.com/IntelRealSense/librealsense/issues/2356) / DSO-10681 - missing python example of alignment with post-processing
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized value(s)
* Global Timestamp: first 15 seconds of frames timestamps are unstable (DSO-12942)
* IMU jitter and drops events [LRS] regression (DSO-12940)


## Release 2.25.0
Release Date: 1 Aug 2019

### API Changes
[link](https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2250)

### New Features & Improvements

* [#4340](https://github.com/IntelRealSense/librealsense/pull/4340) - T265 Demo with Apriltag detection on host
* [#4327](https://github.com/IntelRealSense/librealsense/pull/4327) - T265 Tracking + Depth Example


Enhancements
* [#4544](https://github.com/IntelRealSense/librealsense/pull/4544) - RGB Distortion correction for the supported sensors types (DSO-8307) linked to [IntelRealSense/realsense-ros#779](https://github.com/IntelRealSense/realsense-ros/issues/779)
* [#4321](https://github.com/IntelRealSense/librealsense/pull/4321) - T265 Mapping/Relocalization/Jumping Options
* [#4511](https://github.com/IntelRealSense/librealsense/pull/4511) - T265 advanced options to the python wrapper:
`enable_mapping`, `enable_pose_jumping`, and `enable_relocalization`. Follow up on [#4321](https://github.com/IntelRealSense/librealsense/pull/4321)
* [#4478](https://github.com/IntelRealSense/librealsense/pull/4478) - Support new calibration command `RECPARAMSGET` for D400 series
* [#4472](https://github.com/IntelRealSense/librealsense/pull/4472) - RGB Auto-Exposure ROI control for SR300
* [#4448](https://github.com/IntelRealSense/librealsense/pull/4448) - T265 expose Frame timestamp metadata attribute
* [#4403](https://github.com/IntelRealSense/librealsense/pull/4403) - T265 to support GLOBAL_TIMESTAMP_DOMAIN (TM2-4496)
* [#4402](https://github.com/IntelRealSense/librealsense/pull/4402) - Update for `realsense_device_manager.py` example by 
@NicholasWon47
* [#4389](https://github.com/IntelRealSense/librealsense/pull/4389) - IMU Support for D465.
* [#4380](https://github.com/IntelRealSense/librealsense/pull/4380) - Update SR300 recommended fW version to 2.26.1.0
* [#4367](https://github.com/IntelRealSense/librealsense/pull/4367) - Update 3rd-party `update stb_image_write.h` to v1.13 by @sailfish009
* [#4365](https://github.com/IntelRealSense/librealsense/pull/4365) - Remove the default 2Mb limit for log file generation (RS5-4780)
* [#4358](https://github.com/IntelRealSense/librealsense/pull/4358) - Silence OpenGL deprecation warnings on MacOS 10.14
* [#4314](https://github.com/IntelRealSense/librealsense/pull/4314) - Firmware Update robustness enhancement


### Bug Fixes
* [#4377](https://github.com/IntelRealSense/librealsense/pull/4377) - OpenCV Kinfu example dependency fix by @peterhinson
* [#4360](https://github.com/IntelRealSense/librealsense/pull/4360) - Fix for udev-rules validation routine (RS5-4780) addresses #4350
* [#4329](https://github.com/IntelRealSense/librealsense/pull/4329) - Easy-logging bug fix
* [#4313](https://github.com/IntelRealSense/librealsense/pull/4313) - Fix python readme by @kechako
* Firmware Update on Windows with Realsense-Viewer halts at  (30-50)% progress. Troubleshoot this by disconnecting and reconnecting the camera, then rerun Firmware update process (DSO-13070)

### Known Issues
* Firmware Update with `rs-fw-update` tool. The firmware update process may fail when additional librealsense application runs in background. Make sure to close any librealsense-based application during the Firmware Update routine (DSO-13078)
* Firmware Update on Linux - backup procedure may takes up to two minutes (DSO-13072)
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block
* [MacOS] File-Open / File-Save dialogs are not available in the Viewer / DQT, preventing import of custom presets #3781.(DSO-9162)
* Frame Drops when changing depth controls while depth streaming. (DSO-9065)
* [#2472](https://github.com/IntelRealSense/librealsense/issues/2472) - Application hangs when trying to close file replay pipeline (DSO-10749)
* [#2809](https://github.com/IntelRealSense/librealsense/issues/2809) - Advanced C# examples bug
* (Python) [#2356](https://github.com/IntelRealSense/librealsense/issues/2356) / DSO-10681 - missing python example of alignment with post-processing
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized value(s)
* Global Timestamp: first 15 seconds of frames timestamps are unstable (DSO-12942)
* IMU jitter and drops events [LRS] regression (DSO-12940)



## Release 2.24.0
Release Date: 27 Jun 2019

### API Changes
[link](https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2240)

### New Features & Improvements
Streamlining Firmware Update for D400 and SR300 Depth Cameras:
A new Cross-platform firmware update capability has been introduced to facilitate the camera maintenance.  
Platforms supported: Windows(*), Linux, Android, MacOS. *Before updating device please review errata below.  
The capability is integrated into `realsense-viewer` and a stand-alone `rs-fw-update` tools.
For more info see
* [#4267](https://github.com/IntelRealSense/librealsense/pull/4267) - Firmware Update Capability and `rs-fw-update` tool
* [#4280](https://github.com/IntelRealSense/librealsense/pull/4280) - Firmware Update integration with Viewer
* [#4304](https://github.com/IntelRealSense/librealsense/pull/4304) - Firmware Update in Android camera app

Enhancements
* [#4265](https://github.com/IntelRealSense/librealsense/pull/4265) - Adding Ubuntu Kernel 4.18 support. Non-LTS Kernel 5.0 is also supported with manual installation.
* [#4240](https://github.com/IntelRealSense/librealsense/pull/4240) - Contradictory Camera Axis Labels for T265 #4226
* [#4237](https://github.com/IntelRealSense/librealsense/pull/4237) - Android - dynamic USB read buffer size #3612, #4091, #4215
* [#4203](https://github.com/IntelRealSense/librealsense/pull/4203) - Nodejs support for windows build. Documentation update

### Bug Fixes
* [#4268](https://github.com/IntelRealSense/librealsense/pull/4268) - Viewer - fix variables name #4157
* [#4264](https://github.com/IntelRealSense/librealsense/pull/4264) - Fix Kernel 4.19+ error #2850 (DSO-11696)
* [#4251](https://github.com/IntelRealSense/librealsense/pull/51) -  `rs-terminal` improperly selects target in case of multiple devices connected
* [#4246](https://github.com/IntelRealSense/librealsense/pull/4246) - Linux file descriptors are not released when device is plugged off. Can be related to #3538 (DSO-12768)
* [#4219](https://github.com/IntelRealSense/librealsense/pull/4219) - Android l500 detection issue
* [#4208](https://github.com/IntelRealSense/librealsense/pull/4208) - Matlab and Python wrappers updates. Fixes  #4034, #4146.

### Known Issues
* Firmware Update on Windows with Realsense-Viewer halts at  (30-50)% progress. Troubleshoot this by disconnecting and reconnecting the camera, then rerun Firmware update process (DSO-13070)
* Firmware Update with `rs-fw-update` tool. The firmware update process may fail when additional librealsense application runs in background. Make sure to close any librealsense-based application during the Firmware Update routine (DSO-13078)
* Firmware Update on Linux - backup procedure may takes up to two minutes (DSO-13072)
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block
* [MacOS] File-Open / File-Save dialogs are not available in the Viewer / DQT, preventing import of custom presets #3781.(DSO-9162)
* Frame Drops when changing depth controls while depth streaming. (DSO-9065)
* [#2472](https://github.com/IntelRealSense/librealsense/issues/2472) - Application hangs when trying to close file replay pipeline (DSO-10749)
* [#2809](https://github.com/IntelRealSense/librealsense/issues/2809) - Advanced C# examples bug
* (Python) [#2356](https://github.com/IntelRealSense/librealsense/issues/2356) / DSO-10681 - missing python example of alignment with post-processing
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized value(s)
* Global Timestamp: first 15 seconds of frames timestamps are unstable (DSO-12942)
* IMU jitter and drops events [LRS] regression (DSO-12940)


## Release 2.23.0
Release Date: 10 Jun 2019

### API Changes
[link](https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2230)

### New Features & Improvements
* [#3998](https://github.com/IntelRealSense/librealsense/pull/3998) - Add comprehensive documentation to Python via sphinx - published via `intelrealsense.github.io`[link](https://intelrealsense.github.io/librealsense/python_docs/_generated/pyrealsense2.html#module-pyrealsense2)
* [#4100](https://github.com/IntelRealSense/librealsense/pull/4100),[#4162](https://github.com/IntelRealSense/librealsense/pull/4162) - Depth linearity enhancement - Mitigate the half-pixel disparity issue by adjusting the modulation amplitude. `rs2_set_amp_factor/rs2_get_amp_factor` Advanced-mode parameters functions. Requires FW version 5.11.9+ (DSO-12737)
* [#3992](https://github.com/IntelRealSense/librealsense/pull/3992) - T265: Expose the Fisheye sensor manual exposure control.
* [#4135](https://github.com/IntelRealSense/librealsense/pull/4135) - Propagating IMU HW timestamp via metadata API. Allows to retrieve IMU HW timestamp via the metadata API. Also add support for IMU metadata backend timestamps (DSO-12860).
* [#4159](https://github.com/IntelRealSense/librealsense/pull/4159) - Retrofitting metadata quirks into the linux kerenel patches for additional SKUs.
* [#4012](https://github.com/IntelRealSense/librealsense/pull/4012) - Add python 3.7 support (by @ClimbsRocks)
* [#4133](https://github.com/IntelRealSense/librealsense/pull/4133) - Documentation enhancement (by @VasuAgrawal)
* [#4094](https://github.com/IntelRealSense/librealsense/pull/4094) - CMake improvements 
* [#4122](https://github.com/IntelRealSense/librealsense/pull/4122), [#4152](https://github.com/IntelRealSense/librealsense/pull/4152) - T265: Quiet Static Nodes
* [#4078](https://github.com/IntelRealSense/librealsense/pull/4078) - Add support for D4xx SKUs (DSO-12649).
* [#4149](https://github.com/IntelRealSense/librealsense/pull/4149), [#4119](https://github.com/IntelRealSense/librealsense/pull/4119) - SKU enhancements

### Bug Fixes
* [#4160](https://github.com/IntelRealSense/librealsense/pull/4160) - Revisited and updated RGB extrinsic calibration recovery routine that applies to D435i that underwent upgrade to v5.11.6.200. The changes are transparent to the end user.
Addresses #3788, #3949, #4050. (DSO-12820, DSO-12623)
* [#4117](https://github.com/IntelRealSense/librealsense/pull/4117) - Android examples fix with `config` object 
* [#4093](https://github.com/IntelRealSense/librealsense/pull/4093) - Linux: Restore OpenMP support #4023 
* [#4073](https://github.com/IntelRealSense/librealsense/pull/4073) - Fix GlobalTimer prevents enter power-saving mode (DSO-12794).
* [#4032](https://github.com/IntelRealSense/librealsense/pull/4032) - Fix Android error handling flow.
* [#4029](https://github.com/IntelRealSense/librealsense/pull/4029) - Fix RGB calibration recovery exceptions handling (DSO-12789)
* [#4013](https://github.com/IntelRealSense/librealsense/pull/4013) - T265: Improve precision tolerance of Quaternion<=>Rotation transformation to remain within 6% off the norm.
* [#4000](https://github.com/IntelRealSense/librealsense/pull/4000) - T265: Fix t265_rpy.py demo.

### Known Issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block
* [MacOS] File-Open / File-Save dialogs are not available in the Viewer / DQT, preventing import of custom presets #3781.(DSO-9162)
* Frame Drops when changing depth controls while depth streaming. (DSO-9065)
* [#2472](https://github.com/IntelRealSense/librealsense/issues/2472) - Application hangs when trying to close file replay pipeline (DSO-10749)
* [#2809](https://github.com/IntelRealSense/librealsense/issues/2809) - Advanced C# examples bug
* [#2850](https://github.com/IntelRealSense/librealsense/issues/2850) / DSO-11696 - Linux Kernel 4.19 support
* (Python) [#2356](https://github.com/IntelRealSense/librealsense/issues/2356) / DSO-10681 - missing python example of alignment with post-processing
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialized value(s)
* Global Timestamp: first 15 seconds of frames timestamps are unstable (DSO-12942)
* IMU jitter and drops events [LRS] regression (DSO-12940)

## Release 2.22.0
Release Date: 20 May 2019

### API Changes
[API changes](https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2220)

### New Features & Improvements
* [#3909](https://github.com/IntelRealSense/librealsense/pull/3909) - introducing Global Camera Timestamp. The feature is a prerequisite for inter-cam synchronization and requires frame metadata attributes.
 The PR changes the default behavior of `frame.get_timestamp()` by returning the timestamp in `RS2_TIMESTAMP_DOMAIN_GLOBAL_TIME`. Follow the PR link for more details. Addresses #2922, #2188  (RS-3975). 
* [#3654](https://github.com/IntelRealSense/librealsense/pull/3654) - New Module - GLSL Processing Blocks.
This pull-request introduces auxiliary realsense2-gl module. It can be used to perform common SDK processing tasks on the GPU in a relatively generic way (vendor neutral, unlike CUDA) via GLSL shaders.
In addition, it serves as a proof-of-concept for future SDK extensions beyond core API.(Follow the PR link for in-depth introduction)
* [#3828](https://github.com/IntelRealSense/librealsense/pull/3828) - Adding common cross-platform USB back-end API. Initially, it will be used for cross-platform FW update API and tools(DSO-10947).
In the future the infrastructure will allow unified implementation for UVC/HID/TM2 for all the supported operating systems.
* [#3907](https://github.com/IntelRealSense/librealsense/pull/3907) - Promote TM2 Firmware 0.0.18.5715 - re-localization and tracking stability enhancements
* [#3882](https://github.com/IntelRealSense/librealsense/pull/3882) - T265: Re-enabling re-localization reports.
* [#3869](https://github.com/IntelRealSense/librealsense/pull/3869) - Android OS (Java): Add post-processing filters and demo.
* [#3930](https://github.com/IntelRealSense/librealsense/pull/3930) - Add `realsense2-gl` Debian distribution packages to support PR3654.
* [#3933](https://github.com/IntelRealSense/librealsense/pull/3933) - Adding `rs2_get_frame_sensor` function (DSO-12656)
* [#3951](https://github.com/IntelRealSense/librealsense/pull/3951) - New T265 Example: Generate Depth map from T265 Stereo sensors with OpenCv.
* [#3982](https://github.com/IntelRealSense/librealsense/pull/3982) - Add a processing block to generate depth frame in metric format (RS5-4269)
* [#3993](https://github.com/IntelRealSense/librealsense/pull/3993) - Add comprehensive documentation to Python wrapper via Sphinx.
* [#3956](https://github.com/IntelRealSense/librealsense/pull/3956) - Unreal Engine 4 wrapper supports v4.22.
* [#3972](https://github.com/IntelRealSense/librealsense/pull/3972) - Adding `software_device` support to C# wrapper.
* [#3820](https://github.com/IntelRealSense/librealsense/pull/3820) - RealSense depth viewer community example
* [#3860](https://github.com/IntelRealSense/librealsense/pull/3860) - Python Documentation - sample code enhancement
* [#3826](https://github.com/IntelRealSense/librealsense/pull/3826) - C#: Expose T265-specific APIs (Localization map, Static node).
* [#3883](https://github.com/IntelRealSense/librealsense/pull/3883) - OpenCv: Fix missing `RS2_FORMAT_DISPARITY32` handler.

### Bug Fixes
* [#3884](https://github.com/IntelRealSense/librealsense/pull/3884) - T265: Pose record/playback to provide the required metadata attributes; Enable `rs-enumerate-devices` extract info from ROSBag record files. Addresses #3837 (TM2-4344)
* [#3976](https://github.com/IntelRealSense/librealsense/pull/3976) - D435i: Rectify invalid RGB-Depth extrinsic calibration produced by FW versions 5.10.13+. Addresses #3474, #3788, #3201(DSO-12623)
* [#3900](https://github.com/IntelRealSense/librealsense/pull/3900) - Fix extrinsic graph multiplications order.
* [#3901](https://github.com/IntelRealSense/librealsense/pull/3901) - Fix D435i IMU extrinsic
* [#3971](https://github.com/IntelRealSense/librealsense/pull/3971) - T265 FW: Fix potential issue with UNC paths and rc.exe.

### Known Issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block
* [MacOS] File-Open / File-Save dialogs are not available in the Viewer / DQT, preventing import of custom presets #3781.(DSO-9162)
* Frame Drops when changing depth controls while depth streaming. (DSO-9065)
* [#2472](https://github.com/IntelRealSense/librealsense/issues/2472) - Application hangs when trying to close file replay pipeline (DSO-10749)
* [#2809](https://github.com/IntelRealSense/librealsense/issues/2809) - Advanced C# examples bug
* [#2850](https://github.com/IntelRealSense/librealsense/issues/2850) / DSO-11696 - Linux Kernel 4.19 support
* (Python) [#2356](https://github.com/IntelRealSense/librealsense/issues/2356) / DSO-10681 - missing python example of alignment with post-processing
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialised value(s)



## Release 2.21.0
Release Date: 22 April 2019

### API Changes
[API changes](https://github.com/IntelRealSense/librealsense/wiki/API-Changes#version-2200-to-2210) from 2.20.x versions

### New Features & Improvements
* [#3778](https://github.com/IntelRealSense/librealsense/pull/3778) - T265 Firmware update to 0.0.18.5502 - re-localization and tracking stability enhancements
* [#3775](https://github.com/IntelRealSense/librealsense/pull/3775) - T265 Wheel Odometer documentation updated.
* [#3771](https://github.com/IntelRealSense/librealsense/pull/3771) - Android OS - add Visual Presets support. (DSO-12447)
* [#3739](https://github.com/IntelRealSense/librealsense/pull/3739) - Python wrapper performance enhancement - release GIL during invocation of polling routines (DSO-12493)
* [#3736](https://github.com/IntelRealSense/librealsense/pull/3736) - T265: Disable low power default mode.
* [#3729](https://github.com/IntelRealSense/librealsense/pull/3729) - Streamline Linux installation instructions.  (contributed by [@pedrombmachado](https://github.com/pedrombmachado))
* [#3647](https://github.com/IntelRealSense/librealsense/pull/3647) - CMake enhancements : Simplified T265 FW handling, disabling redundant downloads



### Bug Fixes
* [#3810](https://github.com/IntelRealSense/librealsense/pull/3810) - Fix typo in Raspbian documentation (contributed by [@jonherke](https://github.com/jonherke))
* [#3807](https://github.com/IntelRealSense/librealsense/pull/3807) - Enable `rs-measure` with USB2 configurations
* [#3777](https://github.com/IntelRealSense/librealsense/pull/3777) - Matlab installation build fix with T265 firmware.
* [#3773](https://github.com/IntelRealSense/librealsense/pull/3773) - D435i: Fix D435i IMU timestamp precision (DSO-12084)
* [#3760](https://github.com/IntelRealSense/librealsense/pull/3760) - Fix misalignment when generating 3D Pointcloud from Depth-Aligned-To-Color #3752. (DSO-12515)
* [#3755](https://github.com/IntelRealSense/librealsense/pull/3755) - Fix "Elseif" statement in Matlab wrapper
* [#3726](https://github.com/IntelRealSense/librealsense/pull/3726) - D435i: Fix HID Timestamp trimming. #3776, #3675, (DSO-12084)
* [#3702](https://github.com/IntelRealSense/librealsense/pull/3702) - D435i: Fix Windows Metadata script for IMU-enabled SKUs.

### Known Issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block
* [MacOS] File-Open / File-Save dialogs are not available in the Viewer / DQT, preventing import of custom presets #3781.(DSO-9162)
* Frame Drops when changing depth controls while depth streaming. (DSO-9065)
* [#2472](https://github.com/IntelRealSense/librealsense/issues/2472) - Application hangs when trying to close file replay pipeline (DSO-10749)
* [#2809](https://github.com/IntelRealSense/librealsense/issues/2809) - Advanced C# examples bug
* [#2850](https://github.com/IntelRealSense/librealsense/issues/2850) / DSO-11696 - Linux Kernel 4.19 support
* (Python) [#2356](https://github.com/IntelRealSense/librealsense/issues/2356) / DSO-10681 - missing python example of alignment with post-processing
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialised value(s)


## Release 2.20.0
Release Date: 4 April 2019

### API Changes
[API changes](https://github.com/IntelRealSense/librealsense/wiki/API-Changes#from-version-2190-to-2200) from 2.19.x versions

### New Features & Improvements
* [#3522](https://github.com/IntelRealSense/librealsense/pull/3522) - Adding Pose&Image T265 example
* [#3541](https://github.com/IntelRealSense/librealsense/pull/3541) - Implement T265 Fisheye camera intrinsics
* [#3563](https://github.com/IntelRealSense/librealsense/pull/3563) - Implement T265 extrinsics
* [#3571](https://github.com/IntelRealSense/librealsense/pull/3571) - Adding Roll-pitch-yaw T265 example
* [#3593](https://github.com/IntelRealSense/librealsense/pull/3593) - Adding Basic Augmented Reality T265 example
* [#3600](https://github.com/IntelRealSense/librealsense/pull/3600) - Updating Unity documentation
* [#3602](https://github.com/IntelRealSense/librealsense/pull/3602) - C# development, adding [cookbook.md](https://github.com/ogoshen/librealsense/blob/4b7b6c9cbcc68e96f840e13255924ca2f8e426a7/wrappers/csharp/Documentation/cookbook.md) and addressing [#3418](https://github.com/IntelRealSense/librealsense/issues/3418), [#3419](https://github.com/IntelRealSense/librealsense/issues/3419), [#3557](https://github.com/IntelRealSense/librealsense/issues/3557)
* [#3634](https://github.com/IntelRealSense/librealsense/pull/3634) - Adding pipeline callbacks API for python
* [#3643](https://github.com/IntelRealSense/librealsense/pull/3643) - Simplified udev-rules installation script
* [#3669](https://github.com/IntelRealSense/librealsense/pull/3669) - Integration of T265 firmware 0.0.18.5448, with better long-term stability and calibration APIs
* [#3673](https://github.com/IntelRealSense/librealsense/pull/3673) - Adding documentation on update of Debian packages

### Bug Fixes
* [#3610](https://github.com/IntelRealSense/librealsense/pull/3610) - Fixing Raspbian documentation (contributed by [RitwikSaikia](https://github.com/RitwikSaikia))
* [#3564](https://github.com/IntelRealSense/librealsense/pull/3564) - Fixing rs-convert crash (contributed by [@YangJiao1996](https://github.com/YangJiao1996))
* [#3591](https://github.com/IntelRealSense/librealsense/pull/3591) - Fixing [#3435](https://github.com/IntelRealSense/librealsense/issues/3435), multi-T265 display in the RealSense Viewer
* [#3641](https://github.com/IntelRealSense/librealsense/pull/3641) - Fixing [#3623](https://github.com/IntelRealSense/librealsense/issues/3623), broken CUDA optimization of color conversion
* [#3646](https://github.com/IntelRealSense/librealsense/pull/3646) - Fixes to wheel-odometry unit-test
* [#3666](https://github.com/IntelRealSense/librealsense/pull/3666) - Release GIL in `pipeline::wait_for_frames()` (contributed by [@landersson](https://github.com/landersson))

### Known Issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block
* [MacOS] File-Open / File-Save dialogs are not available in the Viewer / DQT, preventing import of custom presets (DSO-9162)
* Frame Drops when changing depth controls while depth streaming. (DSO-9065)
* [#2472](https://github.com/IntelRealSense/librealsense/issues/2472) - Application hangs when trying to close file replay pipeline (DSO-10749)
* [#2693](https://github.com/IntelRealSense/librealsense/issues/2693) - Error in reading rosbag files
* [#2809](https://github.com/IntelRealSense/librealsense/issues/2809) - Advanced C# examples bug
* [#2850](https://github.com/IntelRealSense/librealsense/issues/2850) / DSO-11696 - Linux Kernel 4.19 support
* (Python) [#2356](https://github.com/IntelRealSense/librealsense/issues/2356) / DSO-10681 - missing python example of alignment with post-processing
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialised value(s)

## Release 2.19.2
Release Date: 27 March 2019

### API Changes
No API changes in this release

### New Features & Improvements
* SDK Examples Enhancements [#3548](https://github.com/IntelRealSense/librealsense/pull/3548)
* [Android] Adding device query and firmware version checking [#3547](https://github.com/IntelRealSense/librealsense/pull/3547)
* Use CASE macro consistently [#3539](https://github.com/IntelRealSense/librealsense/pull/3539) contributed by [@nussetorten](https://github.com/nussetorten)
* [T265] Separate wheel-odometry tests [#3535](https://github.com/IntelRealSense/librealsense/pull/3535)
* [T265] Enforce stream index for fisheye streams of the T265 [#3521](https://github.com/IntelRealSense/librealsense/pull/3521)
* [T265] Adding Trajectory example [#3513](https://github.com/IntelRealSense/librealsense/pull/3513)
* Improved comment on the post processing example [#3509](https://github.com/IntelRealSense/librealsense/pull/3509) contributed by [@Heidelberger](https://github.com/Heidelberger)
* Fix typo in raspbian installation [#3484](https://github.com/IntelRealSense/librealsense/pull/3484) contributed by [yuta-imai](https://github.com/yuta-imai)
* [T265] Get Motion Intrinsics [#3482](https://github.com/IntelRealSense/librealsense/pull/3482)
* [#3478](https://github.com/IntelRealSense/librealsense/pull/3478) Add arguments for python threshold filter, contributed by [@codinglife001](https://github.com/codinglife001)
* [T265] Don't raise a hardware event for relocalizations [#3477](https://github.com/IntelRealSense/librealsense/pull/3477)
* [#3471](https://github.com/IntelRealSense/librealsense/pull/3471) `rs-enumerate-devices` enhancements
* [#3462](https://github.com/IntelRealSense/librealsense/pull/3462) - Wheel odometry API: use translational velocity 
* [#3459](https://github.com/IntelRealSense/librealsense/pull/3459) - Adding CMake option to generate python documentation
* [T265] Use T265 system timestamps [#3453](https://github.com/IntelRealSense/librealsense/pull/3453)
* [Linux] Adding minor version to the SONAME [#3449](https://github.com/IntelRealSense/librealsense/pull/3449) contributed by [@morxa](https://github.com/morxa)

### Bug Fixes
* [D435i][python] IMU FPS can't be changed in Python, tracked on DSO-12326, [#3578](https://github.com/IntelRealSense/librealsense/pull/3578)
* [Matlab] Aligning colour to depth [#3577](https://github.com/IntelRealSense/librealsense/pull/3577) addressing [3338](https://github.com/IntelRealSense/librealsense/issues/3338)
* [Viewer] Fixing SegFault on pose stream info [#3423](https://github.com/IntelRealSense/librealsense/issues/3423)
* Fixing [3494](https://github.com/IntelRealSense/librealsense/issues/3494) - backward compatibility break for older ROS-bag recordings
* [T265] Fix query_devices issue with the tracking camera [#3515](https://github.com/IntelRealSense/librealsense/pull/3515), addressing #3488, #3465, #3361, also related to #3437, #3434, tracked on: TM2-4235
* [#3466](https://github.com/IntelRealSense/librealsense/pull/3466) - Fixing USB2 being identified as USB3 in some cases
* [#3455](https://github.com/IntelRealSense/librealsense/pull/3455) - Trimming HID timestamps to 32-bit, tracked on: DSO-12084

### Known Issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block
* [MacOS] File-Open / File-Save dialogs are not available in the Viewer / DQT, preventing import of custom presets (DSO-9162)
* Frame Drops when changing depth controls while depth streaming. (DSO-9065)
* [#2472](https://github.com/IntelRealSense/librealsense/issues/2472) - Application hangs when trying to close file replay pipeline (DSO-10749)
* [#2693](https://github.com/IntelRealSense/librealsense/issues/2693) - Error in reading rosbag files
* [#2809](https://github.com/IntelRealSense/librealsense/issues/2809) - Advanced C# examples bug
* [#2850](https://github.com/IntelRealSense/librealsense/issues/2850) / DSO-11696 - Linux Kernel 4.19 support
* (Python) [#2356](https://github.com/IntelRealSense/librealsense/issues/2356) / DSO-10681 - missing python example of alignment with post-processing
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* [#3435](https://github.com/IntelRealSense/librealsense/issues/3435) - multiple T265 cameras are not visualized correctly in the RealSense Viewer
* [#3433](https://github.com/IntelRealSense/librealsense/issues/3433) - Valgrind: Conditional jump or move depends on uninitialised value(s)

## Release 2.19.1
Release Date: 10 March 2019

### API Changes
No API changes in this release

### New Features & Improvements
* Intel RealSense D430i support - [#3415](https://github.com/IntelRealSense/librealsense/pull/3415) and [#3424](https://github.com/IntelRealSense/librealsense/pull/3424) (requires firmware 5.11.6)
* Adding [`pose-predict`](https://github.com/IntelRealSense/librealsense/tree/development/examples/pose-predict) example for the T265 tracking camera
* **Preliminary** Mac-OS support for the T265 (see known issues)
* Adding support for 848x100 resolution at 100 FPS (requires firmware 5.11.6)
* [#3425](https://github.com/IntelRealSense/librealsense/pull/3425) - improved Mac OS installation documentation (contributed by [@yousseb](https://github.com/yousseb))

### Known Issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block
* [MacOS] File-Open / File-Save dialogs are not available in the Viewer / DQT, preventing import of custom presets (DSO-9162)
* Frame Drops when changing depth controls while depth streaming. (DSO-9065)
* [#2472](https://github.com/IntelRealSense/librealsense/issues/2472) - Application hangs when trying to close file replay pipeline (DSO-10749)
* [#2693](https://github.com/IntelRealSense/librealsense/issues/2693) - Error in reading rosbag files
* [#2809](https://github.com/IntelRealSense/librealsense/issues/2809) - Advanced C# examples bug
* [#2850](https://github.com/IntelRealSense/librealsense/issues/2850) / DSO-11696 - Linux Kernel 4.19 support
* (Python) [#2356](https://github.com/IntelRealSense/librealsense/issues/2356) / DSO-10681 - missing python example of alignment with post-processing
* [T265][Mac] - Start after stop is not working on Mac with the T265 camera
* [#3435](https://github.com/IntelRealSense/librealsense/issues/3435) - multiple T265 cameras are not visualized correctly in the RealSense Viewer
* [#3423](https://github.com/IntelRealSense/librealsense/issues/3423) - possible SEGFAULT when switching into Pose Info view in the RealSense Viewer

## Release 2.19.0
Release Date: 05 March 2019

## API Changes
* [2.18.1 to 2.19.0](https://github.com/IntelRealSense/librealsense/wiki/API-Changes#from-version-2181-to-2190)

### New Features & Improvements

#### T265 Tracking Camera

* [T265 tracking camera support](https://github.com/IntelRealSense/librealsense/blob/v2.18.1/doc/imu_and_tracking_sensors.md) - [#3331](https://github.com/IntelRealSense/librealsense/pull/3331), [#3359](https://github.com/IntelRealSense/librealsense/pull/3359):
> :pushpin: The T265 tracking camera is not yet supported on Android and Mac-OS via librealsense. Support is planned to be added in a future release.

> :pushpin: Wheel-odometry is required to meet performance of <1% loop closure error on wheeled robots. [Learn More...](https://github.com/IntelRealSense/librealsense/pull/3324)

> :pushpin: T265 map internal format will change in future releases

> :pushpin: Additional examples, white-papers and tutorials on advanced topics including wheel-odometry and relocalization map loading will be published in the future
* [Advanced T265 APIs](https://github.com/IntelRealSense/librealsense/pull/3324) - adding new sensors extensions allowing relocalization map load / store, static node set / get (coordinates transform between different maps) and wheel odometry input
* [Ability to unload tracking camera module](https://github.com/IntelRealSense/librealsense/pull/3339) - for better ROS and [NCS](https://github.com/IntelRealSense/librealsense/issues/2924) compatibility
* [rs-pose](https://github.com/IntelRealSense/librealsense/tree/development/examples/pose) - new example showing the basics of working with T265 tracking camera

#### Android

* [Android Play-Store support](https://github.com/IntelRealSense/librealsense/blob/development/doc/android.md) - [#3083](https://github.com/IntelRealSense/librealsense/pull/3083), [#3317](https://github.com/IntelRealSense/librealsense/pull/3317), [#3337](https://github.com/IntelRealSense/librealsense/pull/3337), [#3366](https://github.com/IntelRealSense/librealsense/pull/3366): using peripheral RealSense cameras on Android no longer require the device to be rooted. This feature includes **basic** Java bindings, code samples and tools. 
> :pushpin: The T265 tracking camera and the D435i depth camera are not yet supported on Android via librealsense. Support is planned to be added in a future release.

#### Other

* [Automated Standards Enforcement](https://github.com/IntelRealSense/librealsense/pull/3265) - enforce basic project standards during CI
* [C# & Unity updates](https://github.com/IntelRealSense/librealsense/pull/3285) - multiple changes to the .NET wrapper, including support for motion and pose frames, addressing [#2854](https://github.com/IntelRealSense/librealsense/pull/2854), [#3250](https://github.com/IntelRealSense/librealsense/pull/3250), [#3275](https://github.com/IntelRealSense/librealsense/pull/3275)
* [Refactoring of options and filters API](https://github.com/IntelRealSense/librealsense/pull/3352) - allows different devices to advertise different post-processing capabilities.
* [API for adding software device into existing context](https://github.com/IntelRealSense/librealsense/pull/3340) - for development of better converters and 3rd-party depth sources

## Bug Fixes
* [Added fstream dependency](https://github.com/IntelRealSense/librealsense/pull/3280) - contributed by [@RanoVeder](https://github.com/RanoVeder) addressing [#3279](https://github.com/IntelRealSense/librealsense/issues/3279) and DSO-12001
* [Update python-rs400-advanced-mode-example.py](https://github.com/IntelRealSense/librealsense/pull/3347) - contributed by [@skylouis](https://github.com/skylouis)
* [Replace vec2mat with reshape 2](https://github.com/IntelRealSense/librealsense/pull/3257) - contributed by [@changh95](https://github.com/changh95)
* DSO-11755 - Viewer crash when fail to set advanced mode setting
* [#3252](https://github.com/IntelRealSense/librealsense/issues/3252) - Cannot change AE mean intensity point in the Viewer
* DSO-9820 - Cannot load JSON with RGB8 format in the Viewer
* [#3297](https://github.com/IntelRealSense/librealsense/issues/3297) - rs-convert does not terminate execution
* [#3214](https://github.com/IntelRealSense/librealsense/issues/3214) - rs-convert does not convert to PLY
* [#3159](https://github.com/IntelRealSense/librealsense/issues/3159) - conditionally define `STRINGIFY` macro
* [#3187](https://github.com/IntelRealSense/librealsense/issues/3187) - missing include in `example.hpp`
* [#2844](https://github.com/IntelRealSense/librealsense/issues/2844) - `rs2_depth_frame_get_distance` doesn't work for software_device sensor
* DSO-11903 - YUY in the Viewer produces white image

### Known Issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block
* [MacOS] File-Open / File-Save dialogs are not available in the Viewer / DQT, preventing import of custom presets (DSO-9162)
* Frame Drops when changing depth controls while depth streaming. (DSO-9065)
* [#2472](https://github.com/IntelRealSense/librealsense/issues/2472) - Application hangs when trying to close file replay pipeline (DSO-10749)
* [#2693](https://github.com/IntelRealSense/librealsense/issues/2693) - Error in reading rosbag files
* [#2809](https://github.com/IntelRealSense/librealsense/issues/2809) - Advanced C# examples bug
* [#2850](https://github.com/IntelRealSense/librealsense/issues/2850) / DSO-11696 - Linux Kernel 4.19 support
* (Python) [#2356](https://github.com/IntelRealSense/librealsense/issues/2356) / DSO-10681 - missing python example of alignment with post-processing

## Release 2.18.1
Feb 07, 2019
 
## API Changes
No C/C++ API changes in this release
 
### New Features & Improvements
* [rs-motion](https://github.com/IntelRealSense/librealsense/blob/development/examples/motion) - new example dedicated to devices with an IMU
* [#3203](https://github.com/IntelRealSense/librealsense/pull/3203) - correct T265 PID reporting
* .NET API to register extrinsics ([#3202](https://github.com/IntelRealSense/librealsense/pull/3202) contributed by [@jb455](https://github.com/jb455))
* Making CV examples work with all versions of OpenCV ([#3196](https://github.com/IntelRealSense/librealsense/pull/3196) contributed by [@devernay](https://github.com/devernay) and [@s-trinh](https://github.com/s-trinh))
* Added link to [realsense-ir-to-vaapi-h264](https://github.com/bmegli/realsense-ir-to-vaapi-h264) community example ([#3194](https://github.com/IntelRealSense/librealsense/pull/3194) contributed by [@bmegli](https://github.com/bmegli))
* Adding Hardware Reset to .NET API ([#3192](https://github.com/IntelRealSense/librealsense/pull/3192) contributed by [@victorsbd](https://github.com/victorsbd))
* Addressing libusb error C2001 ([#3190](https://github.com/IntelRealSense/librealsense/pull/3190) contributed by [@UnaNancyOwen](https://github.com/UnaNancyOwen)) 
* Improvements to IMU calibration documentation ([#3172](https://github.com/IntelRealSense/librealsense/pull/3172) and [#3132](https://github.com/IntelRealSense/librealsense/pull/3132) contributed by [@barnjamin](https://github.com/barnjamin))
* [#3162](https://github.com/IntelRealSense/librealsense/pull/3162) - basic T265 Python example
* [#3161](https://github.com/IntelRealSense/librealsense/pull/3161) - adding IMU documentation
* [#3149](https://github.com/IntelRealSense/librealsense/pull/3149) - don't rebuild libtm after CMake if T265 firmware did not change

### Bug Fixes
* Fixing GLSL issue on Mac OS ([#3195](https://github.com/IntelRealSense/librealsense/pull/3195) contributed by [@devernay](https://github.com/devernay))
* [#3177](https://github.com/IntelRealSense/librealsense/issues/3177) - Depth quality tool issue, no point cloud visualization in 3D mode ([#3130](https://github.com/IntelRealSense/librealsense/pull/3130))
* [#3137](https://github.com/IntelRealSense/librealsense/issues/3137), [#3146](https://github.com/IntelRealSense/librealsense/issues/3146), [#3212](https://github.com/IntelRealSense/librealsense/issues/3212), [#3191](https://github.com/IntelRealSense/librealsense/issues/3191) - Invalid Value in rs2_get_option error after upgrading to firmware 5.11.1.0 ([#3127](https://github.com/IntelRealSense/librealsense/pull/3127))
* [#2965](https://github.com/IntelRealSense/librealsense/issues/2965) - D435i accelero not working on some platforms ([#3200](https://github.com/IntelRealSense/librealsense/pull/3200), DSO-11745)
* [#3113](https://github.com/IntelRealSense/librealsense/pull/3113) / DSO-11799 - Fix bug with depth scale before starting device
* (Python) [#3111](https://github.com/IntelRealSense/librealsense/pull/3111) / DSO-10988 - Add dims=3 option to get_vertices/get_texture for (h,w,n) output format
 
### Known Issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block
* [MacOS] File-Open / File-Save dialogs are not available in the Viewer / DQT, preventing import of custom presets (DSO-9162)
* Frame Drops when changing depth controls while depth streaming. (DSO-9065)
* [#2472](https://github.com/IntelRealSense/librealsense/issues/2472) - Application hangs when trying to close file replay pipeline (DSO-10749)
* [#2693](https://github.com/IntelRealSense/librealsense/issues/2693) - Error in reading rosbag files
* [#2809](https://github.com/IntelRealSense/librealsense/issues/2809) - Advanced C# examples bug
* (Python) DSO-10777 - LIBUSB_ERROR_IO on repeated sensor open/close
* DSO-9820 - Cannot load RGB8 format in the JSON file for D415
* (.NET) [#2854](https://github.com/IntelRealSense/librealsense/issues/2854) / DSO-11095 - c# D400 Series Visual Presets
* [#2924](https://github.com/IntelRealSense/librealsense/issues/2924) / DSO-11705 - usbids conflicts with Movidius Neural Compute Stick
* [#2850](https://github.com/IntelRealSense/librealsense/issues/2850) / DSO-11696 - Linux Kernel 4.19 support
* (Python) [#2356](https://github.com/IntelRealSense/librealsense/issues/2356) / DSO-10681 - missing python example of alignment with post-processing
* DSO-11755 - Realsense Viewer total crash when sliding Rsm controls to max value

## Release 2.18.0
Jan 22, 2019
 
## API Changes
[API changes](https://github.com/IntelRealSense/librealsense/wiki/API-Changes#from-version-2170-to-2180) from 2.17.0 version
 
### New Features & Improvements
* Realsense-viewer enhancements:
   * Rendering 3D camera model in 3D view
   * Depth threshold [min/max] control
   * Adding `Report Issue` button
   * Adding configuration file to control and preserve UX settings
     - Control record file name and location ([#2750](https://github.com/IntelRealSense/librealsense/issues/2750))
     - Recording compression level (follow-up on [#2687](https://github.com/IntelRealSense/librealsense/pull/2687))
   * Enhancing support for POSE frame type ([#3010](https://github.com/IntelRealSense/librealsense/pull/3010))
* D435i IMU intrinsic calibration ([Learn More...](https://github.com/IntelRealSense/librealsense/tree/master/tools/rs-imu-calibration#rs-imu-calibration-tool)) [(#3023)](https://github.com/IntelRealSense/librealsense/pull/3023).
* **[Firmware 5.11+]** Alternating emitter (on/off) flickering pattern ([#3066](https://github.com/IntelRealSense/librealsense/pull/3066) following-up on [#482](https://github.com/IntelRealSense/librealsense/issues/482), [#1299](https://github.com/IntelRealSense/librealsense/issues/1299))
* Upgrade to GLFW 3.3 ([#3051](https://github.com/IntelRealSense/librealsense/pull/3051))
* Adding **Code of Conduct** ([#3034](https://github.com/IntelRealSense/librealsense/pull/3034))
* Improved file structure in the C# wrapper ([#3038](https://github.com/IntelRealSense/librealsense/pull/3038))
* Completing CI migration from Appveyor and consolidation all build types to Travis, including Windows. Enhancing parallel build ([#2993](https://github.com/IntelRealSense/librealsense/pull/2993))
* Adding community-developed [installation guide for **raspbian**](https://github.com/IntelRealSense/librealsense/blob/master/doc/installation_raspbian.md) platform (contributed by [@koji](https://github.com/koji)) 
 
### Bug Fixed
* [#2997](https://github.com/IntelRealSense/librealsense/pull/2997) - Fix IMU Start-up delay on Linux (DSO-11674)
* [#3080](https://github.com/IntelRealSense/librealsense/issues/3080) - **[Matlab]** `get_extrinsics` fix
* [#3047](https://github.com/IntelRealSense/librealsense/issues/3047) - Revert to a stable libusb release
* [#2540](https://github.com/IntelRealSense/librealsense/issues/2540) - Using playback function in unity crash (DSO-10757)
* [#2356](https://github.com/IntelRealSense/librealsense/issues/2356) - Python Decimation Filter with Aligning Frames (DSO-10681)
 
### Known Issues
* [#2860](https://github.com/IntelRealSense/librealsense/issues/2860) - Memory-leak in Pointcloud processing block
* [#2965](https://github.com/IntelRealSense/librealsense/issues/2965) - D435i accel sensor fails to start on some Linux PC
* **[MacOS]** File-Open / File-Save dialogs are not available in the Viewer / DQT, preventing import of custom presets (DSO-9162)
* Frame Drops when changing depth controls while depth streaming. (DSO-9065)
* [#2472](https://github.com/IntelRealSense/librealsense/issues/2492) - Application hangs when trying to close file replay pipeline (DSO-10749)
* [#2693](https://github.com/IntelRealSense/librealsense/issues/2693) - Error in reading rosbag files
* [#2809](https://github.com/IntelRealSense/librealsense/issues/2809) - Advanced C# examples bug

## Release 2.17.0
Nov 28, 2018

## API Changes
[API changes](https://github.com/IntelRealSense/librealsense/wiki/API-Changes#from-version-2160-to-2170) from 2.16.x versions

### New Features & Improvements

* Adding [rs-callback](https://github.com/IntelRealSense/librealsense/tree/development/examples/callback) example for asynchronous (low-latency) processing with pipeline.
* Adding new [python](https://github.com/IntelRealSense/librealsense/tree/master/wrappers/python/examples#pointcloud-visualization) and [Matlab](https://github.com/IntelRealSense/librealsense/tree/development/wrappers/matlab#building-from-source) examples for point-cloud data visualization 
* D435i IMU is now supported on Windows 10 ([#2788](https://github.com/IntelRealSense/librealsense/pull/2788), DSO-10754)
* `rs-capture` will now show IMU data when connected to D435i, and rest of the SDK demos will work properly.

### Bug Fixes

* [#2763](https://github.com/IntelRealSense/librealsense/issues/2763) - SDK can now be compiled with `-Werror=shadow` GCC flag
* [#2645](https://github.com/IntelRealSense/librealsense/issues/2645) - fixing symbols collision with ROS libraries

### Known Issues
* [#866](https://github.com/IntelRealSense/librealsense/issues/866) **[Firmware]** - Read device temperature
* [#774](https://github.com/IntelRealSense/librealsense/issues/774) **[Firmware]** RGB-Depth sync
* [#1086](https://github.com/IntelRealSense/librealsense/issues/1086) **[Firmware]** Fram