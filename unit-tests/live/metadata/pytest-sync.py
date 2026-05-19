# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from pytest_check import check
import time
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_exclude("D401"),
    pytest.mark.context("nightly"),
    pytest.mark.timeout(300),
]

# Test parameters
TS_TOLERANCE_MS = 1.5  # Tolerance for timestamp differences in ms
TS_TOLERANCE_MICROSEC = TS_TOLERANCE_MS * 1000
SKIP_FRAMES_AFTER_DROP = 10  # Frames to skip after detecting drops

CONFIGURATIONS = [
    ((640, 480), 15),
    ((640, 480), 30),
    ((640, 480), 60),
    ((848, 480), 15),
    ((848, 480), 30),
    ((848, 480), 60),
]


def detect_frame_drops(frames_dict, prev_frame_counters):
    """Detect frame drops using hardware frame counters"""
    frame_drop_detected = False
    current_frame_counters = {}

    for stream_name, frame in frames_dict.items():
        if not frame.supports_frame_metadata(rs.frame_metadata_value.frame_counter):
            continue

        current_counter = frame.get_frame_metadata(rs.frame_metadata_value.frame_counter)
        current_frame_counters[stream_name] = current_counter

        prev_counter = prev_frame_counters[stream_name]
        if prev_counter is not None and current_counter != prev_counter + 1:
            dropped_frames = current_counter - prev_counter - 1
            if dropped_frames > 0:
                log.warning(f"Frame drop detected on {stream_name}: {dropped_frames} frames dropped")
            else:
                log.warning(f"Frame drop detected on {stream_name}: current {current_counter}, previous {prev_counter}")
            frame_drop_detected = True

    return frame_drop_detected, current_frame_counters


