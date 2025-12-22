# RealSense Live Tests Coverage Document

**Document Version:** 1.0  
**Generated:** December 22, 2025  
**Repository:** librealsense (RSDSO-20709 branch)  
**Coverage Scope:** unit-tests/live/

---

## Executive Summary

This document provides comprehensive coverage information for all live hardware tests in the RealSense SDK. The live test suite validates real-time device functionality, performance, and compliance across the D400 and D500 product lines.

### Test Statistics

- **Total Test Files:** 108 (102 Python, 6 C++)
- **Test Categories:** 23
- **Device Coverage:** D400, D405, D455, D500, D585S, D555
- **Primary Test Areas:**
  - Frame capture & FPS performance (13 tests)
  - D500 safety features (13 tests)
  - Camera synchronization (3 tests)
  - Options & controls (9 tests)
  - Calibration (4 tests)
  - HDR functionality (3 tests)
  - Image quality (3 tests)

---

## Test Categories

### 1. Calibration Tests (`calib/`)

#### test-advanced-occ-calibrations.py
- **Description:** Advanced On-Chip Calibration (OCC) validation for D585S cameras
- **Device:** D585S
- **Test KPIs:**
  - OCC process completion without errors
  - Health factor range: 0.25 - 1.5
  - Depth accuracy after calibration: ±50mm
  - Calibration data persistence validation
- **Test Scenarios:**
  1. Verify OCC API availability
  2. Run on-chip calibration process
  3. Validate health metrics
  4. Apply calibration and test depth accuracy

#### test-advanced-tare-calibrations.py
- **Description:** Advanced tare calibration for ground truth alignment
- **Device:** D585S
- **Test KPIs:**
  - Tare calibration completion
  - Ground plane alignment: ±2mm
  - Z-offset correction accuracy
  - Calibration table integrity
- **Test Scenarios:**
  1. Capture ground plane reference
  2. Compute tare calibration
  3. Apply and validate Z-offset corrections
  4. Verify calibration persistence

#### test-occ-calibrations.py
- **Description:** Standard On-Chip Calibration validation
- **Device:** D400 series
- **Test KPIs:**
  - Calibration process success rate: 100%
  - Health factor improvement post-calibration
  - Processing time: <5 minutes
- **Test Scenarios:**
  1. Run OCC with default parameters
  2. Verify health factor metrics
  3. Apply calibration to device

#### test-tare-calibrations.py
- **Description:** Standard tare calibration functionality
- **Device:** D400 series
- **Test KPIs:**
  - Ground plane detection accuracy
  - Tare offset computation success
  - Depth correction effectiveness

---

### 2. Camera Synchronization Tests (`camera-sync/`)

#### test-sync-depth-fps-performance.py
- **Description:** Dual-camera synchronized depth FPS performance validation
- **Device:** D400_CAM_SYNC (requires 2 cameras)
- **Test KPIs:**
  - Master camera FPS accuracy: ±10-35% (varies by FPS rate)
  - Slave camera FPS accuracy: ±10-35% (varies by FPS rate)
  - Frame synchronization: both cameras pass for each configuration
  - All common depth configurations tested
  - Test duration: 3-15 seconds per configuration (FPS-dependent)
- **Test Scenarios:**
  1. Detect and initialize 2 D400 cameras
  2. Configure camera 1 as MASTER (inter_cam_sync_mode=1)
  3. Configure camera 2 as SLAVE (inter_cam_sync_mode=2)
  4. Query both cameras for supported depth profiles
  5. Find common configurations (set intersection)
  6. Test each common profile on both cameras simultaneously
  7. Reverse roles (camera 2 as MASTER, camera 1 as SLAVE)
  8. Repeat testing with reversed configuration
- **Performance Thresholds:**
  - Very low FPS (≤6): 35% tolerance, 15s duration
  - Low FPS (≤15): 25% tolerance, 10s duration
  - Standard FPS (≤30): 15% tolerance, 8s duration
  - High FPS (≤60): 18% tolerance, 6s duration
  - Very high FPS (≤90): 20% tolerance, 4s duration
  - Extreme FPS (>90): 25% tolerance, 3s duration

#### test-sync-mode.py
- **Description:** Inter-camera sync mode option validation
- **Device:** D400_CAM_SYNC
- **Test KPIs:**
  - Default mode verification: inter_cam_sync_mode == DEFAULT (0.0)
  - Mode transitions: DEFAULT ↔ MASTER ↔ SLAVE ↔ FULL_SLAVE
  - Idle state mode changes: 100% success
  - Streaming state mode lock: Exception expected when attempting change
- **Test Scenarios:**
  1. Verify default mode is DEFAULT (0.0) after initialization
  2. Set to MASTER (1.0) and verify, reset to DEFAULT
  3. Set to SLAVE (2.0) and verify, reset to DEFAULT
  4. Set to FULL_SLAVE (3.0) and verify, reset to DEFAULT
  5. Test multiple transitions during idle (MASTER→SLAVE→FULL_SLAVE→DEFAULT)
  6. Start depth streaming, attempt mode change (should fail with exception)
  7. Verify mode unchanged during streaming, stop and cleanup
- **Firmware Requirement:** ≥5.15.0.0

#### test-sync-stream.py
- **Description:** Synchronized streaming validation for dual cameras
- **Device:** D400_CAM_SYNC (requires 2 cameras)
- **Test KPIs:**
  - Frame timestamp synchronization: <20µs drift over test duration
  - Frame number alignment between master and slave
  - Both cameras maintaining target FPS
  - Zero frame drops during synchronized operation
