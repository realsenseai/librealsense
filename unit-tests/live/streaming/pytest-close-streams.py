# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs

pytestmark = [pytest.mark.device_each("D400*", "D500*"), pytest.mark.device_type_exclude("DDS")]

# Sensors that do not opt in to incremental streams keep today's strict open/close sequence; the new read-back lists
# what the sensor streams.


def test_strict_sensor_rejects_partial_close(test_device):
    dev, _ = test_device
    depth = dev.first_depth_sensor()
    profile = next(p for p in depth.get_stream_profiles() if p.stream_type() == rs.stream.depth and p.is_default())

    assert depth.get_sensor_active_streams() == []
    depth.open(profile)
    try:
        assert [p.unique_id() for p in depth.get_sensor_active_streams()] == [profile.unique_id()]
        with pytest.raises(RuntimeError, match="not supported by this sensor"):
            depth.close([profile])
        with pytest.raises(RuntimeError, match="already opened"):
            depth.open(profile)
    finally:
        depth.close()
    assert depth.get_sensor_active_streams() == []
