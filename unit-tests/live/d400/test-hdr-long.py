# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

######################################
# This set of tests is valid for any device that supports the HDR feature #
######################################

# test:device D400*
# test:donotrun:!nightly

import pyrealsense2 as rs
from rspy import test, log
import time

def retry_on_exception(func, max_retries=10):
    """
    Runs a function and retries it up to max_retries times if an exception is caught.

    :param func: The function to execute.
    :param max_retries: Maximum number of retries in case of exceptions.
    :param args: Positional arguments to pass to the function.
    :param kwargs: Keyword arguments to pass to the function.
    :return: The result of the function if successful, or None if all retries fail.
    """
    for attempt in range(max_retries):
        try:
            return func()
        except Exception as e:
            print(f"Attempt {attempt + 1} failed with exception: {e}")
    print("Failed to execute the function after maximum retries.")
    return None

# HDR CONFIGURATION TESTS
with test.closure("HDR Config - default config"):
    device, ctx = test.find_first_device_or_exit()
    depth_sensor = device.first_depth_sensor()

    if test.check(depth_sensor and depth_sensor.supports(rs.option.hdr_enabled)):
        exposure_range = depth_sensor.get_option_range(rs.option.exposure)
        gain_range = depth_sensor.get_option_range(rs.option.gain)

        depth_sensor.set_option(rs.option.hdr_enabled, 1)
        test.check(depth_sensor.get_option(rs.option.hdr_enabled) == 1)

        depth_sensor.set_option(rs.option.sequence_id, 1)  # seq id 1 is expected to be the default value
        test.check(depth_sensor.get_option(rs.option.sequence_id) == 1)
        exp = depth_sensor.get_option(rs.option.exposure)
        test.check(depth_sensor.get_option(rs.option.exposure) == exposure_range.default - 1000)  # w/a
        test.check(depth_sensor.get_option(rs.option.gain) == gain_range.default)

        depth_sensor.set_option(rs.option.sequence_id, 2)  # seq id 2 is expected to be the min value
        test.check(depth_sensor.get_option(rs.option.sequence_id) == 2)
        test.check(depth_sensor.get_option(rs.option.exposure) == exposure_range.min)
        test.check(depth_sensor.get_option(rs.option.gain) == gain_range.min)

        depth_sensor.set_option(rs.option.hdr_enabled, 0)
        test.check(depth_sensor.get_option(rs.option.hdr_enabled) == 0)
        

with test.closure("HDR Config - custom config"):
    # Require at least one device to be plugged in
    device, ctx = test.find_first_device_or_exit()
    depth_sensor = device.first_depth_sensor()

    if test.check(depth_sensor and depth_sensor.supports(rs.option.hdr_enabled)):
        depth_sensor.set_option(rs.option.sequence_size, 2)
        test.check(depth_sensor.get_option(rs.option.sequence_size) == 2)

        depth_sensor.set_option(rs.option.sequence_id, 1)
        test.check(depth_sensor.get_option(rs.option.sequence_id) == 1)
        depth_sensor.set_option(rs.option.exposure, 120)
        test.check(depth_sensor.get_option(rs.option.exposure) == 120)
        depth_sensor.set_option(rs.option.gain, 90)
        test.check(depth_sensor.get_option(rs.option.gain) == 90)

        depth_sensor.set_option(rs.option.sequence_id, 2)
        test.check(depth_sensor.get_option(rs.option.sequence_id) == 2)
        depth_sensor.set_option(rs.option.exposure, 1200)
        test.check(depth_sensor.get_option(rs.option.exposure) == 1200)
        depth_sensor.set_option(rs.option.gain, 20)
        test.check(depth_sensor.get_option(rs.option.gain) == 20)

        depth_sensor.set_option(rs.option.hdr_enabled, 1)
        test.check(depth_sensor.get_option(rs.option.hdr_enabled) == 1)

        depth_sensor.set_option(rs.option.hdr_enabled, 0)
        test.check(depth_sensor.get_option(rs.option.hdr_enabled) == 0)

