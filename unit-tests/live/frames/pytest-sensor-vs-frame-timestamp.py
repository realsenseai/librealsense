# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Test that HW frame timestamp is right before sensor timestamp (delta >= 0 and < frame time).
Verifies for both depth and color sensors with global time disabled (HW domain).
"""

import pytest
import pyrealsense2 as rs
import time
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device("D400*"),
]

FPS = 30


def _run_sensor_ts_check(sensor, profile, stream_name):
    """Stream frames and verify 0 < (hw_ts - sensor_ts) < frame_time."""
    failures = []
    has_frame = False
    time_between_frames = 1 / FPS * 1000000  # microseconds

    def check_hw_ts(frame):
        nonlocal has_frame, failures
        has_frame = True
        frame_ts_supported = frame.supports_frame_metadata(rs.frame_metadata_value.frame_timestamp)
        sensor_ts_supported = frame.supports_frame_metadata(rs.frame_metadata_value.sensor_timestamp)
        assert frame_ts_supported and sensor_ts_supported, \
            f"{stream_name}: frame_timestamp or sensor_timestamp metadata not supported"
        hw_ts = frame.get_frame_metadata(rs.frame_metadata_value.frame_timestamp)
        sensor_ts = frame.get_frame_metadata(rs.frame_metadata_value.sensor_timestamp)
        delta = hw_ts - sensor_ts
        if not (0 <= delta <= time_between_frames):
            failures.append(f"hw_ts={hw_ts}, sensor_ts={sensor_ts}, delta={delta}")

    orig_global_time = sensor.get_option(rs.option.global_time_enabled)

    try:
        # Disable global time (use HW domain)
        if orig_global_time:
            sensor.set_option(rs.option.global_time_enabled, 0)
        assert int(sensor.get_option(rs.option.global_time_enabled)) == 0

        sensor.open(profile)
        sensor.start(check_hw_ts)
        time.sleep(1)
        sensor.stop()
        sensor.close()

        assert has_frame, f"{stream_name}: no frames arrived"
        assert not failures, (
            f"{stream_name}: hw_ts - sensor_ts out of range [0, {time_between_frames}]:\n"
            + "\n".join(failures)
        )
    finally:
        # Restore original setting
        if orig_global_time:
            sensor.set_option(rs.option.global_time_enabled, 1)


def test_depth_sensor_vs_frame_timestamp(test_device):
    dev, ctx = test_device

    ds = dev.first_depth_sensor()
    dp = next(p for p in ds.profiles
              if p.fps() == FPS
              and p.stream_type() == rs.stream.depth
              and p.format() == rs.format.z16
              and p.as_video_stream_profile().width() == 1280
              and p.as_video_stream_profile().height() == 720)

    log.info("Testing depth sensor vs frame timestamp")
    _run_sensor_ts_check(ds, dp, "Depth")


# D421/D401/D405 do not have a color sensor support.
@pytest.mark.device_exclude("D421")
@pytest.mark.device_exclude("D401")
@pytest.mark.device_exclude("D405")
def test_color_sensor_vs_frame_timestamp(test_device):
    dev, ctx = test_device

    cs = dev.first_color_sensor()
    cp = next(p for p in cs.profiles
              if p.fps() == FPS
              and p.stream_type() == rs.stream.color
              and p.format() == rs.format.rgb8
              and p.as_video_stream_profile().width() == 1280
              and p.as_video_stream_profile().height() == 720)

    log.info("Testing color sensor vs frame timestamp")
    _run_sensor_ts_check(cs, cp, "Color")
