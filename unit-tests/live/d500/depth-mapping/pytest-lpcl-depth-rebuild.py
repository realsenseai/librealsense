# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import threading
import time

import pytest
import pyrealsense2 as rs


pytestmark = [
    pytest.mark.device_each("D555", "D585"),
    pytest.mark.device_exclude("D585S"),
    pytest.mark.context("nightly"),
]


def profile(sensor, stream, width, height):
    fmt = rs.format.z16 if stream == rs.stream.depth else rs.format.y8
    return next((p for p in sensor.profiles if p.stream_type() == stream
                 and p.format() == fmt
                 and p.fps() == 30 and p.is_video_stream_profile()
                 and p.as_video_stream_profile().width() == width
                 and p.as_video_stream_profile().height() == height), None)


@pytest.mark.parametrize("segmented", [0, 1])
@pytest.mark.parametrize("width,height", [(640, 480), (1280, 720), (1280, 960),
                                          (848, 480), (480, 270), (424, 240), (640, 360)])
def test_lpcl_depth_profile_rebuild(test_device, segmented, width, height):
    device, _ = test_device
    mapping = next((s for s in device.query_sensors()
                    if s.get_info(rs.camera_info.name) == "Depth Mapping Camera"), None)
    if mapping is None:
        pytest.skip("No Depth Mapping sensor")
    lpcl = profile(mapping, rs.stream.labeled_point_cloud, 640, 360)
    if lpcl is None:
        pytest.skip("No native LPCL profile")
    if not mapping.supports(rs.option.lpcl_segmentation):
        pytest.skip("No LPCL segmentation control")
    depth = device.first_depth_sensor()
    requested = profile(depth, rs.stream.depth, width, height)
    if requested is None:
        pytest.skip("Required Depth profile is unavailable")
    mapping.set_option(rs.option.lpcl_segmentation, segmented)
    mapping_frames = []
    depth_frame = threading.Event()
    depth_open = depth_started = False
    mapping.open(lpcl)
    try:
        mapping.start(lambda frame: mapping_frames.append(frame.get_frame_number()))
        try:
            depth.open([requested])
            depth_open = True
            before = len(mapping_frames)
            depth.start(lambda frame: depth_frame.set())
            depth_started = True
            assert depth_frame.wait(5), "No Depth frame after profile validation"
            deadline = time.monotonic() + 5
            while len(mapping_frames) <= before and time.monotonic() < deadline:
                time.sleep(0.01)
            assert len(mapping_frames) > before, "LPCL did not continue after the Depth request"
        finally:
            if depth_started:
                depth.stop()
            if depth_open:
                depth.close()
            mapping.stop()
    finally:
        mapping.close()