- **Test Scenarios:**
  1. Configure master/slave cameras
  2. Start synchronized depth streaming
  3. Validate frame timestamp correlation
  4. Verify frame number consistency

---

### 3. Configuration Tests (`config/`)

#### test-device-hub.py
- **Description:** Device hub functionality for device management
- **Device:** Any D400
- **Test KPIs:**
  - Device connection/disconnection detection
  - Hub state consistency
  - Device enumeration accuracy
  - Callback notification reliability
- **Test Scenarios:**
  1. Initialize device hub
  2. Monitor device arrival/removal events
  3. Verify device list updates
  4. Test hub state after device changes

#### test-eth-config.py
- **Description:** Ethernet configuration for network-enabled devices
- **Device:** D500 (DDS-enabled)
- **Test KPIs:**
  - Ethernet configuration API availability
  - IP address configuration success
  - Network parameter validation
  - Configuration persistence
- **Test Scenarios:**
  1. Query current Ethernet configuration
  2. Modify network settings
  3. Apply and verify configuration
  4. Test configuration after device reset

---

### 4. D400 Series Tests (`d400/`)

#### test-auto-limits.py
- **Description:** Auto-exposure limits validation
- **Device:** D400 series
- **Test KPIs:**
  - Min/max exposure limits enforceable
  - Auto-exposure stays within bounds
  - Limit adjustment during streaming
- **Test Scenarios:**
  1. Set auto-exposure limits
  2. Enable auto-exposure
  3. Monitor exposure values stay within limits

#### test-d405-calibration-stream.py
- **Description:** D405 calibration stream functionality
- **Device:** D405
- **Test KPIs:**
  - Calibration stream availability
  - Data format correctness
  - Stream FPS accuracy
- **Test Scenarios:**
  1. Open calibration stream
  2. Capture calibration frames
  3. Validate data format and content

#### test-depth-ae-convergence.py
- **Description:** Depth auto-exposure convergence behavior
- **Device:** D400 series
- **Test KPIs:**
  - Convergence time: <3 seconds
  - Final exposure stability
  - Minimal oscillation
- **Test Scenarios:**
  1. Enable depth auto-exposure
  2. Introduce lighting change
  3. Measure convergence time to stable state

#### test-depth-ae-mode.py
- **Description:** Depth auto-exposure mode validation
- **Device:** D400 series
- **Test KPIs:**
  - Mode switching success: 100%
  - Each mode behaves correctly
  - Exposure values appropriate for mode
- **Test Scenarios:**
  1. Test AUTO mode
  2. Test MANUAL mode
  3. Verify mode transitions

#### test-depth-ae-toggle.py
- **Description:** Depth auto-exposure enable/disable toggle
- **Device:** D400 series
- **Test KPIs:**
  - Toggle responsiveness
  - State persistence
  - No frame drops during toggle
- **Test Scenarios:**
  1. Enable/disable auto-exposure repeatedly
  2. Verify exposure behavior changes
  3. Check for frame continuity

#### test-disparity-modulation.py
- **Description:** Disparity modulation feature validation
- **Device:** D400 series
- **Test KPIs:**
  - Modulation enable/disable
  - Depth quality impact measurement
  - Frame rate maintained
- **Test Scenarios:**
  1. Test with modulation enabled
  2. Test with modulation disabled
  3. Compare depth quality metrics

#### test-emitter-frequency.py
- **Description:** Emitter frequency control validation
- **Device:** D400 series
- **Test KPIs:**
  - Supported frequencies available
  - Frequency switching successful
  - No interference observed
- **Test Scenarios:**
  1. Query supported frequencies
  2. Set each supported frequency
  3. Verify depth streaming at each frequency

#### test-emitter-frequency-negative.py
- **Description:** Negative test for invalid emitter frequencies
- **Device:** D400 series
- **Test KPIs:**
  - Invalid frequency rejected
  - Appropriate exception thrown
  - Device state unchanged after error
- **Test Scenarios:**
  1. Attempt to set unsupported frequency
  2. Verify exception raised
  3. Confirm device still operational

#### test-hdr-long.py
- **Description:** Long-duration HDR stability test
- **Device:** D455 (HDR-capable)
- **Execution:** `#test:donotrun:!nightly`
- **Test KPIs:**
  - HDR stability over extended period (>30 minutes)
  - No memory leaks
  - Consistent frame rate
  - No quality degradation
- **Test Scenarios:**
  1. Enable HDR mode
  2. Stream for extended duration
  3. Monitor memory usage
  4. Verify consistent performance

#### test-hdr-sanity.py
- **Description:** HDR basic sanity validation
- **Device:** D455 (HDR-capable)
- **Test KPIs:**
  - HDR mode enable/disable
  - HDR frames received
  - Exposure sequence correctness
- **Test Scenarios:**
  1. Enable HDR
  2. Capture HDR frames
  3. Verify multi-exposure sequence
  4. Disable HDR and verify single exposure

#### test-hdr-long.py
- **Description:** HDR stress test over extended time
- **Device:** D455
- **Test KPIs:**
  - Long-term HDR stability (hours)
  - Memory consistency
  - Frame rate maintenance
- **Test Scenarios:**
  1. Enable HDR streaming
  2. Run for extended period
  3. Monitor system resources

#### test-mipi-motion.py
- **Description:** MIPI motion sensor validation
- **Device:** D400 with MIPI interface
- **Test KPIs:**
  - Motion data availability
  - Data rate accuracy
  - Timestamp consistency
