# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Sensor API and pipeline streaming on one device, on a software device driven by the real SDK."""

import pytest
import pyrealsense2 as rs
from unittest.mock import MagicMock

from app.models.stream import StreamConfig
from app.services.rs_manager import RealSenseManager, _profile_row


def _add_stream(sensor, stream, fmt, bpp, uid):
    v = rs.video_stream()
    v.type, v.index, v.uid = stream, 0, uid
    v.width, v.height, v.fps, v.bpp, v.fmt = 640, 480, 30, bpp, fmt
    v.intrinsics = rs.intrinsics()
    return sensor.add_video_stream(v)


@pytest.fixture
def mgr():
    dev = rs.software_device()
    depth = _add_stream(dev.add_sensor("Stereo Module"), rs.stream.depth, rs.format.z16, 2, 0)
    _add_stream(dev.add_sensor("RGB Camera"), rs.stream.color, rs.format.rgb8, 3, 1)
    dev.register_info(rs.camera_info.serial_number, "SW1")
    dev.create_matcher(rs.matchers.default)

    m = RealSenseManager()
    dev.add_to(m.ctx)
    with m.lock:
        m._register_new_device(dev)
    m.start_sensor("SW1", "SW1-sensor-0", [_profile_row(depth)])
    yield m
    m.stop_sensor("SW1", "SW1-sensor-0")


def _streaming(mgr):
    return [p.stream_type() for s in mgr.devices["SW1"].sensors for p in s.get_active_streams()]


def test_pipeline_stop_keeps_sensor_streams(mgr):
    mgr.start_stream("SW1", [StreamConfig(stream="color")])
    assert _streaming(mgr) == [rs.stream.depth, rs.stream.color]

    mgr.stop_stream("SW1")
    assert _streaming(mgr) == [rs.stream.depth]

    mgr.stop_sensor("SW1", "SW1-sensor-0")
    assert not _streaming(mgr)


def test_sdk_refuses_pipeline_on_a_streaming_sensor(mgr):
    with pytest.raises(RuntimeError, match="streaming"):
        mgr.start_stream("SW1", [StreamConfig(stream="depth")])


def test_sdk_refuses_stop_before_start(mgr):
    with pytest.raises(RuntimeError, match="can only be called between a start\\(\\) and a following stop\\(\\)"):
        mgr.stop_stream("SW1")


def test_sdk_refuses_start_twice(mgr):
    mgr.start_stream("SW1", [StreamConfig(stream="color")])
    with pytest.raises(RuntimeError, match=r"start\(\) cannot be called before stop\(\)"):
        mgr.start_stream("SW1", [StreamConfig(stream="color")])
    mgr.stop_stream("SW1")
