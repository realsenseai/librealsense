# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Syncer dynamic FPS throughput test: streams IR (and optionally RGB) at 848x480@60fps
for 10 seconds. At the 5-second mark IR exposure is set to 18000, forcing a FPS drop
to ~30. Verifies that the syncer throughput matches reported actual_fps within 10% for
both the high-FPS and low-FPS periods, and that frame drops remain below 10%.
"""

import time
import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D435"),
    pytest.mark.context("nightly"),
]

RECEIVE_FRAMES_TIME = 10  # seconds
LOW_FPS  = 30.0
HIGH_FPS = 60.0
WIDTH    = 848
HEIGHT   = 480


def _find_profiles(sensor, stream_type, fmt):
    """Return all 848x480@60fps profiles matching stream_type and format."""
    profiles = []
    for sp in sensor.get_stream_profiles():
        if sp.stream_type() != stream_type or sp.format() != fmt or sp.fps() != 60:
            continue
        vid = sp.as_video_stream_profile()
        if vid and vid.width() == WIDTH and vid.height() == HEIGHT:
            profiles.append(sp)
    return profiles


def _collect_frames(sync, duration_sec, on_half_elapsed):
    """
    Pull framesets from the syncer for duration_sec seconds.
    Calls on_half_elapsed() once at the halfway point.
    Returns (ir_info, rgb_info) where each is a list of (hw_timestamp_usec, actual_fps).
    """
    ir_info  = []
    rgb_info = []
    half_elapsed = False
    t_start = time.monotonic()

    while True:
        elapsed = time.monotonic() - t_start
        if elapsed >= duration_sec:
            break
        if not half_elapsed and elapsed >= duration_sec / 2:
            on_half_elapsed()
            half_elapsed = True

        fs = sync.wait_for_frames(5000)
        for f in fs:
            name = f.get_profile().stream_name()
            if name not in ("Infrared 1", "Color"):
                continue
            if (not f.supports_frame_metadata(rs.frame_metadata_value.actual_fps)
                    or not f.supports_frame_metadata(rs.frame_metadata_value.frame_timestamp)):
                continue
            actual_fps = f.get_frame_metadata(rs.frame_metadata_value.actual_fps) / 1000.0
            hw_ts      = f.get_frame_metadata(rs.frame_metadata_value.frame_timestamp)  # usec
            if name == "Infrared 1":
                ir_info.append((hw_ts, actual_fps))
            else:
                rgb_info.append((hw_ts, actual_fps))

    return ir_info, rgb_info


def _check_frame_drops(frames, label):
    """Assert that timestamp gaps exceeding 1.5x the expected interval stay below 10%."""
    drops    = 0
    prev_ts  = None
    for hw_ts, actual_fps in frames:
        if prev_ts is not None:
            expected_dt_ms = 1000.0 / actual_fps
            calc_dt_ms     = (hw_ts - prev_ts) / 1000.0  # usec -> ms
            if calc_dt_ms / expected_dt_ms > 1.5:
                drops += 1
        prev_ts = hw_ts
    drop_ratio = drops / len(frames)
    log.info(f"{label}: {drops}/{len(frames)} frame drops (ratio={drop_ratio:.3f})")
    assert drop_ratio < 0.1, \
        f"{label}: frame drop ratio {drop_ratio:.3f} >= 0.1 ({drops}/{len(frames)} drops)"


def _analyse(frames_info, stream_name):
    """
    Bucket frames by actual_fps: high (~60fps), low (~30fps), extra (transitional).
    For buckets with >= 10 frames: assert calculated FPS matches reported FPS within 10%
    and check for frame drops. Assert extra frames are < 10% of total.
    """
    assert frames_info, f"{stream_name}: no frames received from syncer"

    high_fps_frames = []
    low_fps_frames  = []
    extra_frames    = []
    for hw_ts, fps in frames_info:
        if fps / HIGH_FPS > 0.9:
            high_fps_frames.append((hw_ts, fps))
        elif fps / LOW_FPS > 0.9:
            low_fps_frames.append((hw_ts, fps))
        else:
            extra_frames.append((hw_ts, fps))

    for bucket, label in [(high_fps_frames, f"{stream_name} @60fps"),
                          (low_fps_frames,  f"{stream_name} @30fps")]:
        if len(bucket) < 10:
            continue
        dt_sec     = (bucket[-1][0] - bucket[0][0]) / 1_000_000  # usec -> sec
        actual_fps = bucket[0][1]
        calc_fps   = len(bucket) / dt_sec
        fps_ratio  = abs(1 - calc_fps / actual_fps)
        log.info(
            f"{label}: reported_fps={actual_fps:.1f}, calc_fps={calc_fps:.2f},"
            f" n={len(bucket)}, fps_ratio={fps_ratio:.3f}"
        )
        assert fps_ratio < 0.1, \
            f"{label}: calculated FPS {calc_fps:.2f} deviates from reported {actual_fps:.1f} by {fps_ratio:.1%}"
        _check_frame_drops(bucket, label)

    extra_ratio = len(extra_frames) / len(frames_info)
    log.info(
        f"{stream_name}: extra (unstable FPS) frames ratio={extra_ratio:.3f}"
        f" ({len(extra_frames)}/{len(frames_info)})"
    )
    assert extra_ratio < 0.1, \
        f"{stream_name}: extra frames ratio {extra_ratio:.3f} >= 0.1"


@pytest.mark.parametrize("with_rgb", [False, True], ids=["ir_only", "ir_rgb_exposure"])
def test_syncer_dynamic_fps_throughput(test_device, with_rgb):
    """
    Stream IR (and optionally RGB) at 848x480@60fps. After 5 seconds set IR exposure
    to 18000 to trigger a FPS drop to ~30. Verify syncer throughput and frame integrity
    for both the high-FPS and low-FPS periods.
    """
    dev, ctx = test_device
    sensors  = dev.query_sensors()

    ir_sensor    = None
    rgb_sensor   = None
    ir_profiles  = []
    rgb_profiles = []

    for s in sensors:
        sensor_name = s.get_info(rs.camera_info.name)
        if sensor_name == "Stereo Module":
            ir_sensor   = s
            ir_profiles = _find_profiles(s, rs.stream.infrared, rs.format.y8)
        elif sensor_name == "RGB Camera":
            rgb_sensor   = s
            rgb_profiles = _find_profiles(s, rs.stream.color, rs.format.rgb8)
        if s.supports(rs.option.global_time_enabled):
            s.set_option(rs.option.global_time_enabled, 0)

    assert ir_sensor  is not None, "Stereo Module sensor not found"
    assert rgb_sensor is not None, "RGB Camera sensor not found"
    assert ir_profiles,  f"No IR Y8 profile found for {WIDTH}x{HEIGHT}@60"
    assert rgb_profiles, f"No Color RGB8 profile found for {WIDTH}x{HEIGHT}@60"

    if ir_sensor.supports(rs.option.enable_auto_exposure):
        ir_sensor.set_option(rs.option.enable_auto_exposure, 0)
    if rgb_sensor.supports(rs.option.enable_auto_exposure):
        rgb_sensor.set_option(rs.option.enable_auto_exposure, 0)

    log.info(f"Configuration: {'IR+RGB' if with_rgb else 'IR only'}")

    ir_opened = ir_started = rgb_opened = rgb_started = False
    sync = rs.syncer()
    try:
        ir_sensor.set_option(rs.option.exposure, 1)
        ir_sensor.open(ir_profiles);  ir_opened = True
        ir_sensor.start(sync);        ir_started = True
        if with_rgb:
            rgb_sensor.open(rgb_profiles);  rgb_opened = True
            rgb_sensor.start(sync);         rgb_started = True

        def set_high_exposure():
            log.info("Setting IR exposure to 18000 to trigger FPS drop to ~30")
            if ir_sensor.supports(rs.option.exposure):
                ir_sensor.set_option(rs.option.exposure, 18000)

        ir_info, rgb_info = _collect_frames(sync, RECEIVE_FRAMES_TIME, set_high_exposure)
    finally:
        if ir_started:   ir_sensor.stop()
        if ir_opened:    ir_sensor.close()
        if rgb_started:  rgb_sensor.stop()
        if rgb_opened:   rgb_sensor.close()
        if ir_sensor.supports(rs.option.exposure):
            ir_sensor.set_option(rs.option.exposure, 1)

    _analyse(ir_info, "Infrared 1")
    if with_rgb:
        _analyse(rgb_info, "Color")