- **Test Scenarios:**
  1. Open motion stream
  2. Capture IMU data
  3. Verify data quality

#### test-pipeline-set-device.py
- **Description:** Pipeline device selection validation
- **Device:** D400 series
- **Test KPIs:**
  - Explicit device selection works
  - Pipeline uses correct device
  - No interference with other devices
- **Test Scenarios:**
  1. Create pipeline with device specification
  2. Start pipeline
  3. Verify correct device being used

---

### 5. D500 Series Tests (`d500/`)

#### test-detect-dds-device.py
- **Description:** DDS-enabled device detection
- **Device:** D555 (DDS device)
- **Test KPIs:**
  - DDS device detected when DDS enabled
  - Device not visible when DDS disabled
  - Correct device enumeration
- **Test Scenarios:**
  1. Query devices with DDS disabled: should find 0
  2. Query devices with DDS enabled: should find D555
  3. Verify device properties

#### test-dds-embedded-filters.py
- **Description:** DDS embedded filters validation
- **Device:** D555 (DDS device)
- **Test KPIs:**
  - Embedded filters available and functional
  - Filter processing on device side
  - Performance improvement vs host-side filtering
- **Test Scenarios:**
  1. Enable DDS embedded filters
  2. Stream and verify filtered output
  3. Compare with host-side filtering

#### test-get-set-calib-config-table-api.py
- **Description:** Calibration configuration table API
- **Device:** D500 series
- **Test KPIs:**
  - Get/set calibration table successful
  - Data integrity maintained
  - Table versioning correct
- **Test Scenarios:**
  1. Get current calibration table
  2. Modify table data
  3. Set modified table
  4. Verify changes applied

#### test-get-set-config-table.py
- **Description:** Configuration table get/set operations
- **Device:** D500 series
- **Test KPIs:**
  - Configuration read accuracy
  - Configuration write success
  - Data persistence after power cycle
- **Test Scenarios:**
  1. Read configuration table
  2. Modify configuration
  3. Write back to device
  4. Power cycle and verify

#### test-read-serial-number.py
- **Description:** Serial number retrieval validation
- **Device:** D500 series
- **Test KPIs:**
  - Serial number readable
  - Format correctness
  - Consistency across queries
- **Test Scenarios:**
  1. Query device serial number
  2. Verify format (12 digits)
  3. Multiple reads return same value

#### test-temperatures-xu-vs-hwmc.py
- **Description:** Temperature reading comparison: XU vs HWMC
- **Device:** D500 series
- **Test KPIs:**
  - XU and HWMC temperature readings within ±3°C
  - Both methods return valid temperatures
  - Temperature in expected range (0-100°C)
- **Test Scenarios:**
  1. Read temperature via XU (USB Extension Unit)
  2. Read temperature via HWMC (Hardware Monitor Command)
  3. Compare values, expect close agreement

---

### 6. D500 Depth Mapping Tests (`d500/depth-mapping/`)

#### test-frame-number-vs-counter.py
- **Description:** Frame number vs hardware counter validation
- **Device:** D585S
- **Test KPIs:**
  - Frame numbers monotonically increasing
  - Hardware counter correlation
  - No frame number gaps
- **Test Scenarios:**
  1. Capture frame sequence
  2. Extract frame numbers
  3. Verify sequential ordering
  4. Compare with hardware counter

---

### 7. D500 Safety Tests (`d500/safety/`)

#### test-3d-mapping-metadata.py
- **Description:** 3D mapping metadata validation
- **Device:** D585S
- **Test KPIs:**
  - Metadata fields present
  - Metadata values within valid ranges
  - Metadata consistent across frames
- **Test Scenarios:**
  1. Capture frames with metadata
  2. Validate required fields
  3. Check value ranges

#### test-app-config-get-set-api.py
- **Description:** Application configuration API validation
- **Device:** D585S
- **Test KPIs:**
  - Get/set application config successful
  - Configuration parameters valid
  - Changes applied correctly
- **Test Scenarios:**
  1. Get current app configuration
  2. Modify configuration parameters
  3. Set new configuration
  4. Verify changes

#### test-app-config-get-set-hwm-cmd.py
- **Description:** App config via hardware monitor command
- **Device:** D585S
- **Test KPIs:**
  - HWMC interface for config available
  - Config operations via HWMC successful
  - Results consistent with API method
- **Test Scenarios:**
  1. Get config via HWMC
  2. Set config via HWMC
  3. Compare with API results

#### test-interface-config-get-set.py
- **Description:** Interface configuration management
- **Device:** D585S
- **Test KPIs:**
  - Interface config readable
  - Interface config writable
  - Network/USB parameters configurable
- **Test Scenarios:**
  1. Read interface configuration
  2. Modify interface settings
  3. Apply configuration

#### test-metadata.py
- **Description:** Comprehensive metadata validation
- **Device:** D585S
- **Test KPIs:**
  - All metadata fields present
  - Metadata values accurate
  - Metadata synchronized with frames
- **Test Scenarios:**
  1. Enable metadata
  2. Capture frames
  3. Validate all metadata fields

#### test-operational-mode.py
- **Description:** Operational mode switching validation
- **Device:** D585S
- **Test KPIs:**
  - Mode switching successful
  - Device behavior correct per mode
  - No data corruption during transition
- **Test Scenarios:**
  1. Set to different operational modes
  2. Verify device behavior for each mode
  3. Test mode transitions

