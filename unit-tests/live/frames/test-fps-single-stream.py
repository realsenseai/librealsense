# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

# Currently, we exclude D555 as it's failing
# test:device each(D400*)
# test:device each(D500*) !D555
# test:donotrun:!nightly

import pyrealsense2 as rs
from rspy.stopwatch import Stopwatch
from rspy import test, log
import time
import platform
import fps_helper

# Start depth + color streams and measure frame frequency using sensor API.
# Verify that actual fps is as requested

delta_Hz = 1
tested_fps = [5, 6, 15, 30, 60, 90]
time_to_test_fps = [8, 8, 5, 4, 3, 2] # Get enough frames to check FPS stability
test.check_equal( len(tested_fps), len(time_to_test_fps) )

fps_helper.TIME_FOR_STEADY_STATE = 1.2 # t2ff KPI is 1 second + some extra.

dev, _ = test.find_first_device_or_exit()
product_line = dev.get_info(rs.camera_info.product_line)
camera_name = dev.get_info(rs.camera_info.name)

#####################################################################################################
test.start("Testing depth fps " + product_line + " device - "+ platform.system() + " OS")

ds = dev.first_depth_sensor()
# Set auto-exposure option as it might take precedence over requested FPS
if product_line == "D400":
    if ds.supports(rs.option.enable_auto_exposure):
        ds.set_option(rs.option.enable_auto_exposure, 1)

for i in range(len(tested_fps)):
    requested_fps = tested_fps[i]    
    try:
        dp = next(p for p in ds.profiles
                  if p.fps() == requested_fps
                  and p.stream_type() == rs.stream.depth
                  and p.format() == rs.format.z16
                  # On D585S the operational depth resolution is 1280x720
                  # 1280x960 is also available but only allowed in service mode
                  # 60 fps is only available in bypass mode
                  and ((p.as_video_stream_profile().height() == 720 and p.fps() != 60) if "D585S" in camera_name else True))

    except StopIteration:
        log.i("Requested fps: {:.1f} [Hz], not supported".format(requested_fps))
    else:
        fps_helper.TIME_TO_COUNT_FRAMES = time_to_test_fps[i]
        fps_dict = fps_helper.measure_fps({ds: [dp]})
        fps = fps_dict.get(dp.stream_name(), 0)
        log.i("Requested fps: {:.1f} [Hz], actual fps: {:.1f} [Hz]".format(requested_fps, fps))
        delta_Hz = requested_fps * 0.05 # Validation KPI is 5%
        test.check(fps <= (requested_fps + delta_Hz) and fps >= (requested_fps - delta_Hz))
test.finish()


#####################################################################################################
test.start("Testing color fps " + product_line + " device - "+ platform.system() + " OS")

product_name = dev.get_info(rs.camera_info.name)
cs = None
try:
    cs = dev.first_color_sensor()
except RuntimeError as rte:
    if 'D421' not in product_name and 'D405' not in product_name: # Cameras with no color sensor may fail.
        test.unexpected_exception()

if cs:        
    # Set auto-exposure option as it might take precedence over requested FPS
    if product_line == "D400":
        if cs.supports(rs.option.enable_auto_exposure):
            cs.set_option(rs.option.enable_auto_exposure, 1)
        if cs.supports(rs.option.auto_exposure_priority):
            cs.set_option(rs.option.auto_exposure_priority, 0) # AE priority should be 0 for constant FPS

    for i in range(len(tested_fps)):
        requested_fps = tested_fps[i]
        try:
            cp = next(p for p in cs.profiles
                      if p.fps() == requested_fps
                      and p.stream_type() == rs.stream.color
                      and p.format() == rs.format.rgb8)
                 
        except StopIteration:
            log.i("Requested fps: {:.1f} [Hz], not supported".format(requested_fps))
        else:
            fps_helper.TIME_TO_COUNT_FRAMES = time_to_test_fps[i]
            fps_dict = fps_helper.measure_fps({cs: [cp]})
            fps = fps_dict.get(cp.stream_name(), 0)
            log.i("Requested fps: {:.1f} [Hz], actual fps: {:.1f} [Hz]".format(requested_fps, fps))
            delta_Hz = requested_fps * (0.10 if requested_fps == 5 else 0.05) # Validation KPI is 5% for all non 5 FPS rate
            test.check(fps <= (requested_fps + delta_Hz) and fps >= (requested_fps - delta_Hz))

test.finish()

#####################################################################################################
test.print_results_and_exit()