def hdr_streaming_default_config():
    """
    This function is used to check the default configuration of HDR streaming.
    It enables HDR, starts the pipeline, and checks the exposure and gain values.
    """
    device, ctx = test.find_first_device_or_exit()
    depth_sensor = device.first_depth_sensor()

    if test.check(depth_sensor and depth_sensor.supports(rs.option.hdr_enabled)):
        exposure_range = depth_sensor.get_option_range(rs.option.exposure)
        gain_range = depth_sensor.get_option_range(rs.option.gain)

        depth_sensor.set_option(rs.option.hdr_enabled, 1)
        test.check(depth_sensor.get_option(rs.option.hdr_enabled) == 1)

        cfg = rs.config()
        cfg.enable_stream(rs.stream.depth)
        cfg.enable_stream(rs.stream.infrared, 1)
        pipe = rs.pipeline(ctx)
        pipe.start(cfg)

        for iteration in range(1, 100):
            data = pipe.wait_for_frames()
            out_depth_frame = data.get_depth_frame()
            if iteration < 3:
                continue

            if out_depth_frame.supports_frame_metadata(rs.frame_metadata_value.sequence_id):
                frame_exposure = out_depth_frame.get_frame_metadata(rs.frame_metadata_value.actual_exposure)
                frame_gain = out_depth_frame.get_frame_metadata(rs.frame_metadata_value.gain_level)
                seq_id = out_depth_frame.get_frame_metadata(rs.frame_metadata_value.sequence_id)

                if seq_id == 0:
                    test.check(frame_exposure == exposure_range.default - 1000)  # w/a
                    test.check(frame_gain == gain_range.default)
                else:
                    test.check(frame_exposure == exposure_range.min)
                    test.check(frame_gain == gain_range.min)
        pipe.stop()
        depth_sensor.set_option(rs.option.hdr_enabled, 0)  # disable hdr before next tests

# HDR STREAMING TEST
with test.closure("HDR Streaming - default config"):
    retry_on_exception(hdr_streaming_default_config)


def hdr_running_restart_hdr_at_restream():
    """
    This function is used to check the behavior of HDR after a pipe restart.
    It enables HDR, starts the pipeline, and checks the exposure and gain values.
    """
    device, ctx = test.find_first_device_or_exit()
    depth_sensor = device.first_depth_sensor()

    if test.check(depth_sensor and depth_sensor.supports(rs.option.hdr_enabled)):
        cfg = rs.config()
        cfg.enable_stream(rs.stream.depth)
        pipe = rs.pipeline(ctx)
        pipe.start(cfg)

        depth_sensor.set_option(rs.option.hdr_enabled, 1)
        test.check(depth_sensor.get_option(rs.option.hdr_enabled) == 1)

        for i in range(10):
            data = pipe.wait_for_frames()

        test.check(depth_sensor.get_option(rs.option.hdr_enabled) == 1)
        pipe.stop()
        pipe.start(cfg)
        test.check(depth_sensor.get_option(rs.option.hdr_enabled) == 1)
        pipe.stop()
        depth_sensor.set_option(rs.option.hdr_enabled, 0)  # disable hdr before next tests


# CHECKING HDR AFTER PIPE RESTART
with test.closure("HDR Running - restart hdr at restream"):
    retry_on_exception(hdr_running_restart_hdr_at_restream)


