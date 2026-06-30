# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Not frequently changing, no need to test for each commit

import pytest
from rspy import tests_wrapper as tw
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


@pytest.fixture
def safety_sensor_with_wrapper(test_device):
    dev, _ = test_device
    safety_sensor = dev.first_safety_sensor()
    tw.start_wrapper(dev)
    yield safety_sensor
    tw.stop_wrapper(dev)


def test_valid_read_from_index_0(safety_sensor):
    safety_sensor.get_safety_preset(0)


def test_valid_read_from_index_1(safety_sensor):
    safety_sensor.get_safety_preset(1)


def test_valid_read_and_write_from_index_1_to_0(safety_sensor_with_wrapper):
    safety_preset_at_one = safety_sensor_with_wrapper.get_safety_preset(1)
    safety_sensor_with_wrapper.set_safety_preset(0, safety_preset_at_one)


def test_valid_read_and_write_from_index_1_to_2(safety_sensor_with_wrapper):
    safety_preset_at_one = safety_sensor_with_wrapper.get_safety_preset(1)
    safety_sensor_with_wrapper.set_safety_preset(2, safety_preset_at_one)


def test_valid_read_and_write_from_index_63(safety_sensor_with_wrapper):
    safety_preset_at_63 = safety_sensor_with_wrapper.get_safety_preset(63)
    safety_sensor_with_wrapper.set_safety_preset(63, safety_preset_at_63)


def test_invalid_read_index_out_of_range(safety_sensor_with_wrapper):
    with pytest.raises(Exception):
        safety_sensor_with_wrapper.get_safety_preset(64)


def test_invalid_write_index_out_of_range(safety_sensor_with_wrapper):
    safety_preset_at_zero = safety_sensor_with_wrapper.get_safety_preset(0)
    with pytest.raises(Exception):
        safety_sensor_with_wrapper.set_safety_preset(64, safety_preset_at_zero)
