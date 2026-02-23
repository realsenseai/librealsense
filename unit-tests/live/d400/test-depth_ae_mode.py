# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#test:device:jetson D457
#test:device:!jetson D455

import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy import test, log
from rspy import tests_wrapper as tw

device, _ = test.find_first_device_or_exit();
depth_sensor = device.first_depth_sensor()
fw_version = rsutils.version( device.get_info( rs.camera_info.firmware_version ))
tw.start_wrapper( device )

if fw_version < rsutils.version(5,15,0,0):
    log.i(f"FW version {fw_version} does not support DEPTH_AUTO_EXPOSURE_MODE option, skipping test...")
    test.print_results_and_exit()

REGULAR = 0.0
ACCELERATED = 1.0
################################################################################################

test.start("Verify camera AE mode default is REGULAR")
test.check_equal(depth_sensor.get_option(rs.option.auto_exposure_mode), REGULAR)
test.finish()

################################################################################################

test.start("Verify can set when auto exposure on")
depth_sensor.set_option(rs.option.enable_auto_exposure, True)
test.check_equal(bool(depth_sensor.get_option(rs.option.enable_auto_exposure)), True)
depth_sensor.set_option(rs.option.auto_exposure_mode, ACCELERATED)
test.check_equal(depth_sensor.get_option(rs.option.auto_exposure_mode), ACCELERATED)
depth_sensor.set_option(rs.option.auto_exposure_mode, REGULAR)
test.check_equal(depth_sensor.get_option(rs.option.auto_exposure_mode), REGULAR)
test.finish()

################################################################################################
test.start("Test Set during idle mode")
depth_sensor.set_option(rs.option.enable_auto_exposure, False)
test.check_equal(bool(depth_sensor.get_option(rs.option.enable_auto_exposure)), False)
depth_sensor.set_option(rs.option.auto_exposure_mode, ACCELERATED)
test.check_equal(depth_sensor.get_option(rs.option.auto_exposure_mode), ACCELERATED)
depth_sensor.set_option(rs.option.auto_exposure_mode,REGULAR)
test.check_equal(depth_sensor.get_option(rs.option.auto_exposure_mode), REGULAR)
test.finish()

################################################################################################

test.start("Test Set during streaming mode is not allowed")
# Reset option to REGULAR
depth_sensor.set_option(rs.option.enable_auto_exposure, False)
test.check_equal(bool(depth_sensor.get_option(rs.option.enable_auto_exposure)), False)
depth_sensor.set_option(rs.option.auto_exposure_mode, REGULAR)
test.check_equal(depth_sensor.get_option(rs.option.auto_exposure_mode), REGULAR)
# Start streaming
depth_profile = next(p for p in depth_sensor.profiles if p.stream_type() == rs.stream.depth)
depth_sensor.open(depth_profile)
depth_sensor.start(lambda x: None)
try:
    depth_sensor.set_option(rs.option.auto_exposure_mode,ACCELERATED)
    test.fail("Exception was expected while setting depth auto exposure mode during streaming depth sensor")
except:
    test.check_equal(depth_sensor.get_option(rs.option.auto_exposure_mode), REGULAR)

depth_sensor.stop()
depth_sensor.close()
test.finish()

################################################################################################
tw.stop_wrapper( device )
test.print_results_and_exit()
