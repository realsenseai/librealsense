# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

#test:device each(D400*) !D421 !D405 !D401
#test:device each(D500*)
#test:donotrun:!nightly

import pyrealsense2 as rs
from rspy import test, log

def close_resources(sensor):
    if len(sensor.get_active_streams()) > 0:
        sensor.stop()
        sensor.close()

color_options = [rs.option.backlight_compensation,
                     rs.option.brightness,
                     rs.option.contrast,
                     rs.option.gamma,
                     rs.option.hue,
                     rs.option.saturation,
                     rs.option.sharpness,
                     rs.option.enable_auto_white_balance,
                     rs.option.white_balance]

color_metadata = [rs.frame_metadata_value.backlight_compensation,
                     rs.frame_metadata_value.brightness,
                     rs.frame_metadata_value.contrast,
                     rs.frame_metadata_value.gamma,
                     rs.frame_metadata_value.hue,
                     rs.frame_metadata_value.saturation,
                     rs.frame_metadata_value.sharpness,
                     rs.frame_metadata_value.auto_white_balance_temperature,
                     rs.frame_metadata_value.manual_white_balance]

def check_option_and_metadata_values(option, metadata, value_to_set, frame):
    changed = color_sensor.get_option(option)
    test.check_equal(changed, value_to_set)
    if frame.supports_frame_metadata(metadata):
        changed_md = float( frame.get_frame_metadata(metadata) )
        test.check_equal(changed_md, value_to_set)
    else:
        print("metadata " + repr(metadata) + " not supported")

#############################################################################################
test.start("checking color options")
# test scenario:
# for each option, set value, wait several frames, check value with get_option and get_frame_metadata
# values set for each option are min, max, and default values
ctx = rs.context()
dev = ctx.query_devices()[0]

try:
    color_sensor = dev.first_color_sensor()
    # Using a profile common to known cameras
    color_profile = next(p for p in color_sensor.profiles if p.fps() == 30
                         and p.stream_type() == rs.stream.color
                         and p.format() == rs.format.yuyv
                         and p.as_video_stream_profile().width() == 640
                         and p.as_video_stream_profile().height() == 480)
    color_sensor.open(color_profile)
    lrs_queue = rs.frame_queue(capacity=10, keep_frames=False)
    color_sensor.start(lrs_queue)

    iteration = 0
    option_index = -1  # running over options
    value_index = -1  # 0, 1, 2 for min, max, default
    # number of frames to wait between set_option and checking metadata
    # is set as 15 - the expected delay is ~120ms for Win and ~80-90ms for Linux
    num_of_frames_to_wait = 15
    while True:
        try:
            lrs_frame = lrs_queue.wait_for_frame(5000)
            if iteration == 0:
                option_index = option_index + 1
                if option_index == len(color_options):
                    break
                option = color_options[option_index]
                if not color_sensor.supports(option):
                    continue
                option_range = color_sensor.get_option_range(option)
                metadata = color_metadata[option_index]
                # the following if statement is needed because of some bug in FW - see DSO-17221
                # to be removed after this bug is solved
                if option == rs.option.white_balance:
                    log.d("iteration", iteration, "setting explicitly enable_auto_white_balance to OFF")
                    color_sensor.set_option(rs.option.enable_auto_white_balance, 0)
                    test.check_equal(color_sensor.get_option(rs.option.enable_auto_white_balance) , 0.0)
                value_to_set = option_range.min
            elif iteration == (num_of_frames_to_wait + 1):
                value_to_set = option_range.max
            elif iteration == 2 * (num_of_frames_to_wait + 1):
                value_to_set = option_range.default
            if iteration % (num_of_frames_to_wait + 1) == 0:  # for iterations 0, 16, 32
                log.d("iteration", iteration, "setting option:", option, "to", value_to_set)
                color_sensor.set_option(option, value_to_set)
            if (iteration + 1) % (num_of_frames_to_wait + 1) == 0:  # for iterations 15, 31, 47
                log.d("iteration", iteration, "checking MD:", metadata, "vs option:",  option)
                check_option_and_metadata_values(option, metadata, value_to_set, lrs_frame)
            iteration = (iteration + 1) % (3 * (num_of_frames_to_wait + 1))

        except:
            test.unexpected_exception()

except:
    print("The device found has no color sensor")
finally:
    close_resources(color_sensor)

test.finish()
test.print_results_and_exit()
