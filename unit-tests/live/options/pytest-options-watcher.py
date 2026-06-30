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

# Generation counter: each setup_depth_watcher call bumps this, making previous callbacks no-ops.
_generation = 0
_depth_sensor = None


def setup_depth_watcher(test_device):
    """Register a fresh options-watcher and return (depth_sensor, count).

    Each call advances a generation counter.  Callbacks from earlier generations
    silently ignore events, so there is no need for explicit teardown — stale
    callbacks from crashed or retried tests cannot pollute the counter.

    count is a mutable [0] list; read it as count[0].
    """
    global _generation, _depth_sensor
    _generation += 1
    my_gen = _generation
    count = [0]

    def callback(opt_list):
        if my_gen != _generation:
            return  # stale callback from a previous test — ignore
        log.debug(f"notification_callback called with {len(opt_list)} options")
        for opt in opt_list:
            log.debug(f"    {opt.id} -> {opt.value}")
            if _depth_sensor and not _depth_sensor.is_option_read_only(opt.id):  # ignore temperature noise
                count[0] += 1

    dev, ctx = test_device
    _depth_sensor = dev.first_depth_sensor()
    _depth_sensor.on_options_changed(callback)
    time.sleep(0.5)  # let the options-watcher establish its baseline before making changes
    return _depth_sensor, count


def test_disable_auto_exposure(test_device):
    """Disable AE once; device state persists for subsequent tests."""
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()
    depth_sensor.set_option(rs.option.enable_auto_exposure, 0)
    assert depth_sensor.get_option(rs.option.enable_auto_exposure) == 0.0
    time.sleep(1.5)  # default options-watcher update interval is 1 second


def test_set_one_option(test_device):
    depth_sensor, count = setup_depth_watcher(test_device)

    current_gain = depth_sensor.get_option(rs.option.gain)
    depth_sensor.set_option(rs.option.gain, current_gain + 1)
    assert depth_sensor.get_option(rs.option.gain) == current_gain + 1
    time.sleep(1.5)  # default options-watcher update interval is 1 second
    assert count[0] == 1


def test_set_multiple_options(test_device):
    depth_sensor, count = setup_depth_watcher(test_device)

    current_gain = depth_sensor.get_option(rs.option.gain)
    depth_sensor.set_option(rs.option.gain, current_gain + 1)
    assert depth_sensor.get_option(rs.option.gain) == current_gain + 1
    current_exposure = depth_sensor.get_option(rs.option.exposure)
    depth_sensor.set_option(rs.option.exposure, current_exposure + 1)
    assert depth_sensor.get_option(rs.option.exposure) == current_exposure + 1
    time.sleep(2.5)  # default options-watcher update interval is 1 second, multiple options might be updated on different intervals
    assert count[0] == 2


def test_no_sporadic_changes(test_device):
    _, count = setup_depth_watcher(test_device)

    time.sleep(3)
    assert count[0] == 0


def test_cancel_subscription(test_device):
    global _depth_sensor, _generation
    # Index [1] avoids keeping a local reference to the sensor — only _depth_sensor holds it.
    count = setup_depth_watcher(test_device)[1]

    _generation += 1       # invalidate the callback immediately
    _depth_sensor = None   # last reference gone → GC → subscription cancelled

    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()  # new sensor, no callback registered
    current_gain = depth_sensor.get_option(rs.option.gain)
    depth_sensor.set_option(rs.option.gain, current_gain + 1)
    assert depth_sensor.get_option(rs.option.gain) == current_gain + 1
    time.sleep(1.5)  # default options-watcher update interval is 1 second
    assert count[0] == 0
