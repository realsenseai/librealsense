# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Diagnostic for intermittent enumeration where the device comes up without
# the safety sensor (observed after hub-driven resets, e.g. via Acroname).
# Runs N times; the conftest recycles the device between repeat iterations
# (see module_device_setup is_new_repeat_pass), so each iteration exercises
# a fresh boot.

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [pytest.mark.device_each("D585S")]


@pytest.mark.repeat(5)
def test_safety_sensor_present(test_device):
    dev, _ = test_device

    sensor_names = [s.get_info(rs.camera_info.name) for s in dev.sensors
                    if s.supports(rs.camera_info.name)]
    log.info("Enumerated sensors: %s", sensor_names)

    # first_safety_sensor() raises "Could not find requested sensor type!"
    # when the device enumerated without it -- the exact symptom we are hunting.
    safety_sensor = dev.first_safety_sensor()
    assert safety_sensor is not None
