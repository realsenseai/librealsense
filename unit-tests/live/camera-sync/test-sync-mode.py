# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
# Tracked-On: RSDSO-20709

# test:device D400_CAM_SYNC

import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy import test, log
from rspy import tests_wrapper as tw

device, _ = test.find_first_device_or_exit();
depth_sensor = device.first_depth_sensor()
fw_version = rsutils.version( device.get_info( rs.camera_info.firmware_version ))
tw.start_wrapper( device )

if fw_version < rsutils.version(5,15,0,0):
    log.i(f"FW version {fw_version} does not support INTER_CAM_SYNC_MODE option, skipping test...")
    test.print_results_and_exit()

DEFAULT = 0.0
MASTER = 1.0
SLAVE = 2.0
FULL_SLAVE = 3.0
################################################################################################

test.start("Verify camera inter-cam sync mode default is DEFAULT")
test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), DEFAULT)
test.finish()

################################################################################################

test.start("Verify can set to MASTER mode")
depth_sensor.set_option(rs.option.inter_cam_sync_mode, MASTER)
test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), MASTER)
depth_sensor.set_option(rs.option.inter_cam_sync_mode, DEFAULT)
test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), DEFAULT)
test.finish()

################################################################################################

test.start("Verify can set to SLAVE mode")
depth_sensor.set_option(rs.option.inter_cam_sync_mode, SLAVE)
test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), SLAVE)
depth_sensor.set_option(rs.option.inter_cam_sync_mode, DEFAULT)
test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), DEFAULT)
test.finish()

################################################################################################

test.start("Verify can set to FULL_SLAVE mode")
depth_sensor.set_option(rs.option.inter_cam_sync_mode, FULL_SLAVE)
test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), FULL_SLAVE)
depth_sensor.set_option(rs.option.inter_cam_sync_mode, DEFAULT)
test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), DEFAULT)
test.finish()

################################################################################################

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

test.start("Test Set during streaming mode is not allowed")
# Reset option to DEFAULT
depth_sensor.set_option(rs.option.inter_cam_sync_mode, DEFAULT)
test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), DEFAULT)
# Start streaming
depth_profile = next(p for p in depth_sensor.profiles if p.stream_type() == rs.stream.depth)
depth_sensor.open(depth_profile)
depth_sensor.start(lambda x: None)
try:
    depth_sensor.set_option(rs.option.inter_cam_sync_mode, MASTER)
    test.fail("Exception was expected while setting inter-cam sync mode during streaming depth sensor")
except:
    test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), DEFAULT)

depth_sensor.stop()
depth_sensor.close()
test.finish()

################################################################################################
tw.stop_wrapper( device )
test.print_results_and_exit()
