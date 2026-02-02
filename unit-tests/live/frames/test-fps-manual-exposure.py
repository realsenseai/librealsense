# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# Currently, we exclude D555 as it's failing
# test:device each(D400*)
# test:device each(D500*) !D555
# test:donotrun

import pyrealsense2 as rs
from rspy.stopwatch import Stopwatch
from rspy import test, log
import time
import platform
import fps_helper

# This test mirrors test-fps.py but forces manual exposure where supported
# Start depth + color streams and measure frame frequency using sensor API.
# Verify that actual fps is as requested

def set_exposure_half_frame_time(sensor, requested_fps):
    """Set sensor exposure to half the frame time for the given requested_fps.
    Returns the exposure value that was set (or None if not supported/failed).
    """
    if not sensor.supports(rs.option.exposure):
        return None
    try:
        r = sensor.get_option_range(rs.option.exposure)
        # frame time in seconds
        frame_time_s = 1.0 / float(requested_fps)
        desired_s = frame_time_s / 2.0

        # Decide units based on sensor type: depth -> microseconds; RGB -> 100 microseconds units
        is_color = False
        try:
            for p in sensor.profiles:
                if p.stream_type() == rs.stream.color:
                    is_color = True
                    break
        except Exception:
            # fallback: assume depth unless explicitly color
            is_color = False

        if is_color:
            # RGB sensors: many backends treat exposure as units of 100 microseconds
            desired_units = desired_s * 1e6 / 100.0  # convert seconds -> 100-microsecond units
        else:
            # Depth sensors: use microseconds directly
            desired_units = desired_s * 1e6

        # clamp to reported range
        if desired_units < r.min:
            desired_units = r.min
        if desired_units > r.max:
            desired_units = r.max

        # Set the option (use float/int as appropriate)
        try:
            sensor.set_option(rs.option.exposure, desired_units)
        except Exception:
            sensor.set_option(rs.option.exposure, int(desired_units))
        return desired_s * 1e6  # return in microseconds
    except Exception:
        return None


delta_Hz = 1
tested_fps = [5, 6, 15, 30, 60, 90]
time_to_test_fps = [25, 20, 13, 10, 5, 4]
test.check_equal( len(tested_fps), len(time_to_test_fps) )

dev, _ = test.find_first_device_or_exit()
product_line = dev.get_info(rs.camera_info.product_line)
camera_name = dev.get_info(rs.camera_info.name)
firmware_version = dev.get_info(rs.camera_info.firmware_version)
serial = dev.get_info(rs.camera_info.serial_number)
log.i(f"Device: {camera_name}, product_line: {product_line}, serial: {serial}, firmware: {firmware_version}")

#####################################################################################################
test.start("Testing depth fps (manual exposure) " + product_line + " device - "+ platform.system() + " OS")

ds = dev.first_depth_sensor()
# Prepare depth sensor for manual exposure if supported
if product_line == "D400":
    if ds.supports(rs.option.enable_auto_exposure):
        # disable auto exposure to force manual exposure
        ds.set_option(rs.option.enable_auto_exposure, 0)

for i in range(len(tested_fps)):
    requested_fps = tested_fps[i]
    try:
        dp = next(p for p in ds.profiles
                  if p.fps() == requested_fps
                  and p.stream_type() == rs.stream.depth
                  and p.format() == rs.format.z16
                  and ((p.as_video_stream_profile().height() == 720 and p.fps() != 60) if "D585S" in camera_name else True))

    except StopIteration:
        log.i("Requested fps: {:.1f} [Hz], not supported".format(requested_fps))
    else:
        # set exposure to half frame time for this requested fps if supported
        exposure_val = set_exposure_half_frame_time(ds, requested_fps)
        # use shared fps helper which expects a dict of {sensor: [profile]}
        fps_helper.TIME_TO_COUNT_FRAMES = time_to_test_fps[i]
        fps_dict = fps_helper.measure_fps({ds: [dp]})
        fps = fps_dict.get(dp.stream_name(), 0)
        log.i("Exposure: {:.1f} [msec], requested fps: {:.1f} [Hz], actual fps: {:.1f} [Hz]".format((exposure_val or 0)/1000, requested_fps, fps))
        delta_Hz = requested_fps * 0.05 # Validation KPI is 5%
        test.check(fps <= (requested_fps + delta_Hz) and fps >= (requested_fps - delta_Hz))
test.finish()


#####################################################################################################
test.start("Testing color fps (manual exposure) " + product_line + " device - "+ platform.system() + " OS")

product_name = dev.get_info(rs.camera_info.name)
cs = None
try:
    cs = dev.first_color_sensor()
except RuntimeError as rte:
    if 'D421' not in product_name and 'D405' not in product_name: # Cameras with no color sensor may fail.
        test.unexpected_exception()

if cs:
    # Try to force manual exposure on color sensor
    if product_line == "D400":
        if cs.supports(rs.option.enable_auto_exposure):
            cs.set_option(rs.option.enable_auto_exposure, 0)
        if cs.supports(rs.option.auto_exposure_priority):
            cs.set_option(rs.option.auto_exposure_priority, 0) # AE priority should be 0 for constant FPS

    for i in range(len(tested_fps)):
        requested_fps = tested_fps[i]
        try:
            # collect matching color profiles and pick median resolution
            candidates = [p for p in cs.profiles
                          if p.fps() == requested_fps
                          and p.stream_type() == rs.stream.color
                          and p.format() == rs.format.rgb8]
            if not candidates:
                raise StopIteration
            candidates.sort(key=lambda pr: pr.as_video_stream_profile().width() * pr.as_video_stream_profile().height())
            cp = candidates[len(candidates)//2]

        except StopIteration:
            log.i("Requested fps: {:.1f} [Hz], not supported".format(requested_fps))
        else:
            # set exposure to half frame time for this requested fps if supported
            exposure_val = set_exposure_half_frame_time(cs, requested_fps)
            fps_dict = fps_helper.measure_fps({cs: [cp]})
            fps = fps_dict.get(cp.stream_name(), 0)
            log.i("Exposure: {:.1f} [msec], requested fps: {:.1f} [Hz], actual fps: {:.1f} [Hz]".format((exposure_val or 0)/1000, requested_fps, fps))
            delta_Hz = requested_fps * (0.10 if requested_fps == 5 else 0.05) # Validation KPI is 5% for all non 5 FPS rate
            test.check(fps <= (requested_fps + delta_Hz) and fps >= (requested_fps - delta_Hz))

test.finish()

#####################################################################################################
test.print_results_and_exit()
