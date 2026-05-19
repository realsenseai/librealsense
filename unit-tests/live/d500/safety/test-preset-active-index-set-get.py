# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

# Not frequently changing, no need to test for each commit
# test:donotrun:!nightly
#test:device D585S

import pyrealsense2 as rs
from rspy import test
import time

#############################################################################################
# Tests
#############################################################################################

test.start("Verify Safety Sensor Extension")
ctx = rs.context()
dev = ctx.query_devices()[0]
safety_sensor = dev.first_safety_sensor()
test.finish()

#############################################################################################

test.start("Check if safety sensor supports the option")
test.check_equal(safety_sensor.supports(rs.option.safety_preset_active_index), True)
test.finish()

#############################################################################################
test.start("Valid get/set scenario")

# default index at start should be 0
current_index = safety_sensor.get_option(rs.option.safety_preset_active_index)
test.check_equal( int(current_index), 0)

safety_sensor.set_option(rs.option.safety_preset_active_index, 1)

current_index = safety_sensor.get_option(rs.option.safety_preset_active_index)
test.check_equal( int(current_index), 1)

safety_sensor.set_option(rs.option.safety_preset_active_index, 20)

current_index = safety_sensor.get_option(rs.option.safety_preset_active_index)
test.check_equal( int(current_index), 20)

safety_sensor.set_option(rs.option.safety_preset_active_index, 63)

current_index = safety_sensor.get_option(rs.option.safety_preset_active_index)
test.check_equal( int(current_index), 63)

test.finish()

#############################################################################################

test.start("Invalid set - index out of range")
try:
    safety_sensor.set_option(rs.option.safety_preset_active_index, 64)
except:
    pass
else:
    test.unexpected_exception()
test.finish()

#############################################################################################

test.print_results_and_exit()
