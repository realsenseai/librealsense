# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

######################################
# This set of tests is valid for any device that supports the HDR feature #
######################################

import pytest
import pyrealsense2 as rs
import pyrsutils as rsutils
from pytest_check import check
from rspy.pytest.device_helpers import require_min_fw_version
import logging
import time
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device("D400*"),
    pytest.mark.context("nightly"),
]


@pytest.fixture(autouse=True)
def _disable_hdr(test_device):
    yield
    dev, _ = test_device
    depth_sensor = dev.first_depth_sensor()
    if depth_sensor and depth_sensor.supports(rs.option.hdr_enabled):
        depth_sensor.set_option(rs.option.hdr_enabled, 0)


def _skip_if_fw_unsupported(dev):
    require_min_fw_version(dev, rsutils.version(5, 17, 2, 11))


def retry_on_exception(func, max_retries=10):
    last_exception = None
    for attempt in range(max_retries):
        try:
            return func()
        except Exception as e:
            print(f"Attempt {attempt + 1} failed with exception: {e}")
            last_exception = e
    raise last_exception


# HDR CONFIGURATION TESTS
def test_hdr_config_default_config(test_device):
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()
    exposure_range = depth_sensor.get_option_range(rs.option.exposure)
    gain_range = depth_sensor.get_option_range(rs.option.gain)

    depth_sensor.set_option(rs.option.hdr_enabled, 1)
    assert depth_sensor.get_option(rs.option.hdr_enabled) == 1

    depth_sensor.set_option(rs.option.sequence_id, 1)  # seq id 1 is expected to be the default value
    assert depth_sensor.get_option(rs.option.sequence_id) == 1
    exp = depth_sensor.get_option(rs.option.exposure)
    assert depth_sensor.get_option(rs.option.exposure) == exposure_range.default - 1000  # w/a
    assert depth_sensor.get_option(rs.option.gain) == gain_range.default

    depth_sensor.set_option(rs.option.sequence_id, 2)  # seq id 2 is expected to be the min value
    assert depth_sensor.get_option(rs.option.sequence_id) == 2
    assert depth_sensor.get_option(rs.option.exposure) == exposure_range.min
    assert depth_sensor.get_option(rs.option.gain) == gain_range.min

    depth_sensor.set_option(rs.option.hdr_enabled, 0)
    assert depth_sensor.get_option(rs.option.hdr_enabled) == 0


def test_hdr_config_custom_config(test_device):
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()
    depth_sensor.set_option(rs.option.sequence_size, 2)
    assert depth_sensor.get_option(rs.option.sequence_size) == 2

    depth_sensor.set_option(rs.option.sequence_id, 1)
    assert depth_sensor.get_option(rs.option.sequence_id) == 1
    depth_sensor.set_option(rs.option.exposure, 120)
    assert depth_sensor.get_option(rs.option.exposure) == 120
    depth_sensor.set_option(rs.option.gain, 90)
    assert depth_sensor.get_option(rs.option.gain) == 90

    depth_sensor.set_option(rs.option.sequence_id, 2)
    assert depth_sensor.get_option(rs.option.sequence_id) == 2
    depth_sensor.set_option(rs.option.exposure, 1200)
    assert depth_sensor.get_option(rs.option.exposure) == 1200
    depth_sensor.set_option(rs.option.gain, 20)
    assert depth_sensor.get_option(rs.option.gain) == 20

    depth_sensor.set_option(rs.option.hdr_enabled, 1)
    assert depth_sensor.get_option(rs.option.hdr_enabled) == 1

    depth_sensor.set_option(rs.option.hdr_enabled, 0)
    assert depth_sensor.get_option(rs.option.hdr_enabled) == 0


def _hdr_streaming_default_config(dev, ctx):
    depth_sensor = dev.first_depth_sensor()
    exposure_range = depth_sensor.get_option_range(rs.option.exposure)
    gain_range = depth_sensor.get_option_range(rs.option.gain)

    depth_sensor.set_option(rs.option.hdr_enabled, 1)
    assert depth_sensor.get_option(rs.option.hdr_enabled) == 1

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
                check.is_true(frame_exposure == exposure_range.default - 1000)  # w/a
                check.is_true(frame_gain == gain_range.default)
            else:
                check.is_true(frame_exposure == exposure_range.min)
                check.is_true(frame_gain == gain_range.min)
    pipe.stop()