"""
helper method
checks that the frames resulting from the hdr_merge processing block are generated
from frames streamed within this streaming session (and not using old frames) - see RSDSO-17341.

To check that, we check that the hdr counter is between depth_counter and depth_counter-2, as in any case, if we use
new frames we should be in that range, example:
depth # |  hdr #    
1           1
2           1 (merged frames 1+2)
3           1 (no frame to merge with 3 as 4 didn't arrive yet)
4           3 (merged frames 3+4)
5           3 (no frame to merge with 5 as 6 didn't arrive yet)
6           5 (merged frames 5+6)
... (more frames, until a frame counter reset)
1           1 (hdr # should be the value we reset to)
2           1 (continues normally)
"""
def check_hdr_frame_counter(pipe, num_of_frames, merging_filter):
    """
    :return: False if some metadata could not be read - otherwise, True
    """
    prev_depth_counter = -1
    for i in range(num_of_frames):
        data = pipe.wait_for_frames()
        # get depth frame data
        depth_frame = data.get_depth_frame()
        if not depth_frame.supports_frame_metadata(rs.frame_metadata_value.frame_counter):
            log.d("frame counter could not be read from depth frame!")
            return

        depth_counter = depth_frame.get_frame_metadata(rs.frame_metadata_value.frame_counter)

        # apply HDR Merge process
        merged_frame = merging_filter.process(data)

        # get hdr frame data
        if not test.check(merged_frame.supports_frame_metadata(rs.frame_metadata_value.frame_counter)):
            log.d("frame counter could not be read from merged frame!")
            return

        hdr_counter = merged_frame.get_frame_metadata(rs.frame_metadata_value.frame_counter)

        if depth_counter < prev_depth_counter:
            log.d(f"frame counter reset! {prev_depth_counter}->{depth_counter}, got hdr counter {hdr_counter}")

        test.info("prev_depth_counter:", prev_depth_counter)
        test.info("depth_counter:", depth_counter)
        test.info("hdr counter:", hdr_counter)
        test.check(depth_counter - 2 <= hdr_counter <= depth_counter)
        prev_depth_counter = depth_counter


def hdr_running_hdr_merge_after_hdr_restart():
    """
    This function is used to check the behavior of HDR merge after a pipe restart.
    It enables HDR, starts the pipeline, and checks the exposure and gain values.
    """
    device, ctx = test.find_first_device_or_exit()
    depth_sensor = device.first_depth_sensor()

    if test.check(depth_sensor and depth_sensor.supports(rs.option.hdr_enabled)):
        # initializing the merging filter
        merging_filter = rs.hdr_merge()

        depth_sensor.set_option(rs.option.hdr_enabled, 1)
        test.check(depth_sensor.get_option(rs.option.hdr_enabled) == 1)

        cfg = rs.config()
        cfg.enable_stream(rs.stream.depth)
        pipe = rs.pipeline(ctx)
        pipe.start(cfg)

        frames_to_stream = 10

        check_hdr_frame_counter(pipe, frames_to_stream, merging_filter)

        depth_sensor.set_option(rs.option.hdr_enabled, 0)
        test.check(depth_sensor.get_option(rs.option.hdr_enabled) == 0)

        time.sleep(1)  # in D457, the first frame without HDR here might come too soon before the previous ones discard
        check_hdr_frame_counter(pipe, frames_to_stream, merging_filter)

        depth_sensor.set_option(rs.option.hdr_enabled, 1)
        test.check(depth_sensor.get_option(rs.option.hdr_enabled) == 1)

        check_hdr_frame_counter(pipe, frames_to_stream, merging_filter)

        pipe.stop()
        depth_sensor.set_option(rs.option.hdr_enabled, 0)  # disable hdr before next tests


# CHECKING HDR MERGE AFTER HDR RESTART
with test.closure("HDR Running - hdr merge after hdr restart"):
    retry_on_exception(hdr_running_hdr_merge_after_hdr_restart)


def check_sequence_id_on_frame(frame, prev_frame_counter, old_sequence_id):
    """
    given a frame and values from a previous frame
    this function checks if frames are sequential, if so test that sequence id is as expected
    """
    check_ok = False
    frame_counter = frame.get_frame_metadata(rs.frame_metadata_value.frame_counter)
    frame_seq_id = frame.get_frame_metadata(rs.frame_metadata_value.sequence_id)
    if frame_counter != prev_frame_counter + 1:  # can only compare sequential frames
        expected_sequence_id = frame_seq_id
    else:
        expected_sequence_id = 1 if old_sequence_id == 0 else 0

        test.info("expected sequence id:", expected_sequence_id)
        test.info("frame seq id:", frame_seq_id)
        test.info("frame counter:", frame_counter)
        test.info("prev frame counter:", prev_frame_counter)
        if test.check(expected_sequence_id == frame_seq_id):
            check_ok = True

    prev_frame_counter = frame_counter

    return check_ok, prev_frame_counter, expected_sequence_id