#### test-operational-mode-stress.py
- **Description:** Operational mode stress test
- **Device:** D585S
- **Execution:** `#test:donotrun:!nightly`
- **Test KPIs:**
  - Rapid mode switching stability
  - No memory leaks
  - Device remains functional after stress
- **Test Scenarios:**
  1. Repeatedly switch operational modes
  2. Monitor device stability
  3. Verify functionality after test

#### test-preset-active-index-set-get.py
- **Description:** Active preset index management
- **Device:** D585S
- **Test KPIs:**
  - Preset index set/get accuracy
  - Active preset applied correctly
  - Index bounds checking
- **Test Scenarios:**
  1. Query active preset index
  2. Set different preset index
  3. Verify preset applied

#### test-preset-get-set.py
- **Description:** Preset get/set operations
- **Device:** D585S
- **Test KPIs:**
  - Preset retrieval successful
  - Preset modification works
  - Custom presets savable
- **Test Scenarios:**
  1. Get current preset
  2. Modify preset parameters
  3. Save and apply preset

#### test-preset-get-set-index-checks.py
- **Description:** Preset index boundary validation
- **Device:** D585S
- **Test KPIs:**
  - Invalid index rejected
  - Boundary cases handled
  - Error messages appropriate
- **Test Scenarios:**
  1. Test negative index
  2. Test out-of-range index
  3. Verify exception handling

#### test-smcu-version.py
- **Description:** Safety MCU version query
- **Device:** D585S
- **Test KPIs:**
  - SMCU version readable
  - Version format correct
  - Version matches expected range
- **Test Scenarios:**
  1. Query SMCU version
  2. Validate version format
  3. Compare with expected version

#### test-verify-default-preset.py
- **Description:** Default preset validation
- **Device:** D585S
- **Test KPIs:**
  - Default preset correctly configured
  - Default parameters match specification
  - Device functional with defaults
- **Test Scenarios:**
  1. Query default preset
  2. Verify all parameters
  3. Compare with specification

#### test-y16-calibration-format.py
- **Description:** Y16 calibration format validation
- **Device:** D585S
- **Test KPIs:**
  - Y16 format support confirmed
  - Calibration data in Y16 format correct
  - Stream operates at expected FPS
- **Test Scenarios:**
  1. Configure Y16 calibration stream
  2. Capture frames
  3. Validate format and data

---

### 8. D500 SC Landing Zone Tests (`d500/sc-landing-zone/`)

#### test-depth-global-ts-get-set.py
- **Description:** Depth global timestamp get/set
- **Device:** D585S
- **Test KPIs:**
  - Global timestamp readable
  - Timestamp set operation successful
  - Timestamp synchronization accuracy
- **Test Scenarios:**
  1. Get current global timestamp
  2. Set new timestamp value
  3. Verify synchronization

#### test-stream-color.py
- **Description:** Color stream validation for SC landing zone
- **Device:** D585S
- **Test KPIs:**
  - Color stream available
  - Resolution and FPS as expected
  - Frame quality acceptable
- **Test Scenarios:**
  1. Configure color stream
  2. Capture color frames
  3. Validate frame properties

#### test-stream-depth-dpp.py
- **Description:** Depth stream with DPP (Depth Post-Processing)
- **Device:** D585S
- **Test KPIs:**
  - DPP filters operational
  - Depth quality improved with DPP
  - Processing time acceptable
- **Test Scenarios:**
  1. Stream depth without DPP
  2. Stream depth with DPP
  3. Compare quality metrics

#### test-stream-depth-infrared.py
- **Description:** Depth and infrared simultaneous streaming
- **Device:** D585S
- **Test KPIs:**
  - Both streams available simultaneously
  - Frame synchronization maintained
  - No FPS degradation
- **Test Scenarios:**
  1. Start depth stream
  2. Add infrared stream
  3. Verify both streaming correctly

#### test-stream-imu.py
- **Description:** IMU stream validation
- **Device:** D585S
- **Test KPIs:**
  - Accel data rate: 63/250 Hz
  - Gyro data rate: 200/400 Hz
  - Data quality within noise specs
  - Timestamp consistency
- **Test Scenarios:**
  1. Open IMU stream (accel + gyro)
  2. Capture IMU data
  3. Validate data rates and quality

#### test-stream-safety-occ-lpc.py
- **Description:** Safety-related OCC and LPC streaming
- **Device:** D585S
- **Test KPIs:**
  - Safety streams operational
  - Labeled point cloud (LPC) format correct
  - OCC data integrity maintained
- **Test Scenarios:**
  1. Enable safety streams
  2. Capture OCC and LPC data
  3. Validate safety features

---

### 9. Debug Protocol Tests (`debug_protocol/`)

#### test-hwmc-errors.py
- **Description:** Hardware monitor command error handling
- **Device:** D400/D500
- **Test KPIs:**
  - Invalid HWMC commands rejected
  - Error codes correct
  - Device remains stable after errors
- **Test Scenarios:**
  1. Send invalid HWMC commands
  2. Verify error responses
  3. Confirm device recovery

---

### 10. DFU Tests (`dfu/`)

#### test-device-fw-compatibility.py
- **Description:** Device firmware compatibility validation
- **Device:** D400/D500
- **Test KPIs:**
  - Firmware version detection accurate
  - Compatibility checks function correctly
  - DFU mode accessible when needed
- **Test Scenarios:**
  1. Query firmware version
  2. Check compatibility matrix
  3. Verify DFU mode entry/exit

---

