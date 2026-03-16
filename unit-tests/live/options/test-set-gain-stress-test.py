# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

# Test multiple set_pu commands checking that the set control event polling works as expected.
# We expect no exception thrown -  See [RSDSO-17185]
# Moving bug check to weekly run frequency

# test:device each(D400*)
# test:device each(D500*)
# test:donotrun:!weekly
# test:timeout 600

import pyrealsense2 as rs
from rspy import test, log
import time
import datetime
from rspy import tests_wrapper as tw

test_iterations = 200
gain_values = [16,74,132,190,248]

dev, _ = test.find_first_device_or_exit()
time.sleep( 3 ) # The device starts at D0 (Operational) state, allow time for it to get into idle state
tw.start_wrapper( dev )

################################################################################################
test.start("Stress test for setting a PU (gain) option")

depth_ir_sensor = dev.first_depth_sensor()

# Test only devices that supports set gain option
if not depth_ir_sensor.supports(rs.option.gain):
    test.finish()
    tw.stop_wrapper( dev )
    test.print_results_and_exit()


for i in range(test_iterations):
    log.d("{} Iteration {} {}".format("=" * 50, i, "=" * 50))

    # Reset Control values
    log.d("Resetting Controls...")

    if (depth_ir_sensor.supports(rs.option.enable_auto_exposure)):
        depth_ir_sensor.set_option(rs.option.enable_auto_exposure, 0)
    if (depth_ir_sensor.supports(rs.option.exposure)):
        depth_ir_sensor.set_option(rs.option.exposure, 1)

        depth_ir_sensor.set_option(rs.option.gain, 248)

    log.d("Resetting Controls Done")

    time.sleep(0.1)
    for val in gain_values:
        log.d("Setting Gain To: {}".format(val))
        depth_ir_sensor.set_option(rs.option.gain, val)
        get_val = depth_ir_sensor.get_option(rs.option.gain)
        test.check(val == get_val)
        log.d("Gain Set To: {}".format(get_val))

test.finish()

################################################################################################
tw.stop_wrapper( dev )
test.print_results_and_exit()
