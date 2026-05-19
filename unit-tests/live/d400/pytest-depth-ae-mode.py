# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

# AE mode is supported on D455 with FW version 5.15.0.0 and above https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-device.cpp#L835

import pytest
import platform
import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy import tests_wrapper as tw
from rspy.pytest.device_helpers import require_min_fw_version
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device("D455"),
    pytest.mark.skipif(platform.machine() == "aarch64", reason="D455 not available on CI Jetson"),
]

REGULAR = 0.0
ACCELERATED = 1.0


@pytest.fixture(autouse=True)
def _start_stop_wrapper(test_device):
    dev, _ = test_device
    tw.start_wrapper(dev)
    yield
    tw.stop_wrapper(dev)


@pytest.fixture
def depth_sensor(test_device):
    dev, _ = test_device
    require_min_fw_version(dev, rsutils.version(5, 15, 0, 0), "DEPTH_AUTO_EXPOSURE_MODE")
    return dev.first_depth_sensor()


def test_verify_camera_ae_mode_default_is_regular(depth_sensor):
    assert depth_sensor.get_option(rs.option.auto_exposure_mode) == REGULAR


def test_verify_can_set_when_auto_exposure_on(depth_sensor):
    depth_sensor.set_option(rs.option.enable_auto_exposure, True)
    assert bool(depth_sensor.get_option(rs.option.enable_auto_exposure)) == True
    depth_sensor.set_option(rs.option.auto_exposure_mode, ACCELERATED)
    assert depth_sensor.get_option(rs.option.auto_exposure_mode) == ACCELERATED
    depth_sensor.set_option(rs.option.auto_exposure_mode, REGULAR)
    assert depth_sensor.get_option(rs.option.auto_exposure_mode) == REGULAR


def test_set_during_idle_mode(depth_sensor):
    depth_sensor.set_option(rs.option.enable_auto_exposure, False)
    assert bool(depth_sensor.get_option(rs.option.enable_auto_exposure)) == False
    depth_sensor.set_option(rs.option.auto_exposure_mode, ACCELERATED)
    assert depth_sensor.get_option(rs.option.auto_exposure_mode) == ACCELERATED
    depth_sensor.set_option(rs.option.auto_exposure_mode, REGULAR)
    assert depth_sensor.get_option(rs.option.auto_exposure_mode) == REGULAR


def test_set_during_streaming_mode_not_allowed(depth_sensor):
    # Reset option to REGULAR
    depth_sensor.set_option(rs.option.enable_auto_exposure, False)
    assert bool(depth_sensor.get_option(rs.option.enable_auto_exposure)) == False
    depth_sensor.set_option(rs.option.auto_exposure_mode, REGULAR)
    assert depth_sensor.get_option(rs.option.auto_exposure_mode) == REGULAR
    # Start streaming
    depth_profile = next(p for p in depth_sensor.profiles if p.stream_type() == rs.stream.depth)
    depth_sensor.open(depth_profile)
    depth_sensor.start(lambda x: None)
    try:
        with pytest.raises(Exception):
            depth_sensor.set_option(rs.option.auto_exposure_mode, ACCELERATED)
        assert depth_sensor.get_option(rs.option.auto_exposure_mode) == REGULAR
    finally:
        depth_sensor.stop()
        depth_sensor.close()
