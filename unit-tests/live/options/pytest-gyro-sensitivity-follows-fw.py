# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import platform
import pyrealsense2 as rs
from imu_sensitivity_helpers import restore_sensitivity, stream_imu_once
import logging
log = logging.getLogger(__name__)

# On USB (HID) the SDK must follow the FW sensitivity and write it only when the user sets the option.
# Note: the value set by one application stays in FW and is seen by the next one.
pytestmark = [
    pytest.mark.device_each("D455", "D500*"),
    pytest.mark.device_type_exclude("DDS", "GMSL"),
    pytest.mark.skipif(platform.machine() == "aarch64", reason="D455 not available on CI Jetson"),
    pytest.mark.skipif(platform.system() != "Linux", reason="FW sensitivity read-back is implemented on Linux backends only"),
]

OPTIONS = [rs.option.gyro_sensitivity, rs.option.accel_sensitivity]


def _motion_sensor_of_same_device(ctx, serial):
    for d in ctx.query_devices():
        if d.get_info(rs.camera_info.serial_number) == serial:
            return d, d.first_motion_sensor()
    pytest.fail(f"device {serial} not found in a new context")


@pytest.fixture
def restore_fw_sensitivity(test_device):
    dev, ctx = test_device
    ms = dev.first_motion_sensor()
    original = {opt: ms.get_option(opt) for opt in OPTIONS if ms.supports(opt)}
    yield original
    restore_sensitivity(dev, ctx, original)


@pytest.mark.parametrize("level", [0.0, 1.0, 2.0, 3.0], ids=lambda v: f"level{int(v)}")  # valid for both gyro and accel
@pytest.mark.parametrize("option", OPTIONS, ids=lambda o: str(o))
def test_value_set_by_one_context_is_seen_by_the_next(test_device, restore_fw_sensitivity, option, level):
    dev, ctx = test_device
    ms = dev.first_motion_sensor()
    if not ms.supports(option):
        pytest.skip(f"{option} not supported on this device/FW")
    serial = dev.get_info(rs.camera_info.serial_number)

    new_value = level

    # Context A: explicitly set, FW must receive it on open
    ms.set_option(option, new_value)
    stream_imu_once(dev, ctx)

    # Context B: nothing set by the user, query() must report what FW holds, and opening must not change it
    ctx_b = rs.context()
    dev_b, ms_b = _motion_sensor_of_same_device(ctx_b, serial)
    assert ms_b.get_option(option) == new_value
    stream_imu_once(dev_b, ctx_b)

    # Context C: still the same value after B opened the sensor without setting anything
    ctx_c = rs.context()
    _, ms_c = _motion_sensor_of_same_device(ctx_c, serial)
    assert ms_c.get_option(option) == new_value
