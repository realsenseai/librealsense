# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

# Supported on D400 devices with global shutter and FW version 5.12.10.11 and above. https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-device.cpp#L1026

import pytest
import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy.pytest.device_helpers import require_min_fw_version
import logging
log = logging.getLogger(__name__)

pytestmark = [pytest.mark.device("D400*")]

_depth_sensor = None


@pytest.fixture(autouse=True)
def _require_auto_limit_support(test_device):
    global _depth_sensor
    if _depth_sensor is not None:
        return
    dev, _ = test_device
    sensor = dev.first_depth_sensor()
    _check_required_options(sensor)
    require_min_fw_version(dev, rsutils.version(5, 12, 10, 11), "AUTO EXPOSURE LIMIT")
    _depth_sensor = sensor  # set only after all checks pass

# 1. Scenario 1:
    #          - Change control value few times
    #         - Turn toggle off
    #          - Turn toggle on
    #          - Check that control limit value is the latest value
# 2. Scenario 2:
    #       - Init 2 devices
    #        - toggle on both dev1 and dev2 and set two distinct values for the auto-exposure/gain.
    #        - toggle both dev1and dev2 off.
    #        2.1. toggle dev1 on :
    #                  * verify that the limit value is the value that was stored (cached) in dev1.
    #                  * verify that for dev2 both the limit and the toggle values are similar to those of dev1
    #        2.2. toggle dev2 on :
    #                  * verify that the limit value is the value that was stored (cached) in dev2.


def _check_required_options(depth_sensor):
    required_options = [rs.option.auto_exposure_limit_toggle, rs.option.auto_exposure_limit, rs.option.auto_gain_limit_toggle, rs.option.auto_gain_limit]
    for option in required_options:
        if not depth_sensor.supports(option):
            pytest.skip(f"Device does not support {option}, skipping test...")


def test_auto_exposure_toggle_one_device(test_device):
    dev, _ = test_device
    depth_sensor = dev.first_depth_sensor()

    # Scenario 1:
    sensor = depth_sensor
    option_range = sensor.get_option_range(rs.option.auto_exposure_limit)
    values = [option_range.min + 5.0, option_range.max / 4.0, option_range.max * 0.75]
    for val in values:
        sensor.set_option(rs.option.auto_exposure_limit, val)
    sensor.set_option(rs.option.auto_exposure_limit_toggle, 0.0)  # off
    sensor.set_option(rs.option.auto_exposure_limit_toggle, 1.0)  # on
    limit = sensor.get_option(rs.option.auto_exposure_limit)
    assert limit == values[2]


def test_auto_exposure_two_devices(test_device):
    _, ctx = test_device

    # Scenario 2: 2 device instances (s1 and s2) pointing to the same physical sensor.
    # Each instance has its own independent SW cache for the limit value.
    # The exposure limit value is cached in SW and is only applied to HW when the toggle
    # is turned ON. Reading the limit while the toggle is OFF returns the current HW value,
    # not the SW cached value.
    device1 = ctx.query_devices().front()
    s1 = device1.first_depth_sensor()
    device2 = ctx.query_devices().front()
    s2 = device2.first_depth_sensor()

    option_range = s1.get_option_range(rs.option.auto_exposure_limit)  # same range for both instances
    # Set distinct limit values on each instance while toggle is OFF — values stay in SW cache only
    s1.set_option(rs.option.auto_exposure_limit, option_range.max * 0.25)  # cached in s1, not yet applied to HW
    s1.set_option(rs.option.auto_exposure_limit_toggle, 0.0)  # off
    s2.set_option(rs.option.auto_exposure_limit, option_range.max * 0.75)  # cached in s2, not yet applied to HW
    s2.set_option(rs.option.auto_exposure_limit_toggle, 0.0)  # off

    # 2.1 - Turn toggle ON for s1: its cached value (max*0.25) is now applied to HW
    s1.set_option(rs.option.auto_exposure_limit_toggle, 1.0)  # on
    limit1 = s1.get_option(rs.option.auto_exposure_limit)
    assert limit1 == option_range.max * 0.25
    # s2 toggle is still OFF: reading the limit returns the current HW value (set by s1), not s2's SW cache
    limit2 = s2.get_option(rs.option.auto_exposure_limit)
    assert limit1 == limit2

    # 2.2 - Turn toggle ON for s2: its own cached value (max*0.75) is now applied to HW
    s2.set_option(rs.option.auto_exposure_limit_toggle, 1.0)  # on
    limit2 = s2.get_option(rs.option.auto_exposure_limit)
    assert limit2 == option_range.max * 0.75


def test_gain_toggle_one_device(test_device):
    dev, _ = test_device
    depth_sensor = dev.first_depth_sensor()

    # Scenario 1:
    sensor = depth_sensor
    option_range = sensor.get_option_range(rs.option.auto_gain_limit)
    # 1. Scenario 1:
    # - Change control value few times
    # - Turn toggle off
    # - Turn toggle on
    # - Check that control limit value is the latest value
    values = [option_range.min + 5.0, option_range.max / 4.0, option_range.max * 0.75]
    for val in values:
        sensor.set_option(rs.option.auto_gain_limit, val)
    sensor.set_option(rs.option.auto_gain_limit_toggle, 0.0)  # off
    sensor.set_option(rs.option.auto_gain_limit_toggle, 1.0)  # on
    limit = sensor.get_option(rs.option.auto_gain_limit)
    assert limit == values[2]


def test_gain_toggle_two_devices(test_device):
    _, ctx = test_device

    # Scenario 2: 2 device instances (s1 and s2) pointing to the same physical sensor.
    # Each instance has its own independent SW cache for the limit value.
    # The gain limit value is cached in SW and is only applied to HW when the toggle
    # is turned ON. Reading the limit while the toggle is OFF returns the current HW value,
    # not the SW cached value.
    device1 = ctx.query_devices().front()
    s1 = device1.first_depth_sensor()
    device2 = ctx.query_devices().front()
    s2 = device2.first_depth_sensor()

    option_range = s1.get_option_range(rs.option.auto_gain_limit)  # same range for both instances
    # Set distinct limit values on each instance while toggle is OFF — values stay in SW cache only
    s1.set_option(rs.option.auto_gain_limit, option_range.max * 0.25)  # cached in s1, not yet applied to HW
    s1.set_option(rs.option.auto_gain_limit_toggle, 0.0)  # off
    s2.set_option(rs.option.auto_gain_limit, option_range.max * 0.75)  # cached in s2, not yet applied to HW
    s2.set_option(rs.option.auto_gain_limit_toggle, 0.0)  # off

    # 2.1 - Turn toggle ON for s1: its cached value (max*0.25) is now applied to HW
    s1.set_option(rs.option.auto_gain_limit_toggle, 1.0)  # on
    limit1 = s1.get_option(rs.option.auto_gain_limit)
    assert limit1 == option_range.max * 0.25
    # s2 toggle is still OFF: reading the limit returns the current HW value (set by s1), not s2's SW cache
    limit2 = s2.get_option(rs.option.auto_gain_limit)
    assert limit1 == limit2

    # 2.2 - Turn toggle ON for s2: its own cached value (max*0.75) is now applied to HW
    s2.set_option(rs.option.auto_gain_limit_toggle, 1.0)  # on
    limit2 = s2.get_option(rs.option.auto_gain_limit)
    assert limit2 == option_range.max * 0.75
