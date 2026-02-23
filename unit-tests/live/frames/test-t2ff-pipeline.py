# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

# test:device each(D400*)
# test:device each(D500*) 


import pyrealsense2 as rs
from rspy.stopwatch import Stopwatch
from rspy import test, log
import time
import platform


# Start depth + color streams and measure the time from stream opened until first frame arrived using pipeline API.
# Verify that the time do not exceeds the maximum time allowed
# Note - Using Windows Media Foundation to handle power management between USB actions take time (~27 ms)


# Set maximum delay for first frame according to product line
dev, ctx = test.find_first_device_or_exit()

# The device power up at D0 (Operational) state, allow time for it to get into idle state
# Note, it goes back to idle after streaming ends, no need to sleep between depth and color streaming.
time.sleep(3)

product_line = dev.get_info(rs.camera_info.product_line)
if product_line == "D400":
    max_delay_for_depth_frame = 1
    max_delay_for_color_frame = 1
elif product_line == "D500":
    max_delay_for_depth_frame = 1
    max_delay_for_color_frame = 1
else:
    log.f("Not supported product line " + product_line)


def time_to_first_frame(config):
    pipe = rs.pipeline(ctx)
    start_call_stopwatch = Stopwatch()
    pipe.start(config)
    pipe.wait_for_frames()
    delay = start_call_stopwatch.get_elapsed()
    pipe.stop()
    return delay


################################################################################################
test.start("Testing pipeline first depth frame delay on " + product_line + " device - " + platform.system() + " OS")
depth_cfg = rs.config()
depth_cfg.enable_stream(rs.stream.depth, rs.format.z16, 30)
frame_delay = time_to_first_frame(depth_cfg)
print("Delay from pipeline.start() until first depth frame is: {:.3f} [sec] max allowed is: {:.1f} [sec] ".format(frame_delay, max_delay_for_depth_frame))
test.check(frame_delay < max_delay_for_depth_frame)
test.finish()


################################################################################################
product_name = dev.get_info(rs.camera_info.name)
if 'D421' not in product_name and 'D405' not in product_name and 'D430' not in product_name: # Cameras with no color sensor
    test.start("Testing pipeline first color frame delay on " + product_line + " device - " + platform.system() + " OS")
    color_cfg = rs.config()
    color_cfg.enable_stream(rs.stream.color, rs.format.rgb8, 30)
    frame_delay = time_to_first_frame(color_cfg)
    print("Delay from pipeline.start() until first color frame is: {:.3f} [sec] max allowed is: {:.1f} [sec] ".format(frame_delay, max_delay_for_color_frame))
    test.check(frame_delay < max_delay_for_color_frame)
    test.finish()


################################################################################################
test.print_results_and_exit()
