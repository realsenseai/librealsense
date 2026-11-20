# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
IMU stream continuity: streams accel + gyro via sensor.start(callback) and fails on any frame-number gap,
hardware-timestamp gap (> TIMESTAMP_GAP_FACTOR periods, or backward), or a rate more than RATE_TOLERANCE
below the requested one. wait_for_frames / poll_for_frames are not used: they fabricate gaps.
"""

import sys
import os
import time
import logging

import pytest
import pyrealsense2 as rs

sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "frames"))
import fps_helper

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_exclude("D401"),
    pytest.mark.device_exclude("D405"),
    pytest.mark.device_each("D585"),  # matches D585 and D585S (substring match)
    pytest.mark.context("nightly"),
]

STANDALONE_DURATION_S = 3
VIDEO_DURATION_S = 10
MAX_FRAME_NUMBER_GAPS = 0
MAX_TIMESTAMP_GAPS = 0
TIMESTAMP_GAP_FACTOR = 1.5
RATE_TOLERANCE = 0.02
MIN_FRAMES_PER_STREAM = 10
TIMESTAMP_WRAP_MS = 2 ** 32 / 1000.0  # 32-bit microsecond counter; a nightly can outlast one wrap (~71.6 min)
VIDEO_FPS = 30

# Standalone: every rate the video case does not cover
IMU_RATE_PAIRS = [(50, 50), (63, 200), (100, 100), (100, 200), (200, 200)]
# With video: the first pair the device supports, i.e. its top rate; 200/200 is the GMSL fallback
VIDEO_RATE_PAIRS = [(400, 400), (250, 400), (200, 400), (200, 200)]


class StreamRecord:
    """Per-stream capture. Each instance is written from a single callback path (one stream type, one thread),
    so add() needs no lock."""

    def __init__(self):
        self.frame_numbers = []
        self.timestamps_ms = []
        self.domains = []

    def add(self, frame):
        self.frame_numbers.append(frame.get_frame_number())
        self.timestamps_ms.append(frame.get_timestamp())
        self.domains.append(frame.get_frame_timestamp_domain())


def count_frame_number_gaps(frame_numbers):
    """Number of missing frame numbers (a jump of N counts N - 1; repeats/rewinds count 1)."""
    return sum(max(cur - prev - 1, 1) for prev, cur in zip(frame_numbers, frame_numbers[1:]) if cur - prev != 1)


def unwrapped_deltas_ms(timestamps_ms):
    deltas = [b - a for a, b in zip(timestamps_ms, timestamps_ms[1:])]
    return [d + TIMESTAMP_WRAP_MS if d < -TIMESTAMP_WRAP_MS / 2 else d for d in deltas]


def evaluate_stream(name, rec, fps):
    """Return a list of violation strings for one recorded stream (empty when it passes)."""
    n = len(rec.frame_numbers)
    if n < MIN_FRAMES_PER_STREAM:
        return [f"{name}@{fps}: only {n} frames received"]

    problems = []
    fn_gaps = count_frame_number_gaps(rec.frame_numbers)
    if fn_gaps > MAX_FRAME_NUMBER_GAPS:
        problems.append(f"{name}@{fps}: {fn_gaps} missing frame numbers (max {MAX_FRAME_NUMBER_GAPS})")

    # Without a hardware clock the timestamps are host arrival times, so the gap and rate checks would be fake
    non_hw = [d for d in rec.domains if d != rs.timestamp_domain.hardware_clock]
    if non_hw:
        problems.append(f"{name}@{fps}: {len(non_hw)}/{n} frames not in hardware_clock domain "
                        f"({', '.join(sorted({str(d) for d in non_hw}))})")
        return problems

    period_ms = 1000.0 / fps
    deltas = unwrapped_deltas_ms(rec.timestamps_ms)
    gaps = [(i + 1, d) for i, d in enumerate(deltas) if d > TIMESTAMP_GAP_FACTOR * period_ms or d < 0]
    if len(gaps) > MAX_TIMESTAMP_GAPS:
        missing = sum(max(round(d / period_ms) - 1, 1) for _, d in gaps)
        worst = max(gaps, key=lambda g: abs(g[1]))
        problems.append(f"{name}@{fps}: {len(gaps)} timestamp gaps (~{missing} samples, worst {worst[1]:.1f} ms "
                        f"at frame #{rec.frame_numbers[worst[0]]}; max {MAX_TIMESTAMP_GAPS})")

    span_ms = sum(deltas)
    rate = (n - 1) * 1000.0 / span_ms if span_ms > 0 else 0.0
    log.info("%s@%d: %d frames, frame #%d..#%d, %.2f Hz", name, fps, n,
             rec.frame_numbers[0], rec.frame_numbers[-1], rate)
    if rate < fps * (1 - RATE_TOLERANCE):
        problems.append(f"{name}@{fps}: measured {rate:.2f} Hz, at least {fps * (1 - RATE_TOLERANCE):.2f} Hz expected")
    elif rate > fps * (1 + RATE_TOLERANCE):
        log.warning("%s@%d: measured %.2f Hz, faster than requested (no loss)", name, fps, rate)
    return problems


def _find_profile(sensor, stream, fps):
    return next((p for p in sensor.profiles if p.stream_type() == stream and p.fps() == fps
                 and p.format() == rs.format.motion_xyz32f), None)


def _first_motion_sensor(dev):
    try:
        return dev.first_motion_sensor()
    except RuntimeError:
        pytest.skip("Device has no motion sensor")


def _pair_supported(dev, motion, accel_fps, gyro_fps):
    # GMSL motion is UVC (D457, D585): the SDK needs equal accel and gyro fps there
    gmsl = dev.supports(rs.camera_info.connection_type) and dev.get_info(rs.camera_info.connection_type) == "GMSL"
    return ((accel_fps == gyro_fps or not gmsl)
            and _find_profile(motion, rs.stream.accel, accel_fps) is not None
            and _find_profile(motion, rs.stream.gyro, gyro_fps) is not None)


def _video_profiles(dev):
    """[(sensor, profile)] at VIDEO_FPS for depth and, when present, color."""
    depth_sensor = dev.first_depth_sensor()
    result = [(depth_sensor, fps_helper.get_profile(depth_sensor, rs.stream.depth, fps=VIDEO_FPS))]
    try:
        color_sensor = dev.first_color_sensor()
        result.append((color_sensor, fps_helper.get_profile(color_sensor, rs.stream.color, fps=VIDEO_FPS)))
    except RuntimeError:
        log.info("Device has no color sensor; video load is depth only")
    if any(p is None for _, p in result):
        pytest.skip(f"No depth/color profile at {VIDEO_FPS} fps")
    return result


def _stream_and_record(dev, motion, accel_fps, gyro_fps, with_video, duration_s):
    """Stream for duration_s and return {'accel': StreamRecord, 'gyro': StreamRecord}."""
    accel_profile = _find_profile(motion, rs.stream.accel, accel_fps)
    gyro_profile = _find_profile(motion, rs.stream.gyro, gyro_fps)
    video = _video_profiles(dev) if with_video else []

    records = {rs.stream.accel: StreamRecord(), rs.stream.gyro: StreamRecord()}
    video_frames = [0]

    def motion_callback(frame):
        rec = records.get(frame.get_profile().stream_type())
        if rec is not None:
            rec.add(frame)

    def video_callback(frame):
        video_frames[0] += 1

    # Raw device timestamps rather than the host-mapped global time
    restore_global_time = None
    opened = []
    started = []
    try:
        if motion.supports(rs.option.global_time_enabled):
            restore_global_time = motion.get_option(rs.option.global_time_enabled)
            motion.set_option(rs.option.global_time_enabled, 0)
        motion.open([accel_profile, gyro_profile])
        opened.append(motion)
        for sensor, profile in video:
            sensor.open([profile])
            opened.append(sensor)
        for sensor, _ in video:
            sensor.start(video_callback)
            started.append(sensor)
        motion.start(motion_callback)
        started.append(motion)
        time.sleep(duration_s)
    finally:
        # Restore while the sensor is still open: set_option on a closed sensor can throw and mask the result
        if restore_global_time is not None:
            try:
                motion.set_option(rs.option.global_time_enabled, restore_global_time)
            except Exception as e:
                log.warning("Could not restore global_time_enabled: %s", e)
        for sensor in reversed(started):
            sensor.stop()
        for sensor in reversed(opened):
            sensor.close()

    if with_video:
        log.info("Video frames received during the run: %d", video_frames[0])
        assert video_frames[0] > 0, "Video streams delivered no frames, IMU was not measured under video load"
    return records


def _run_continuity(dev, motion, accel_fps, gyro_fps, with_video, duration_s):
    records = _stream_and_record(dev, motion, accel_fps, gyro_fps, with_video, duration_s)
    problems = (evaluate_stream("accel", records[rs.stream.accel], accel_fps) +
                evaluate_stream("gyro", records[rs.stream.gyro], gyro_fps))
    if problems:
        pytest.fail("IMU continuity failed:\n  " + "\n  ".join(problems))


def _pair_id(pair):
    return f"accel{pair[0]}-gyro{pair[1]}"


@pytest.mark.parametrize("accel_fps,gyro_fps", IMU_RATE_PAIRS, ids=[_pair_id(p) for p in IMU_RATE_PAIRS])
def test_imu_continuity_standalone(test_device, accel_fps, gyro_fps):
    dev, _ = test_device
    motion = _first_motion_sensor(dev)
    if not _pair_supported(dev, motion, accel_fps, gyro_fps):
        pytest.skip(f"accel {accel_fps} / gyro {gyro_fps} not supported on this device")
    _run_continuity(dev, motion, accel_fps, gyro_fps, with_video=False, duration_s=STANDALONE_DURATION_S)


def test_imu_continuity_with_video(test_device):
    dev, _ = test_device
    motion = _first_motion_sensor(dev)
    pair = next((p for p in VIDEO_RATE_PAIRS if _pair_supported(dev, motion, *p)), None)
    if pair is None:
        pytest.skip("No pair from VIDEO_RATE_PAIRS is supported on this device")
    log.info("With video: %s", _pair_id(pair))
    _run_continuity(dev, motion, *pair, with_video=True, duration_s=VIDEO_DURATION_S)
