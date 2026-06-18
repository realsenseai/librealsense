# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from pytest_check import check
import logging
log = logging.getLogger(__name__)

# disabled until HKR FW will be stable
pytestmark = [
    pytest.mark.device_each("D585S"),
    pytest.mark.skip(reason="disabled until HKR FW will be stable"),
]


ITERATIONS_COUNT = 20


# since we see many regressions on operational mode switching we add a short stress test
def test_operational_mode_stress(test_device):
    device, _ = test_device
    safety_sensor = device.first_safety_sensor()

    for i in range(ITERATIONS_COUNT):
        log.debug("stress test iteration: %s", i)
        log.debug("command service mode")
        safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.service)
        check.equal(safety_sensor.get_option(rs.option.safety_mode), float(rs.safety_mode.service))

        log.debug("command standby mode")
        safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.standby)
        check.equal(safety_sensor.get_option(rs.option.safety_mode), float(rs.safety_mode.standby))

        log.debug("command run mode")
        safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.run)
        check.equal(safety_sensor.get_option(rs.option.safety_mode), float(rs.safety_mode.run))
