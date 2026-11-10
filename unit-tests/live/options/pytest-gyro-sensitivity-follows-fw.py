# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import platform
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

# On USB (HID) the SDK must follow the FW sensitivity default and write it only when the user sets the option
pytestmark = [
    pytest.mark.device_each("D455"),
    pytest.mark.device_each("D500*"),
    pytest.mark.device_type_exclude("DDS", "GMSL"),
    pytest.mark.skipif(platform.machine() == "aarch64", reason="D455 not available on CI Jetson"),
]


def _stream_imu_once(dev, ctx):
    pipe = rs.pipeline(ctx)
    pipe.set_device(dev)
    cfg = rs.config()
    cfg.enable_stream(rs.stream.accel)
    cfg.enable_stream(rs.stream.gyro)
    pipe.start(cfg)
    pipe.stop()


def test_open_does_not_override_fw_default(test_device):
    dev, ctx = test_device
    motion_sensor = dev.first_motion_sensor()
    if not motion_sensor.supports(rs.option.gyro_sensitivity):
        pytest.skip("Gyro Sensitivity option not supported on this device/FW")

    fw_default = motion_sensor.get_option(rs.option.gyro_sensitivity)
    log.info(f"gyro sensitivity reported before open: {fw_default}")

    _stream_imu_once(dev, ctx)

    assert motion_sensor.get_option(rs.option.gyro_sensitivity) == fw_default


def test_user_value_stays_in_fw(test_device):
    dev, ctx = test_device
    motion_sensor = dev.first_motion_sensor()
    if not motion_sensor.supports(rs.option.gyro_sensitivity):
        pytest.skip("Gyro Sensitivity option not supported on this device/FW")

    original = motion_sensor.get_option(rs.option.gyro_sensitivity)
    new_value = 3.0 if original != 3.0 else 2.0
    try:
        motion_sensor.set_option(rs.option.gyro_sensitivity, new_value)
        _stream_imu_once(dev, ctx)

        # A new context has no cached user value: the option must report what FW holds
        ctx2 = rs.context()
        dev2 = ctx2.query_devices()[0]
        readback = dev2.first_motion_sensor().get_option(rs.option.gyro_sensitivity)
        assert readback == new_value
    finally:
        motion_sensor.set_option(rs.option.gyro_sensitivity, original)
        _stream_imu_once(dev, ctx)
