# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import platform
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D455"),
    pytest.mark.device_each("D500*"),
    pytest.mark.device_type_exclude("DDS"),  # USB-focused: DDS advertises the option through a separate transport
    pytest.mark.skipif(platform.machine() == "aarch64", reason="D455 not available on CI Jetson"),
]


def test_gyro_sensitivity_all_levels(test_device):
    dev, ctx = test_device
    motion_sensor = dev.first_motion_sensor()
    if not motion_sensor.supports(rs.option.gyro_sensitivity):
        pytest.skip("Gyro Sensitivity option not supported on this device/FW")

    for value in range(5):  # RS2_GYRO_SENSITIVITY_* enum: 0..4
        expected = float(value)
        motion_sensor.set_option(rs.option.gyro_sensitivity, expected)

        pipe = rs.pipeline(ctx)
        pipe.set_device(dev)
        cfg = rs.config()
        cfg.enable_stream(rs.stream.accel)
        cfg.enable_stream(rs.stream.gyro)

        started = False
        try:
            profile = pipe.start(cfg)
            started = True
            sensor = profile.get_device().first_motion_sensor()
            readback = sensor.get_option(rs.option.gyro_sensitivity)
            assert readback == expected, f"level {value}: readback {readback}"
        finally:
            if started:
                pipe.stop()
