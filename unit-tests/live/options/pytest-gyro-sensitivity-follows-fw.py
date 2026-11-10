# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import platform
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

# On USB (HID) the SDK must follow the FW sensitivity and write it only when the user sets the option.
# Note: the value set by one application stays in FW and is seen by the next one.
pytestmark = [
    pytest.mark.device_each("D455", "D500*"),
    pytest.mark.device_type_exclude("DDS", "GMSL"),
    pytest.mark.skipif(platform.machine() == "aarch64", reason="D455 not available on CI Jetson"),
]

OPTIONS = [rs.option.gyro_sensitivity, rs.option.accel_sensitivity]


def _motion_sensor_of_same_device(ctx, serial):
    for d in ctx.query_devices():
        if d.get_info(rs.camera_info.serial_number) == serial:
            return d, d.first_motion_sensor()
    pytest.fail(f"device {serial} not found in a new context")


def _stream_imu_once(dev, ctx):
    pipe = rs.pipeline(ctx)
    pipe.set_device(dev)
    cfg = rs.config()
    cfg.enable_stream(rs.stream.accel)
    cfg.enable_stream(rs.stream.gyro)
    pipe.start(cfg)
    pipe.stop()


@pytest.fixture
def restore_fw_sensitivity(test_device):
    """Remember the FW values before the test and write them back after it (FW keeps what the SDK writes)."""
    dev, ctx = test_device
    ms = dev.first_motion_sensor()
    original = {opt: ms.get_option(opt) for opt in OPTIONS if ms.supports(opt)}
    yield original
    for opt, value in original.items():
        ms.set_option(opt, value)
    _stream_imu_once(dev, ctx)


@pytest.mark.parametrize("option", OPTIONS, ids=lambda o: str(o))
def test_value_set_by_one_context_is_seen_by_the_next(test_device, restore_fw_sensitivity, option):
    dev, ctx = test_device
    ms = dev.first_motion_sensor()
    if not ms.supports(option):
        pytest.skip(f"{option} not supported on this device/FW")
    serial = dev.get_info(rs.camera_info.serial_number)

    original = restore_fw_sensitivity[option]
    new_value = 2.0 if original != 2.0 else 1.0

    # Context A: explicitly set, FW must receive it on open
    ms.set_option(option, new_value)
    _stream_imu_once(dev, ctx)

    # Context B: nothing set by the user, query() must report what FW holds, and opening must not change it
    ctx_b = rs.context()
    dev_b, ms_b = _motion_sensor_of_same_device(ctx_b, serial)
    assert ms_b.get_option(option) == new_value
    _stream_imu_once(dev_b, ctx_b)

    # Context C: still the same value after B opened the sensor without setting anything
    ctx_c = rs.context()
    _, ms_c = _motion_sensor_of_same_device(ctx_c, serial)
    assert ms_c.get_option(option) == new_value