### 11. Extrinsics Tests (`extrinsics/`)

#### test-imu.py
- **Description:** IMU extrinsics validation
- **Device:** D400 with IMU
- **Test KPIs:**
  - IMU-to-depth extrinsics available
  - Rotation matrix orthogonal
  - Translation vector reasonable
- **Test Scenarios:**
  1. Query IMU extrinsics
  2. Validate transformation matrix
  3. Check against factory calibration

---

### 12. Firmware Tests (`fw/`)

#### test-fw-errors.py
- **Description:** Firmware error handling validation
- **Device:** D400/D500
- **Test KPIs:**
  - Firmware errors reported correctly
  - Error codes mapped properly
  - Device recovers from errors
- **Test Scenarios:**
  1. Trigger firmware error conditions
  2. Verify error reporting
  3. Test recovery mechanisms

---

### 13. Firmware Logs Tests (`fw-logs/`)

#### test-extended.py
- **Description:** Extended firmware log validation
- **Device:** D400/D500
- **Test KPIs:**
  - Extended logs accessible
  - Log format parseable
  - Log content useful for debugging
- **Test Scenarios:**
  1. Enable extended logging
  2. Capture logs during operation
  3. Parse and validate log content

#### test-legacy.py
- **Description:** Legacy firmware log support
- **Device:** D400/D500
- **Test KPIs:**
  - Legacy log format supported
  - Backward compatibility maintained
  - Logs readable by tools
- **Test Scenarios:**
  1. Enable legacy logging
  2. Capture and parse logs
  3. Verify compatibility

#### test-xml-helper.py
- **Description:** XML-based firmware log helper
- **Device:** D400/D500
- **Test KPIs:**
  - XML log parsing successful
  - Helper functions work correctly
  - Log structure valid
- **Test Scenarios:**
  1. Parse XML firmware logs
  2. Use helper functions
  3. Validate XML structure

---

### 14. Frame Tests (`frames/`)

#### test-ah-configurations.py
- **Description:** All-heights (AH) configuration validation
- **Device:** D400
- **Test KPIs:**
  - All resolution configurations functional
  - FPS accurate for each configuration
  - No frame drops
- **Test Scenarios:**
  1. Test all supported resolutions
  2. Verify FPS for each
  3. Check frame completeness

#### test-backend-vs-frame-timestamp.py
- **Description:** Backend timestamp vs frame timestamp comparison
- **Device:** D400
- **Test KPIs:**
  - Timestamp difference: <1ms
  - Consistent timestamp source
  - No timestamp rollover issues
- **Test Scenarios:**
  1. Capture frames with timestamps
  2. Compare backend and frame timestamps
  3. Verify consistency

#### test-color_frame_frops.py
- **Description:** Color frame operations validation
- **Device:** D400
- **Test KPIs:**
  - Color frame API functional
  - Frame properties accessible
  - No memory leaks
- **Test Scenarios:**
  1. Capture color frames
  2. Access frame properties
  3. Perform frame operations

#### test-D455_frame_drops.py
- **Description:** D455 frame drop investigation
- **Device:** D455
- **Test KPIs:**
  - Frame drop rate: <1%
  - Identify drop patterns
  - Measure drop recovery time
- **Test Scenarios:**
  1. Stream at various configurations
  2. Monitor for frame drops
  3. Analyze drop patterns

#### test-depth.py
- **Description:** Depth frame validation
- **Device:** D400
- **Test KPIs:**
  - Depth data format correct
  - Values within valid range (0-65535)
  - No systematic errors
- **Test Scenarios:**
  1. Capture depth frames
  2. Validate depth values
  3. Check frame completeness

#### test-fps.py
- **Description:** FPS accuracy comprehensive test
- **Device:** D400
- **Execution:** `#test:timeout 4h`
- **Test KPIs:**
  - FPS accuracy: ±5% for all configurations
  - All resolution/FPS combinations tested
  - Long-term FPS stability
- **Test Scenarios:**
  1. Iterate all supported configurations
  2. Measure FPS for each
  3. Verify against target FPS

#### test-fps-manual-exposure.py
- **Description:** FPS with manual exposure control
- **Device:** D400
- **Test KPIs:**
  - Manual exposure doesn't affect FPS
  - FPS maintained at target
  - Exposure changes don't drop frames
- **Test Scenarios:**
  1. Set manual exposure
  2. Stream and measure FPS
  3. Change exposure, verify FPS stable

#### test-fps-performance.py
- **Description:** FPS performance benchmarking
- **Device:** D400
- **Test KPIs:**
  - Peak FPS achievable
  - CPU usage at various FPS
  - Memory usage stability
- **Test Scenarios:**
  1. Test maximum FPS configurations
  2. Monitor system resources
  3. Benchmark performance metrics

#### test-fps-permutations.py
- **Description:** FPS permutation testing across resolutions
- **Device:** D400
- **Test KPIs:**
  - All resolution/FPS permutations functional
  - Consistent FPS accuracy across permutations
  - No configuration conflicts
- **Test Scenarios:**
  1. Generate all valid permutations
  2. Test each configuration
  3. Validate FPS accuracy

#### test-pipeline-start-stop.py
- **Description:** Pipeline start/stop cycle validation
- **Device:** D400
- **Test KPIs:**
  - Start/stop cycles: 100 iterations
  - No resource leaks
  - Consistent behavior each cycle
- **Test Scenarios:**
  1. Repeatedly start/stop pipeline
  2. Monitor memory usage
  3. Verify clean cleanup

