# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy.pytest.device_helpers import require_min_fw_version
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D585"),  # matches D585 and D585S (prefix match)
    pytest.mark.context("nightly"),
]


# Constants
FRAMES_TO_COLLECT = 3
# Accel 400 Hz and gyro 100 Hz exist only on D585 (not D585S) over USB, from FW 7.62
D585_ONLY_RATES = { rs.stream.accel: 400, rs.stream.gyro: 100 }


def _check_imu_streaming(test_device, stream, fps):
    dev, ctx = test_device
    if fps == D585_ONLY_RATES[stream]:
        if "D585S" in dev.get_info( rs.camera_info.name ) or dev.get_info( rs.camera_info.connection_type ) == "GMSL":
            pytest.skip( f"{stream} at {fps} Hz is D585 USB only" )
        require_min_fw_version( dev, rsutils.version( "7.62.0.0" ), f"{stream} at {fps} Hz" )
    cfg = rs.config()
    cfg.enable_device(dev.get_info(rs.camera_info.serial_number))
    cfg.enable_stream(stream, 0, rs.format.motion_xyz32f, fps)
    pipe = rs.pipeline(ctx)
    pipe.start(cfg)
    iterations = 0
    while iterations < FRAMES_TO_COLLECT:
        iterations += 1
        f = pipe.wait_for_frames()
    assert iterations == FRAMES_TO_COLLECT
    pipe.stop()


@pytest.mark.parametrize("fps", [100, 200, 400])
def test_accel_streaming(test_device, fps):
    _check_imu_streaming(test_device, rs.stream.accel, fps)


@pytest.mark.parametrize("fps", [100, 200, 400])
def test_gyro_streaming(test_device, fps):
    _check_imu_streaming(test_device, rs.stream.gyro, fps)
