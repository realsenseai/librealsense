# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Not frequently changing, no need to test for each commit

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D585S"),
    pytest.mark.context("nightly"),
]


@pytest.fixture
def safety_sensor(test_device):
    dev, _ = test_device
    return dev.first_safety_sensor()


def test_safety_sensor_supports_option(safety_sensor):
    assert safety_sensor.supports(rs.option.safety_preset_active_index)


def test_valid_get_set_active_index(safety_sensor):
    # default index at start should be 0
    current_index = safety_sensor.get_option(rs.option.safety_preset_active_index)
    assert int(current_index) == 0

    safety_sensor.set_option(rs.option.safety_preset_active_index, 1)
    current_index = safety_sensor.get_option(rs.option.safety_preset_active_index)
    assert int(current_index) == 1

    safety_sensor.set_option(rs.option.safety_preset_active_index, 20)
    current_index = safety_sensor.get_option(rs.option.safety_preset_active_index)
    assert int(current_index) == 20

    safety_sensor.set_option(rs.option.safety_preset_active_index, 63)
    current_index = safety_sensor.get_option(rs.option.safety_preset_active_index)
    assert int(current_index) == 63


def test_invalid_set_index_out_of_range(safety_sensor):
    with pytest.raises(Exception):
        safety_sensor.set_option(rs.option.safety_preset_active_index, 64)