def run_test(device, ctx, resolution, fps):
    """Run timestamp synchronization test for a specific resolution and FPS"""
    pipeline = rs.pipeline(ctx)
    cfg = rs.config()
    cfg.enable_stream(rs.stream.depth, resolution[0], resolution[1], rs.format.z16, fps)
    cfg.enable_stream(rs.stream.infrared, 1, resolution[0], resolution[1], rs.format.y8, fps)
    cfg.enable_stream(rs.stream.infrared, 2, resolution[0], resolution[1], rs.format.y8, fps)
    cfg.enable_stream(rs.stream.color, resolution[0], resolution[1], rs.format.yuyv, fps)
    if not cfg.can_resolve(pipeline):
        log.info(f"Configuration {resolution[0]}x{resolution[1]} @ {fps}fps is not supported by the device")
        return

    depth_sensor = device.first_depth_sensor()
    color_sensor = device.first_color_sensor()

    for sensor in [depth_sensor, color_sensor]:
        if sensor.supports(rs.option.global_time_enabled):
            if not sensor.get_option(rs.option.global_time_enabled):
                sensor.set_option(rs.option.global_time_enabled, 1)
        else:
            pytest.fail(f"Sensor {sensor.name} does not support global time option")

    pipeline.start(cfg)
    time.sleep(5)  # Longer stabilization to prevent initial frame drop issues

    prev_frame_counters = {'depth': None, 'ir1': None, 'ir2': None, 'color': None}
    frames_to_skip = 0
    consecutive_drops = 0
    unskipped_frames = 0

    try:
        while unskipped_frames < 100:
            frames = pipeline.wait_for_frames()
            depth_frame = frames.get_depth_frame()
            ir1_frame = frames.get_infrared_frame(1)
            ir2_frame = frames.get_infrared_frame(2)
            color_frame = frames.get_color_frame()

            if not all([depth_frame, ir1_frame, ir2_frame, color_frame]):
                log.error("One or more frames are missing")
                continue

            # Skip frames during recovery
            if frames_to_skip > 0:
                frames_to_skip -= 1
                if frames_to_skip == 0:
                    prev_frame_counters = {'depth': None, 'ir1': None, 'ir2': None, 'color': None}
                continue

            # Check for frame drops
            frames_dict = {'depth': depth_frame, 'ir1': ir1_frame, 'ir2': ir2_frame, 'color': color_frame}
            frame_drop_detected, current_frame_counters = detect_frame_drops(frames_dict, prev_frame_counters)
            unskipped_frames += 1

            # Handle frame drops
            if frame_drop_detected and not frames_to_skip:
                consecutive_drops += 1
                assert consecutive_drops <= 20, \
                    f"Continuous frame drops detected ({consecutive_drops} consecutive). Hardware issue."

                frames_to_skip = SKIP_FRAMES_AFTER_DROP
                log.warning(f"Frame drop at frame {unskipped_frames}, skipping next {frames_to_skip} frames")
                prev_frame_counters = current_frame_counters
                continue

            prev_frame_counters = current_frame_counters
            consecutive_drops = 0

            # Test timestamp synchronization
            log.debug(f"Global TS - Depth:{depth_frame.timestamp}, IR1:{ir1_frame.timestamp}, "
                       f"IR2:{ir2_frame.timestamp}, Color:{color_frame.timestamp}")

            check.almost_equal(depth_frame.timestamp, ir1_frame.timestamp, abs=TS_TOLERANCE_MS,
                msg=f"Depth-IR1 timestamp diff {abs(depth_frame.timestamp - ir1_frame.timestamp):.3f}ms exceeds tolerance {TS_TOLERANCE_MS}ms")
            check.almost_equal(depth_frame.timestamp, ir2_frame.timestamp, abs=TS_TOLERANCE_MS,
                msg=f"Depth-IR2 timestamp diff {abs(depth_frame.timestamp - ir2_frame.timestamp):.3f}ms exceeds tolerance {TS_TOLERANCE_MS}ms")
            check.almost_equal(depth_frame.timestamp, color_frame.timestamp, abs=TS_TOLERANCE_MS,
                msg=f"Depth-Color timestamp diff {abs(depth_frame.timestamp - color_frame.timestamp):.3f}ms exceeds tolerance {TS_TOLERANCE_MS}ms")

            # Test frame metadata timestamps if supported
            if all(f.supports_frame_metadata(rs.frame_metadata_value.frame_timestamp) for f in frames_dict.values()):
                frame_timestamps = {name: f.get_frame_metadata(rs.frame_metadata_value.frame_timestamp)
                                    for name, f in frames_dict.items()}

                log.debug(f"Frame TS - Depth:{frame_timestamps['depth']}, IR1:{frame_timestamps['ir1']}, "
                           f"IR2:{frame_timestamps['ir2']}, Color:{frame_timestamps['color']}")

                check.almost_equal(frame_timestamps['depth'], frame_timestamps['ir1'], abs=TS_TOLERANCE_MICROSEC,
                    msg=f"Depth-IR1 frame TS diff exceeds tolerance {TS_TOLERANCE_MICROSEC}us")
                check.almost_equal(frame_timestamps['depth'], frame_timestamps['ir2'], abs=TS_TOLERANCE_MICROSEC,
                    msg=f"Depth-IR2 frame TS diff exceeds tolerance {TS_TOLERANCE_MICROSEC}us")
                check.almost_equal(frame_timestamps['depth'], frame_timestamps['color'], abs=TS_TOLERANCE_MICROSEC,
                    msg=f"Depth-Color frame TS diff exceeds tolerance {TS_TOLERANCE_MICROSEC}us")
    finally:
        pipeline.stop()


def test_synchronized_frames(test_device):
    """Verify that timestamps of depth, infrared and color frames are consistent across configurations"""
    device, ctx = test_device

    time.sleep(1)  # let device settle after hub power-cycle, before first pipeline.start
    for resolution, fps in CONFIGURATIONS:
        log.info(f"Timestamp Synchronization Test {resolution[0]}x{resolution[1]} @ {fps}fps")
        run_test(device, ctx, resolution, fps)
        time.sleep(2)  # let hardware settle between configurations after pipeline.stop
