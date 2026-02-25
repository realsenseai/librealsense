# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

# test:device each(D400*)
# test:device each(D500*) 

import pyrealsense2 as rs
from rspy.stopwatch import Stopwatch
from rspy import test, log
import time
import platform

# Start depth + color streams and measure the time from stream opened until first frame arrived using sensor API.
# Verify that the time do not exceeds the maximum time allowed
# Note - Using Windows Media Foundation to handle power management between USB actions take time (~27 ms)


def time_to_first_frame(sensor, profile, max_delay_allowed):
    """
    Wait for the first frame for 'max_delay_allowed' + 1 extra second
    If the frame arrives it will return the seconds it took since open() call
    If no frame it will return 'max_delay_allowed'
    """
    first_frame_time = -1
    open_call_stopwatch = Stopwatch()

    def frame_cb(frame):
        nonlocal first_frame_time, open_call_stopwatch
        if first_frame_time == -1:
            first_frame_time = open_call_stopwatch.get_elapsed()

    open_call_stopwatch.reset()
    sensor.open(profile)
    sensor.start(frame_cb)

    # Wait condition:
    # 1. first frame did not arrive yet
    # 2. timeout of 'max_delay_allowed' + 1 extra second reached.
    while first_frame_time == -1 and open_call_stopwatch.get_elapsed() < max_delay_allowed + 1:
        time.sleep(0.05)

    sensor.stop()
    sensor.close()

    return first_frame_time


# The device power up at D0 (Operational) state, allow time for it to get into idle state
# Note, it goes back to idle after streaming ends, no need to sleep between depth and color streaming.
time.sleep(3)


#####################################################################################################
test.start("Testing device creation time on " + platform.system() + " OS")
device_creation_stopwatch = Stopwatch()
ctx = rs.context( { "dds" : { "enabled" : False } } )
devs = ctx.devices
if len(devs) == 0:
    # No devices found, try to find a device with DDS enabled
    ctx = rs.context( { "dds" : { "enabled" : True } } )
    device_creation_stopwatch.reset() # Start measuring after the DDS participant creation
    devs = ctx.devices
device_creation_time = device_creation_stopwatch.get_elapsed()
dev = devs[0]
is_dds = dev.supports(rs.camera_info.connection_type) and dev.get_info(rs.camera_info.connection_type) == "DDS"
max_time_for_device_creation = 1 if not is_dds else 5  # Querying for DDS devices can block with default max wait of 5 seconds.
print("Device creation time is: {:.3f} [sec] max allowed is: {:.1f} [sec] ".format(device_creation_time, max_time_for_device_creation))
test.check(device_creation_time < max_time_for_device_creation)
test.finish()

product_line = dev.get_info(rs.camera_info.product_line)
max_delay_for_depth_frame = 1
max_delay_for_color_frame = 1


#####################################################################################################
test.start("Testing first depth frame delay on " + product_line + " device - "+ platform.system() + " OS")
ds = dev.first_depth_sensor()
dp = next(p for p in
          ds.profiles if p.fps() == 30
          and p.stream_type() == rs.stream.depth
          and p.format() == rs.format.z16
          and p.is_default())
first_depth_frame_delay = time_to_first_frame(ds, dp, max_delay_for_depth_frame)
test.check(first_depth_frame_delay != -1, "depth frames did not arrive for {:.1f} seconds".format(max_delay_for_depth_frame))
if first_depth_frame_delay > -1:
    print("Time until first depth frame is: {:.3f} [sec] max allowed is: {:.1f} [sec] ".format(first_depth_frame_delay, max_delay_for_depth_frame))
    test.check(first_depth_frame_delay < max_delay_for_depth_frame)
test.finish()


#####################################################################################################
test.start("Testing first color frame delay on " + product_line + " device - "+ platform.system() + " OS")
product_name = dev.get_info(rs.camera_info.name)
cs = None
try:
    cs = dev.first_color_sensor()
except RuntimeError as rte:
    if 'D421' not in product_name and 'D405' not in product_name: # Cameras with no color sensor may fail.
        test.unexpected_exception()

if cs:
    cp = next(p for p in
              cs.profiles if p.fps() == 30
              and p.stream_type() == rs.stream.color
              and p.format() == rs.format.rgb8
              and p.is_default())
    first_color_frame_delay = time_to_first_frame(cs, cp, max_delay_for_color_frame)
    test.check(first_color_frame_delay != -1, "color frames did not arrive for {:.1f} seconds".format(max_delay_for_color_frame))
    if first_color_frame_delay > -1:
        print("Time until first color frame is: {:.3f} [sec] max allowed is: {:.1f} [sec] ".format(first_color_frame_delay, max_delay_for_color_frame))
        test.check(first_color_frame_delay < max_delay_for_color_frame)
test.finish()


#####################################################################################################
test.print_results_and_exit()