def check_sequence_id(pipe):
    """
    given a started pipe, this function is making sure the sequence id for the depth and ir streams is ok.
    """
    depth_seq_id = -1
    ir_seq_id = -1
    iterations_for_preparation = 14
    prev_frame_counter = -1
    prev_ir_frame_counter = -1
    seq_id_check_depth = True
    seq_id_check_ir = True
    checks_ok = 0
    while checks_ok < 50:
        data = pipe.wait_for_frames()
        if iterations_for_preparation > 0:
            iterations_for_preparation -= 1
            continue

        depth_frame = data.get_depth_frame()
        ir_frame = data.get_infrared_frame(1)

        if depth_frame.supports_frame_metadata(rs.frame_metadata_value.sequence_id):
            seq_id_check_depth, prev_frame_counter, depth_seq_id = (
                check_sequence_id_on_frame(depth_frame, prev_frame_counter, depth_seq_id))

        if ir_frame.supports_frame_metadata(rs.frame_metadata_value.sequence_id):
            seq_id_check_ir, prev_ir_frame_counter, ir_seq_id = (
                check_sequence_id_on_frame(ir_frame, prev_ir_frame_counter, ir_seq_id))

        # if both frames are sequential (after the previous ones), and sequence id test passes, that check is ok
        if seq_id_check_depth and seq_id_check_ir:
            checks_ok += 1


def hdr_streaming_checking_sequence_id():
    """
    This function is used to check the behavior of HDR streaming and sequence ID.
    It enables HDR, starts the pipeline, and checks the sequence ID values.
    """
    device, ctx = test.find_first_device_or_exit()
    depth_sensor = device.first_depth_sensor()

    if test.check(depth_sensor and depth_sensor.supports(rs.option.hdr_enabled)):
        cfg = rs.config()
        cfg.enable_stream(rs.stream.depth)
        cfg.enable_stream(rs.stream.infrared, 1)
        pipe = rs.pipeline(ctx)
        pipe.start(cfg)

        depth_sensor.set_option(rs.option.hdr_enabled, 1)
        test.check(depth_sensor.get_option(rs.option.hdr_enabled) == 1)

        check_sequence_id(pipe)

        pipe.stop()
        depth_sensor.set_option(rs.option.hdr_enabled, 0)  # disable hdr before next tests


# CHECKING SEQUENCE ID WHILE STREAMING
with test.closure("HDR Streaming - checking sequence id"):
    retry_on_exception(hdr_streaming_checking_sequence_id)
        

def emitter_on_off_check_sequence_id():
    """
    This function is used to check the behavior of emitter on/off and sequence ID.
    It enables the emitter, starts the pipeline, and checks the sequence ID values.
    """
    device, ctx = test.find_first_device_or_exit()
    depth_sensor = device.first_depth_sensor()

    if test.check(depth_sensor and depth_sensor.supports(rs.option.emitter_on_off)):
        cfg = rs.config()
        cfg.enable_stream(rs.stream.depth)
        cfg.enable_stream(rs.stream.infrared, 1)
        pipe = rs.pipeline(ctx)
        pipe.start(cfg)

        depth_sensor.set_option(rs.option.emitter_on_off, 1)
        test.check(depth_sensor.get_option(rs.option.emitter_on_off) == 1)

        check_sequence_id(pipe)

        pipe.stop()
        depth_sensor.set_option(rs.option.emitter_on_off, 0)  # disable emitter before next tests


with test.closure("Emitter on/off - checking sequence id"):
    retry_on_exception(emitter_on_off_check_sequence_id)


