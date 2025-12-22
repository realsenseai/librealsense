# RealSense LibRealSense Live Unit Tests - Comprehensive Coverage Analysis

**Generated:** December 22, 2025  
**Repository:** librealsense  
**Branch:** RSDSO-20709  
**Base Path:** `/unit-tests/live/`

---

## Executive Summary

- **Total Test Files:** 108
- **Python Tests:** 102
- **C++ Tests:** 6
- **Test Categories:** 22

---

## Table of Contents

1. [Calibration Tests](#1-calibration-tests-calib)
2. [Camera Synchronization Tests](#2-camera-synchronization-tests-camera-sync)
3. [Configuration Tests](#3-configuration-tests-config)
4. [D400 Series Tests](#4-d400-series-tests-d400)
5. [D500 Series Tests](#5-d500-series-tests-d500)
6. [Debug Protocol Tests](#6-debug-protocol-tests-debug_protocol)
7. [DFU Tests](#7-dfu-tests-dfu)
8. [Extrinsics Tests](#8-extrinsics-tests-extrinsics)
9. [Frame Tests](#9-frame-tests-frames)
10. [Firmware Tests](#10-firmware-tests-fw-fw-logs)
11. [HDR Tests](#11-hdr-tests-hdr)
12. [Hardware Reset Tests](#12-hardware-reset-tests-hw-reset)
13. [Image Quality Tests](#13-image-quality-tests-image-quality)
14. [Intrinsics Tests](#14-intrinsics-tests-intrinsics)
15. [Memory Tests](#15-memory-tests-memory)
16. [Metadata Tests](#16-metadata-tests-metadata)
17. [Options Tests](#17-options-tests-options)
18. [Record/Playback Tests](#18-recordplayback-tests-rec-play)
19. [Streaming Tests](#19-streaming-tests-streaming)
20. [Syncer Tests](#20-syncer-tests-syncer)
21. [Tools Tests](#21-tools-tests-tools)
22. [Wrapper Tests](#22-wrapper-tests-wrappers)
23. [Root Level Tests](#23-root-level-tests)

---

## 1. Calibration Tests (`calib/`)

### 1.1 test-advanced-occ-calibrations.py

**Description:** Advanced On-Chip Calibration (OCC) test with calibration table modifications

**Test Flow:**
1. Read and log base principal points (reference)
2. Measure baseline average depth; establish ground truth if not provided
3. Apply manual principal-point perturbation (ppx/ppy shift) to calibration table
4. Re-read and verify modification was applied (delta vs base within tolerance)
5. Measure average depth after modification (pre-OCC)
6. Run OCC calibration
7. Post-OCC: measure average depth again and verify improvement

**Device Requirements:** Not specified (generic calibration device)

**Key Performance Indicators:**
- `HEALTH_FACTOR_THRESHOLD_AFTER_MODIFICATION`: 1.0
- Pixel correction: -2.0 pixels
- Epsilon tolerance: 0.001

**Test Status:** `#test:donotrun` - Disabled until lab stabilization

**Category:** Calibration, Quality Assurance

---

### 1.2 test-advanced-tare-calibrations.py

**Description:** Advanced Tare calibration test with calibration table modifications

**Test Flow:**
1. Read and log base principal points (reference)
2. Measure baseline average depth; establish ground truth if not provided
3. Apply manual principal-point perturbation (ppx/ppy shift) to calibration table
4. Re-read and verify modification was applied (delta vs base within tolerance)
5. Measure average depth after modification (pre-Tare)
6. Run Tare calibration
7. Post-Tare: measure average depth again and verify improvement

**Device Requirements:** Not specified

**Key Performance Indicators:**
- `HEALTH_FACTOR_THRESHOLD`: 0.25
- `HEALTH_FACTOR_THRESHOLD_AFTER_MODIFICATION`: 2.0
- `TARGET_Z_MIN`: 600mm
- `TARGET_Z_MAX`: 1500mm
- Timeout: 30 seconds

**Test Status:** `#test:donotrun` - Disabled until lab stabilization

**Category:** Calibration, Quality Assurance

---

### 1.3 test-occ-calibrations.py

**Description:** Basic On-Chip Calibration (OCC) test without host assistance

**Device Requirements:** Non-MIPI devices (MIPI devices do not support OCC without host assistance)

**Key Performance Indicators:**
- `HEALTH_FACTOR_THRESHOLD`: 1.5 (temporary W/A for cameras in low position in lab; proper value is 0.25)
- Resolution: 256x144 @ 90 FPS

**Test Status:** `#test:donotrun` - Disabled until lab stabilization

**Category:** Calibration, Quality Assurance

---

### 1.4 test-tare-calibrations.py

**Description:** Tare calibration test for depth accuracy

**Device Requirements:** Not specified

**Key Performance Indicators:**
- `HEALTH_FACTOR_THRESHOLD`: 0.25
- `TARGET_Z_MIN`: 600mm
- `TARGET_Z_MAX`: 1500mm
- Resolution: 256x144 @ 90 FPS
- Timeout: 30 seconds
- Number of images: 50+

**Test Status:** `#test:donotrun` - Disabled until lab stabilization

**Category:** Calibration, Quality Assurance

---

## 2. Camera Synchronization Tests (`camera-sync/`)

### 2.1 test-sync-depth-fps-performance.py

**Description:** Dual-camera synchronized depth FPS performance test. Tests FPS accuracy for synchronized depth streaming on two cameras in MASTER-SLAVE mode.

**Device Requirements:** 
- `#test:device D400_CAM_SYNC`
- Requires 2 cameras with sync capability

**Test Flow:**
1. Configure cameras for MASTER-SLAVE synchronization
2. Test all supported depth stream configurations (resolution + FPS combinations)
3. Measure actual FPS vs expected FPS for both cameras
4. Verify frame synchronization between cameras

**Key Performance Indicators:**
- FPS accuracy tolerance: ±5% (configurable)
- Minimum test duration: 60% of expected duration
- Minimum frame count for low FPS: 5 frames
- Test configurations: All supported depth resolutions and FPS combinations

**Test Status:** `#test:donotrun:!weekly` - Weekly run only

**Timeout:** 300 seconds

**Category:** Multi-camera, Synchronization, Performance

---

### 2.2 test-sync-mode.py

**Description:** Inter-camera sync mode validation test. Tests the inter_cam_sync_mode option for D400 cameras with hardware sync capability.

**Device Requirements:** 
- `#test:device D400_CAM_SYNC`

**Test Scenarios:**
1. Verify camera inter-cam sync mode default is DEFAULT
2. Verify can set to MASTER mode
3. Verify can set to SLAVE mode
4. Verify can set to FULL_SLAVE mode
5. Test set during idle mode
6. Test set during streaming mode is not allowed

**Key Performance Indicators:**
- Mode switching validation
- State consistency checks

**Category:** Multi-camera, Synchronization, Configuration

---

### 2.3 test-sync-stream.py

**Description:** Master-slave synchronization with calibrated frame time. Tests timestamp drift between master and slave cameras over extended periods.

**Device Requirements:** 
- `#test:device D400_CAM_SYNC`
- Two D400 cameras connected (D405 excluded as it lacks sync port)
- Cameras connected via sync cable
- FW version >= 5.15.0.0 for inter_cam_sync_mode support

**Test Configuration:**
- `CALIBRATION_DURATION`: 5 seconds (default)
- `DRIFT_TEST_DURATION`: 90 seconds (default)
- `ENABLE_DRIFT_PLOT`: False (default)

**Key Performance Indicators:**
- `MASTER_SLAVE_OFFSET_THRESHOLD_MAX`: 20 µs (maximum first segment offset for MASTER-SLAVE mode)
- `MASTER_SLAVE_DRIFT_RATE_THRESHOLD_MAX`: 20 µs/minute (maximum drift rate for MASTER-SLAVE mode)
- `DEFAULT_OFFSET_THRESHOLD_MIN`: 100 µs (minimum first segment offset for DEFAULT mode)

**Test Status:** `#test:donotrun:!sync_test` - Requires special setup

**Timeout:** 300 seconds

**Category:** Multi-camera, Synchronization, Timestamp Accuracy

---

## 3. Configuration Tests (`config/`)

### 3.1 test-device-hub.py

**Description:** Device hub functionality testing - wait_for_device, is_connected, and disconnect detection

**Device Requirements:**
- `#test:device each(D400*)`
- `#test:device each(D500*)`

**Test Scenarios:**
1. device_hub: wait_for_device & is_connected
2. device_hub: detect disconnect after hardware_reset

**Key Performance Indicators:**
- `MAX_TRIES`: 3 attempts
- Timeout: 2 seconds per operation

**Test Status:** `#test:donotrun:!nightly` - Nightly run only

**Category:** Device Management, Configuration

---

### 3.2 test-eth-config.py

**Description:** Ethernet/DDS configuration testing for network-connected devices

**Device Requirements:**
- `#test:device each(D555)` - Currently only D555 supports DDS configuration natively

**Key Performance Indicators:**
- Timeout: 2 seconds

**Category:** Network Configuration, DDS

---

## 4. D400 Series Tests (`d400/`)

### 4.1 test-auto-limits.py

**Description:** Auto exposure/gain limits functionality testing

**Device Requirements:**
- `#test:device D455`

**Test Scenarios:**
1. Change control value few times
2. Turn toggle off
3. Turn toggle on
4. Check that control values are within expected limits

**Category:** Auto-Exposure, Options

---

### 4.2 test-d405-calibration-stream.py

**Description:** D405 calibration stream configurations

**Device Requirements:**
- `#test:device D400*`

**Test Scenarios:**
1. D405 explicit configuration - IR calibration, Color in HD
2. D405 explicit configuration - IR calibration, Color in VGA
3. D405 implicit configuration - IR calibration, Color

**Category:** Streaming, Configuration, D405-specific

---

### 4.3 test-depth_ae_convergence.py

**Description:** Depth Auto-Exposure (AE) convergence qualification test. Measures time for depth AE to converge after large manual exposure change.

**Device Requirements:**
- `#test:device each(D400*) !D457` (D457 excluded due to failures)

**Test Modes:**
1. REGULAR AE mode convergence
2. ACCELERATED AE mode convergence (if supported)

**Key Performance Indicators:**
- Test timeout: 2.0 seconds per profile
- Tests all supported depth profiles
- Convergence measurement: time from AE enable until stable

**Test Status:** `#test:donotrun:!nightly` - Nightly run only

**Timeout:** 600 seconds (10 minutes for comprehensive profile testing)

**Category:** Auto-Exposure, Performance, Depth

---

### 4.4 test-depth_ae_mode.py

**Description:** Depth AE mode option validation (REGULAR vs ACCELERATED)

**Device Requirements:**
- `#test:device D455`

**Test Scenarios:**
1. Verify camera AE mode default is REGULAR
2. Verify can set when auto exposure on
3. Test set during idle mode
4. Test set during streaming mode is not allowed

**Category:** Auto-Exposure, Options

---

### 4.5 test-depth-ae-toggle.py

**Description:** Depth auto-exposure robustness validation while streaming. Tests camera stability during rapid AE toggling.

**Device Requirements:**
- `#test:device each(D400*)`

**Test Flow:**
1. Baseline streaming test - measure frame timing with AE enabled but no toggling
2. Rapid AE toggle test - toggle AE on/off repeatedly and measure frame timing stability
3. Manual-to-AE test - switch from long manual exposure (2x frame time) to AE and verify recovery

**Validation:**
- Camera continues streaming without stalls
- Frame timing spikes (gaps > 110% of expected frame time) stay below acceptable threshold
- Average frame time remains close to expected value
- AUTO_EXPOSURE metadata matches the expected AE state

**Key Performance Indicators:**
- FPS: 30
- Frame gap threshold: 10.0% above expected frame time

**Test Status:** `#test:donotrun:!nightly` - Nightly run only

**Timeout:** 300 seconds

**Category:** Auto-Exposure, Stability, Robustness

---

### 4.6 test-disparity-modulation.py

**Description:** Disparity modulation A Factor testing

**Device Requirements:**
- `#test:device D400*`

**Test Description:** Validates that A Factor of Disparity can be changed in advanced mode

**Category:** Advanced Mode, Depth Processing

---

### 4.7 test-emitter-frequency.py

**Description:** Emitter frequency option testing for supported devices

**Device Requirements:**
- `#test:device:jetson D457`
- `#test:device:!jetson D455`

**Test Scenarios:**
1. Verify camera defaults
2. Test set On/Off during idle mode
3. Test set On/Off during streaming mode is not allowed

**Category:** Emitter Control, Options

---

### 4.8 test-emitter-frequency-negative.py

**Description:** Negative test for emitter frequency on legacy devices

**Device Requirements:**
- `#test:device each(D400*) !D455`

**Test Description:** Verifies that emitter frequency is not supported on legacy devices

**Test Status:** `#test:donotrun:jetson`

**Category:** Negative Testing, Emitter Control

---

### 4.9 test-hdr-long.py

**Description:** Extended HDR feature testing

**Device Requirements:**
- `#test:device D400*`

**Key Performance Indicators:**
- Max retries: 10

**Test Status:** `#test:donotrun:!nightly` - Nightly run only

**Category:** HDR, Extended Testing

---

### 4.10 test-hdr-sanity.py

**Description:** HDR feature sanity testing

**Device Requirements:**
- `#test:device D400*`

**Category:** HDR, Sanity

---

### 4.11 test-mipi-motion.py

**Description:** MIPI motion sensor testing

**Device Requirements:**
- `#test:device:jetson D457`

**Key Performance Indicators:**
- FPS: 100 Hz for accel/gyro

**Test Status:** 
- `#test:donotrun:!jetson`
- `#test:retries 3`

**Category:** MIPI, IMU, Motion

---

### 4.12 test-pipeline-set-device.py

**Description:** Pipeline device configuration testing

**Device Requirements:**
- `#test:device D455`

**Category:** Pipeline, Configuration

---

## 5. D500 Series Tests (`d500/`)

### 5.1 D500 Safety Tests (`d500/safety/`)

#### 5.1.1 test-3d-mapping-metadata.py

**Description:** 3D mapping metadata validation for occupancy and labeled point cloud streams

**Device Requirements:**
- `#test:device D585S`

**Test Scenarios:**
1. Checking occupancy stream metadata received
2. Checking labeled point cloud stream metadata received
3. Checking occupancy stream metadata frame counter and timestamp increasing
4. Checking labeled point cloud stream metadata frame counter and timestamp increasing

**Key Performance Indicators:**
- FPS: 30 Hz

**Category:** Safety, Metadata, 3D Mapping

---

#### 5.1.2 test-app-config-get-set-api.py

**Description:** Application configuration table get/set API validation

**Device Requirements:**
- `#test:device D585S`

**Test Scenarios:**
1. Get app config table
2. Set app config table and check writing
3. Restoring config table

**Category:** Safety, Configuration API

---

#### 5.1.3 test-app-config-get-set-hwm-cmd.py

**Description:** Application configuration get/set via HWM command

**Device Requirements:**
- `#test:device D585S`

**Key Performance Indicators:**
- Threshold: 80
- Min buffer sizes: 216 bytes, 206 bytes

**Test Scenarios:**
1. Get app config table
2. Set app config table and check writing
3. Restoring config table

**Category:** Safety, Configuration, HWM

---

#### 5.1.4 test-interface-config-get-set.py

**Description:** Safety Interface Configuration (SIC) get/set testing

**Device Requirements:**
- `#test:device D585S`

**Test Scenarios:**
1. Valid get/set scenario
2. Verify same table after camera reboot
3. Checking config is the same in flash and in RAM

**Key Performance Indicators:**
- Threshold: 90%

**Test Status:** 
- `#test:donotrun:!nightly` - Nightly run only
- `#test:priority 9` - High priority (runs before other safety tests)

**Category:** Safety, Configuration, Persistence

---

#### 5.1.5 test-metadata.py

**Description:** Safety stream metadata validation

**Device Requirements:**
- `#test:device D585S`

**Test Scenarios:**
1. Checking safety stream metadata received
2. Checking safety stream metadata frame counter and timestamp increasing

**Key Performance Indicators:**
- FPS: 30 Hz

**Category:** Safety, Metadata

---

#### 5.1.6 test-operational-mode.py

**Description:** Safety operational mode switching testing

**Device Requirements:**
- `#test:device D585S`

**Category:** Safety, Operational Modes

---

#### 5.1.7 test-operational-mode-stress.py

**Description:** Stress test for safety operational mode switching

**Device Requirements:**
- `#test:device D585S`

**Test Status:** `#test:donotrun` - Disabled until HKR FW stabilization

**Description:** Short stress test due to many regressions seen in operational mode switching

**Category:** Safety, Stress Testing, Operational Modes

---

#### 5.1.8 test-preset-active-index-set-get.py

**Description:** Safety preset active index get/set validation

**Device Requirements:**
- `#test:device D585S`

**Test Scenarios:**
1. Verify Safety Sensor Extension
2. Check if safety sensor supports the option
3. Valid get/set scenario
4. Invalid set - index out of range

**Category:** Safety, Presets, Configuration

---

#### 5.1.9 test-preset-get-set-index-checks.py

**Description:** Safety preset index boundary checking

**Device Requirements:**
- `#test:device D585S`

**Test Scenarios:**
1. Verify Safety Sensor Extension
2. Valid read from index 0
3. Valid read from index 1
4. Valid read and write from index 1 to 0
5. Valid read and write from index 1 to 2
6. Valid read and write from index 63
7. Invalid read - index out of range
8. Invalid write - index out of range

**Category:** Safety, Presets, Boundary Testing

---

#### 5.1.10 test-preset-get-set.py

**Description:** Safety preset configuration get/set comprehensive testing

**Device Requirements:**
- `#test:device D585S`

**Test Scenarios:**
1. Verify Safety Sensor Extension
2. Init all safety zones
3. Writing safety preset to random index, then reading and comparing safety presets JSONs

**Key Performance Indicators:**
- Threshold: 99%

**Test Status:** 
- `#test:donotrun:!nightly` - Nightly run only
- `#test:priority 10` - Highest priority
- `#test:retries 3` - Add retries as HKR FW occasionally fails during initialization

**Category:** Safety, Presets, Configuration

---

#### 5.1.11 test-smcu-version.py

**Description:** SMCU (Safety MCU) version verification

**Device Requirements:**
- `#test:device D585S`

**Category:** Safety, Version Verification

---

#### 5.1.12 test-verify-default-preset.py

**Description:** Verify default preset values match startup values

**Device Requirements:**
- `#test:device D585S`

**Test Scenario:** Check startup values are the same as the default preset values

**Category:** Safety, Presets, Validation

---

#### 5.1.13 test-y16-calibration-format.py

**Description:** Y16 calibration format streaming test

**Device Requirements:**
- `#test:device D585S`

**Test Scenario:** Check that y16 is streaming

**Category:** Safety, Streaming, Calibration Format

---

### 5.2 D500 SC Landing Zone Tests (`d500/sc-landing-zone/`)

#### 5.2.1 test-depth-global-ts-get-set.py

**Description:** Depth global timestamp get/set testing

**Device Requirements:**
- `#test:device D585S`

**Key Performance Indicators:**
- `MAX_TIME_TO_WAIT_FOR_FRAMES`: 5 seconds

**Test Status:** `#test:donotrun:!nightly` - Nightly run only

**Category:** Timestamps, Configuration

---

#### 5.2.2 test-stream-color.py

**Description:** Color stream validation for various resolutions and FPS

**Device Requirements:**
- `#test:device D585S`

**Test Status:** `#test:donotrun:!nightly` - Nightly run only

**Category:** Streaming, Color

---

#### 5.2.3 test-stream-depth-dpp.py

**Description:** Depth DPP (Depth Processing Pipeline) streaming validation

**Device Requirements:**
- `#test:device D585S`

**Test Status:** `#test:donotrun:!nightly` - Nightly run only

**Category:** Streaming, Depth, DPP

---

#### 5.2.4 test-stream-depth-infrared.py

**Description:** Depth and infrared streaming validation

**Device Requirements:**
- `#test:device D585S`

**Test Status:** `#test:donotrun:!nightly` - Nightly run only

**Category:** Streaming, Depth, Infrared

---

#### 5.2.5 test-stream-imu.py

**Description:** IMU streaming validation for accel and gyro

**Device Requirements:**
- `#test:device D585S`

**Key Performance Indicators:**
- Accel FPS: 100 Hz, 200 Hz
- Gyro FPS: 100 Hz, 200 Hz

**Test Status:** `#test:donotrun:!nightly` - Nightly run only

**Category:** Streaming, IMU, Motion

---

#### 5.2.6 test-stream-safety-occ-lpc.py

**Description:** Safety occupancy and labeled point cloud streaming

**Device Requirements:**
- `#test:device D585S`

**Test Scenarios:**
- Stream occupancy
- Stream labeled point cloud

**Category:** Streaming, Safety, 3D Mapping

---

### 5.3 D500 Depth Mapping Tests (`d500/depth-mapping/`)

#### 5.3.1 test-frame-number-vs-counter.py

**Description:** Verify frame number and frame counter synchronization

**Device Requirements:**
- `#test:device D585S`

**Key Performance Indicators:**
- `MAX_TIME_TO_WAIT_FOR_FRAMES`: 5 seconds

**Category:** Frame Synchronization, Metadata

---

### 5.4 D500 General Tests

#### 5.4.1 test-dds-embedded-filters.py

**Description:** DDS embedded filters testing

**Device Requirements:**
- `#test:device D555`

**Key Performance Indicators:**
- FPS: 30 Hz
- `MAX_TIME_TO_WAIT_FOR_FRAMES`: 10 seconds

**Test Status:** `#test:donotrun:!dds` - Requires DDS environment

**Category:** DDS, Filters

---

#### 5.4.2 test-detect-dds-device.py

**Description:** DDS device detection test

**Device Requirements:**
- `#test:device D555`

**Test Description:** Query for device, test check it is DDS and exit

**Test Status:** `#test:donotrun:!dds` - Requires DDS environment

**Category:** DDS, Device Detection

---

#### 5.4.3 test-get-set-calib-config-table-api.py

**Description:** Calibration configuration table API testing

**Device Requirements:** Auto-calibrated device

**Test Scenarios:**
1. Verify auto calibrated device extension
2. Writing calibration config, then reading and comparing calibration config JSONs
3. Trying to write bad calib config with a missing field
4. Trying to write bad calib config with a missing field from camera position
5. Trying to write bad calib config with a missing ROI values or wrong ROI types
6. Trying to write bad calib config with wrong crypto_signature values
7. Restore original calibration config table

**Test Status:** `#test:donotrun` - Requires specific device setup

**Category:** Calibration, API, Negative Testing

---

#### 5.4.4 test-get-set-config-table.py

**Description:** Configuration table read/write testing for extended buffers (> 1KB)

**Device Requirements:**
- `#test:device D585S`

**Test Scenarios:**
1. Get ds5 standard buffer (GVD)
2. Get buffer less than 1 KB
3. Get buffer more than 1 KB - getting the whole table at once
4. Get buffer more than 1 KB - getting the table chunk by chunk

**Note:** Test only tests the 'read' part to avoid ruining calibration tables

**Category:** Configuration, Buffer Management

---

#### 5.4.5 test-read-serial-number.py

**Description:** Serial number verification from GVD

**Device Requirements:**
- `#test:device D500*`

**Test Description:** Verifies we read a 12-digit serial number from GVD and it matches the SDK reported device serial number

**Category:** Device Info, Serial Number

---

#### 5.4.6 test-temperatures-xu-vs-hwmc.py

**Description:** Temperature reading comparison between XU and HWM commands

**Device Requirements:**
- `#test:device D500*`

**Key Performance Indicators:**
- Tolerance: 3.0°C

**Test Description:** Validates that the same temperature values are received whether XU command or HWM Command are used

**Category:** Temperature Monitoring, HWM, XU

---

## 6. Debug Protocol Tests (`debug_protocol/`)

### 6.1 test-build-command.py

**Description:** Debug protocol command building testing

**Device Requirements:**
- `#test:device D400*`
- `#test:device each(D555)`

**Test Scenarios:**
1. Init
2. Old Scenario Test
3. New Scenario Test

**Category:** Debug Protocol, Command Building

---

### 6.2 test-hwmc-errors.py

**Description:** HWM error reporting mechanism testing

**Device Requirements:**
- `#test:device each(D400*)`
- `#test:device each(D500*)`

**Test Scenarios:**
1. Init
2. Invalid command
3. No Data to Return
4. Wrong Parameter

**Test Description:** When HWM command is successful, expect command opcode reflected in first bytes of reply. In case of failure, negative value indicates failure reason.

**Category:** HWM, Error Handling, Debug Protocol

---

## 7. DFU Tests (`dfu/`)

### 7.1 test-device-fw-compatibility.py

**Description:** Firmware compatibility verification

**Device Requirements:**
- `#test:device D400*`

**Test Scenario:** Checking firmware compatibility with device

**Note:** Test depends on files deployed on LibCI machines (Windows + Linux)

**Category:** DFU, Firmware, Compatibility

---

## 8. Extrinsics Tests (`extrinsics/`)

### 8.1 test-consistency.cpp

**Description:** Extrinsics consistency validation (C++)

**Key Performance Indicators:**
- Tolerance: 0.00001
- Tolerance: 0.0001
- Max value: 1.0

**Language:** C++

**Category:** Extrinsics, Calibration

---

### 8.2 test-imu.py

**Description:** IMU extrinsics validation

**Device Requirements:**
- `#test:device each(D400*)`
- `#test:device each(D500*) !D555`

**Note:** D555 can be enabled when RSDEV-3159 is resolved

**Category:** Extrinsics, IMU

---

## 9. Frame Tests (`frames/`)

### 9.1 test-ah-configurations.py

**Description:** Application heap (AH) configurations testing

**Device Requirements:**
- `#test:device D585S`

**Test Status:** `#test:donotrun:!nightly` - Nightly run only

**Category:** Frame Management, Configuration

---

### 9.2 test-backend-vs-frame-timestamp.py

**Description:** Backend vs frame timestamp comparison

**Device Requirements:**
- `#test:device D400* !D457`

**Key Performance Indicators:**
- FPS: 30 Hz
- Delta: 0

**Category:** Timestamps, Frame Timing

---

### 9.3 test-color_frame_frops.py

**Description:** Color frame drops testing

**Device Requirements:**
- `#test:device D400*`

**Key Performance Indicators:**
- FPS: 60 Hz
- Delta: 1000 frames

**Test Status:** `#test:donotrun`

**Category:** Frame Drops, Color

---

### 9.4 test-D455_frame_drops.py

**Description:** D455 specific frame drops testing with RGB stream at 90 FPS

**Device Requirements:**
- `#test:device D455`

**Key Performance Indicators:**
- FPS: 90 Hz
- Tolerance: 95%
- Delta tolerance: 95%

**Test Description:** Find frame drops by checking HW timestamp of each frame

**Test Status:** `#test:donotrun`

**Category:** Frame Drops, Color, D455-specific

---

### 9.5 test-depth.py

**Description:** Depth frame validation with variance check

**Device Requirements:**
- `#test:device each(D400*)`
- `#test:device each(D500*)`

**Test Flow:**
1. Start depth + color streams
2. Go through frames to verify depth image
3. Color stream used to display camera facing direction
4. Verify frame has variance - therefore showing depth image
5. In debug mode, display and save images with frames found

**Key Performance Indicators:**
- Threshold: 0.5
- Max depth value: 10m
- Max diff percentage: 100.0%

**Test Scenario:** Testing depth frame - laser ON

**Category:** Depth, Frame Validation

---

### 9.6 test-fps-manual-exposure.py

**Description:** FPS testing with manual exposure (mirrors test-fps.py but forces manual exposure)

**Device Requirements:**
- `#test:device each(D400*)`
- `#test:device each(D500*) !D555`

**Key Performance Indicators:**
- Tested FPS: 5, 6, 15, 30, 60, 90 Hz
- Delta tolerance: ±1 Hz (±5% for most FPS rates)

**Test Status:** `#test:donotrun`

**Category:** FPS, Manual Exposure, Performance

---

### 9.7 test-fps-performance.py

**Description:** Comprehensive FPS performance testing for all supported resolutions and frame rates

**Device Requirements:**
- `#test:device D400*`
- `#test:device D500*`

**Test Coverage:**
- All supported depth resolutions and FPS combinations
- All supported color resolutions and FPS combinations
- All supported IR resolutions and FPS combinations
- Multi-stream combinations (Depth + Color)

**Key Performance Indicators:**
- FPS: 5, 30+ Hz (various)
- Timeout: 10 seconds (device creation)
- DDS device creation timeout: 30 seconds
- Minimum frame count for low FPS: 5 frames
- Minimum test duration: 60% of expected duration

**Code Organization:**
- Consolidated generic functions eliminate ~300 lines of redundant code
- CI optimization mode available (limits configurations tested)

**Test Status:** `#test:donotrun:!weekly` - Weekly run only

**Timeout:** 14400 seconds (4 hours for comprehensive testing)

**Category:** FPS, Performance, Comprehensive

---

### 9.8 test-fps-permutations.py

**Description:** FPS testing for all stream permutations (pairs and all-on)

**Device Requirements:**
- `#test:device D400* !D457`

**Test Scenarios:**
- Test each pair of streams
- Test all streams on simultaneously

**Test Status:** `#test:donotrun:!nightly` - Nightly run only

**Timeout:** 300 seconds
- Formula: ((8 choose 2)+1) * (TIME_FOR_STEADY_STATE + TIME_TO_COUNT_FRAMES)

**Category:** FPS, Stream Combinations

---

### 9.9 test-fps.py

**Description:** Standard FPS testing for depth and color streams

**Device Requirements:**
- `#test:device each(D400*)`
- `#test:device each(D500*) !D555`

**Key Performance Indicators:**
- Tested FPS: 5, 6, 15, 30, 60, 90 Hz
- Delta tolerance: ±5% (10% for 5 FPS)

**Test Scenarios:**
1. Testing depth fps
2. Testing color fps

**Test Status:** `#test:donotrun:!nightly` - Nightly run only

**Category:** FPS, Performance, Streaming

---

### 9.10 test-pipeline-start-stop.py

**Description:** Multiple pipeline start/stop stress test

**Device Requirements:**
- `#test:device each(D400*)`
- `#test:device each(D500*)`

**Test Description:** Run multiple start/stop of all streams and verify we get a frame for each once

**Test Status:** `#test:donotrun:!nightly` - Nightly run only

**Timeout:** 220 seconds (on D455 and units with IMU ~4 seconds per iteration)

**Note:** Relaxed to 3 iterations as 50 was failing often (See LRS-1213)

**Category:** Pipeline, Stress Testing, Start/Stop

---

### 9.11 test-sensor-vs-frame-timestamp.py

**Description:** Sensor timestamp vs frame timestamp comparison

**Device Requirements:**
- `#test:device D400*`

**Key Performance Indicators:**
- FPS: 30 Hz
- Delta: 0

**Category:** Timestamps, Frame Timing

---

### 9.12 test-t2ff-pipeline.py

**Description:** Time to first frame measurement using pipeline API

**Device Requirements:**
- `#test:device each(D400*)`
- `#test:device each(D500*)`

**Test Scenarios:**
1. Testing pipeline first depth frame delay
2. Testing pipeline first color frame delay

**Key Performance Indicators:**
- Max delay varies by product line

**Note:** Windows Media Foundation power management adds ~27ms delay

**Category:** Latency, Pipeline, Time to First Frame

---

### 9.13 test-t2ff-sensor.py

**Description:** Time to first frame measurement using sensor API

**Device Requirements:**
- `#test:device each(D400*)`
- `#test:device each(D500*)`

**Test Scenarios:**
1. Testing device creation time
2. Testing first depth frame delay
3. Testing first color frame delay

**Key Performance Indicators:**
- FPS: 30 Hz
- Max delay: varies by product line

**Note:** Windows Media Foundation power management adds ~27ms delay

**Category:** Latency, Sensor API, Time to First Frame

---

## 10. Firmware Tests (`fw/`, `fw-logs/`)

### 10.1 test-fw-errors.py

**Description:** Firmware error notification monitoring during streaming

**Device Requirements:**
- `#test:device each(D400*)`
- `#test:device each(D500*)`

**Test Description:** Monitor firmware error notifications during streaming to ensure hardware stability

**Key Performance Indicators:**
- Tolerance: 0 errors (excluding known errors)
- Duration: 10 seconds
- Min duration: 10 seconds

**Test Status:** `#test:donotrun:!nightly` - Nightly run only

**Category:** Firmware, Error Monitoring, Stability

---

### 10.2 test-extended.py

**Description:** Extended firmware logs testing

**Device Requirements:**
- `#test:device each(D500*) !D555`

**Note:** DDS devices have not implemented firmware_logger interface yet

**Test Status:** `#test:donotrun:#:!dds`

**Category:** Firmware Logs, Extended

---

### 10.3 test-legacy.py

**Description:** Legacy firmware logs testing

**Device Requirements:**
- `#test:device each(D400*)`

**Note:** DDS devices have not implemented firmware_logger interface yet

**Test Status:** `#test:donotrun:#:!dds`

**Category:** Firmware Logs, Legacy

---

### 10.4 test-xml-helper.py

**Description:** XML helper functions for firmware logs

**Device Requirements:**
- `#test:device D585S`

**Note:** Currently testing with D585S as it's the only module supporting some features like module verbosity and version verification

**Category:** Firmware Logs, XML Parsing

---

## 11. HDR Tests (`hdr/`)

### 11.1 test-hdr-configurations.py

**Description:** HDR configurations testing with various resolutions

**Device Requirements:**
- `#test:device:jetson D457`
- `#test:device:!jetson D455`

**Test Configurations:**
- Depth resolutions: 640x480, 848x480, 1280x720
- Auto and Manual HDR configurations
- Various number of HDR items

**Test Scenarios:**
- Test each configuration with different resolutions
- Test disabling Auto-HDR and returning to default behavior

**Test Status:** `#test:donotrun:!nightly` - Nightly run only

**Category:** HDR, Configurations, Depth

---

### 11.2 test-hdr-performance.py

**Description:** HDR performance testing with various configurations

**Device Requirements:**
- `#test:device:jetson D457`
- `#test:device:!jetson D455`

**Key Performance Indicators:**
- EXPECTED_FPS: 30 Hz

**Test Status:** `#test:donotrun:!nightly` - Nightly run only

**Category:** HDR, Performance

---

### 11.3 test-hdr-preset.py

**Description:** HDR preset validation

**Device Requirements:**
- `#test:device:jetson D457`
- `#test:device:!jetson D455`

**Category:** HDR, Presets

---

## 12. Hardware Reset Tests (`hw-reset/`)

### 12.1 test-sanity.py

**Description:** Hardware reset sanity - verify disconnect and reconnect

**Device Requirements:**
- `#test:device each(D400*) !D457` (D457 known for HW reset issues)
- `#test:device each(D500*)`

**Test Description:** Verify device disconnect & reconnect successfully after HW reset

**Category:** Hardware Reset, Sanity

---

### 12.2 test-t2enum.py

**Description:** Hardware reset to enumeration time measurement

**Device Requirements:**
- `#test:device each(D400*) !D457` (D457 known for HW reset issues)
- `#test:device each(D500*)`

**Key Performance Indicators:**
- Max enumeration time: 5 seconds (typical)
- Max enumeration time: 15 seconds (extended)

**Test Status:** `#test:donotrun:!nightly` - Nightly run only

**Category:** Hardware Reset, Enumeration Time

---

## 13. Image Quality Tests (`image-quality/`)

### 13.1 test-basic-color.py

**Description:** Basic color image quality validation using lab setup with color chart

**Device Requirements:**
- `#test:device D400*`

**Test Setup:**
- Requires lab setup with ArUco markers (IDs 4,5,6,7)
- Color chart with predefined colors for validation

**Key Performance Indicators:**
- `FRAMES_PASS_THRESHOLD`: 0.8 (80% of frames must pass)
- `COLOR_TOLERANCE`: 60 (acceptable color deviation)
- Number of frames: 100

**Test Configurations:**
- Default: 1280x720 @ 30fps
- Nightly: Additional arbitrary configurations

**Debug Mode:** Displays IR image with transformed view

**Category:** Image Quality, Color, Lab Testing

---

### 13.2 test-basic-depth.py

**Description:** Basic depth image quality validation using lab setup with depth targets

**Device Requirements:**
- `#test:device D400*`

**Test Setup:**
- Requires lab setup with ArUco markers (IDs 4,5,6,7)
- Cube at center of page (expected distance: 0.53m)
- Background at left edge (expected distance: 0.67m)

**Key Performance Indicators:**
- `FRAMES_PASS_THRESHOLD`: 0.8 (80% of frames must pass)
- `DEPTH_TOLERANCE`: 0.05m (50mm acceptable deviation)
- Number of frames: 100

**Test Configurations:**
- Default: 1280x720 @ 30fps
- Nightly: Additional arbitrary configurations

**Debug Mode:** Displays IR and depth images

**Test Status:** `#test:donotrun`

**Category:** Image Quality, Depth, Lab Testing

---

### 13.3 test-texture-mapping.py

**Description:** Texture mapping quality validation

**Device Requirements:**
- `#test:device D400*`

**Test Setup:**
- Requires lab setup with ArUco markers
- Validates texture mapping accuracy

**Key Performance Indicators:**
- `FRAMES_PASS_THRESHOLD`: 0.8 (80% of frames must pass)
- `COLOR_TOLERANCE`: 60
- `DEPTH_TOLERANCE`: 0.05m

**Test Status:** `#test:donotrun`

**Category:** Image Quality, Texture Mapping, Lab Testing

---

## 14. Intrinsics Tests (`intrinsics/`)

### 14.1 test-motion.py

**Description:** Motion sensor intrinsics validation

**Device Requirements:**
- `#test:device each(D400*)`
- `#test:device:!jetson each(D500*)`

**Test Description:** Check existence of motion intrinsic data in accel and gyro profiles

**Test Scenario:** Check intrinsics in motion sensor

**Category:** Intrinsics, IMU, Motion

---

## 15. Memory Tests (`memory/`)

### 15.1 test-extrinsics.cpp

**Description:** Extrinsics memory and performance testing (C++)

**Key Performance Indicators:**
- FPS: 6 Hz, 250 Hz
- Threshold: 4

**Test Status:** `#test:donotrun://`

**Timeout:** 480 seconds

**Language:** C++

**Category:** Memory, Extrinsics, Performance

---

### 15.2 test-sensor-option.cpp

**Description:** Sensor option memory testing (C++)

**Language:** C++

**Category:** Memory, Options

---

## 16. Metadata Tests (`metadata/`)

### 16.1 test-alive.py

**Description:** Metadata alive validation - verify increasing counters and timestamps

**Test Scenarios:**
1. Verifying increasing counter for profile
2. Verifying increasing time for profile
3. Verifying increasing sensor timestamp for profile
4. Verifying sensor timestamp is different than frame timestamp for profile

**Category:** Metadata, Validation

---

### 16.2 test-connection-type-found.py

**Description:** Connection type metadata detection

**Device Requirements:**
- `#test:device each(D400*)`
- `#test:device each(D500*)`

**Test Scenario:** Testing connection type can be detected

**Category:** Metadata, Connection Type

---

### 16.3 test-depth-unit.py

**Description:** Depth units metadata validation

**Device Requirements:**
- `#test:device D400*`
- `#test:device D500* !D555`

**Test Scenario:** Get metadata depth units value and make sure it's non-zero and equal to depth sensor matching option value

**Category:** Metadata, Depth Units

---

### 16.4 test-enabled.py

**Description:** Metadata enabled verification

**Device Requirements:**
- `#test:device each(D400*)`
- `#test:device each(D500*)`

**Test Scenario:** Checking metadata is enabled

**Test Priority:** `#test:priority 1`

**Category:** Metadata, Enabled Status

---

### 16.5 test-sync.py

**Description:** Metadata synchronization and frame drop detection

**Test Description:**
1. Detect frame drops using hardware frame counters
2. Check that timestamps of depth, infrared and color frames are consistent

**Key Performance Indicators:**
- `TS_TOLERANCE_MS`: 1.5ms (timestamp tolerance)

**Category:** Metadata, Synchronization, Frame Drops

---

### 16.6 test-usb-type-found.py

**Description:** USB type metadata detection

**Device Requirements:**
- `#test:device each(D400*)`
- `#test:device each(D500*)`

**Test Scenario:** Testing USB type can be detected

**Category:** Metadata, USB Type

---

## 17. Options Tests (`options/`)

### 17.1 test-advanced-mode.py

**Description:** Advanced mode options testing

**Key Performance Indicators:**
- Thresholds: 13, 23
- Max value: 200

**Test Status:** `#test:donotrun:!nightly` - Nightly run only

**Category:** Options, Advanced Mode

---

### 17.2 test-drops-on-set.py

**Description:** Frame drops testing when setting options

**Device Requirements:**
- `#test:device D400* !D457`

**Test Scenarios:**
1. Checking for frame drops when setting laser power several times
2. Checking frame drops when setting options on depth
3. Checking frame drops when setting options on color

**Test Status:** `#test:donotrun:!nightly` - Nightly run only

**Category:** Options, Frame Drops, Stability

---

### 17.3 test-options-watcher.py

**Description:** Options watcher functionality testing

**Test Status:** `#test:donotrun:!nightly` - Nightly run only

**Category:** Options, Watcher

---

### 17.4 test-out-of-range-throw.py

**Description:** Out of range options validation (negative test)

**Test Scenarios:**
- Below min value
- Above max value

**Category:** Options, Negative Testing, Range Validation

---

### 17.5 test-presets.py

**Description:** Presets functionality testing

**Key Performance Indicators:**
- Thresholds: 250

**Test Status:** `#test:donotrun:!nightly` - Nightly run only

**Category:** Options, Presets

---

### 17.6 test-rgb-options-metadata-consistency.py

**Description:** RGB options and metadata consistency validation

**Device Requirements:**
- `#test:device each(D400*) !D421 !D405`
- `#test:device each(D500*)`

**Key Performance Indicators:**
- FPS: 30 Hz

**Test Scenario:** Checking color options consistency with metadata

**Test Status:** `#test:donotrun:!nightly`

**Category:** Options, Metadata, Consistency, Color

---

### 17.7 test-set-gain-stress-test.py

**Description:** Stress test for setting PU (gain) option

**Test Status:** `#test:donotrun:!nightly`

**Timeout:** 600 seconds

**Category:** Options, Stress Testing, Gain

---

### 17.8 test-timestamp-domain.py

**Description:** Timestamp domain option testing

**Test Scenarios:**
1. Check setting global time domain: depth sensor - timestamp domain is OFF
2. Check setting global time domain: depth sensor - timestamp domain is ON
3. Check setting global time domain: color sensor - timestamp domain is OFF
4. Check setting global time domain: color sensor - timestamp domain is ON

**Category:** Options, Timestamp Domain

---

### 17.9 test-uvc-power-stress-test.py

**Description:** UVC power locking mechanism stress test

**Test Description:** Check locking mechanism on UVC devices (MIPI classes extend UVC). HWMC locks device and are also "invoke_power"ed. This test runs multiple HWMCs in parallel to the UVC interface to validate no deadlocks occur.

**Category:** Options, UVC, Power Management, Stress Testing

---

## 18. Record/Playback Tests (`rec-play/`)

### 18.1 test-got-playback-frames.py

**Description:** Record and playback frame validation

**Test Scenarios:**
1. Trying to record and playback using pipeline interface
2. Trying to record and playback using sensor interface
3. Trying to record and playback using sensor interface with syncer

**Category:** Record/Playback, Pipeline, Sensor

---

### 18.2 test-non-realtime.py

**Description:** Non-realtime playback testing

**Test Scenario:** Playback with non realtime isn't stuck at stop

**Note:** Running nightly as this tests specific bug fix in code that is seldom touched

**Category:** Record/Playback, Non-Realtime

---

### 18.3 test-pause-playback-frames.py

**Description:** Pause/resume playback functionality

**Device Requirements:**
- `#test:device D400* !D457`
- `#test:device D585S`

**Test Scenarios:**
1. Immediate pause & test
2. Immediate pause & delayed resume test
3. Delayed pause & delayed resume test
4. Multiple delay & pause test

**Key Performance Indicators:**
- Timeouts: 1s, 3s
- Duration: 3s

**Note:** Running nightly as this tests specific bug fix in code that is seldom touched

**Category:** Record/Playback, Pause/Resume

---

### 18.4 test-playback-stress.py

**Description:** Playback stress testing

**Key Performance Indicators:**
- Timeout: 15 seconds

**Test Status:** `#test:donotrun:!nightly`

**Timeout:** 1500 seconds

**Category:** Record/Playback, Stress Testing

---

### 18.5 test-record-and-stream.py

**Description:** Simultaneous record and stream testing

**Category:** Record/Playback, Streaming

---

### 18.6 test-record-software-device.py

**Description:** Software device recording

**Key Performance Indicators:**
- FPS: 60 Hz (depth/color)
- FPS: 200 Hz (IMU)

**Test Scenario:** Record software-device

**Category:** Record/Playback, Software Device

---

## 19. Streaming Tests (`streaming/`)

### 19.1 test-jpeg-compressed-format.py

**Description:** JPEG compressed format streaming validation

**Category:** Streaming, JPEG, Compression

---

### 19.2 test-y16-calibration-format.py

**Description:** Y16 calibration format streaming

**Device Requirements:**
- `#test:device each(D555)`

**Test Scenario:** Check that y16 is streaming

**Category:** Streaming, Y16, Calibration Format

---

## 20. Syncer Tests (`syncer/`)

### 20.1 test-throughput.cpp

**Description:** Syncer throughput testing (C++)

**Key Performance Indicators:**
- FPS: 30 Hz, 60 Hz

**Test Status:** `#test:donotrun://:!nightly`

**Language:** C++

**Category:** Syncer, Throughput, Performance

---

## 21. Root Level Tests

### 21.1 test-deadlock.cpp

**Description:** Deadlock detection test (C++)

**Test Status:** `#test:donotrun://:!nightly`

**Timeout:** 12 seconds

**Language:** C++

**Category:** Deadlock, Stability

---

### 21.2 test-profile-eq.cpp

**Description:** Profile equality testing (C++)

**Language:** C++

**Category:** Profiles, Equality

---

## 22. Tools Tests (`tools/`)

### 22.1 test-enumerate-devices.py

**Description:** Device enumeration runtime test

**Device Requirements:**
- `#test:device each(D400*)`
- `#test:device each(D500*)`

**Key Performance Indicators:**
- Threshold: 5
- Timeout: 10 seconds

**Test Scenario:** Run enumerate-devices runtime test

**Test Status:** `#test:donotrun:!nightly`

**Category:** Tools, Device Enumeration

---

## 23. Wrapper Tests (`wrappers/`)

### 23.1 test-rest-api-wrapper.py

**Description:** REST API wrapper testing

**Device Requirements:**
- `#test:device D455`

**Key Performance Indicators:**
- Timeout: 10 seconds

**Test Scenario:** Run test-rest-api-wrapper test

**Test Status:** `#test:donotrun:!linux`

**Category:** Wrappers, REST API

---

## Appendix A: Test Execution Flags

### Common Flags
- `#test:donotrun` - Test is disabled
- `#test:donotrun:!nightly` - Run only in nightly builds
- `#test:donotrun:!weekly` - Run only in weekly builds
- `#test:donotrun:!sync_test` - Requires special sync test setup
- `#test:donotrun:!dds` - Requires DDS environment
- `#test:donotrun:!jetson` - Skip on Jetson platforms
- `#test:donotrun:jetson` - Run only on Jetson platforms
- `#test:donotrun:!linux` - Skip on Linux
- `#test:timeout <seconds>` - Set test timeout
- `#test:retries <count>` - Number of retry attempts
- `#test:priority <number>` - Test execution priority (higher runs first)

### Device Specifications
- `#test:device D400*` - All D400 series devices
- `#test:device D500*` - All D500 series devices
- `#test:device each(D400*)` - Run separately for each D400 device
- `#test:device each(D500*)` - Run separately for each D500 device
- `#test:device D455` - Specific device
- `#test:device D585S` - D585S safety device
- `#test:device D555` - D555 DDS device
- `#test:device D400_CAM_SYNC` - Dual D400 camera sync setup
- `#test:device !D457` - Exclude D457
- `#test:device !D555` - Exclude D555
- `#test:device !D421 !D405` - Exclude multiple devices

---

## Appendix B: Key Performance Indicators Summary

### FPS Testing
- **Standard FPS rates tested:** 5, 6, 15, 30, 60, 90 Hz
- **FPS tolerance:** ±5% (most tests), ±10% (5 FPS tests)
- **IMU rates:** 100 Hz, 200 Hz, 250 Hz

### Latency/Timing
- **Time to first frame:** Varies by device and product line
- **Device creation timeout:** 10s (standard), 30s (DDS devices)
- **Frame gap threshold:** 110% of expected frame time
- **Timestamp drift threshold:** 20 µs/minute (MASTER-SLAVE)
- **Timestamp offset threshold:** 20 µs (MASTER-SLAVE), 100 µs (DEFAULT)

### Depth Accuracy
- **Depth tolerance:** 0.05m (50mm)
- **Health factor threshold:** 0.25 (good calibration), 1.5 (acceptable)

### Color Accuracy
- **Color tolerance:** 60 (RGB units)

### Pass/Fail Thresholds
- **Frame pass threshold:** 80% (most image quality tests)
- **Temperature tolerance:** 3.0°C

### Test Duration
- **Typical streaming test:** 10-30 seconds
- **Extended drift test:** 90 seconds
- **Calibration duration:** 5 seconds
- **Comprehensive FPS test:** Up to 4 hours

---

## Appendix C: Test Categories Matrix

| Category | Count | Primary Focus |
|----------|-------|---------------|
| Calibration | 4 | OCC, Tare, accuracy |
| Camera Sync | 3 | Multi-camera, timing |
| Configuration | 2 | Device hub, network |
| D400 Series | 12 | D400-specific features |
| D500 Series | 20 | D500-specific, safety |
| Debug Protocol | 2 | HWM commands, errors |
| DFU | 1 | Firmware compatibility |
| Extrinsics | 2 | Calibration consistency |
| Frames | 13 | FPS, timing, latency |
| Firmware | 4 | Error monitoring, logs |
| HDR | 3 | HDR configurations |
| Hardware Reset | 2 | Reset, enumeration |
| Image Quality | 3 | Depth/color accuracy |
| Intrinsics | 1 | IMU intrinsics |
| Memory | 2 | Memory management |
| Metadata | 6 | Metadata validation |
| Options | 9 | Option get/set, ranges |
| Record/Playback | 6 | Recording, playback |
| Streaming | 2 | Format streaming |
| Syncer | 1 | Throughput |
| Tools | 1 | Device enumeration |
| Wrappers | 1 | REST API |
| Root Level | 2 | Deadlock, profiles |

---

## Appendix D: Device Coverage Summary

### D400 Series Devices
- D400* (generic) - 25 tests
- D455 - 8 tests
- D457 - 5 tests (with exclusions)
- D405 - 3 tests
- D421 - 2 tests (with exclusions)
- D435i - 1 test

### D500 Series Devices
- D585S - 19 tests (safety focus)
- D555 - 4 tests (DDS focus)
- D500* (generic) - 7 tests

### Special Configurations
- D400_CAM_SYNC - 3 tests (dual-camera sync)
- MIPI devices - 1 test
- DDS devices - 2 tests

---

**End of Document**

*This document was automatically generated from test file analysis. For the most up-to-date information, refer to the individual test files.*
