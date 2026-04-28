# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
import time
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device("D400*"),
    pytest.mark.device_each("D555"),
    pytest.mark.context("nightly"),
]


def make_callback(get_sensor):
    """Return a (callback, count) pair. get_sensor() returns the current sensor,
    used to skip read-only options (e.g. temperature) that may change spuriously."""
    count = [0]

    def notification_callback(opt_list):
        log.debug(f"notification_callback called with {len(opt_list)} options")
        for opt in opt_list:
            log.debug(f"    {opt.id} -> {opt.value}")
            if not get_sensor().is_option_read_only(opt.id):  # Ignore accidental temperature changes
                count[0] += 1

    return notification_callback, count


def test_disable_auto_exposure(test_device):
    """Disable AE once; device state persists for subsequent tests."""
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()
    depth_sensor.set_option(rs.option.enable_auto_exposure, 0)
    assert depth_sensor.get_option(rs.option.enable_auto_exposure) == 0.0
    time.sleep(1.5)  # default options-watcher update interval is 1 second


def test_set_one_option(test_device):
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()
    callback, count = make_callback(lambda: depth_sensor)
    depth_sensor.on_options_changed(callback)

    current_gain = depth_sensor.get_option(rs.option.gain)
    depth_sensor.set_option(rs.option.gain, current_gain + 1)
    assert depth_sensor.get_option(rs.option.gain) == current_gain + 1
    time.sleep(1.5)  # default options-watcher update interval is 1 second
    assert count[0] == 1


def test_set_multiple_options(test_device):
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()
    callback, count = make_callback(lambda: depth_sensor)
    depth_sensor.on_options_changed(callback)

    current_gain = depth_sensor.get_option(rs.option.gain)
    depth_sensor.set_option(rs.option.gain, current_gain + 1)
    assert depth_sensor.get_option(rs.option.gain) == current_gain + 1
    current_exposure = depth_sensor.get_option(rs.option.exposure)
    depth_sensor.set_option(rs.option.exposure, current_exposure + 1)
    assert depth_sensor.get_option(rs.option.exposure) == current_exposure + 1
    time.sleep(2.5)  # default options-watcher update interval is 1 second, multiple options might be updated on different intervals
    assert count[0] == 2


def test_no_sporadic_changes(test_device):
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()
    callback, count = make_callback(lambda: depth_sensor)
    depth_sensor.on_options_changed(callback)

    time.sleep(3)
    assert count[0] == 0


def test_cancel_subscription(test_device):
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()
    callback, count = make_callback(lambda: depth_sensor)
    depth_sensor.on_options_changed(callback)

    depth_sensor = dev.first_depth_sensor()  # Get new sensor, old sensor subscription is canceled
    current_gain = depth_sensor.get_option(rs.option.gain)
    depth_sensor.set_option(rs.option.gain, current_gain + 1)
    assert depth_sensor.get_option(rs.option.gain) == current_gain + 1
    time.sleep(1.5)  # default options-watcher update interval is 1 second
    assert count[0] == 0