# HDR STREAMING TEST
def test_hdr_streaming_default_config(test_device):
    dev, ctx = test_device
    retry_on_exception(lambda: _hdr_streaming_default_config(dev, ctx))


def _hdr_running_restart_hdr_at_restream(dev, ctx):
    depth_sensor = dev.first_depth_sensor()
    cfg = rs.config()
    cfg.enable_stream(rs.stream.depth)
    pipe = rs.pipeline(ctx)
    pipe.start(cfg)
    try:
        depth_sensor.set_option(rs.option.hdr_enabled, 1)
        assert depth_sensor.get_option(rs.option.hdr_enabled) == 1

        for i in range(10):
            data = pipe.wait_for_frames()

        assert depth_sensor.get_option(rs.option.hdr_enabled) == 1
        pipe.stop()
        pipe.start(cfg)
        assert depth_sensor.get_option(rs.option.hdr_enabled) == 1
        pipe.stop()
    except Exception:
        try:
            pipe.stop()
        except Exception:
            pass
        raise


# CHECKING HDR AFTER PIPE RESTART
def test_hdr_running_restart_hdr_at_restream(test_device):
    dev, ctx = test_device
    retry_on_exception(lambda: _hdr_running_restart_hdr_at_restream(dev, ctx))


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
def _check_hdr_frame_counter(pipe, num_of_frames, merging_filter):
    """
    :return: False if some metadata could not be read - otherwise, True
    """
    prev_depth_counter = -1
    for i in range(num_of_frames):
        data = pipe.wait_for_frames()
        # get depth frame data
        depth_frame = data.get_depth_frame()
        if not depth_frame.supports_frame_metadata(rs.frame_metadata_value.frame_counter):
            log.debug("frame counter could not be read from depth frame!")
            return

        depth_counter = depth_frame.get_frame_metadata(rs.frame_metadata_value.frame_counter)

        # apply HDR Merge process
        merged_frame = merging_filter.process(data)

        # get hdr frame data
        if not merged_frame.supports_frame_metadata(rs.frame_metadata_value.frame_counter):
            log.debug("frame counter could not be read from merged frame!")
            return

        hdr_counter = merged_frame.get_frame_metadata(rs.frame_metadata_value.frame_counter)

        if depth_counter < prev_depth_counter:
            log.debug(f"frame counter reset! {prev_depth_counter}->{depth_counter}, got hdr counter {hdr_counter}")

        log.info(f"prev_depth_counter: {prev_depth_counter}")
        log.info(f"depth_counter: {depth_counter}")
        log.info(f"hdr counter: {hdr_counter}")
        check.is_true(depth_counter - 2 <= hdr_counter <= depth_counter)
        prev_depth_counter = depth_counter


def _hdr_running_hdr_merge_after_hdr_restart(dev, ctx):
    depth_sensor = dev.first_depth_sensor()
    # initializing the merging filter
    merging_filter = rs.hdr_merge()

    depth_sensor.set_option(rs.option.hdr_enabled, 1)
    assert depth_sensor.get_option(rs.option.hdr_enabled) == 1

    cfg = rs.config()
    cfg.enable_stream(rs.stream.depth)
    pipe = rs.pipeline(ctx)
    pipe.start(cfg)
    try:
        frames_to_stream = 10

        _check_hdr_frame_counter(pipe, frames_to_stream, merging_filter)

        depth_sensor.set_option(rs.option.hdr_enabled, 0)
        assert depth_sensor.get_option(rs.option.hdr_enabled) == 0

        time.sleep(1)  # in D457, the first frame without HDR here might come too soon before the previous ones discard
        _check_hdr_frame_counter(pipe, frames_to_stream, merging_filter)

        depth_sensor.set_option(rs.option.hdr_enabled, 1)
        assert depth_sensor.get_option(rs.option.hdr_enabled) == 1

        _check_hdr_frame_counter(pipe, frames_to_stream, merging_filter)
    finally:
        pipe.stop()
        depth_sensor.set_option(rs.option.hdr_enabled, 0)  # disable hdr before next tests


# CHECKING HDR MERGE AFTER HDR RESTART
def test_hdr_running_hdr_merge_after_hdr_restart(test_device):
    dev, ctx = test_device
    retry_on_exception(lambda: _hdr_running_hdr_merge_after_hdr_restart(dev, ctx))