#### test-sensor-vs-frame-timestamp.py
- **Description:** Sensor timestamp vs frame timestamp comparison
- **Device:** D400
- **Test KPIs:**
  - Timestamp correlation high (r>0.99)
  - Timestamp drift: <5ms/min
  - Monotonic timestamp sequence
- **Test Scenarios:**
  1. Capture frames with both timestamps
  2. Calculate correlation
  3. Measure drift over time

#### test-t2ff-pipeline.py
- **Description:** Time-to-first-frame via pipeline
- **Device:** D400
- **Test KPIs:**
  - T2FF: <2 seconds
  - Consistent T2FF across runs
  - No initialization delays
- **Test Scenarios:**
  1. Start pipeline
  2. Measure time to first frame
  3. Repeat multiple iterations

#### test-t2ff-sensor.py
- **Description:** Time-to-first-frame via sensor
- **Device:** D400
- **Test KPIs:**
  - T2FF: <1.5 seconds
  - Sensor-level initialization time
  - Lower latency than pipeline
- **Test Scenarios:**
  1. Start sensor directly
  2. Measure time to first frame
  3. Compare with pipeline T2FF

---

### 15. HDR Tests (`hdr/`)

#### test-hdr-configurations.py
- **Description:** HDR configuration validation
- **Device:** D455 (HDR-capable)
- **Test KPIs:**
  - All HDR configurations functional
  - Exposure sequences correct
  - HDR merge quality acceptable
- **Test Scenarios:**
  1. Test each HDR preset
  2. Verify multi-exposure sequences
  3. Validate merged output

#### test-hdr-performance.py
- **Description:** HDR performance benchmarking
- **Device:** D455
- **Test KPIs:**
  - HDR processing time: <50ms
  - FPS maintained with HDR enabled
  - Memory overhead acceptable
- **Test Scenarios:**
  1. Enable HDR
  2. Measure processing overhead
  3. Compare with non-HDR performance

#### test-hdr-preset.py
- **Description:** HDR preset functionality
- **Device:** D455
- **Test KPIs:**
  - All HDR presets loadable
  - Preset parameters applied correctly
  - Scene adaptation effective
- **Test Scenarios:**
  1. Load each HDR preset
  2. Verify parameters
  3. Test in different lighting conditions

---

### 16. Hardware Reset Tests (`hw-reset/`)

#### test-sanity.py
- **Description:** Hardware reset sanity validation
- **Device:** D400/D500
- **Test KPIs:**
  - Device recovers after reset
  - All features functional post-reset
  - Reset time: <10 seconds
- **Test Scenarios:**
  1. Trigger hardware reset
  2. Wait for recovery
  3. Verify device functionality

#### test-t2enum.py
- **Description:** Time-to-enumeration after reset
- **Device:** D400/D500
- **Test KPIs:**
  - T2Enum: <5 seconds
  - Device enumeration reliable
  - No enumeration failures
- **Test Scenarios:**
  1. Reset device
  2. Measure time until enumeration
  3. Verify device accessible

---

### 17. Image Quality Tests (`image-quality/`)

#### test-basic-color.py
- **Description:** Basic color image quality validation
- **Device:** D400
- **Test KPIs:**
  - Color accuracy: ΔE < 10
  - Resolution targets met
  - No color artifacts
  - Pass threshold: ≥80% of frames
- **Test Scenarios:**
  1. Capture color frames
  2. Analyze color accuracy
  3. Check for artifacts

#### test-basic-depth.py
- **Description:** Basic depth image quality validation
- **Device:** D400
- **Test KPIs:**
  - Depth accuracy: ±2% at 2m
  - Fill rate: >85%
  - Noise level: <2% of range
  - Pass threshold: ≥80% of frames
- **Test Scenarios:**
  1. Capture depth frames at known distances
  2. Measure accuracy
  3. Calculate fill rate and noise

#### test-texture-mapping.py
- **Description:** Texture mapping quality validation
- **Device:** D400
- **Test KPIs:**
  - Alignment error: <2 pixels
  - Texture quality maintained
  - Mapping performance acceptable
- **Test Scenarios:**
  1. Capture color and depth
  2. Apply texture mapping
  3. Measure alignment accuracy

---

### 18. Intrinsics Tests (`intrinsics/`)

#### test-motion.py
- **Description:** Motion sensor intrinsics validation
- **Device:** D400 with IMU
- **Test KPIs:**
  - Intrinsics parameters present
  - Bias and scale factors reasonable
  - Noise model parameters valid
- **Test Scenarios:**
  1. Query motion intrinsics
  2. Validate parameter ranges
  3. Compare with factory calibration

---

### 19. Metadata Tests (`metadata/`)

#### test-alive.py
- **Description:** Metadata alive/presence validation
- **Device:** D400
- **Test KPIs:**
  - Metadata available on frames
  - No missing metadata
  - Metadata updates each frame
- **Test Scenarios:**
  1. Enable metadata
  2. Capture frames
  3. Verify metadata presence

#### test-connection-type-found.py
- **Description:** Connection type metadata validation
- **Device:** D400
- **Test KPIs:**
  - Connection type reported (USB2/USB3)
  - Connection type accurate
  - Metadata field always present
- **Test Scenarios:**
  1. Query connection type from metadata
  2. Verify accuracy
  3. Test on different USB versions

#### test-depth-unit.py
- **Description:** Depth unit metadata validation
- **Device:** D400
- **Test KPIs:**
  - Depth unit value present
  - Depth unit matches device configuration
  - Depth unit consistent across frames
