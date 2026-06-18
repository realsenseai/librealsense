# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Not frequently changing, no need to test for each commit

import pytest
import pyrealsense2 as rs
import pyrsutils as rsutils
from pytest_check import check
from rspy.timer import Timer
import time
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D555"),
    pytest.mark.context("nightly"),
    pytest.mark.context("dds"),
]


MAX_TIME_TO_WAIT_FOR_FRAMES = 10  # [sec]


def check_option_in_list(option_id, options_list):
    for option in options_list:
        if option_id == option:
            return True
    return False


def set_get_filter_option_value(embedded_filter, option, value_to_assign):
    initial_value = embedded_filter.get_option(option)
    option_step = embedded_filter.get_option_range(option).step
    embedded_filter.set_option(option, value_to_assign)
    check.almost_equal(embedded_filter.get_option(option), value_to_assign, abs=option_step)
    embedded_filter.set_option(option, initial_value)
    check.almost_equal(embedded_filter.get_option(option), initial_value, abs=option_step)


def is_fw_version_below(curr_fw, min_fw):
    current_fw_version = rsutils.version(curr_fw)
    min_fw_version = rsutils.version(min_fw)
    return current_fw_version < min_fw_version


@pytest.fixture
def depth_sensor(test_device):
    dev, _ = test_device
    return dev.first_depth_sensor()


@pytest.fixture
def depth_profile(depth_sensor):
    return next(p for p in
                depth_sensor.profiles if p.fps() == 30
                and p.stream_type() == rs.stream.depth
                and p.format() == rs.format.z16
                and p.as_video_stream_profile().width() == 640
                and p.as_video_stream_profile().height() == 360)


@pytest.fixture
def fw_version(test_device):
    dev, _ = test_device
    return dev.get_info(rs.camera_info.firmware_version)


# this test will get options values for the decimation embedded filter, from the depth sensor
# - get DDS device
# - get depth sensor
# - get embedded decimation filter
# - get options values for this filter
# - check 2 options available
def test_decimation_embedded_filter_options(depth_sensor, fw_version):
    if is_fw_version_below(fw_version, '7.56.36850.1229'):
        pytest.skip("Decimation Embedded Filter not supported in this FW")

    decimation_embedded_filter = depth_sensor.get_embedded_filter(rs.embedded_filter_type.decimation)
    assert decimation_embedded_filter

    decimation_options = decimation_embedded_filter.get_supported_options()
    check.equal(len(decimation_options), 2)
    check.is_true(check_option_in_list(rs.option.embedded_filter_enabled, decimation_options))
    check.is_true(check_option_in_list(rs.option.filter_magnitude, decimation_options))


def test_decimation_embedded_filter_set_get_options(depth_sensor, fw_version):
    if is_fw_version_below(fw_version, '7.56.36850.1229'):
        pytest.skip("Decimation Embedded Filter not supported in this FW")

    decimation_embedded_filter = depth_sensor.get_embedded_filter(rs.embedded_filter_type.decimation)
    set_get_filter_option_value(decimation_embedded_filter, rs.option.embedded_filter_enabled, 1.0)
    # not setting magnitude because it is R/O option
    check.equal(decimation_embedded_filter.get_option(rs.option.filter_magnitude), 2.0)


def test_decimation_embedded_filter_metadata(depth_sensor, depth_profile, fw_version):
    if is_fw_version_below(fw_version, '7.56.36850.1229'):
        pytest.skip("Decimation Embedded Filter not supported in this FW")

    decimation_embedded_filter = depth_sensor.get_embedded_filter(rs.embedded_filter_type.decimation)

    state = {"waiting_for_test": False, "decimation_enabled": False}
    wait_for_frames_timer = Timer(MAX_TIME_TO_WAIT_FOR_FRAMES)

    def decimation_check_callback(frame):
        # Only stop waiting once we've actually checked a frame carrying the embedded_filters
        # metadata - otherwise an early frame with no metadata could silently pass the test.
        if frame.supports_frame_metadata(rs.frame_metadata_value.embedded_filters):
            md_val = frame.get_frame_metadata(rs.frame_metadata_value.embedded_filters)
            value_to_check = 1 if state["decimation_enabled"] else 0
            check.equal(md_val & 1, value_to_check)
            state["waiting_for_test"] = False

    def stream_and_check_decimation_filter():
        depth_sensor.open(depth_profile)
        depth_sensor.start(decimation_check_callback)
        wait_for_frames_timer.start()
        state["waiting_for_test"] = True
        while state["waiting_for_test"] and not wait_for_frames_timer.has_expired():
            time.sleep(0.5)
        check.is_true(not wait_for_frames_timer.has_expired())
        depth_sensor.stop()
        depth_sensor.close()

    def enable_decimation_filter():
        decimation_embedded_filter.set_option(rs.option.embedded_filter_enabled, 1.0)
        check.equal(decimation_embedded_filter.get_option(rs.option.embedded_filter_enabled), 1.0)

    def disable_decimation_filter():
        decimation_embedded_filter.set_option(rs.option.embedded_filter_enabled, 0.0)
        check.equal(decimation_embedded_filter.get_option(rs.option.embedded_filter_enabled), 0.0)

    state["decimation_enabled"] = False
    stream_and_check_decimation_filter()
    time.sleep(1)
    enable_decimation_filter()
    state["decimation_enabled"] = True
    stream_and_check_decimation_filter()
    time.sleep(1)
    disable_decimation_filter()


