# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Test that backend timestamp is greater than frame timestamp when global time is enabled.
Verifies for both depth and color sensors.
"""

import pytest
import pyrealsense2 as rs
import time
import logging
from rspy.snippets import is_dds_dev
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device("D400*"),
    pytest.mark.device_each("D500*"),
]

FPS = 30


def _run_timestamp_check(sensor, profile, stream_name):
    """Stream frames and verify backend_ts > frame_ts for each."""
    failures = []
    has_frame = False

    def check_backend_ts(frame):
        nonlocal has_frame, failures
        has_frame = True
        assert frame.supports_frame_metadata(rs.frame_metadata_value.backend_timestamp), \
            f"{stream_name}: backend_timestamp metadata not supported"
        frame_ts = frame.get_frame_timestamp()
        backend_ts = frame.get_frame_metadata(rs.frame_metadata_value.backend_timestamp)
        delta = backend_ts - frame_ts
        if delta <= 0:
            failures.append(f"frame_ts={frame_ts}, backend_ts={backend_ts}, delta={delta}")

    orig_global_time = sensor.get_option(rs.option.global_time_enabled)

    try:
        # Enable global time
        if not orig_global_time:
            sensor.set_option(rs.option.global_time_enabled, 1)
        assert int(sensor.get_option(rs.option.global_time_enabled)) == 1

        sensor.open(profile)
        sensor.start(check_backend_ts)
        time.sleep(1)
        sensor.stop()
        sensor.close()

        assert has_frame, f"{stream_name}: no frames arrived"
        assert not failures, f"{stream_name}: backend_ts <= frame_ts:\n" + "\n".join(failures)
    finally:
        # Restore original setting
        if not orig_global_time:
            sensor.set_option(rs.option.global_time_enabled, 0)


def test_depth_backend_vs_frame_timestamp(test_device):
    dev, ctx = test_device

    ds = dev.first_depth_sensor()
    dp = next(p for p in ds.profiles
              if p.fps() == FPS
              and p.stream_type() == rs.stream.depth
              and p.format() == rs.format.z16
              and p.as_video_stream_profile().width() == 1280
              and p.as_video_stream_profile().height() == 720)

    log.info("Testing depth backend vs frame timestamp")
    _run_timestamp_check(ds, dp, "Depth")

    # Allow some time to close the depth pipe completely, stream stops when DDS reader closure is detected by device
    if is_dds_dev(dev):
        time.sleep(1)


# D421/D401/D405 do not have a color sensor support.
@pytest.mark.device_exclude("D421")
@pytest.mark.device_exclude("D401")
@pytest.mark.device_exclude("D405")
def test_color_backend_vs_frame_timestamp(test_device):
    dev, ctx = test_device

    cs = dev.first_color_sensor()
    cp = next(p for p in cs.profiles
              if p.fps() == FPS
              and p.stream_type() == rs.stream.color
              and p.format() == rs.format.rgb8
              and p.as_video_stream_profile().width() == 1280
              and p.as_video_stream_profile().height() == 720)

    log.info("Testing color backend vs frame timestamp")
    _run_timestamp_check(cs, cp, "Color")
