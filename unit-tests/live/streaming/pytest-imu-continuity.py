# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
IMU stream continuity (RSDEV-16379).

Streams accel + gyro through the motion sensor for STREAM_DURATION_S and checks, per stream and
independently of each other:

  1. Frame-number gaps      - consecutive frames must differ by exactly 1.
  2. Hardware-timestamp gaps - every motion frame must report rs.timestamp_domain.hardware_clock (a frame that
                               fell back to system_time carries host arrival time, which would make this check
                               meaningless, so it fails explicitly); a delta above TIMESTAMP_GAP_FACTOR nominal
                               periods, or a backward delta (clock rollback), is a gap; the number of missing
                               samples in it is estimated as round(delta / period) - 1.
                               (RSDEV-13643: timestamps can jump while frame numbers stay continuous.)
  3. Measured rate          - (frames - 1) / arrival span must not fall below the requested rate by more than
                               RATE_TOLERANCE. A faster stream loses nothing and is only logged (D457 GMSL
                               delivers gyro at 100 Hz when 50 is requested).

Thresholds (named constants below, all zero-tolerance for gaps; RSDEV-13643 acceptance is zero):
  MAX_FRAME_NUMBER_GAPS = 0       missing frame numbers per stream
  MAX_TIMESTAMP_GAPS    = 0       timestamp gaps per stream
  TIMESTAMP_GAP_FACTOR  = 1.5     gap threshold, in nominal periods
  RATE_TOLERANCE        = 0.02    allowed relative rate error (RSDEV-13850 reference: 200.0 +/- 0.5 Hz at 200 Hz)

Two variants: IMU alone, and IMU with depth + color streaming (drops in RSDEV-13850 / RSDSO-21807 depend on
video load). Frames are consumed through sensor.start(callback) only: wait_for_frames / poll_for_frames
fabricate gaps (RSDSO-21592, RSDSO-21711, RSDSO-21096). Callbacks only record, so a failure is a real loss.

On Linux D400 USB, motion frame numbers are counted on the host (RSDSO-21711), so check 1 cannot catch loss
there and check 2 is the real detector. Stock kernels without the HID metadata patches fall back to system_time
and fail with the domain message.