def hdr_merge_discard_merged_frame():
    """
    This function is used to check the behavior of HDR merge and discard merged frame.
    It enables HDR, starts the pipeline, and checks the sequence ID values.
    """
    device, ctx = test.find_first_device_or_exit()
    depth_sensor = device.first_depth_sensor()

    if test.check(depth_sensor and depth_sensor.supports(rs.option.hdr_enabled)):
        depth_sensor.set_option(rs.option.hdr_enabled, 1)
        test.check(depth_sensor.get_option(rs.option.hdr_enabled) == 1)

        cfg = rs.config()
        cfg.enable_stream(rs.stream.depth)
        pipe = rs.pipeline(ctx)
        pipe.start(cfg)

        # initializing the merging filter
        merging_filter = rs.hdr_merge()

        num_of_iterations_in_series = 10
        first_series_last_merged_ts = -1
        at_least_one_frame_supported_seq_id = False
        for i in range(0, num_of_iterations_in_series):
            data = pipe.wait_for_frames()
            out_depth_frame = data.get_depth_frame()

            if out_depth_frame.supports_frame_metadata(rs.frame_metadata_value.sequence_id):
                at_least_one_frame_supported_seq_id = True
                # merging the frames from the different HDR sequence IDs
                merged_frameset = merging_filter.process(data)
                merged_depth_frame = merged_frameset.as_frameset().get_depth_frame()

                frame_ts = merged_depth_frame.get_frame_metadata(rs.frame_metadata_value.frame_timestamp)

                if i == (num_of_iterations_in_series - 1):
                    first_series_last_merged_ts = frame_ts

        pipe.stop()

        if test.check(at_least_one_frame_supported_seq_id):
            test.check(first_series_last_merged_ts != -1)
            test.check(depth_sensor.get_option(rs.option.hdr_enabled) == 1)
            pipe.start(cfg)

            for i in range(0, 10):
                data = pipe.wait_for_frames()
                out_depth_frame = data.get_depth_frame()

                if out_depth_frame.supports_frame_metadata(rs.frame_metadata_value.sequence_id):
                    # merging the frames from the different HDR sequence IDs
                    merged_frameset = merging_filter.process(data)
                    merged_depth_frame = merged_frameset.as_frameset().get_depth_frame()

                    frame_ts = merged_depth_frame.get_frame_metadata(rs.frame_metadata_value.frame_timestamp)
                    test.check(frame_ts > first_series_last_merged_ts)
            pipe.stop()

        depth_sensor.set_option(rs.option.hdr_enabled, 0)  # disable hdr before next tests


# This tests checks that the previously saved merged frame is discarded after a pipe restart
with test.closure("HDR Merge - discard merged frame"):
    retry_on_exception(hdr_merge_discard_merged_frame)
        

def hdr_start_stop_recover_manual_exposure_and_gain():
    """
    This function is used to check the behavior of HDR start/stop and recover manual exposure and gain.
    It enables HDR, starts the pipeline, and checks the sequence ID values.
    """
    device, ctx = test.find_first_device_or_exit()
    depth_sensor = device.first_depth_sensor()
    if test.check(depth_sensor and depth_sensor.supports(rs.option.hdr_enabled)):

        gain_before_hdr = 50
        depth_sensor.set_option(rs.option.gain, gain_before_hdr)
        test.check(depth_sensor.get_option(rs.option.gain) == gain_before_hdr)

        exposure_before_hdr = 5000
        depth_sensor.set_option(rs.option.exposure, exposure_before_hdr)
        test.check(depth_sensor.get_option(rs.option.exposure) == exposure_before_hdr)

        depth_sensor.set_option(rs.option.hdr_enabled, 1)
        test.check(depth_sensor.get_option(rs.option.hdr_enabled) == 1)

        cfg = rs.config()
        cfg.enable_stream(rs.stream.depth)
        pipe = rs.pipeline(ctx)
        pipe.start(cfg)


        iteration_for_disable = 50
        iteration_to_check_after_disable = iteration_for_disable + 5  # Was 2, aligned to validation KPI's [DSO-18682]
        for iteration in range(1, 70):

            data = pipe.wait_for_frames()

            out_depth_frame = data.get_depth_frame()

            if out_depth_frame.supports_frame_metadata(rs.frame_metadata_value.sequence_id):

                frame_gain = out_depth_frame.get_frame_metadata(rs.frame_metadata_value.gain_level)
                frame_exposure = out_depth_frame.get_frame_metadata(rs.frame_metadata_value.actual_exposure)

                if iteration > iteration_for_disable and iteration < iteration_to_check_after_disable:
                    continue

                if iteration == iteration_for_disable:
                    depth_sensor.set_option(rs.option.hdr_enabled, 0)
                    test.check(depth_sensor.get_option(rs.option.hdr_enabled) == 0)

                elif iteration >= iteration_to_check_after_disable:
                    test.check(frame_gain == gain_before_hdr)

                    test.info("iteration", iteration)
                    test.info("iteration_to_check_after_disable", iteration_to_check_after_disable)
                    test.check(frame_exposure == exposure_before_hdr)

        pipe.stop()


