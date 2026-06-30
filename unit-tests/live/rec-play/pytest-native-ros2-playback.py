# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Verifies playback of native ROS2 bags - bags produced by `ros2 bag record` against
# realsense2_camera or any source publishing standard sensor_msgs/Image, CameraInfo,
# and Imu topics. These are read by ros2_native_reader (not ros2_reader); the factory
# dispatches by sniffing the bag's first message.
#
# Two device origins are covered:
#  - D555 (DDS-native): single combined Motion topic -> rs.stream.motion
#  - D455 via realsense2_camera USB with unite_imu_method=0: separate accel/gyro
#    topics -> rs.stream.accel + rs.stream.gyro
# A zstd-compressed variant of each exercises the shared decompress_if_needed path.

import logging
import os.path
from collections import Counter

import pytest
import pyrealsense2 as rs
from pytest_check import check
from rspy import repo
from playback_helper import PlaybackStatusVerifier

log = logging.getLogger(__name__)

PLAYBACK_TIMEOUT = 30  # generous; fixtures hold ~5s of data, walked at full speed

VIDEO_STREAMS = (rs.stream.color, rs.stream.depth, rs.stream.infrared)
COMBINED_MOTION = (rs.stream.motion,)
SPLIT_MOTION    = (rs.stream.accel, rs.stream.gyro)
# Streams that MUST be empty (zero frames) to confirm the IMU layout discriminator.
EXCLUDED_FOR_COMBINED = SPLIT_MOTION
EXCLUDED_FOR_SPLIT    = COMBINED_MOTION

FIXTURES = [
    ("d555_all_streams_native_ros2.db3",                     VIDEO_STREAMS + COMBINED_MOTION, EXCLUDED_FOR_COMBINED),
    ("d555_all_streams_native_ros2_zstd_compressed.db3",     VIDEO_STREAMS + COMBINED_MOTION, EXCLUDED_FOR_COMBINED),
    ("d455_all_streams_imu_native_ros2.db3",                 VIDEO_STREAMS + SPLIT_MOTION,    EXCLUDED_FOR_SPLIT),
    ("d455_all_streams_imu_native_ros2_zstd_compressed.db3", VIDEO_STREAMS + SPLIT_MOTION,    EXCLUDED_FOR_SPLIT),
]


@pytest.mark.parametrize("filename,expected_streams,excluded_streams", FIXTURES)
def test_native_ros2_playback(filename, expected_streams, excluded_streams):
    path = os.path.join(repo.build, "unit-tests", "recordings", filename)
    log.debug("playing back %s", path)

    counts = Counter()
    timestamps = []
    def on_frame(f):
        counts[f.get_profile().stream_type()] += 1
        timestamps.append(f.get_timestamp())

    dev = rs.context().load_device(path)
    psv = PlaybackStatusVerifier(dev)
    dev.set_real_time(False)

    sensors = dev.query_sensors()
    for s in sensors: s.open(s.get_stream_profiles())
    for s in sensors: s.start(on_frame)

    psv.wait_for_status_changes(2, PLAYBACK_TIMEOUT)  # playing -> stopped
    statuses = psv.get_statuses()
    check.equal(statuses[0], rs.playback_status.playing)
    check.equal(statuses[1], rs.playback_status.stopped)

    for s in sensors: s.stop()
    for s in sensors: s.close()

    for stream in expected_streams:
        check.greater(counts[stream], 0, f"no frames received on {stream}")
    # Discriminator: split-IMU fixtures must carry zero combined-motion frames and vice versa.
    for stream in excluded_streams:
        check.equal(counts[stream], 0, f"expected zero {stream} frames, got {counts[stream]}")
    # Timestamps must be bag-relative (small, not epoch). 1 min is generous.
    if timestamps:
        span_ms = max(timestamps) - min(timestamps)
        check.less(span_ms, 60_000, f"timestamps look unrebased: span={span_ms}ms (suggests epoch ns)")