def _check_sequence_id_on_frame(frame, prev_frame_counter, old_sequence_id):
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

        log.info(f"expected sequence id: {expected_sequence_id}")
        log.info(f"frame seq id: {frame_seq_id}")
        log.info(f"frame counter: {frame_counter}")
        log.info(f"prev frame counter: {prev_frame_counter}")
        check.is_true(expected_sequence_id == frame_seq_id)
        if expected_sequence_id == frame_seq_id:
            check_ok = True

    prev_frame_counter = frame_counter

    return check_ok, prev_frame_counter, expected_sequence_id


def _check_sequence_id(pipe):
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
                _check_sequence_id_on_frame(depth_frame, prev_frame_counter, depth_seq_id))

        if ir_frame.supports_frame_metadata(rs.frame_metadata_value.sequence_id):
            seq_id_check_ir, prev_ir_frame_counter, ir_seq_id = (
                _check_sequence_id_on_frame(ir_frame, prev_ir_frame_counter, ir_seq_id))

        # if both frames are sequential (after the previous ones), and sequence id test passes, that check is ok
        if seq_id_check_depth and seq_id_check_ir:
            checks_ok += 1


def _hdr_streaming_checking_sequence_id(dev, ctx):
    depth_sensor = dev.first_depth_sensor()
    cfg = rs.config()
    cfg.enable_stream(rs.stream.depth)
    cfg.enable_stream(rs.stream.infrared, 1)
    pipe = rs.pipeline(ctx)
    pipe.start(cfg)
    try:
        depth_sensor.set_option(rs.option.hdr_enabled, 1)
        assert depth_sensor.get_option(rs.option.hdr_enabled) == 1

        _check_sequence_id(pipe)
    finally:
        pipe.stop()
        depth_sensor.set_option(rs.option.hdr_enabled, 0)  # disable hdr before next tests
    assert depth_sensor.get_option(rs.option.hdr_enabled) == 0


# CHECKING SEQUENCE ID WHILE STREAMING
def test_hdr_streaming_checking_sequence_id(test_device):
    dev, ctx = test_device
    retry_on_exception(lambda: _hdr_streaming_checking_sequence_id(dev, ctx))


def _emitter_on_off_check_sequence_id(dev, ctx):
    depth_sensor = dev.first_depth_sensor()

    if not (depth_sensor and depth_sensor.supports(rs.option.emitter_on_off)):
        pytest.skip("Emitter on/off option not supported on this device")
    cfg = rs.config()
    cfg.enable_stream(rs.stream.depth)
    cfg.enable_stream(rs.stream.infrared, 1)
    pipe = rs.pipeline(ctx)
    pipe.start(cfg)
    try:
        depth_sensor.set_option(rs.option.emitter_on_off, 1)
        assert depth_sensor.get_option(rs.option.emitter_on_off) == 1

        _check_sequence_id(pipe)
    finally:
        pipe.stop()
        depth_sensor.set_option(rs.option.emitter_on_off, 0)  # disable emitter before next tests
    assert depth_sensor.get_option(rs.option.emitter_on_off) == 0


def test_emitter_on_off_checking_sequence_id(test_device):
    dev, ctx = test_device
    _skip_if_fw_unsupported(dev)
    retry_on_exception(lambda: _emitter_on_off_check_sequence_id(dev, ctx))


def _hdr_merge_discard_merged_frame(dev, ctx):
    depth_sensor = dev.first_depth_sensor()
    depth_sensor.set_option(rs.option.hdr_enabled, 1)
    assert depth_sensor.get_option(rs.option.hdr_enabled) == 1

    cfg = rs.config()
    cfg.enable_stream(rs.stream.depth)
    pipe = rs.pipeline(ctx)

    # initializing the merging filter
    merging_filter = rs.hdr_merge()

    num_of_iterations_in_series = 10
    first_series_last_merged_ts = -1
    at_least_one_frame_supported_seq_id = False

    pipe.start(cfg)
    try:
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
    finally:
        pipe.stop()

    assert at_least_one_frame_supported_seq_id
    assert first_series_last_merged_ts != -1
    assert depth_sensor.get_option(rs.option.hdr_enabled) == 1

    try:
        pipe.start(cfg)
        try:
            for i in range(0, 10):
                data = pipe.wait_for_frames()
                out_depth_frame = data.get_depth_frame()

                if out_depth_frame.supports_frame_metadata(rs.frame_metadata_value.sequence_id):
                    # merging the frames from the different HDR sequence IDs
                    merged_frameset = merging_filter.process(data)
                    merged_depth_frame = merged_frameset.as_frameset().get_depth_frame()

                    frame_ts = merged_depth_frame.get_frame_metadata(rs.frame_metadata_value.frame_timestamp)
                    check.is_true(frame_ts > first_series_last_merged_ts)
        finally:
            pipe.stop()
    finally:
        depth_sensor.set_option(rs.option.hdr_enabled, 0)  # disable hdr before next tests


