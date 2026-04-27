# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

# LibCI doesn't have D435i so //test:device D435I// is disabled for now

import pytest
import platform
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device("D455"),
    pytest.mark.skipif(platform.machine() == "aarch64", reason="D455 not available on CI Jetson"),
]

gyro_sensitivity_value = 4.0


def test_pipeline_set_device(test_device):
    dev, ctx = test_device
    motion_sensor = dev.first_motion_sensor()
    pipe = rs.pipeline(ctx)
    pipe.set_device(dev)

    motion_sensor.set_option(rs.option.gyro_sensitivity, gyro_sensitivity_value)

    cfg = rs.config()
    cfg.enable_stream(rs.stream.accel)
    cfg.enable_stream(rs.stream.gyro)

    profile = pipe.start(cfg)
    device_from_profile = profile.get_device()
    sensor = device_from_profile.first_motion_sensor()
    sensor_gyro_sensitivity_value = sensor.get_option(rs.option.gyro_sensitivity)
    assert gyro_sensitivity_value == sensor_gyro_sensitivity_value
    pipe.stop()
