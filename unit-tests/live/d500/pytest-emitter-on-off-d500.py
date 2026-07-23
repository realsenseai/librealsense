# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from pytest_check import check
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D555"),
]


def test_emitter_on_off_set_get(test_device):
    dev, _ = test_device
    depth_sensor = dev.first_depth_sensor()

    if not depth_sensor.supports(rs.option.emitter_on_off):
        pytest.skip("Device does not support emitter_on_off option")

    orig = depth_sensor.get_option(rs.option.emitter_on_off)
    try:
        depth_sensor.set_option(rs.option.emitter_on_off, 1)
        check.equal(depth_sensor.get_option(rs.option.emitter_on_off), 1.0)
        depth_sensor.set_option(rs.option.emitter_on_off, 0)
        check.equal(depth_sensor.get_option(rs.option.emitter_on_off), 0.0)
    finally:
        depth_sensor.set_option(rs.option.emitter_on_off, orig)