# This tests checks that the previously saved merged frame is discarded after a pipe restart
def test_hdr_merge_discard_merged_frame(test_device):
    dev, ctx = test_device
    retry_on_exception(lambda: _hdr_merge_discard_merged_frame(dev, ctx))


def _hdr_start_stop_recover_manual_exposure_and_gain(dev, ctx):
    depth_sensor = dev.first_depth_sensor()
    gain_before_hdr = 50
    depth_sensor.set_option(rs.option.gain, gain_before_hdr)
    assert depth_sensor.get_option(rs.option.gain) == gain_before_hdr

    exposure_before_hdr = 5000
    depth_sensor.set_option(rs.option.exposure, exposure_before_hdr)
    assert depth_sensor.get_option(rs.option.exposure) == exposure_before_hdr

    depth_sensor.set_option(rs.option.hdr_enabled, 1)
    assert depth_sensor.get_option(rs.option.hdr_enabled) == 1

    cfg = rs.config()
    cfg.enable_stream(rs.stream.depth)
    pipe = rs.pipeline(ctx)
    pipe.start(cfg)
    try:
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
                    assert depth_sensor.get_option(rs.option.hdr_enabled) == 0

                elif iteration >= iteration_to_check_after_disable:
                    check.is_true(frame_gain == gain_before_hdr)

                    log.info(f"iteration: {iteration}")
                    log.info(f"iteration_to_check_after_disable: {iteration_to_check_after_disable}")
                    check.is_true(frame_exposure == exposure_before_hdr)
    finally:
        pipe.stop()


def test_hdr_start_stop_recover_manual_exposure_and_gain(test_device):
    dev, ctx = test_device
    retry_on_exception(lambda: _hdr_start_stop_recover_manual_exposure_and_gain(dev, ctx))


def _hdr_active_set_locked_options(dev, ctx):
    depth_sensor = dev.first_depth_sensor()
    
    if not depth_sensor.supports(rs.option.laser_power):
        pytest.skip("Laser power option not supported on this device")
    
    # setting laser ON if emitter_enabled is supported
    if depth_sensor.supports(rs.option.emitter_enabled):
        depth_sensor.set_option(rs.option.emitter_enabled, 1)

    laser_power_before_hdr = depth_sensor.get_option(rs.option.laser_power)

    time.sleep(1.5)

    depth_sensor.set_option(rs.option.hdr_enabled, 1)
    assert depth_sensor.get_option(rs.option.hdr_enabled) == 1

    # the following calls should not be performed and should send a LOG_WARNING
    if depth_sensor.supports(rs.option.enable_auto_exposure):
        depth_sensor.set_option(rs.option.enable_auto_exposure, 1)
        assert depth_sensor.get_option(rs.option.enable_auto_exposure) == 0

    if depth_sensor.supports(rs.option.emitter_enabled):
        depth_sensor.set_option(rs.option.emitter_enabled, 0)
        assert depth_sensor.get_option(rs.option.emitter_enabled) == 1

    if depth_sensor.supports(rs.option.emitter_on_off):
        depth_sensor.set_option(rs.option.emitter_on_off, 1)
        assert depth_sensor.get_option(rs.option.emitter_on_off) == 0

    depth_sensor.set_option(rs.option.laser_power, laser_power_before_hdr - 30)
    assert depth_sensor.get_option(rs.option.laser_power) == laser_power_before_hdr

    depth_sensor.set_option(rs.option.hdr_enabled, 0)  # disable hdr before next tests


# CONTROLS STABILITY WHILE HDR ACTIVE
def test_hdr_active_set_locked_options(test_device):
    dev, ctx = test_device
    retry_on_exception(lambda: _hdr_active_set_locked_options(dev, ctx))