with test.closure("HDR Start Stop - recover manual exposure and gain"):
    retry_on_exception(hdr_start_stop_recover_manual_exposure_and_gain)
        
def hdr_active_set_locked_options():
    """
    This function is used to check the behavior of HDR active set locked options.
    It enables HDR, checks that the locked options' values do not change.
    """
    device, ctx = test.find_first_device_or_exit()
    depth_sensor = device.first_depth_sensor()

    if test.check(depth_sensor and depth_sensor.supports(rs.option.hdr_enabled)):
        # setting laser ON
        if depth_sensor.supports(rs.option.emitter_enabled):
            depth_sensor.set_option(rs.option.emitter_enabled, 1)

        test.check(depth_sensor.supports(rs.option.laser_power))
        laser_power_before_hdr = depth_sensor.get_option(rs.option.laser_power)

        time.sleep(1.5)

        depth_sensor.set_option(rs.option.hdr_enabled, 1)
        test.check(depth_sensor.get_option(rs.option.hdr_enabled) == 1)

        # the following calls should not be performed and should send a LOG_WARNING
        if depth_sensor.supports(rs.option.enable_auto_exposure):
            depth_sensor.set_option(rs.option.enable_auto_exposure, 1)
            test.check(depth_sensor.get_option(rs.option.enable_auto_exposure) == 0)

        if depth_sensor.supports(rs.option.emitter_enabled):
            depth_sensor.set_option(rs.option.emitter_enabled, 0)
            test.check(depth_sensor.get_option(rs.option.emitter_enabled) == 1)

        if depth_sensor.supports(rs.option.emitter_on_off):
            depth_sensor.set_option(rs.option.emitter_on_off, 1)
            test.check(depth_sensor.get_option(rs.option.emitter_on_off) == 0)

        # We check laser power support in the beginning of this block
        depth_sensor.set_option(rs.option.laser_power, laser_power_before_hdr - 30)
        test.check(depth_sensor.get_option(rs.option.laser_power) == laser_power_before_hdr)

        depth_sensor.set_option(rs.option.hdr_enabled, 0)  # disable hdr before next tests


# CONTROLS STABILITY WHILE HDR ACTIVE
with test.closure("HDR Active - set locked options"):
    retry_on_exception(hdr_active_set_locked_options)
        

