# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import hashlib

import pyrealsense2 as rs
import pytest


pytestmark = [pytest.mark.device_each("D585 Proto Dual RGB"), pytest.mark.context("nightly")]


@pytest.mark.parametrize("kind,fmt,indices", [
    (rs.stream.depth, rs.format.z16, [0]),
    (rs.stream.infrared, rs.format.y8, [1, 2]),
])
def test_retained_pixels_survive_restart(test_device, kind, fmt, indices):
    device, _ = test_device
    sensor = device.first_depth_sensor()
    profiles = [p for p in sensor.get_stream_profiles()
                if p.stream_type() == kind and p.format() == fmt and p.stream_index() in indices
                and p.fps() == 30 and (p.as_video_stream_profile().width(),
                                       p.as_video_stream_profile().height()) == (1280, 960)]
    assert len(profiles) == len(indices)
    held = []

    def fingerprint(frame):
        return hashlib.sha256(memoryview(frame.get_data())).digest()

    for _ in range(2):
        frames = rs.frame_queue(2)
        sensor.open(profiles)
        try:
            sensor.start(frames)
            try:
                for _ in range(40):
                    frame = frames.wait_for_frame(5000)
                    if len(held) < 6:
                        held.append((frame, fingerprint(frame)))
            finally:
                sensor.stop()
        finally:
            sensor.close()
        assert len(held) == 6
        for frame, original in held:
            assert fingerprint(frame) == original