def _hdr_streaming_set_locked_options(dev, ctx):
    depth_sensor = dev.first_depth_sensor()
    
    if not depth_sensor.supports(rs.option.laser_power):
        pytest.skip("Laser power option not supported on this device")
    
    # setting laser ON if emitter_enabled is supported
    if depth_sensor.supports(rs.option.emitter_enabled):
        depth_sensor.set_option(rs.option.emitter_enabled, 1)
    
    laser_power_before_hdr = depth_sensor.get_option(rs.option.laser_power)

    time.sleep(1.5)

    depth_sensor.set_option(rs.option.hdr_enabled, 1)
    assert depth_sensor.get_option(rs.option.hdr_enabled) == 1

    cfg = rs.config()
    cfg.enable_stream(rs.stream.depth)
    pipe = rs.pipeline(ctx)
    pipe.start(cfg)
    try:
        for iteration in range(1, 50):
            data = pipe.wait_for_frames()

            if iteration == 20:
                # the following calls should not be performed and should send a LOG_WARNING
                depth_sensor.set_option(rs.option.enable_auto_exposure, 1)
                assert depth_sensor.get_option(rs.option.enable_auto_exposure) == 0

                depth_sensor.set_option(rs.option.emitter_enabled, 0)
                assert depth_sensor.get_option(rs.option.emitter_enabled) == 1

                depth_sensor.set_option(rs.option.emitter_on_off, 1)
                assert depth_sensor.get_option(rs.option.emitter_on_off) == 0

                depth_sensor.set_option(rs.option.laser_power, laser_power_before_hdr - 30)
                assert depth_sensor.get_option(rs.option.laser_power) == laser_power_before_hdr
    finally:
        pipe.stop()
        depth_sensor.set_option(rs.option.hdr_enabled, 0)  # disable hdr before next tests


def test_hdr_streaming_set_locked_options(test_device):
    dev, ctx = test_device
    retry_on_exception(lambda: _hdr_streaming_set_locked_options(dev, ctx))


def _hdr_streaming_enable_runtime_exposure_update(dev, ctx):
    depth_sensor = dev.first_depth_sensor()
    exposure_range = depth_sensor.get_option_range(rs.option.exposure)
    gain_range = depth_sensor.get_option_range(rs.option.gain)

    depth_sensor.set_option(rs.option.hdr_enabled, 1)
    assert depth_sensor.get_option(rs.option.hdr_enabled) == 1

    cfg = rs.config()
    cfg.enable_stream(rs.stream.depth)
    cfg.enable_stream(rs.stream.infrared, 1)
    pipe = rs.pipeline(ctx)
    pipe.start(cfg)
    try:
        #change exposure and gain for seq id 1
        depth_sensor.set_option(rs.option.sequence_id, 1)  # seq id 1 is expected to be the default value
        assert depth_sensor.get_option(rs.option.sequence_id) == 1
        exp = depth_sensor.get_option(rs.option.exposure)
        assert depth_sensor.get_option(rs.option.exposure) == exposure_range.default - 1000  # w/a
        depth_sensor.set_option(rs.option.exposure, exposure_range.default - 2000)
        assert depth_sensor.get_option(rs.option.exposure) == exposure_range.default - 2000

        assert depth_sensor.get_option(rs.option.gain) == gain_range.default
        depth_sensor.set_option(rs.option.gain, gain_range.default + 2)
        assert depth_sensor.get_option(rs.option.gain) == gain_range.default + 2

        # change exposure and gain for seq id 2
        depth_sensor.set_option(rs.option.sequence_id, 2)# seq id 2 is expected to be the min value
        assert depth_sensor.get_option(rs.option.sequence_id) == 2
        exp = depth_sensor.get_option(rs.option.exposure)
        assert depth_sensor.get_option(rs.option.exposure) == exposure_range.min  # w/a
        depth_sensor.set_option(rs.option.exposure, exposure_range.default)
        assert depth_sensor.get_option(rs.option.exposure) == exposure_range.default

        assert depth_sensor.get_option(rs.option.gain) == gain_range.min
        depth_sensor.set_option(rs.option.gain, gain_range.default + 10)
        assert depth_sensor.get_option(rs.option.gain) == gain_range.default + 10
    finally:
        pipe.stop()
        depth_sensor.set_option(rs.option.hdr_enabled, 0)  # disable hdr before next tests


def test_hdr_streaming_enable_runtime_exposure_update_in_hdr_mode(test_device):
    dev, ctx = test_device
    retry_on_exception(lambda: _hdr_streaming_enable_runtime_exposure_update(dev, ctx))