def hdr_streaming_set_locked_options():
    """
    This function is used to check the behavior of HDR streaming and set locked options.
    It enables HDR, starts the pipeline, and checks the sequence ID values.
    """
    device, ctx = test.find_first_device_or_exit()
    depth_sensor = device.first_depth_sensor()
    if test.check(depth_sensor and depth_sensor.supports(rs.option.hdr_enabled)):
        # setting laser ON
        depth_sensor.set_option(rs.option.emitter_enabled, 1)
        laser_power_before_hdr = depth_sensor.get_option(rs.option.laser_power)

        time.sleep(1.5)

        depth_sensor.set_option(rs.option.hdr_enabled, 1)
        test.check(depth_sensor.get_option(rs.option.hdr_enabled) == 1)

        cfg = rs.config()
        cfg.enable_stream(rs.stream.depth)
        pipe = rs.pipeline(ctx)
        pipe.start(cfg)

        for iteration in range(1, 50):
            data = pipe.wait_for_frames()

            if iteration == 20:
                # the following calls should not be performed and should send a LOG_WARNING
                depth_sensor.set_option(rs.option.enable_auto_exposure, 1)
                test.check(depth_sensor.get_option(rs.option.enable_auto_exposure) == 0)

                depth_sensor.set_option(rs.option.emitter_enabled, 0)
                test.check(depth_sensor.get_option(rs.option.emitter_enabled) == 1)

                depth_sensor.set_option(rs.option.emitter_on_off, 1)
                test.check(depth_sensor.get_option(rs.option.emitter_on_off) == 0)

                depth_sensor.set_option(rs.option.laser_power, laser_power_before_hdr - 30)
                test.check(depth_sensor.get_option(rs.option.laser_power) == laser_power_before_hdr)

        pipe.stop()
        depth_sensor.set_option(rs.option.hdr_enabled, 0)  # disable hdr before next tests


with test.closure("HDR Streaming - set locked options"):
    retry_on_exception(hdr_streaming_set_locked_options)


def hdr_streaming_enable_runtime_exposure_update():
    """
    This function is used to check the behavior of HDR streaming and enable runtime exposure update.
    It enables HDR, starts the pipeline, and checks the sequence ID values.
    """
    device, ctx = test.find_first_device_or_exit()
    depth_sensor = device.first_depth_sensor()

    if test.check(depth_sensor and depth_sensor.supports(rs.option.hdr_enabled)):
        exposure_range = depth_sensor.get_option_range(rs.option.exposure)
        gain_range = depth_sensor.get_option_range(rs.option.gain)

        depth_sensor.set_option(rs.option.hdr_enabled, 1)
        test.check(depth_sensor.get_option(rs.option.hdr_enabled) == 1)

        cfg = rs.config()
        cfg.enable_stream(rs.stream.depth)
        cfg.enable_stream(rs.stream.infrared, 1)
        pipe = rs.pipeline(ctx)
        pipe.start(cfg)

        #change exposure and gain for seq id 1
        depth_sensor.set_option(rs.option.sequence_id, 1)  # seq id 1 is expected to be the default value
        test.check(depth_sensor.get_option(rs.option.sequence_id) == 1)
        exp = depth_sensor.get_option(rs.option.exposure)
        test.check(depth_sensor.get_option(rs.option.exposure) == exposure_range.default - 1000)  # w/a
        depth_sensor.set_option(rs.option.exposure, exposure_range.default - 2000)
        test.check(depth_sensor.get_option(rs.option.exposure) == exposure_range.default - 2000)

        test.check(depth_sensor.get_option(rs.option.gain) == gain_range.default)
        depth_sensor.set_option(rs.option.gain, gain_range.default + 2)
        test.check(depth_sensor.get_option(rs.option.gain) == gain_range.default + 2)

        # change exposure and gain for seq id 2
        depth_sensor.set_option(rs.option.sequence_id, 2)# seq id 2 is expected to be the min value
        test.check(depth_sensor.get_option(rs.option.sequence_id) == 2)
        exp = depth_sensor.get_option(rs.option.exposure)
        test.check(depth_sensor.get_option(rs.option.exposure) == exposure_range.min)  # w/a
        depth_sensor.set_option(rs.option.exposure, exposure_range.default)
        test.check(depth_sensor.get_option(rs.option.exposure) == exposure_range.default)

        test.check(depth_sensor.get_option(rs.option.gain) == gain_range.min)
        depth_sensor.set_option(rs.option.gain, gain_range.default + 10)
        test.check(depth_sensor.get_option(rs.option.gain) == gain_range.default + 10)

        pipe.stop()
        depth_sensor.set_option(rs.option.hdr_enabled, 0)  # disable hdr before next tests


with test.closure("HDR Streaming - enable runtime exposure update in HDR mode"):
    retry_on_exception(hdr_streaming_enable_runtime_exposure_update)

test.print_results_and_exit()
