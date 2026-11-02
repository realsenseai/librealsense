# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import math
import pytest
import platform
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D500*"),
    pytest.mark.device_exclude("D585S"),  # no accel range control on the safety SKU
    pytest.mark.device_type_exclude("DDS"),  # USB/GMSL-focused: DDS advertises the option through a separate transport
]

GRAVITY = 9.80665  # m/s^2, ~1000 mg
GRAVITY_TOLERANCE = 0.5
FRAMES_TO_CHECK = 20


def test_accel_sensitivity_all_levels(test_device):
    dev, ctx = test_device
    motion_sensor = dev.first_motion_sensor()
    if not motion_sensor.supports(rs.option.accel_sensitivity):
        pytest.skip("Accel Sensitivity option not supported on this device/FW/driver")

    for value in range(len(rs.accel_sensitivity.__members__)):
        expected = float(value)
        motion_sensor.set_option(rs.option.accel_sensitivity, expected)
        assert motion_sensor.get_option(rs.option.accel_sensitivity) == expected, f"level {value}: readback before streaming"

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
            assert sensor.get_option(rs.option.accel_sensitivity) == expected, f"level {value}: readback while streaming"

            # The level selects the saturation point only: a unit at rest reads ~1g at every level
            checked = 0
            while checked < FRAMES_TO_CHECK:
                for frame in pipe.wait_for_frames():
                    if frame.get_profile().stream_type() != rs.stream.accel:
                        continue
                    a = frame.as_motion_frame().get_motion_data()
                    magnitude = math.sqrt(a.x ** 2 + a.y ** 2 + a.z ** 2)
                    assert abs(magnitude - GRAVITY) < GRAVITY_TOLERANCE, \
                        f"level {value}: accel magnitude {magnitude} m/s^2, expected ~{GRAVITY} (unit must be at rest)"
                    checked += 1
        finally:
            if started:
                pipe.stop()
