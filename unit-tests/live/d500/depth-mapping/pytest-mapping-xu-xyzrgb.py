# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import time
import zlib

import numpy as np
import pyrealsense2 as rs
import pytest


pytestmark = [
    pytest.mark.device_each("D585"),
    pytest.mark.device_exclude("D585S"),
    pytest.mark.device_type_exclude("GMSL", "DDS"),
    pytest.mark.context("nightly"),
    pytest.mark.requires(sensor="Depth Mapping Camera"),
]


@pytest.fixture
def mapping(test_device):
    device, _ = test_device
    sensor = next(sensor for sensor in device.query_sensors()
                  if sensor.get_info(rs.camera_info.name) == "Depth Mapping Camera")
    rgb = [profile for profile in sensor.profiles if profile.format() == rs.format.xyzrgb32f]
    if not rgb:
        pytest.skip("Firmware does not advertise XYZRGB capability")
    yield sensor
    if sensor.get_active_streams():
        sensor.close()


def select(sensor, stream, fmt, width, height, fps):
    return next(profile for profile in sensor.profiles
                if profile.stream_type() == stream and profile.format() == fmt
                and profile.as_video_stream_profile().width() == width
                and profile.as_video_stream_profile().height() == height
                and profile.fps() == fps)


def capture(sensor, profile, count=12):
    frames = []
    sensor.open(profile)
    try:
        sensor.start(lambda frame: frames.append(frame) if len(frames) < count else None)
        try:
            deadline = time.monotonic() + 8
            while len(frames) < count and time.monotonic() < deadline:
                time.sleep(0.02)
            assert len(frames) == count
        finally:
            sensor.stop()
    finally:
        sensor.close()
    return frames


@pytest.mark.parametrize("width,height", [(640, 480), (1280, 720)])
@pytest.mark.parametrize("occlusion", [0, 1])
def test_xyzrgb_payload_metadata(mapping, width, height, occlusion):
    assert mapping.supports(rs.option.pcl_occlusion_removal)
    original = mapping.get_option(rs.option.pcl_occlusion_removal)
    try:
        mapping.set_option(rs.option.pcl_occlusion_removal, occlusion)
        profile = select(mapping, rs.stream.point_cloud, rs.format.xyzrgb32f, width, height, 15)
        frames = capture(mapping, profile)
        colored = 0
        for frame in frames:
            assert frame.get_profile().format() == rs.format.xyzrgb32f
            assert frame.get_data_size() == width * height * 16
            payload = bytes(frame.get_data())
            points = np.frombuffer(payload, dtype=np.dtype([
                ("xyz", "<f4", 3), ("bgra", "u1", 4)]))
            assert np.isfinite(points["xyz"]).all()
            alpha = points["bgra"][:, 3]
            assert np.isin(alpha, [0, 255]).all()
            invalid = points["xyz"][:, 2] == 0
            assert (points["xyz"][invalid] == 0).all()
            assert (points["bgra"][alpha == 0] == 0).all()
            colored += np.count_nonzero(alpha)
            assert frame.supports_frame_metadata(rs.frame_metadata_value.crc)
            assert frame.get_frame_metadata(rs.frame_metadata_value.crc) == zlib.crc32(payload)
            assert frame.get_frame_metadata(rs.frame_metadata_value.pcl_occlusion_removal) == occlusion
            if np.count_nonzero(alpha):
                assert frame.supports_frame_metadata(rs.frame_metadata_value.color_frame_counter)
                assert abs(frame.get_frame_metadata(rs.frame_metadata_value.color_sync_delta)) < 10000
        assert colored > 0
    finally:
        mapping.set_option(rs.option.pcl_occlusion_removal, original)


def test_mapping_shared_endpoint_switch_and_reopen(mapping):
    requests = [
        (rs.stream.point_cloud, rs.format.xyz32f, 640, 480, 30),
        (rs.stream.point_cloud, rs.format.xyzrgb32f, 640, 480, 15),
        (rs.stream.labeled_point_cloud, rs.format.y8, 640, 360, 30),
        (rs.stream.occupancy, rs.format.y8, 320, 256, 30),
        (rs.stream.point_cloud, rs.format.xyz32f, 1280, 720, 15),
        (rs.stream.point_cloud, rs.format.xyzrgb32f, 1280, 720, 15),
        (rs.stream.point_cloud, rs.format.xyz32f, 1280, 720, 15),
    ]
    for cycle in range(3):
        for request in requests:
            print("Mapping switch", cycle, request, flush=True)
            frames = capture(mapping, select(mapping, *request), count=6)
            assert all(frame.get_profile().format() == request[1] for frame in frames)
            assert all(frame.get_profile().stream_type() == request[0] for frame in frames)
            stride = (16 if request[1] == rs.format.xyzrgb32f else
                      12 if request[0] == rs.stream.point_cloud else
                      13 if request[0] == rs.stream.labeled_point_cloud else 1)
            expected_size = request[2] * request[3] * stride
            assert all(frame.get_data_size() == expected_size for frame in frames)
            for frame in frames:
                assert frame.get_frame_metadata(rs.frame_metadata_value.crc) == zlib.crc32(bytes(frame.get_data()))
    first = select(mapping, *requests[0])
    second = select(mapping, *requests[1])
    with pytest.raises(RuntimeError):
        mapping.open([first, second])
    capture(mapping, first, count=6)


@pytest.mark.parametrize("width,height", [(640, 480), (1280, 720)])
def test_xyzrgb_depth_peer_stop_restart(mapping, test_device, width, height):
    device, _ = test_device
    depth = device.first_depth_sensor()
    depth_profile = select(depth, rs.stream.depth, rs.format.z16, width, height, 15)
    cloud_profile = select(mapping, rs.stream.point_cloud, rs.format.xyzrgb32f, width, height, 15)
    clouds = []
    mapping.open(cloud_profile)
    try:
        mapping.start(lambda frame: clouds.append(frame.get_frame_number()))
        try:
            for _ in range(3):
                frames = capture(depth, depth_profile, count=6)
                assert all(frame.get_data_size() == width * height * 2 for frame in frames)
                before = len(clouds)
                time.sleep(0.3)
                assert len(clouds) > before
        finally:
            mapping.stop()
    finally:
        mapping.close()
    assert len(clouds) >= 6
    assert all(second > first for first, second in zip(clouds, clouds[1:]))


def test_regular_streams_after_mapping_close(mapping, test_device):
    device, _ = test_device
    cloud = select(mapping, rs.stream.point_cloud, rs.format.xyzrgb32f, 640, 480, 15)
    tested = set()
    for sensor in device.query_sensors():
        candidates = [profile for profile in sensor.profiles
                      if profile.stream_type() in (rs.stream.depth, rs.stream.infrared, rs.stream.color)
                      and profile.fps() == 30
                      and profile.as_video_stream_profile().width() == 640
                      and profile.as_video_stream_profile().height() == 360
                      and profile.format() in (rs.format.z16, rs.format.y8, rs.format.nv12)]
        for profile in candidates:
            identity = (profile.stream_type(), profile.stream_index())
            if identity in tested:
                continue
            capture(mapping, cloud, count=6)
            frames = capture(sensor, profile, count=6)
            assert all(frame.get_data_size() > 0 for frame in frames)
            assert all(frame.get_profile().stream_type() == identity[0] for frame in frames)
            assert frames[-1].get_frame_number() > frames[0].get_frame_number()
            tested.add(identity)
    assert any(stream == rs.stream.depth for stream, _ in tested)
    assert any(stream == rs.stream.color for stream, _ in tested)
    assert any(stream == rs.stream.infrared for stream, _ in tested)