Expected to fail on the known regressions: D585 over GMSL without IMU pair batching (RSDEV-13850), Windows
D585 USB HID scheduling (RSDEV-13643), sequential drops of 10-26 on D457/D430 GMSL at 50-400 Hz (RSDSO-21807).
"""

import platform
import time
import logging

import pytest
import pyrealsense2 as rs

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D455"),
    pytest.mark.device_each("D457"),
    pytest.mark.device_each("D435I"),
    pytest.mark.device_each("D430I"),
    pytest.mark.device_each("D585"),  # matches D585 and D585S (substring match)
    pytest.mark.context("nightly"),
    pytest.mark.timeout(300),
]

# --- Thresholds ---
STREAM_DURATION_S = 60
MAX_FRAME_NUMBER_GAPS = 0
MAX_TIMESTAMP_GAPS = 0
TIMESTAMP_GAP_FACTOR = 1.5
RATE_TOLERANCE = 0.02
MIN_FRAMES_PER_STREAM = 10

# (accel fps, gyro fps) pairs, chosen so every rate of D400 USB/GMSL (accel 50/63/100/200/250,
# gyro 50/100/200/400) and D585 (accel 100/200, gyro 200/400) is covered at least once.
# Pairs the device does not expose are skipped, and so are unequal pairs on devices whose motion sensor is UVC
# based (D457 GMSL): the SDK requires equal accel and gyro fps there.
IMU_RATE_PAIRS = [(50, 50), (100, 100), (100, 200), (200, 200), (200, 400), (63, 200), (250, 400)]

VIDEO_FPS = 30
PREFERRED_DEPTH_WIDTHS = [848, 640, 1280]
PREFERRED_COLOR_FORMATS = [rs.format.yuyv, rs.format.rgb8]
PREFERRED_COLOR_WIDTHS = [640, 848, 1280]

# Known open escapees, appended to a failure message to speed up triage.
ESCAPEE_HINTS = {
    "D585 over GMSL": "RSDEV-13850 (IMU pair batching: MIPI driver + SDK)",
    "D585 USB on Windows": "RSDEV-13643 (USB HID scheduling; RSDEV-14313 tracks Windows coverage)",
    "D400 over GMSL": "RSDSO-21807 / RSDSO-21592 (Tegra VI short-frame loss on JP6)",
}


class StreamRecord:
    """Per-stream capture. Each instance is written from a single callback path (one stream type, one thread),
    so add() needs no lock."""

    def __init__(self):
        self.frame_numbers = []
        self.timestamps_ms = []
        self.domains = []
        self.arrival_s = []

    def add(self, frame):
        self.frame_numbers.append(frame.get_frame_number())
        self.timestamps_ms.append(frame.get_timestamp())
        self.domains.append(frame.get_frame_timestamp_domain())
        self.arrival_s.append(time.perf_counter())


def count_frame_number_gaps(frame_numbers):
    """Number of missing frame numbers (a jump of N counts N - 1; repeats/rewinds count 1)."""
    gaps = 0
    for prev, cur in zip(frame_numbers, frame_numbers[1:]):
        step = cur - prev
        if step != 1:
            gaps += max(step - 1, 1)
    return gaps


def find_timestamp_gaps(timestamps_ms, fps, factor=TIMESTAMP_GAP_FACTOR):
    """Return [(index, delta_ms, estimated_missing_samples)] for every delta above factor * period or below 0."""
    period_ms = 1000.0 / fps
    gaps = []
    for i in range(1, len(timestamps_ms)):
        delta = timestamps_ms[i] - timestamps_ms[i - 1]
        if delta > factor * period_ms or delta < 0:
            gaps.append((i, delta, max(round(delta / period_ms) - 1, 1)))
    return gaps


def measured_rate_hz(arrival_s):
    span = arrival_s[-1] - arrival_s[0] if len(arrival_s) > 1 else 0.0
    return (len(arrival_s) - 1) / span if span > 0 else 0.0


def evaluate_stream(name, rec, fps):
    """Return a list of violation strings for one recorded stream (empty when it passes)."""
    if len(rec.frame_numbers) < MIN_FRAMES_PER_STREAM:
        return [f"{name}@{fps}: only {len(rec.frame_numbers)} frames received"]

    problems = []
    fn_gaps = count_frame_number_gaps(rec.frame_numbers)
    if fn_gaps > MAX_FRAME_NUMBER_GAPS:
        problems.append(f"{name}@{fps}: {fn_gaps} missing frame numbers (max {MAX_FRAME_NUMBER_GAPS})")

    # Without a hardware clock the timestamps are host arrival times: report that instead of fake gaps
    non_hw = [d for d in rec.domains if d != rs.timestamp_domain.hardware_clock]
    if non_hw:
        problems.append(f"{name}@{fps}: {len(non_hw)}/{len(rec.domains)} frames not in hardware_clock domain "
                        f"({', '.join(sorted({str(d) for d in non_hw}))}); timestamp-gap check is not meaningful")
        ts_gaps = []
    else:
        ts_gaps = find_timestamp_gaps(rec.timestamps_ms, fps)
    if len(ts_gaps) > MAX_TIMESTAMP_GAPS:
        missing = sum(g[2] for g in ts_gaps)
        worst = max(ts_gaps, key=lambda g: abs(g[1]))
        problems.append(f"{name}@{fps}: {len(ts_gaps)} timestamp gaps (~{missing} samples, worst {worst[1]:.1f} ms "
                        f"at frame #{rec.frame_numbers[worst[0]]}; max {MAX_TIMESTAMP_GAPS})")

    rate = measured_rate_hz(rec.arrival_s)
    if rate < fps * (1 - RATE_TOLERANCE):
        problems.append(f"{name}@{fps}: measured {rate:.2f} Hz, at least {fps * (1 - RATE_TOLERANCE):.2f} Hz expected "
                        f"(-{RATE_TOLERANCE * 100:.0f}%)")
    elif rate > fps * (1 + RATE_TOLERANCE):
        log.warning("%s@%d: measured %.2f Hz, faster than requested (no loss)", name, fps, rate)
    return problems


def escapee_hint(dev):
    """Name the open escapee matching this device/platform, for the failure message."""
    is_d585 = "D585" in dev.get_info(rs.camera_info.name)
    gmsl = dev.supports(rs.camera_info.connection_type) and dev.get_info(rs.camera_info.connection_type) == "GMSL"
    if is_d585 and gmsl:
        return ESCAPEE_HINTS["D585 over GMSL"]
    if is_d585 and platform.system() == "Windows":
        return ESCAPEE_HINTS["D585 USB on Windows"]
    if gmsl:
        return ESCAPEE_HINTS["D400 over GMSL"]
    return None


def _find_profile(sensor, stream, fps, fmt=None):
    return next((p for p in sensor.profiles
                 if p.stream_type() == stream and p.fps() == fps and (fmt is None or p.format() == fmt)), None)


def _pick_video_profile(sensor, stream, formats, widths):
    """Best-effort 30 fps profile: preferred formats first, then preferred widths."""
    candidates = [p for p in sensor.profiles if p.stream_type() == stream and p.fps() == VIDEO_FPS]
    for fmt in formats:
        for width in widths:
            for p in candidates:
                if p.format() == fmt and p.as_video_stream_profile().width() == width:
                    return p
        for p in candidates:
            if p.format() == fmt:
                return p
    return None


def _video_profiles(dev):
    """[(sensor, profile)] for depth and, when present, color."""
    result = []
    depth_sensor = dev.first_depth_sensor()
    depth_profile = _pick_video_profile(depth_sensor, rs.stream.depth, [rs.format.z16], PREFERRED_DEPTH_WIDTHS)
    if depth_profile is None:
        pytest.skip(f"No depth z16 profile at {VIDEO_FPS} fps")
    result.append((depth_sensor, depth_profile))
    try:
        color_sensor = dev.first_color_sensor()
    except RuntimeError:
        log.info("Device has no color sensor; video load is depth only")
        return result
    color_profile = _pick_video_profile(color_sensor, rs.stream.color, PREFERRED_COLOR_FORMATS, PREFERRED_COLOR_WIDTHS)
    if color_profile is not None:
        result.append((color_sensor, color_profile))
    return result


def _stream_and_record(dev, accel_fps, gyro_fps, with_video):
    """Stream for STREAM_DURATION_S and return {'accel': StreamRecord, 'gyro': StreamRecord}."""
    try:
        motion = dev.first_motion_sensor()
    except RuntimeError:
        pytest.skip("Device has no motion sensor")

    accel_profile = _find_profile(motion, rs.stream.accel, accel_fps, rs.format.motion_xyz32f)
    gyro_profile = _find_profile(motion, rs.stream.gyro, gyro_fps, rs.format.motion_xyz32f)
    if accel_profile is None or gyro_profile is None:
        pytest.skip(f"accel {accel_fps} / gyro {gyro_fps} not exposed by this device")

    video = _video_profiles(dev) if with_video else []

    records = {"accel": StreamRecord(), "gyro": StreamRecord()}
    video_frames = [0]

    def motion_callback(frame):
        stream = frame.get_profile().stream_type()
        if stream == rs.stream.accel:
            records["accel"].add(frame)
        elif stream == rs.stream.gyro:
            records["gyro"].add(frame)

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
        try:
            motion.open([accel_profile, gyro_profile])
        except RuntimeError as e:
            if "to be equal for this device" in str(e):
                pytest.skip(f"accel {accel_fps} / gyro {gyro_fps}: this device needs equal accel and gyro fps")
            raise
        opened.append(motion)
        for sensor, profile in video:
            sensor.open([profile])
            opened.append(sensor)
        for sensor, _ in video:
            sensor.start(video_callback)
            started.append(sensor)
        motion.start(motion_callback)
        started.append(motion)
        time.sleep(STREAM_DURATION_S)
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


def _run_continuity(test_device, accel_fps, gyro_fps, with_video):
    dev, _ = test_device
    records = _stream_and_record(dev, accel_fps, gyro_fps, with_video)

    problems = []
    for name, fps in (("accel", accel_fps), ("gyro", gyro_fps)):
        rec = records[name]
        log.info("%s@%d: %d frames, frame #%s..#%s, %.2f Hz", name, fps, len(rec.frame_numbers),
                 rec.frame_numbers[0] if rec.frame_numbers else "-",
                 rec.frame_numbers[-1] if rec.frame_numbers else "-", measured_rate_hz(rec.arrival_s))
        problems += evaluate_stream(name, rec, fps)

    if problems:
        hint = escapee_hint(dev)
        pytest.fail("IMU continuity failed:\n  " + "\n  ".join(problems) +
                    (f"\n  Known escapee: {hint}" if hint else ""))


def _pair_id(pair):
    return f"accel{pair[0]}-gyro{pair[1]}"


@pytest.mark.parametrize("accel_fps,gyro_fps", IMU_RATE_PAIRS, ids=[_pair_id(p) for p in IMU_RATE_PAIRS])
def test_imu_continuity_standalone(test_device, accel_fps, gyro_fps):
    _run_continuity(test_device, accel_fps, gyro_fps, with_video=False)


@pytest.mark.parametrize("accel_fps,gyro_fps", IMU_RATE_PAIRS, ids=[_pair_id(p) for p in IMU_RATE_PAIRS])
def test_imu_continuity_with_video(test_device, accel_fps, gyro_fps):
    _run_continuity(test_device, accel_fps, gyro_fps, with_video=True)
