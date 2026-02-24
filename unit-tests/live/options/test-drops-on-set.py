# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2020 RealSense, Inc. All Rights Reserved.

# Currently, we exclude D457 and D555 as it's failing

# test:device D400* !D457
# test:donotrun:!nightly

import platform
import pyrealsense2 as rs
from rspy import test
from rspy import log
from rspy import tests_wrapper as tw
import time

dev, _ = test.find_first_device_or_exit()
product_name = dev.get_info(rs.camera_info.name)
product_line = dev.get_info(rs.camera_info.product_line)
depth_sensor = dev.first_depth_sensor()
color_sensor = None
try:
    color_sensor = dev.first_color_sensor()
except RuntimeError as rte:
    if 'D421' not in product_name and 'D405' not in product_name: # Cameras with no color sensor may fail.
        test.unexpected_exception()

previous_depth_frame_number = -1
previous_color_frame_number = -1
after_set_option = False

tw.start_wrapper( dev )

def get_allowed_drops():
    global after_set_option
    # On Linux, there is a known issue (RS5-7148) where up to 4 frame drops can occur
    # sequentially after setting control values during streaming... on Windows this
    # does not occur.
    if platform.system() == 'Linux' and after_set_option:
        return 4
    # Our KPI is to prevent sequential frame drops, therefore single frame drop is allowed.
    return 1

def set_new_value(sensor, option, value):
    global after_set_option
    after_set_option = True
    sensor.set_option(option, value)
    time.sleep(0.5)  # collect frames
    after_set_option = False


def check_depth_frame_drops(frame):
    global previous_depth_frame_number
    allowed_drops = get_allowed_drops()
    is_d400 = 0
    if product_line == "D400":
        is_d400 = 1
    test.check_frame_drops(frame, previous_depth_frame_number, allowed_drops, allow_frame_counter_reset = is_d400)
    previous_depth_frame_number = frame.get_frame_number()


def check_color_frame_drops(frame):
    global previous_color_frame_number
    allowed_drops = get_allowed_drops()
    test.check_frame_drops(frame, previous_color_frame_number, allowed_drops)
    previous_color_frame_number = frame.get_frame_number()


# Use a default profile to fit every camera model
depth_profile = next(p for p in depth_sensor.profiles if p.is_default())

if color_sensor:
    color_profile = next(p for p in color_sensor.profiles if p.is_default())

depth_sensor.open(depth_profile)
depth_sensor.start(check_depth_frame_drops)
if color_sensor:
    color_sensor.open(color_profile)
    color_sensor.start(check_color_frame_drops)

#############################################################################################
# Test #1

test.start("Checking for frame drops when setting laser power several times")

curr_value = depth_sensor.get_option(rs.option.laser_power)
opt_range = depth_sensor.get_option_range(rs.option.laser_power)

new_value = opt_range.min
while new_value <= opt_range.max:   
   set_new_value(depth_sensor, rs.option.laser_power, new_value)
   new_value += opt_range.step

set_new_value(depth_sensor, rs.option.laser_power, curr_value) # Restore

test.finish()


#############################################################################################

options_to_ignore = []

# ignore reasons:
# visual_preset       --> frame drops are expected during visual_preset change
# inter_cam_sync_mode --> frame drops are expected during inter_cam_sync_mode change
# emitter_frequency   --> Not allowed to be set during streaming
# auto_exposure_mode  --> Not allowed to be set during streaming
if product_line == "D400":
    options_to_ignore = [rs.option.visual_preset, rs.option.inter_cam_sync_mode, rs.option.emitter_frequency, rs.option.auto_exposure_mode]

def test_option_changes(sensor):
    global options_to_ignore
    options = sensor.get_supported_options()
    for option in options:
        try:
            if option in options_to_ignore:
                continue
            if sensor.is_option_read_only(option):
                continue
            orig_opt_value = sensor.get_option_value(option)
            if orig_opt_value.type == rs.option_type.integer or orig_opt_value.type == rs.option_type.float:
                old_value = orig_opt_value.value
                range = sensor.get_option_range(option)
                new_value = range.min
                if old_value == new_value:
                    new_value = range.max
                if not log.d(str(option), old_value, '->', new_value):
                    test.info(str(option), new_value, persistent=True)
                set_new_value(sensor, option, new_value)
                sensor.set_option(option, old_value)
            else:
                log.d(str(option), "is of", str(orig_opt_value.type), "- skipping.")
        except:
            test.unexpected_exception()
            break
        finally:
            test.reset_info(persistent=True)


#############################################################################################
time.sleep(0.5)  # jic
test.start("Checking frame drops when setting options on depth")
test_option_changes(depth_sensor)
test.finish()

#############################################################################################
if color_sensor:
    time.sleep(0.5)  # jic
    test.start("Checking frame drops when setting options on color")
    test_option_changes( color_sensor )
    test.finish()


#############################################################################################
depth_sensor.stop()
depth_sensor.close()

if color_sensor:
    color_sensor.stop()
    color_sensor.close()
    
tw.stop_wrapper( dev )
test.print_results_and_exit()