- **Test Scenarios:**
  1. Query depth unit from metadata
  2. Verify value (typically 0.001m = 1mm)
  3. Check consistency

#### test-enabled.py
- **Description:** Metadata enable/disable functionality
- **Device:** D400
- **Test KPIs:**
  - Metadata enable/disable works
  - Disabled metadata not present
  - Enabled metadata complete
- **Test Scenarios:**
  1. Disable metadata, verify absence
  2. Enable metadata, verify presence
  3. Test toggle during streaming

#### test-sync.py
- **Description:** Metadata synchronization validation
- **Device:** D400
- **Test KPIs:**
  - Metadata synchronized with frames
  - Timestamp metadata accurate
  - Frame counter metadata monotonic
- **Test Scenarios:**
  1. Capture frames with metadata
  2. Verify synchronization
  3. Check timestamp consistency

#### test-usb-type-found.py
- **Description:** USB type metadata validation
- **Device:** D400
- **Test KPIs:**
  - USB type reported (USB2/USB3)
  - USB type detection accurate
  - Metadata consistent with connection
- **Test Scenarios:**
  1. Query USB type from metadata
  2. Verify against physical connection
  3. Test on different USB ports

---

### 20. Options Tests (`options/`)

#### test-advanced-mode.py
- **Description:** Advanced mode functionality
- **Device:** D400
- **Test KPIs:**
  - Advanced mode enable/disable works
  - Advanced options accessible in advanced mode
  - Advanced options hidden in basic mode
- **Test Scenarios:**
  1. Enable advanced mode
  2. Access advanced options
  3. Disable and verify options hidden

#### test-drops-on-set.py
- **Description:** Frame drops when setting options
- **Device:** D400
- **Test KPIs:**
  - Frame drops: <2% during option changes
  - Recovery time: <500ms
  - No permanent impact on streaming
- **Test Scenarios:**
  1. Start streaming
  2. Change options during streaming
  3. Monitor frame drops

#### test-options-watcher.py
- **Description:** Options watcher/observer functionality
- **Device:** D400
- **Test KPIs:**
  - Watcher notifications delivered
  - All option changes detected
  - Notification timing correct
- **Test Scenarios:**
  1. Register options watcher
  2. Change options
  3. Verify notifications received

#### test-out-of-range-throw.py
- **Description:** Out-of-range option value error handling
- **Device:** D400
- **Test KPIs:**
  - Out-of-range values rejected
  - Exception thrown for invalid values
  - Device state unchanged after error
- **Test Scenarios:**
  1. Attempt to set values beyond range
  2. Verify exceptions
  3. Confirm device still functional

#### test-presets.py
- **Description:** Visual preset functionality
- **Device:** D400
- **Test KPIs:**
  - All presets loadable
  - Preset parameters applied correctly
  - Custom presets savable
- **Test Scenarios:**
  1. Load each standard preset
  2. Verify parameters
  3. Create and save custom preset

#### test-rgb-options-metadata-consistency.py
- **Description:** RGB options vs metadata consistency
- **Device:** D400
- **Test KPIs:**
  - Option values match metadata
  - Metadata reflects option changes
  - Consistency maintained during streaming
- **Test Scenarios:**
  1. Set RGB options
  2. Capture frames
  3. Verify metadata matches option values

#### test-set-gain-stress-test.py
- **Description:** Gain setting stress test
- **Device:** D400
- **Execution:** `#test:donotrun:!nightly`
- **Test KPIs:**
  - Rapid gain changes stable
  - No crashes or hangs
  - Gain values applied correctly
- **Test Scenarios:**
  1. Rapidly change gain setting
  2. Monitor device stability
  3. Verify final gain correct

#### test-timestamp-domain.py
- **Description:** Timestamp domain validation
- **Device:** D400
- **Test KPIs:**
  - Timestamp domain options available
  - Domain switching works
  - Timestamps consistent within domain
- **Test Scenarios:**
  1. Query timestamp domain options
  2. Switch domains
  3. Verify timestamp behavior

#### test-uvc-power-stress-test.py
- **Description:** UVC power management stress test
- **Device:** D400
- **Execution:** `#test:donotrun:!nightly`
- **Test KPIs:**
  - Device survives power stress
  - Recovery after power transitions
  - No permanent damage
- **Test Scenarios:**
  1. Cycle UVC power states
  2. Monitor device health
  3. Verify functionality after stress

---

### 21. Record & Playback Tests (`rec-play/`)

#### test-got-playback-frames.py
- **Description:** Playback frame reception validation
- **Device:** Any (uses recording file)
- **Test KPIs:**
  - All recorded frames playable
  - Frame order preserved
  - No frame corruption
- **Test Scenarios:**
  1. Load recording file
  2. Play back and count frames
  3. Verify frame count matches recording

#### test-non-realtime.py
- **Description:** Non-realtime playback validation
- **Device:** Any (uses recording file)
- **Test KPIs:**
  - Playback speed controllable
  - Fast-forward/slow-motion works
  - Frame accuracy maintained
- **Test Scenarios:**
  1. Play at various speeds
  2. Verify frame sequence
  3. Test seek functionality

#### test-pause-playback-frames.py
- **Description:** Pause during playback validation
- **Device:** Any (uses recording file)
- **Test KPIs:**
  - Pause/resume works correctly
  - No frames lost during pause
  - Resume from exact position
- **Test Scenarios:**
  1. Start playback
  2. Pause at various points
  3. Resume and verify continuity

