# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2020 RealSense, Inc. All Rights Reserved.

# Currently, we exclude D457 and D401 as it's failing

import platform
import pytest
import pyrealsense2 as rs
import time
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device("D400*"),
    pytest.mark.device_exclude("D457"),
    pytest.mark.device_exclude("D401"),
    pytest.mark.context("nightly"),
]

# Options that may cause expected frame drops and should be skipped
_D400_OPTIONS_TO_IGNORE = [
    rs.option.visual_preset,       # frame drops are expected during visual_preset change
    rs.option.inter_cam_sync_mode, # frame drops are expected during inter_cam_sync_mode change
    rs.option.emitter_frequency,   # Not allowed to be set during streaming
    rs.option.auto_exposure_mode,  # Not allowed to be set during streaming
]


class FrameDropChecker:
    """Thread-safe frame drop checker for use as a streaming callback."""

    def __init__(self, product_line, is_depth=True):
        self._prev = -1
        self._errors = []
        self._after_set_option = False
        self._product_line = product_line
        self._is_depth = is_depth

    def get_allowed_drops(self):
        # On Linux, up to 4 frame drops can occur after setting control values (RS5-7148)
        if platform.system() == 'Linux' and self._after_set_option:
            return 4
        return 1

    def __call__(self, frame):
        frame_number = frame.get_frame_number()
        allow_reset = self._is_depth and self._product_line == "D400"
        allowed = self.get_allowed_drops()

        if self._prev > 0 and not (allow_reset and frame_number < 5):
            dropped = frame_number - (self._prev + 1)
            if dropped > allowed:
                self._errors.append(f"{dropped} frame(s) dropped before frame {frame_number}")
            elif dropped < 0:
                self._errors.append(f"Frames out of order: got {frame_number} after {self._prev}")

        self._prev = frame_number

    def assert_no_errors(self):
        if self._errors:
            pytest.fail("\n".join(self._errors))


def set_new_value(checker, sensor, option, value):
    checker._after_set_option = True
    try:
        sensor.set_option(option, value)
        time.sleep(0.5)  # collect frames
    finally:
        checker._after_set_option = False


def run_option_changes(sensor, checker, product_line):
    """Iterate all writable numeric options on sensor, set each to a new value and restore."""
    options_to_ignore = _D400_OPTIONS_TO_IGNORE if product_line == "D400" else []

    for option in sensor.get_supported_options():
        if option in options_to_ignore:
            continue
        if sensor.is_option_read_only(option):
            continue
        try:
            orig_opt_value = sensor.get_option_value(option)
            if orig_opt_value.type in (rs.option_type.integer, rs.option_type.float):
                old_value = orig_opt_value.value
                opt_range = sensor.get_option_range(option)
                new_value = opt_range.min if old_value != opt_range.min else opt_range.max
                log.debug(f"{option}: {old_value} -> {new_value}")
                set_new_value(checker, sensor, option, new_value)
                sensor.set_option(option, old_value)  # Restore
        except Exception as e:
            pytest.fail(f"Exception while setting option {option}: {e}")


def test_laser_power_frame_drops(test_device_wrapped):
    """No frame drops when sweeping laser power through its full range."""
    dev, ctx = test_device_wrapped
    product_line = dev.get_info(rs.camera_info.product_line)
    depth_sensor = dev.first_depth_sensor()
    depth_profile = next(p for p in depth_sensor.profiles if p.is_default())
    checker = FrameDropChecker(product_line, is_depth=True)

    depth_sensor.open(depth_profile)
    depth_sensor.start(checker)
    try:
        curr_value = depth_sensor.get_option(rs.option.laser_power)
        opt_range = depth_sensor.get_option_range(rs.option.laser_power)
        new_value = opt_range.min
        while new_value <= opt_range.max:
            set_new_value(checker, depth_sensor, rs.option.laser_power, new_value)
            new_value += opt_range.step
        set_new_value(checker, depth_sensor, rs.option.laser_power, curr_value)  # Restore
    finally:
        depth_sensor.stop()
        depth_sensor.close()

    checker.assert_no_errors()


def test_depth_options_frame_drops(test_device_wrapped):
    """No frame drops when cycling through all writable depth sensor options."""
    dev, ctx = test_device_wrapped
    product_line = dev.get_info(rs.camera_info.product_line)
    depth_sensor = dev.first_depth_sensor()
    depth_profile = next(p for p in depth_sensor.profiles if p.is_default())
    checker = FrameDropChecker(product_line, is_depth=True)

    depth_sensor.open(depth_profile)
    depth_sensor.start(checker)
    try:
        time.sleep(0.5)  # let stream settle
        run_option_changes(depth_sensor, checker, product_line)
    finally:
        depth_sensor.stop()
        depth_sensor.close()

    checker.assert_no_errors()


def test_color_options_frame_drops(test_device_wrapped):
    """No frame drops when cycling through all writable color sensor options."""
    dev, ctx = test_device_wrapped
    product_line = dev.get_info(rs.camera_info.product_line)
    product_name = dev.get_info(rs.camera_info.name)

    try:
        color_sensor = dev.first_color_sensor()
    except RuntimeError:
        if 'D421' in product_name or 'D405' in product_name:
            pytest.skip("No color sensor")
        raise

    color_profile = next(p for p in color_sensor.profiles if p.is_default())
    checker = FrameDropChecker(product_line, is_depth=False)

    color_sensor.open(color_profile)
    color_sensor.start(checker)
    try:
        time.sleep(0.5)  # let stream settle
        run_option_changes(color_sensor, checker, product_line)
    finally:
        color_sensor.stop()
        color_sensor.close()

    checker.assert_no_errors()