# below FW number should be adjusted after Embedded Temporal Filter is in FW
def test_temporal_embedded_filter_options(depth_sensor, fw_version):
    if is_fw_version_below(fw_version, '9.9.9.9'):
        pytest.skip("Temporal Embedded Filter not supported in this FW")

    temporal_embedded_filter = depth_sensor.get_embedded_filter(rs.embedded_filter_type.temporal)
    assert temporal_embedded_filter

    temporal_options = temporal_embedded_filter.get_supported_options()
    check.equal(len(temporal_options), 4)
    check.is_true(check_option_in_list(rs.option.embedded_filter_enabled, temporal_options))
    check.is_true(check_option_in_list(rs.option.filter_smooth_alpha, temporal_options))
    check.is_true(check_option_in_list(rs.option.filter_smooth_delta, temporal_options))
    check.is_true(check_option_in_list(rs.option.holes_fill, temporal_options))


def test_temporal_embedded_filter_set_get_options(depth_sensor, fw_version):
    if is_fw_version_below(fw_version, '9.9.9.9'):
        pytest.skip("Temporal Embedded Filter not supported in this FW")

    temporal_embedded_filter = depth_sensor.get_embedded_filter(rs.embedded_filter_type.temporal)
    set_get_filter_option_value(temporal_embedded_filter, rs.option.embedded_filter_enabled, 1.0)
    set_get_filter_option_value(temporal_embedded_filter, rs.option.filter_smooth_alpha, 0.2)
    set_get_filter_option_value(temporal_embedded_filter, rs.option.filter_smooth_delta, 30.0)
    set_get_filter_option_value(temporal_embedded_filter, rs.option.holes_fill, 6)


def test_temporal_embedded_filter_metadata(depth_sensor, depth_profile, fw_version):
    if is_fw_version_below(fw_version, '9.9.9.9'):
        pytest.skip("Temporal Embedded Filter not supported in this FW")

    temporal_embedded_filter = depth_sensor.get_embedded_filter(rs.embedded_filter_type.temporal)

    state = {"waiting_for_test": False, "temporal_enabled": False}
    wait_for_frames_timer = Timer(MAX_TIME_TO_WAIT_FOR_FRAMES)

    def temporal_check_callback(frame):
        # Only stop waiting once we've actually checked a frame carrying the embedded_filters
        # metadata - otherwise an early frame with no metadata could silently pass the test.
        if frame.supports_frame_metadata(rs.frame_metadata_value.embedded_filters):
            md_val = frame.get_frame_metadata(rs.frame_metadata_value.embedded_filters)
            value_to_check = 1 if state["temporal_enabled"] else 0
            check.equal(md_val & 0b100, value_to_check)
            state["waiting_for_test"] = False

    def stream_and_check_temporal_filter():
        depth_sensor.open(depth_profile)
        depth_sensor.start(temporal_check_callback)
        wait_for_frames_timer.start()
        state["waiting_for_test"] = True
        while state["waiting_for_test"] and not wait_for_frames_timer.has_expired():
            time.sleep(0.5)
        check.is_true(not wait_for_frames_timer.has_expired())
        depth_sensor.stop()
        depth_sensor.close()

    def enable_temporal_filter():
        temporal_embedded_filter.set_option(rs.option.embedded_filter_enabled, 1.0)
        check.equal(temporal_embedded_filter.get_option(rs.option.embedded_filter_enabled), 1.0)

    def disable_temporal_filter():
        temporal_embedded_filter.set_option(rs.option.embedded_filter_enabled, 0.0)
        check.equal(temporal_embedded_filter.get_option(rs.option.embedded_filter_enabled), 0.0)

    state["temporal_enabled"] = False
    stream_and_check_temporal_filter()
    time.sleep(1)
    enable_temporal_filter()
    state["temporal_enabled"] = True
    stream_and_check_temporal_filter()
    time.sleep(1)
    disable_temporal_filter()
