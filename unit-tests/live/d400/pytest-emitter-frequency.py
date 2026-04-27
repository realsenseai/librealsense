# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy.pytest.device_helpers import require_min_fw_version
import logging
log = logging.getLogger(__name__)

pytestmark = [pytest.mark.device_each("D455", "D457")]

EMITTER_FREQUENCY_57_KHZ = 0.0
EMITTER_FREQUENCY_91_KHZ = 1.0


def test_verify_camera_defaults(test_device):
    dev, _ = test_device
    depth_sensor = dev.first_depth_sensor()
    require_min_fw_version(dev, rsutils.version(5, 14, 0, 0), "EMITTER_FREQUENCY", inclusive=False)

    device_name = dev.get_info(rs.camera_info.name)
    if "D455" in device_name:
        assert depth_sensor.get_option(rs.option.emitter_frequency) == EMITTER_FREQUENCY_57_KHZ
    elif "D457" in device_name:
        assert depth_sensor.get_option(rs.option.emitter_frequency) == EMITTER_FREQUENCY_91_KHZ
    else:
        pytest.fail("Unexpected device name found: " + device_name)


def test_set_on_off_during_idle_mode(test_device):
    dev, _ = test_device
    depth_sensor = dev.first_depth_sensor()
    require_min_fw_version(dev, rsutils.version(5, 14, 0, 0), "EMITTER_FREQUENCY", inclusive=False)

    depth_sensor.set_option(rs.option.emitter_frequency, EMITTER_FREQUENCY_57_KHZ)
    assert depth_sensor.get_option(rs.option.emitter_frequency) == EMITTER_FREQUENCY_57_KHZ
    depth_sensor.set_option(rs.option.emitter_frequency, EMITTER_FREQUENCY_91_KHZ)
    assert depth_sensor.get_option(rs.option.emitter_frequency) == EMITTER_FREQUENCY_91_KHZ


def test_set_on_off_during_streaming_mode_not_allowed(test_device):
    dev, _ = test_device
    depth_sensor = dev.first_depth_sensor()
    require_min_fw_version(dev, rsutils.version(5, 14, 0, 0), "EMITTER_FREQUENCY", inclusive=False)

    # Reset option to 57 [KHZ]
    depth_sensor.set_option(rs.option.emitter_frequency, EMITTER_FREQUENCY_57_KHZ)
    assert depth_sensor.get_option(rs.option.emitter_frequency) == EMITTER_FREQUENCY_57_KHZ
    depth_profile = next(p for p in depth_sensor.profiles if p.stream_type() == rs.stream.depth)
    depth_sensor.open(depth_profile)
    depth_sensor.start(lambda x: None)
    try:
        with pytest.raises(Exception):
            depth_sensor.set_option(rs.option.emitter_frequency, EMITTER_FREQUENCY_91_KHZ)
        assert depth_sensor.get_option(rs.option.emitter_frequency) == EMITTER_FREQUENCY_57_KHZ
    finally:
        depth_sensor.stop()
        depth_sensor.close()