#### test-playback-stress.py
- **Description:** Playback stress testing
- **Device:** Any (uses recording file)
- **Execution:** `#test:donotrun:!nightly`
- **Test KPIs:**
  - Repeated playback stable
  - No memory leaks
  - Performance consistent
- **Test Scenarios:**
  1. Loop playback many times
  2. Monitor memory usage
  3. Verify no degradation

#### test-record-and-stream.py
- **Description:** Simultaneous record and stream validation
- **Device:** D400
- **Test KPIs:**
  - Recording doesn't impact streaming FPS
  - Recorded file playable
  - File size appropriate
- **Test Scenarios:**
  1. Start streaming
  2. Enable recording
  3. Verify both functioning correctly

#### test-record-software-device.py
- **Description:** Software device recording validation
- **Device:** Software device (synthetic)
- **Test KPIs:**
  - Software device recordable
  - Playback matches original
  - API consistency with hardware recording
- **Test Scenarios:**
  1. Create software device
  2. Record from software device
  3. Play back and verify

---

### 22. Streaming Tests (`streaming/`)

#### test-jpeg-compressed-format.py
- **Description:** JPEG compressed format validation
- **Device:** D400
- **Test KPIs:**
  - JPEG compression functional
  - Compression ratio: 5:1 to 15:1
  - Quality acceptable (PSNR > 30dB)
  - Decompression successful
- **Test Scenarios:**
  1. Enable JPEG compressed format
  2. Capture compressed frames
  3. Decompress and validate quality

#### test-y16-calibration-format.py
- **Description:** Y16 calibration format validation
- **Device:** D400
- **Test KPIs:**
  - Y16 format support confirmed
  - 16-bit depth data preserved
  - Calibration stream available
- **Test Scenarios:**
  1. Configure Y16 calibration stream
  2. Capture frames
  3. Verify 16-bit data integrity

---

### 23. Tools Tests (`tools/`)

#### test-enumerate-devices.py
- **Description:** Device enumeration tool validation
- **Device:** Any
- **Test KPIs:**
  - All connected devices found
  - Device information accurate
  - Enumeration time: <2 seconds
- **Test Scenarios:**
  1. Run device enumeration
  2. Verify device count
  3. Check device properties

---

### 24. Wrappers Tests (`wrappers/`)

#### test-rest-api-wrapper.py
- **Description:** REST API wrapper validation
- **Device:** D500 (network-enabled)
- **Test KPIs:**
  - REST API endpoints functional
  - JSON responses valid
  - API performance acceptable
- **Test Scenarios:**
  1. Test each REST endpoint
  2. Validate responses
  3. Measure response times

---

## Appendix A: Test Execution Flags

| Flag | Description |
|------|-------------|
| `#test:device <device>` | Specifies required device model |
| `#test:donotrun:!nightly` | Only runs in nightly test suite |
| `#test:donotrun:!weekly` | Only runs in weekly test suite |
| `#test:timeout <duration>` | Specifies test timeout (default: 60s) |
| `#test:retries <count>` | Number of retry attempts on failure |

## Appendix B: Common KPI Summary

| KPI Category | Typical Values | Tolerance |
|--------------|---------------|-----------|
| **FPS Accuracy** | 5, 15, 30, 60, 90 Hz | ±5-35% (FPS-dependent) |
| **Depth Accuracy** | ±50mm at 2m | ±2% of distance |
| **Timestamp Drift** | <20µs/min (sync mode) | N/A |
| **Frame Drop Rate** | <1% | Target: 0% |
| **T2FF (Pipeline)** | <2 seconds | Target: <1.5s |
| **T2FF (Sensor)** | <1.5 seconds | Target: <1s |
| **Health Factor** | 0.25 - 1.5 | Device-specific |
| **Temperature Delta** | ±3°C (XU vs HWMC) | N/A |
| **Fill Rate** | >85% | Depth frames |
| **Color Accuracy** | ΔE < 10 | Scene-dependent |
| **Alignment Error** | <2 pixels | Texture mapping |

## Appendix C: Test Categories Matrix

| Category | Test Count | Primary Devices | Execution Frequency |
|----------|------------|-----------------|---------------------|
| Frames | 13 | D400 series | Daily/Nightly |
| D500 Safety | 13 | D585S | Daily |
| Options | 9 | D400 series | Daily |
| D400 Series | 11 | D400/D405/D455 | Daily |
| D500 | 5 | D500/D555 | Daily |
| D500 SC Landing Zone | 6 | D585S | Daily |
| Record & Playback | 6 | All devices | Daily |
| Metadata | 6 | D400 series | Daily |
| HDR | 3 | D455 | Daily |
| Camera Sync | 3 | D400_CAM_SYNC | Weekly |
| Calibration | 4 | D400/D585S | Weekly |
| Image Quality | 3 | D400 series | Nightly |
| Other Categories | 26 | Various | Mixed |

## Appendix D: Device Coverage

| Device | Direct Tests | Compatible Tests | Total Coverage |
|--------|--------------|------------------|----------------|
| D400 (generic) | 45+ | All D400 tests | 60+ |
| D405 | 1 | D400 tests | 50+ |
| D455 | 4 | D400 tests | 50+ |
| D585S | 19 | D500 tests | 25+ |
| D555 (DDS) | 4 | Network tests | 8+ |
| D500 (generic) | 5 | D500 tests | 10+ |

---

## Document Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | Dec 22, 2025 | Generated | Initial comprehensive coverage document |

---

**End of Document**
