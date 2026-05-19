# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# test:device D400_CAM_SYNC

"""
RealSense Inter-Camera Sync Mode Test

Tests the inter_cam_sync_mode option for D400 cameras with hardware sync capability.
Validates that cameras can be configured in different synchronization modes:
- DEFAULT (0): No synchronization, independent operation
- MASTER (1): Camera generates sync signals for other cameras
- SLAVE (2): Camera follows sync signals from master
- FULL_SLAVE (3): Full slave mode with additional constraints

Requires: D400 camera with firmware >= 5.15.0.0
"""

import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy import test, log
from rspy import tests_wrapper as tw

# Initialize device and get depth sensor for testing
device, _ = test.find_first_device_or_exit();
depth_sensor = device.first_depth_sensor()
fw_version = rsutils.version( device.get_info( rs.camera_info.firmware_version ))
tw.start_wrapper( device )

# Inter-camera sync mode requires firmware 5.15.0.0 or later
if fw_version < rsutils.version(5,15,0,0):
    log.i(f"FW version {fw_version} does not support INTER_CAM_SYNC_MODE option, skipping test...")
    test.print_results_and_exit()

# Inter-camera sync mode option values
DEFAULT = 0.0      # No synchronization - camera operates independently
MASTER = 1.0       # Master mode - generates sync signals for slave cameras
SLAVE = 2.0        # Slave mode - follows master's sync signals
FULL_SLAVE = 3.0   # Full slave mode - additional sync constraints
################################################################################################
# Test 1: Verify default inter-camera sync mode
# After initialization, the camera should be in DEFAULT mode (no sync)
test.start("Verify camera inter-cam sync mode default is DEFAULT")
test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), DEFAULT)
test.finish()

################################################################################################
# Test 2: Verify MASTER mode can be set and reset
# MASTER mode configures the camera to generate sync signals
test.start("Verify can set to MASTER mode")
depth_sensor.set_option(rs.option.inter_cam_sync_mode, MASTER)
test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), MASTER)
depth_sensor.set_option(rs.option.inter_cam_sync_mode, DEFAULT)
test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), DEFAULT)
test.finish()

################################################################################################
# Test 3: Verify SLAVE mode can be set and reset
# SLAVE mode configures the camera to follow master's sync signals
test.start("Verify can set to SLAVE mode")
depth_sensor.set_option(rs.option.inter_cam_sync_mode, SLAVE)
test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), SLAVE)
depth_sensor.set_option(rs.option.inter_cam_sync_mode, DEFAULT)
test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), DEFAULT)
test.finish()

################################################################################################
# Test 4: Verify FULL_SLAVE mode can be set and reset
# FULL_SLAVE mode provides additional synchronization constraints beyond SLAVE mode
test.start("Verify can set to FULL_SLAVE mode")
depth_sensor.set_option(rs.option.inter_cam_sync_mode, FULL_SLAVE)
test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), FULL_SLAVE)
depth_sensor.set_option(rs.option.inter_cam_sync_mode, DEFAULT)
test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), DEFAULT)
test.finish()

################################################################################################
# Test 5: Verify multiple mode transitions during idle state
# Tests that all sync modes can be set sequentially when sensor is not streaming
test.start("Test Set during idle mode")
depth_sensor.set_option(rs.option.inter_cam_sync_mode, MASTER)
test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), MASTER)
depth_sensor.set_option(rs.option.inter_cam_sync_mode, SLAVE)
test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), SLAVE)
depth_sensor.set_option(rs.option.inter_cam_sync_mode, FULL_SLAVE)
test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), FULL_SLAVE)
depth_sensor.set_option(rs.option.inter_cam_sync_mode, DEFAULT)
test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), DEFAULT)
test.finish()

################################################################################################
# Test 6: Verify sync mode cannot be changed during streaming
# Inter-camera sync mode is read-only while the sensor is actively streaming
# This prevents mid-stream synchronization changes that could cause frame drops
test.start("Test Set during streaming mode is not allowed")
# Reset option to DEFAULT
depth_sensor.set_option(rs.option.inter_cam_sync_mode, DEFAULT)
test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), DEFAULT)
# Start streaming with a simple callback
depth_profile = next(p for p in depth_sensor.profiles if p.stream_type() == rs.stream.depth)
depth_sensor.open(depth_profile)
depth_sensor.start(lambda x: None)
# Attempt to change sync mode during streaming - should raise exception
try:
    depth_sensor.set_option(rs.option.inter_cam_sync_mode, MASTER)
    test.fail("Exception was expected while setting inter-cam sync mode during streaming depth sensor")
except:
    # Exception is expected - verify mode remains unchanged
    test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), DEFAULT)

# Stop streaming and cleanup
depth_sensor.stop()
depth_sensor.close()
test.finish()

################################################################################################
# Cleanup and exit
tw.stop_wrapper( device )
test.print_results_and_exit()
