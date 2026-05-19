# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# Not frequently changing, no need to test for each commit
# test:donotrun:!nightly
# test:donotrun:!dds
# test:device D555

import pyrealsense2 as rs
from rspy import test, log
from rspy.timer import Timer
import pyrsutils as rsutils
import time

def check_option_in_list(option_id, options_list):
    for option in options_list:
        if option_id == option:
            return True
    return False

def set_get_filter_option_value(embedded_filter, option, value_to_assign):
    initial_value = embedded_filter.get_option(option)
    option_step =  embedded_filter.get_option_range(option).step
    embedded_filter.set_option(option, value_to_assign)
    test.check_approx_abs(embedded_filter.get_option(option), value_to_assign, option_step)
    embedded_filter.set_option(option, initial_value)
    test.check_approx_abs(embedded_filter.get_option(option), initial_value, option_step)

def is_fw_version_below(curr_fw, min_fw):
    current_fw_version = rsutils.version(curr_fw)
    min_fw_version = rsutils.version(min_fw)
    return current_fw_version < min_fw_version

waiting_for_test = False
MAX_TIME_TO_WAIT_FOR_FRAMES = 10  # [sec]
wait_for_frames_timer = Timer(MAX_TIME_TO_WAIT_FOR_FRAMES)
decimation_enabled = False
temporal_enabled = False
dev, _ = test.find_first_device_or_exit()
depth_sensor = dev.first_depth_sensor()
fw_version = dev.get_info(rs.camera_info.firmware_version)

depth_profile = next(p for p in
                     depth_sensor.profiles if p.fps() == 30
                     and p.stream_type() == rs.stream.depth
                     and p.format() == rs.format.z16
                     and p.as_video_stream_profile().width() == 640
                     and p.as_video_stream_profile().height() == 360)

test_decimation_filter = not is_fw_version_below(fw_version, '7.56.36850.1229')

if test_decimation_filter:
    # this test will get options values for the decimation embedded filter, from the depth sensor
    # - get DDS device
    # - get depth sensor
    # - get embedded decimation filter
    # - get options values for this filter
    # - check 2 options available
    with test.closure("Get Decimation embedded filter options"):
        decimation_embedded_filter = depth_sensor.get_embedded_filter(rs.embedded_filter_type.decimation)
        test.check(decimation_embedded_filter)

        decimation_options = decimation_embedded_filter.get_supported_options()
        test.check_equal(len(decimation_options), 2)
        check_option_in_list(rs.option.embedded_filter_enabled, decimation_options)
        check_option_in_list(rs.option.filter_magnitude, decimation_options)

    with test.closure("Decimation embedded filter set/get options"):
        set_get_filter_option_value(decimation_embedded_filter, rs.option.embedded_filter_enabled, 1.0)
        # not setting magnitude because it is R/O option
        test.check_equal(decimation_embedded_filter.get_option(rs.option.filter_magnitude), 2.0)

    def decimation_check_callback(frame):
        global waiting_for_test, decimation_enabled
        if frame.supports_frame_metadata(rs.frame_metadata_value.embedded_filters):
            md_val = frame.get_frame_metadata(rs.frame_metadata_value.embedded_filters)
            value_to_check = 1 if decimation_enabled else 0
            test.check_equal(md_val & 1, value_to_check)
        waiting_for_test = False

    def stream_and_check_decimation_filter():
        global waiting_for_test
        depth_sensor.open(depth_profile)
        depth_sensor.start(decimation_check_callback)
        wait_for_frames_timer.start()
        waiting_for_test = True
        while waiting_for_test and not wait_for_frames_timer.has_expired():
            time.sleep(0.5)
        test.check(not wait_for_frames_timer.has_expired())
        depth_sensor.stop()
        depth_sensor.close()

    def enable_decimation_filter():
        decimation_embedded_filter.set_option(rs.option.embedded_filter_enabled, 1.0)
        test.check_equal(decimation_embedded_filter.get_option(rs.option.embedded_filter_enabled), 1.0)

    def disable_decimation_filter():
        decimation_embedded_filter.set_option(rs.option.embedded_filter_enabled, 0.0)
        test.check_equal(decimation_embedded_filter.get_option(rs.option.embedded_filter_enabled), 0.0)

    with test.closure("Decimation embedded filter metadata member"):
        decimation_enabled = False
        stream_and_check_decimation_filter()
        time.sleep(1)
        enable_decimation_filter()
        decimation_enabled = True
        stream_and_check_decimation_filter()
        time.sleep(1)
        disable_decimation_filter()

else:
    print("Decimation Embedded Filter mot tested")

# below FW number should be adjusted after Embedded Temporal Filter is in FW
test_temporal_filter = not is_fw_version_below(fw_version, '9.9.9.9')

if test_temporal_filter:
    # same test for temporal filter
    with test.closure("Get Temporal embedded filter options"):
        temporal_embedded_filter = depth_sensor.get_embedded_filter(rs.embedded_filter_type.temporal)
        test.check(temporal_embedded_filter)

        temporal_options = temporal_embedded_filter.get_supported_options()
        test.check_equal(len(temporal_options), 4)
        check_option_in_list(rs.option.embedded_filter_enabled, temporal_options)
        check_option_in_list(rs.option.filter_smooth_alpha, temporal_options)
        check_option_in_list(rs.option.filter_smooth_delta, temporal_options)
        check_option_in_list(rs.option.holes_fill, temporal_options)

    with test.closure("Temporal embedded filter set/get options"):
        set_get_filter_option_value(temporal_embedded_filter, rs.option.embedded_filter_enabled, 1.0)
        set_get_filter_option_value(temporal_embedded_filter, rs.option.filter_smooth_alpha, 0.2)
        set_get_filter_option_value(temporal_embedded_filter, rs.option.filter_smooth_delta, 30.0)
        set_get_filter_option_value(temporal_embedded_filter, rs.option.holes_fill, 6)


    def temporal_check_callback(frame):
        global waiting_for_test, temporal_enabled
        if frame.supports_frame_metadata(rs.frame_metadata_value.embedded_filters):
            md_val = frame.get_frame_metadata(rs.frame_metadata_value.embedded_filters)
            value_to_check = 1 if temporal_enabled else 0
            test.check_equal(md_val & 0b100, value_to_check)
        waiting_for_test = False


    def stream_and_check_temporal_filter():
        global waiting_for_test
        depth_sensor.open(depth_profile)
        depth_sensor.start(temporal_check_callback)
        wait_for_frames_timer.start()
        waiting_for_test = True
        while waiting_for_test and not wait_for_frames_timer.has_expired():
            time.sleep(0.5)
        test.check(not wait_for_frames_timer.has_expired())
        depth_sensor.stop()
        depth_sensor.close()


    def enable_temporal_filter():
        temporal_embedded_filter.set_option(rs.option.embedded_filter_enabled, 1.0)
        test.check_equal(temporal_embedded_filter.get_option(rs.option.embedded_filter_enabled), 1.0)


    def disable_temporal_filter():
        temporal_embedded_filter.set_option(rs.option.embedded_filter_enabled, 0.0)
        test.check_equal(temporal_embedded_filter.get_option(rs.option.embedded_filter_enabled), 0.0)


    with test.closure("Temporal embedded filter metadata member"):
        temporal_enabled = False
        stream_and_check_temporal_filter()
        time.sleep(1)
        enable_temporal_filter()
        temporal_enabled = True
        stream_and_check_temporal_filter()
        time.sleep(1)
        disable_temporal_filter()
else:
    print("Temporal Embedded Filter not tested")




test.print_results_and_exit()
