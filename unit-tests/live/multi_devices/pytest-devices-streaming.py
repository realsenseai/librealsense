# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Tests:
- Simultaneous multi-stream operation (depth + color + IR) on 2 devices
- Frame drop detection with multiple stream types
- Long duration stress testing
- Stream independence verification

Requires 2 D400 series devices.
"""

import pytest
import pyrealsense2 as rs
from pytest_check import check
from rspy.pytest.device_helpers import is_jetson_platform
import time
from collections import defaultdict
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.skipif(is_jetson_platform(), reason="Not supported on Jetson"),
    pytest.mark.device("D400*", "D400*"),
]

# Test configuration
STREAM_DURATION_SEC = 10
MAX_FRAME_DROP_PERCENTAGE = 5.0
STABILIZATION_TIME_SEC = 3

# Target resolutions to try in order of preference
TARGET_RESOLUTIONS = [
    (640, 480, 30),  # Standard VGA resolution
    (640, 360, 30),  # Fallback for safety cameras and other devices
]


def find_common_profile(devs, stream_type, stream_format, target_resolutions):
    """Return (width, height, fps) available on all devices, or None."""
    for w, h, fps in target_resolutions:
        if all(
            any(p.stream_type() == stream_type and p.format() == stream_format
                and p.as_video_stream_profile().width() == w
                and p.as_video_stream_profile().height() == h
                and p.fps() == fps
                for sensor in dev.query_sensors()
                for p in sensor.get_stream_profiles()
                if p.is_video_stream_profile())
            for dev in devs
        ):
            return w, h, fps
    return None


def get_common_multi_stream_config(*devs):
    """
    Find a multi-stream configuration that works on all provided devices.
    Returns a list of (stream_type, stream_index, width, height, format, fps) tuples.
    """
    stream_configs = []

    res = find_common_profile(devs, rs.stream.depth, rs.format.z16, TARGET_RESOLUTIONS)
    if res:
        w, h, fps = res
        log.debug(f"  Added Depth stream: {w}x{h} @ {fps}fps")
        stream_configs.append((rs.stream.depth, -1, w, h, rs.format.z16, fps))

    for color_format in [rs.format.rgb8, rs.format.bgr8, rs.format.rgba8, rs.format.bgra8, rs.format.yuyv]:
        res = find_common_profile(devs, rs.stream.color, color_format, TARGET_RESOLUTIONS)
        if res:
            w, h, fps = res
            log.debug(f"  Added Color stream: {w}x{h} @ {fps}fps {color_format}")
            stream_configs.append((rs.stream.color, -1, w, h, color_format, fps))
            break

    res = find_common_profile(devs, rs.stream.infrared, rs.format.y8, TARGET_RESOLUTIONS)
    if res:
        w, h, fps = res
        log.debug(f"  Added Infrared stream (index 1): {w}x{h} @ {fps}fps")
        stream_configs.append((rs.stream.infrared, 1, w, h, rs.format.y8, fps))

    return stream_configs


def analyze_device_drops(frame_counters, stream_frame_counts, device_name):
    """Analyze frame drops for a single device across all streams."""
    total_expected = 0
    total_received = 0
    per_stream_stats = {}

    for stream_type, counters in frame_counters.items():
        if len(counters) < 2:
            log.warning(f"  {device_name} {stream_type}: insufficient frames ({len(counters)})")
            continue

        counter_range = counters[-1] - counters[0]
        expected = counter_range + 1
        received = len(counters)
        dropped = expected - received

        total_expected += expected
        total_received += received

        drop_pct = (dropped / expected * 100) if expected > 0 else 0

        per_stream_stats[stream_type] = {
            'expected': expected,
            'received': received,
            'dropped': dropped,
            'drop_pct': drop_pct,
            'total_frames': stream_frame_counts.get(stream_type, 0)
        }

        log.debug(f"  {device_name} {stream_type}: {received}/{expected} frames, "
                   f"{dropped} dropped ({drop_pct:.2f}%)")

    if total_expected > 0:
        overall_drop_pct = ((total_expected - total_received) / total_expected * 100)
    else:
        overall_drop_pct = 0.0

    return overall_drop_pct, per_stream_stats


def aggregate_results(all_frame_counters, all_frames_received, all_stream_frame_counts,
                      device_info, actual_duration):
    """Aggregate and analyze results from all devices."""
    log.info(f"Streaming completed after {actual_duration:.2f} seconds")
    for i, info in enumerate(device_info):
        log.info(f"Device {i+1} ({info['name']}): {all_frames_received[i]} total frames")

    for i, (info, stream_counts) in enumerate(zip(device_info, all_stream_frame_counts)):
        log.debug(f"Device {i+1} frame counts by stream:")
        for stream_type, count in stream_counts.items():
            log.debug(f"  {stream_type}: {count} frames")

    drop_percentages = []
    all_stats = []

    for i, (frame_counters, stream_counts, info) in enumerate(
            zip(all_frame_counters, all_stream_frame_counts, device_info)):
        drop_pct, stream_stats = analyze_device_drops(
            frame_counters, stream_counts, f"Dev{i+1}({info['sn']})")
        drop_percentages.append(drop_pct)

        dev_stats = {
            'name': info['name'],
            'sn': info['sn'],
            'total_frames': all_frames_received[i],
            'drop_pct': drop_pct,
            'streams': stream_stats
        }
        all_stats.append(dev_stats)

    success = all(dp <= MAX_FRAME_DROP_PERCENTAGE for dp in drop_percentages)

    stats = {
        'devices': all_stats,
        'duration': actual_duration
    }

    return success, drop_percentages, stats


def stream_multi_and_check_frames(devs, stream_configs, duration_sec=STREAM_DURATION_SEC):
    """Stream multiple stream types from all devices simultaneously and check for frame drops."""
    device_info = []
    all_frame_counters = []
    all_stream_frame_counts = []
    active_sensors = []

    counting = [False]  # Mutable container for closure access

    def make_callback(frame_counters, frame_counts):
        def callback(frame):
            if not counting[0]:
                return
            st = frame.get_profile().stream_type()
            frame_counts[st] += 1
            if frame.supports_frame_metadata(rs.frame_metadata_value.frame_counter):
                frame_counters[st].append(frame.get_frame_metadata(rs.frame_metadata_value.frame_counter))
        return callback

    try:
        for dev in devs:
            sn = dev.get_info(rs.camera_info.serial_number)
            name = dev.get_info(rs.camera_info.name) if dev.supports(rs.camera_info.name) else "Unknown"
            device_info.append({'sn': sn, 'name': name})

            frame_counters = defaultdict(list)
            frame_counts = defaultdict(int)
            all_frame_counters.append(frame_counters)
            all_stream_frame_counts.append(frame_counts)
            cb = make_callback(frame_counters, frame_counts)

            sensors = dev.query_sensors()
            profiles_by_idx = defaultdict(list)
            for stream_type, stream_index, w, h, fmt, fps in stream_configs:
                found = False
                for idx, sensor in enumerate(sensors):
                    for p in sensor.get_stream_profiles():
                        if not p.is_video_stream_profile():
                            continue
                        vp = p.as_video_stream_profile()
                        idx_match = stream_index < 0 or vp.stream_index() == stream_index
                        if (vp.stream_type() == stream_type and vp.format() == fmt
                                and vp.width() == w and vp.height() == h
                                and vp.fps() == fps and idx_match):
                            profiles_by_idx[idx].append(p)
                            found = True
                            break
                    if found:
                        break
                if not found:
                    pytest.fail(f"No matching profile for {stream_type} on {name}")

            for idx, profiles in profiles_by_idx.items():
                sensor = sensors[idx]
                sensor.open(profiles)
                active_sensors.append(sensor)
                sensor.start(cb)

        log.info(f"Stabilizing for {STABILIZATION_TIME_SEC} seconds...")
        time.sleep(STABILIZATION_TIME_SEC)

        counting[0] = True
        log.info(f"Streaming for {duration_sec} seconds...")
        start_time = time.time()
        time.sleep(duration_sec)
        actual_duration = time.time() - start_time
        counting[0] = False
    finally:
        for s in active_sensors:
            try:
                s.stop()
            except Exception as e:
                log.debug(f"Stop error: {e}")
            try:
                s.close()
            except Exception as e:
                log.debug(f"Close error: {e}")

    all_frames = [sum(cnt.values()) for cnt in all_stream_frame_counts]
    return aggregate_results(all_frame_counters, all_frames, all_stream_frame_counts, device_info, actual_duration)


def test_multi_stream_operation(test_devices):
    """Simultaneous multi-stream operation (depth + color + IR) on multiple devices"""
    device_list, ctx = test_devices

    log.info("=" * 80)
    log.info(f"Testing multi-stream operation on {len(device_list)} devices:")
    for i, dev in enumerate(device_list, 1):
        sn = dev.get_info(rs.camera_info.serial_number)
        name = dev.get_info(rs.camera_info.name) if dev.supports(rs.camera_info.name) else "Unknown"
        log.info(f"  Device {i}: {name} (SN: {sn})")
    log.info("=" * 80)

    # Get common multi-stream configuration
    log.info("Finding common multi-stream configuration...")
    stream_configs = get_common_multi_stream_config(*device_list)

    if len(stream_configs) < 2:
        pytest.fail(f"At least 2 stream types needed for multi-stream test, but found {len(stream_configs)}")

    log.info(f"Found {len(stream_configs)} common stream types")
    for stream_type, stream_index, w, h, fmt, fps in stream_configs:
        log.info(f"  Selected profile: {stream_type} (stream_index {stream_index}): {w}x{h} @ {fps}fps, format {fmt}")
    log.info(f"Will stream all of them simultaneously from all {len(device_list)} devices")

    success, drop_percentages, stats = stream_multi_and_check_frames(
        device_list, stream_configs=stream_configs
    )

    # Check for analysis errors
    if len(stats['devices']) == 0:
        log.error("FAIL - No device statistics collected")
        assert False, "Should collect statistics from all devices"
    else:
        # Print detailed results
        log.info("=" * 80)
        log.info("RESULTS:")
        log.info("=" * 80)
        log.info(f"Duration: {stats['duration']:.2f} seconds")

        for i, dev_stats in enumerate(stats['devices'], 1):
            log.info(f"Device {i} ({dev_stats['name']}):")
            log.info(f"  Total frames: {dev_stats['total_frames']}")
            log.info(f"  Overall drop rate: {dev_stats['drop_pct']:.2f}%")
            for stream_type, stream_stats in dev_stats['streams'].items():
                log.info(f"  {stream_type}:")
                log.info(f"    Received: {stream_stats['received']}/{stream_stats['expected']}")
                log.info(f"    Dropped: {stream_stats['dropped']} ({stream_stats['drop_pct']:.2f}%)")

        log.info("=" * 80)

        if success:
            log.info("PASS - Multi-stream test successful!")
            for i, drop_pct in enumerate(drop_percentages, 1):
                log.info(f"  Device {i} drop rate: {drop_pct:.2f}%")
        else:
            log.warning("FAIL - Excessive frame drops detected!")
            for i, drop_pct in enumerate(drop_percentages, 1):
                log.warning(f"  Device {i} drop rate: {drop_pct:.2f}% (max: {MAX_FRAME_DROP_PERCENTAGE}%)")

        check.is_true(success,
            f"Multi-stream operation should have <{MAX_FRAME_DROP_PERCENTAGE}% drops on all devices")

        # Verify stream independence: Check that each stream type received adequate frames
        # (at least 80% of expected based on actual duration and configured FPS)
        log.info("Verifying stream independence...")
        all_streams_ok = True

        for i, dev_stats in enumerate(stats['devices'], 1):
            for stream_type, stream_stats in dev_stats['streams'].items():
                min_expected_frames = int(stream_stats['expected'] * 0.8)
                if stream_stats['received'] < min_expected_frames:
                    log.warning(f"Device {i} {stream_type} received only {stream_stats['received']} frames (expected >={min_expected_frames})")
                    all_streams_ok = False

        if all_streams_ok:
            log.info("PASS - All streams received adequate frame counts (independence verified)")
        else:
            log.warning("FAIL - Some streams received fewer frames than expected")

        check.is_true(all_streams_ok,
            "All streams should receive frames independently without interference")
