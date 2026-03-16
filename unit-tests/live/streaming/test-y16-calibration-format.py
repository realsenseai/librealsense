# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

# test:device D400*
# Y16 is not streamed with metadata over GMSL causing all frames to drop
# test:type !GMSL
# test:device each(D555)
# This test checks streaming y16 profile

import time
import pyrealsense2 as rs
from rspy import test, log
from rspy.timer import Timer


y16_streamed = False


def close_resources(sensor):
    """
    Stop and Close sensor.
    :sensor: sensor of device
    """
    if len(sensor.get_active_streams()) > 0:
        sensor.stop()
        sensor.close()


def frame_callback(frame):
    global y16_streamed
    y16_streamed = True


timer = Timer(5)

device, _ = test.find_first_device_or_exit()
depth_sensor = device.first_depth_sensor()

test.start('Check that y16 is streaming:')

profile_y16 = next(p for p in depth_sensor.profiles if p.format() == rs.format.y16)
test.check(profile_y16)
log.d(str(profile_y16))

if profile_y16:
    depth_sensor.open(profile_y16)
    depth_sensor.start(frame_callback)

    timer.start()
    while not timer.has_expired():
        if y16_streamed:
            break
        time.sleep(0.1)

    test.check(not timer.has_expired())


close_resources(depth_sensor)
test.finish()
test.print_results_and_exit()
