# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
import logging
from pytest_check import check
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_exclude("D421"),
    pytest.mark.device_exclude("D405"),
    pytest.mark.device_exclude("D401"),
    pytest.mark.device_each("D500*"),
    pytest.mark.context("nightly"),
]

color_options = [
    rs.option.backlight_compensation,
    rs.option.brightness,
    rs.option.contrast,
    rs.option.gamma,
    rs.option.hue,
    rs.option.saturation,
    rs.option.sharpness,
    rs.option.enable_auto_white_balance,
    rs.option.white_balance,
]

color_metadata = [
    rs.frame_metadata_value.backlight_compensation,
    rs.frame_metadata_value.brightness,
    rs.frame_metadata_value.contrast,
    rs.frame_metadata_value.gamma,
    rs.frame_metadata_value.hue,
    rs.frame_metadata_value.saturation,
    rs.frame_metadata_value.sharpness,
    rs.frame_metadata_value.auto_white_balance_temperature,
    rs.frame_metadata_value.manual_white_balance,
]


def check_option_and_metadata_values(color_sensor, option, metadata, value_to_set, frame):
    changed = color_sensor.get_option(option)
    check.equal(changed, value_to_set, f"Option {option}: expected {value_to_set}, got {changed}")
    if frame.supports_frame_metadata(metadata):
        changed_md = float(frame.get_frame_metadata(metadata))
        check.equal(changed_md, value_to_set, f"Metadata {metadata}: expected {value_to_set}, got {changed_md}")
    else:
        log.debug(f"metadata {metadata!r} not supported")


def test_rgb_options_metadata_consistency(test_device):
    """For each color option, set min/max/default and verify get_option and frame metadata agree."""
    dev, ctx = test_device

    color_sensor = dev.first_color_sensor()

    color_profile = next(
        (p for p in color_sensor.profiles
         if p.fps() == 30 # For faster run times, lower FPS will work as well but will increase run time
         and p.stream_type() == rs.stream.color
         and p.format() == rs.format.rgb8), # Make sure no raw or calibration profile selected, SDK converts native camera format to this common format.
        None
    )
    assert color_profile is not None, "Required 640x480@30fps YUYV color profile not available"

    color_sensor.open(color_profile)
    lrs_queue = rs.frame_queue(capacity=10, keep_frames=False)
    color_sensor.start(lrs_queue)

    try:
        iteration = 0
        option_index = -1
        value_to_set = None
        option = None
        option_range = None
        metadata = None
        # number of frames to wait between set_option and checking metadata
        # expected delay is ~120ms on Win and ~80-90ms on Linux
        num_of_frames_to_wait = 15

        while True:
            lrs_frame = lrs_queue.wait_for_frame(5000)

            if iteration == 0:
                option_index += 1
                if option_index == len(color_options):
                    break
                option = color_options[option_index]
                if not color_sensor.supports(option):
                    continue
                option_range = color_sensor.get_option_range(option)
                metadata = color_metadata[option_index]
                # Workaround for FW bug DSO-17221: explicitly disable AWB before setting white_balance
                if option == rs.option.white_balance:
                    log.debug(f"iteration {iteration}: setting enable_auto_white_balance to OFF")
                    color_sensor.set_option(rs.option.enable_auto_white_balance, 0)
                    assert color_sensor.get_option(rs.option.enable_auto_white_balance) == 0.0
                value_to_set = option_range.min
            elif iteration == (num_of_frames_to_wait + 1):
                value_to_set = option_range.max
            elif iteration == 2 * (num_of_frames_to_wait + 1):
                value_to_set = option_range.default

            if iteration % (num_of_frames_to_wait + 1) == 0:  # iterations 0, 16, 32
                log.debug(f"iteration {iteration}: setting option {option} to {value_to_set}")
                color_sensor.set_option(option, value_to_set)

            if (iteration + 1) % (num_of_frames_to_wait + 1) == 0:  # iterations 15, 31, 47
                log.debug(f"iteration {iteration}: checking metadata {metadata} vs option {option}")
                check_option_and_metadata_values(color_sensor, option, metadata, value_to_set, lrs_frame)

            iteration = (iteration + 1) % (3 * (num_of_frames_to_wait + 1))
    finally:
        if len(color_sensor.get_active_streams()) > 0:
            color_sensor.stop()
            color_sensor.close()
