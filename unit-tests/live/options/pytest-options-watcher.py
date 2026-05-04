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

changed_options = 0
_depth_sensor = None  # module-level ref; cleared before each test to release old subscription


def _notification_callback(opt_list):
    global changed_options
    log.debug(f"notification_callback called with {len(opt_list)} options")
    for opt in opt_list:
        log.debug(f"    {opt.id} -> {opt.value}")
        if _depth_sensor and not _depth_sensor.is_option_read_only(opt.id):  # Ignore accidental temperature changes
            changed_options += 1


def setup_depth_watcher(test_device):
    """Release old sensor subscription, zero changed_options, register watcher, return depth_sensor."""
    global changed_options, _depth_sensor
    _depth_sensor = None  # drop old sensor ref → GC → old callback subscription cancelled
    changed_options = 0
    dev, ctx = test_device
    _depth_sensor = dev.first_depth_sensor()
    _depth_sensor.on_options_changed(_notification_callback)
    return _depth_sensor


def test_disable_auto_exposure(test_device):
    """Disable AE once; device state persists for subsequent tests."""
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()
    depth_sensor.set_option(rs.option.enable_auto_exposure, 0)
    assert depth_sensor.get_option(rs.option.enable_auto_exposure) == 0.0
    time.sleep(1.5)  # default options-watcher update interval is 1 second


def test_set_one_option(test_device):
    depth_sensor = setup_depth_watcher(test_device)

    current_gain = depth_sensor.get_option(rs.option.gain)
    depth_sensor.set_option(rs.option.gain, current_gain + 1)
    assert depth_sensor.get_option(rs.option.gain) == current_gain + 1
    time.sleep(1.5)  # default options-watcher update interval is 1 second
    assert changed_options == 1


def test_set_multiple_options(test_device):
    depth_sensor = setup_depth_watcher(test_device)

    current_gain = depth_sensor.get_option(rs.option.gain)
    depth_sensor.set_option(rs.option.gain, current_gain + 1)
    assert depth_sensor.get_option(rs.option.gain) == current_gain + 1
    current_exposure = depth_sensor.get_option(rs.option.exposure)
    depth_sensor.set_option(rs.option.exposure, current_exposure + 1)
    assert depth_sensor.get_option(rs.option.exposure) == current_exposure + 1
    time.sleep(2.5)  # default options-watcher update interval is 1 second, multiple options might be updated on different intervals
    assert changed_options == 2


def test_no_sporadic_changes(test_device):
    setup_depth_watcher(test_device)

    time.sleep(3)
    assert changed_options == 0


def test_cancel_subscription(test_device):
    global _depth_sensor
    setup_depth_watcher(test_device)

    _depth_sensor = None  # release sensor, cancelling its subscription
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()  # new sensor, no callback registered
    current_gain = depth_sensor.get_option(rs.option.gain)
    depth_sensor.set_option(rs.option.gain, current_gain + 1)
    assert depth_sensor.get_option(rs.option.gain) == current_gain + 1
    time.sleep(1.5)  # default options-watcher update interval is 1 second
    assert changed_options == 0
