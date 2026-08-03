# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
RealSense FPS Performance Test
Comprehensive testing of all supported resolutions and frame rates for Depth, Color, and IR streams
Includes multi-stream combination testing for Depth + Color combinations

Code Organization & Redundancy Reduction:
- check_stream_fps_accuracy_generic(): Consolidated function that eliminates ~200 lines of
  redundant code from the original three separate FPS test functions
- get_supported_stream_configurations(): Generic function that consolidates six "get_supported"
  functions, reducing ~100 lines of redundant code
- Multi-stream combination testing: Tests all combinations of depth and color configurations
- Overall code reduction: ~300 lines eliminated while adding comprehensive multi-stream testing
"""

import pytest
from rspy.stopwatch import Stopwatch
import pyrealsense2 as rs
import numpy as np
import platform
import time
import sys
from collections import deque
from typing import List, Tuple, Dict
import logging
log = logging.getLogger(__name__)

# No context marker: this test is always collected and scales its coverage by the active tier
# (see resolve_coverage_tier). gating runs everywhere, semi adds coverage on --context nightly,
# full runs the whole matrix on --context weekly.
pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_exclude("D401")
]

# Constants for device initialization and testing
DEVICE_INIT_SLEEP_SEC = 3  # Sleep time to allow device to get into idle state
DEFAULT_DEVICE_CREATION_TIMEOUT = 10  # Default timeout for device creation (seconds)
DDS_DEVICE_CREATION_TIMEOUT = 30  # Extended timeout for DDS devices (seconds)
MIN_FRAME_COUNT_LOW_FPS = 5  # Minimum frame count for low FPS tests
MIN_TEST_DURATION_PERCENT = 0.6  # Minimum test duration percentage (60%)

# ---------------------------------------------------------------------------------------------
# Coverage tiers
# ---------------------------------------------------------------------------------------------
# This test is split into three coverage tiers, selected by the active --context so each build
# runs exactly one tier (the highest present):
#   gating (no --context)      : very short sanity - runs on every build, incl. PR gating
#   semi   (--context nightly) : capped representative subset, kept well under ~10 min
#   full   (--context weekly)  : all supported permutations
# Rationale: back-to-back UVC open/start/stop/close storms can wedge the host USB controller on
# CI machines (and take the whole agent offline where the NIC shares that controller).
TIER_GATING = "gating"
TIER_SEMI = "semi"
TIER_FULL = "full"

# Per-tier caps (None = no cap, exercise every supported permutation).
TIER_CONFIG_CAP = {TIER_GATING: 1, TIER_SEMI: 8, TIER_FULL: None}
TIER_FPS_CAP = {TIER_GATING: 1, TIER_SEMI: 6, TIER_FULL: None}
# Full multistream is capped (not "all 930 combos") to keep weekly bounded; matches the prior
# CI cap. Config/FPS tests stay uncapped at full.
TIER_MULTISTREAM_CAP = {TIER_GATING: 1, TIER_SEMI: 6, TIER_FULL: 50}

# Let USB endpoints tear down between stream reconfigurations. Back-to-back stop/close -> open
# storms can wedge the host USB controller on CI machines.
SETTLE_DELAY = 0.5  # seconds


def resolve_coverage_tier(config):
    """Pick the highest active coverage tier from the --context option."""
    context = config.getoption("--context", default="").split()
    if "weekly" in context:
        return TIER_FULL
    if "nightly" in context:
        return TIER_SEMI
    return TIER_GATING


def _settle():
    """Pause so USB endpoints fully tear down before the next open()."""
    time.sleep(SETTLE_DELAY)


def format_duration(seconds):
    """
    Format duration in seconds to a human-readable string

    Args:
        seconds: Duration in seconds

    Returns:
        Formatted string (e.g., "2m 30s", "1h 15m 30s", "45s")
    """
    if seconds < 60:
        return f"{seconds:.1f}s"
    elif seconds < 3600:
        minutes = int(seconds // 60)
        remaining_seconds = seconds % 60
        return f"{minutes}m {remaining_seconds:.1f}s"
    else:
        hours = int(seconds // 3600)
        remaining_minutes = int((seconds % 3600) // 60)
        remaining_seconds = seconds % 60
        if remaining_seconds > 0:
            return f"{hours}h {remaining_minutes}m {remaining_seconds:.1f}s"
        elif remaining_minutes > 0:
            return f"{hours}h {remaining_minutes}m"
        else:
            return f"{hours}h"

def _evenly_spaced_subset(items, max_count):
    """
    Return a representative subset of a pre-sorted list with at most max_count entries.

    For max_count == 1 returns the middle item (a reliable smoke config for the gating tier -
    avoids both the highest-bandwidth and lowest-FPS extremes). For max_count >= 2 keeps the
    first and last item (the extremes) and spreads the remaining slots evenly across the middle.
    Order is preserved. max_count None means "return everything".

    Works for both (width, height, fps) config tuples and plain FPS-rate ints - the caller
    passes lists already sorted by the ordering that matters for that type.
    """
    if max_count is None or len(items) <= max_count:
        return items
    if max_count == 1:
        # A middle item - reliable smoke config for the gating tier. Avoids both extremes: the
        # highest-bandwidth mode (flaky FPS on constrained benches) and the lowest-FPS mode
        # (too few frames for a stable measurement).
        return [items[len(items) // 2]]

    # Evenly spaced across the full range, endpoints included. Distinct indices for len > max_count
    # (the >1 step keeps consecutive picks apart), so exactly max_count items are returned.
    n = len(items)
    idx = sorted({round(i * (n - 1) / (max_count - 1)) for i in range(max_count)})
    return [items[i] for i in idx]


class FPSMonitor:
    """Monitor and calculate FPS statistics"""

    def __init__(self, window_size: int = 30):
        self.window_size = window_size
        self.frame_times = deque(maxlen=window_size)
        self.start_time = None
        self.total_frames = 0

    def reset(self):
        """Reset the FPS monitor"""
        self.frame_times.clear()
        self.start_time = None
        self.total_frames = 0

    def update(self, frame_time: float):
        """Update with new frame timestamp"""
        if self.start_time is None:
            self.start_time = frame_time

        self.frame_times.append(frame_time)
        self.total_frames += 1

    def get_current_fps(self) -> float:
        """Calculate current FPS based on recent frames"""
        if len(self.frame_times) < 2:
            return 0.0

        time_diff = self.frame_times[-1] - self.frame_times[0]
        if time_diff <= 0:
            return 0.0

        return (len(self.frame_times) - 1) / time_diff

    def get_average_fps(self) -> float:
        """Calculate average FPS since start"""
        if self.start_time is None or self.total_frames < 2:
            return 0.0

        elapsed_time = self.frame_times[-1] - self.start_time
        if elapsed_time <= 0:
            return 0.0

        return self.total_frames / elapsed_time

# Start depth + color streams and measure the time from stream opened until first frame arrived using sensor API.
# Verify that the time do not exceeds the maximum time allowed
# Note - Using Windows Media Foundation to handle power management between USB actions take time (~27 ms)


def time_to_first_frame(sensor, profile, max_delay_allowed):
    """
    Wait for the first frame for 'max_delay_allowed' + 1 extra second
    If the frame arrives it will return the seconds it took since open() call
    If no frame it will return 'max_delay_allowed'
    """
    first_frame_time = max_delay_allowed
    open_call_stopwatch = Stopwatch()

    def frame_cb(frame):
        nonlocal first_frame_time, open_call_stopwatch
        if first_frame_time == max_delay_allowed:
            first_frame_time = open_call_stopwatch.get_elapsed()

    open_call_stopwatch.reset()
    sensor.open(profile)
    sensor.start(frame_cb)

    # Wait condition:
    # 1. first frame did not arrive yet
    # 2. timeout of 'max_delay_allowed' + 1 extra second reached.
    while first_frame_time == max_delay_allowed and open_call_stopwatch.get_elapsed() < max_delay_allowed + 1:
        time.sleep(0.05)

    sensor.stop()
    sensor.close()
    _settle()

    return first_frame_time


def check_depth_fps_accuracy(device, expected_fps: int, width: int = None, height: int = None, test_duration: float = 10.0, fps_tolerance: float = 0.1):
    """
    Test depth FPS accuracy over a specified duration

    Args:
        device: RealSense device
        expected_fps: Expected FPS (e.g., 30, 60)
        width: Expected width (optional - if None, finds any profile)
        height: Expected height (optional - if None, finds any profile)
        test_duration: How long to test in seconds
        fps_tolerance: Allowed FPS deviation (e.g., 0.1 = 10%)

    Returns:
        Tuple[bool, float, Dict]: (passed, actual_fps, stats)
    """
    return check_stream_fps_accuracy_generic(
        device, "depth", rs.stream.depth, rs.format.z16,
        lambda d: d.first_depth_sensor(),
        expected_fps, width, height, test_duration, fps_tolerance
    )


def check_color_fps_accuracy(device, expected_fps: int, width: int = None, height: int = None, test_duration: float = 10.0, fps_tolerance: float = 0.1):
    """
    Test color/RGB FPS accuracy over a specified duration

    Args:
        device: RealSense device
        expected_fps: Expected FPS (e.g., 30, 60)
        width: Expected width (optional - if None, finds any profile)
        height: Expected height (optional - if None, finds any profile)
        test_duration: How long to test in seconds
        fps_tolerance: Allowed FPS deviation (e.g., 0.1 = 10%)

    Returns:
        Tuple[bool, float, Dict]: (passed, actual_fps, stats)
    """
    def get_color_sensor(device):
        try:
            return device.first_color_sensor()
        except RuntimeError:
            raise RuntimeError("No color sensor found on device")

    return check_stream_fps_accuracy_generic(
        device, "color", rs.stream.color, [rs.format.rgb8, rs.format.bgr8, rs.format.yuyv],
        get_color_sensor,
        expected_fps, width, height, test_duration, fps_tolerance
    )


def check_ir_fps_accuracy(device, expected_fps: int, width: int = None, height: int = None, test_duration: float = 10.0, fps_tolerance: float = 0.1):
    """
    Test IR FPS accuracy over a specified duration

    Args:
        device: RealSense device
        expected_fps: Expected FPS (e.g., 30, 60)
        width: Expected width (optional - if None, finds any profile)
        height: Expected height (optional - if None, finds any profile)
        test_duration: How long to test in seconds
        fps_tolerance: Allowed FPS deviation (e.g., 0.1 = 10%)

    Returns:
        Tuple[bool, float, Dict]: (passed, actual_fps, stats)
    """
    def get_ir_sensor(device):
        for sensor in device.sensors:
            for profile in sensor.profiles:
                if profile.stream_type() == rs.stream.infrared:
                    return sensor
        raise RuntimeError("No IR sensor found on device")

    return check_stream_fps_accuracy_generic(
        device, "IR", rs.stream.infrared, rs.format.y8,
        get_ir_sensor,
        expected_fps, width, height, test_duration, fps_tolerance
    )


def check_stream_fps_accuracy_generic(device, stream_name, stream_type, formats, get_sensor_func, expected_fps: int, width: int = None, height: int = None, test_duration: float = 10.0, fps_tolerance: float = 0.1):
    """
    Generic FPS accuracy test for any stream type - eliminates code duplication

    Args:
        device: RealSense device
        stream_name: Name for logging (e.g., "depth", "color", "IR")
        stream_type: rs.stream type
        formats: Single format or list of formats to try
        get_sensor_func: Function to get sensor from device
        expected_fps: Expected FPS (e.g., 30, 60)
        width: Expected width (optional - if None, finds any profile)
        height: Expected height (optional - if None, finds any profile)
        test_duration: How long to test in seconds
        fps_tolerance: Allowed FPS deviation (e.g., 0.1 = 10%)

    Returns:
        Tuple[bool, float, Dict]: (passed, actual_fps, stats)
    """
    fps_monitor = FPSMonitor(window_size=60)

    # Get sensor
    try:
        sensor = get_sensor_func(device)
    except RuntimeError as e:
        return False, 0.0, {"error": str(e)}

    # Ensure formats is a list
    if not isinstance(formats, list):
        formats = [formats]

    # Find profile with the specified FPS and optionally resolution
    profile = None
    for format_type in formats:
        for p in sensor.profiles:
            if (p.fps() == expected_fps and
                p.stream_type() == stream_type and
                p.format() == format_type):
                if width is not None and height is not None:
                    vp = p.as_video_stream_profile()
                    if vp.width() == width and vp.height() == height:
                        profile = p
                        break
                else:
                    profile = p
                    break
        if profile:
            break

    if not profile:
        error_msg = f"No {stream_name} profile found with {expected_fps} FPS"
        if width is not None and height is not None:
            error_msg += f" and resolution {width}x{height}"
        log.error(error_msg)
        return False, 0.0, {"error": error_msg}

    # Frame callback to collect timing data
    frame_count = 0
    fps_measurements = []
    start_test_time = None

    # Adjust warmup frames and measurement interval based on FPS rate
    if expected_fps <= 6:
        warmup_frames = 2      # Minimal warmup for very slow FPS to maximize measurement time
        # For very low FPS we measure every frame after warmup to avoid having only a single data point
        # (previously interval=2 caused too few measurements for short tests)
        measurement_interval = 1  # Measure on every frame post-warmup
        log.info(f"Very low FPS mode for {expected_fps} FPS: warmup={warmup_frames}, interval={measurement_interval} (per-frame measurement)")
    elif expected_fps <= 15:
        warmup_frames = 5      # Reduced warmup for slow FPS
        measurement_interval = 10
        log.info(f"Medium-low FPS mode for {expected_fps} FPS: warmup={warmup_frames}, interval={measurement_interval}")
    elif expected_fps <= 30:
        warmup_frames = 15     # Reduced warmup for 30 FPS
        measurement_interval = 15  # More frequent measurements for better statistics
        log.info(f"Standard FPS mode for {expected_fps} FPS: warmup={warmup_frames}, interval={measurement_interval}")
    elif expected_fps <= 60:
        warmup_frames = 20     # Moderate warmup for 60 FPS
        measurement_interval = 20  # Optimized for sufficient measurements
        log.info(f"High FPS mode for {expected_fps} FPS: warmup={warmup_frames}, interval={measurement_interval}")
    else:
        warmup_frames = 25     # Higher warmup for very high FPS (90+)
        measurement_interval = 25  # Balance between frequency and performance
        log.info(f"Very high FPS mode for {expected_fps} FPS: warmup={warmup_frames}, interval={measurement_interval}")

    def frame_callback(frame):
        nonlocal frame_count, start_test_time
        current_time = time.time()

        if start_test_time is None:
            start_test_time = current_time

        fps_monitor.update(current_time)
        frame_count += 1

        # Record FPS after warmup period, using adaptive measurement interval
        if frame_count > warmup_frames and frame_count % measurement_interval == 0:
            current_fps = fps_monitor.get_current_fps()
            if current_fps > 0:
                fps_measurements.append(current_fps)

    # Start streaming
    try:
        sensor.open(profile)
        sensor.start(frame_callback)

        # Wait for test duration with adaptive early exit for all FPS rates
        test_stopwatch = Stopwatch()
        # Adjust minimum measurements needed based on FPS rate
        if expected_fps <= 6:
            min_measurements_needed = 2  # Reduced requirement for very low FPS
        else:
            min_measurements_needed = 3  # Standard requirement for other FPS rates

        while test_stopwatch.get_elapsed() < test_duration:
            time.sleep(0.1)

            # Early exit logic based on FPS rate and sufficient measurements
            if len(fps_measurements) >= min_measurements_needed:
                elapsed = test_stopwatch.get_elapsed()

                if expected_fps <= 6:
                    # Very low FPS: exit early after 50% of test duration
                    if elapsed >= (test_duration * 0.5):
                        log.info(f"Very low FPS test ({expected_fps} FPS) collected {len(fps_measurements)} measurements in {elapsed:.1f}s - sufficient for analysis")
                        break
                elif expected_fps <= 15:
                    # Low FPS: exit early after 65% of test duration
                    if elapsed >= (test_duration * 0.65):
                        log.info(f"Medium-low FPS test ({expected_fps} FPS) collected {len(fps_measurements)} measurements in {elapsed:.1f}s - sufficient for analysis")
                        break
                elif expected_fps <= 30:
                    # Standard FPS: exit early after 70% of test duration
                    if elapsed >= (test_duration * 0.70):
                        log.info(f"Standard FPS test ({expected_fps} FPS) collected {len(fps_measurements)} measurements in {elapsed:.1f}s - sufficient for analysis")
                        break
                elif expected_fps <= 60:
                    # High FPS: exit early after 75% of test duration
                    if elapsed >= (test_duration * 0.75):
                        log.info(f"High FPS test ({expected_fps} FPS) collected {len(fps_measurements)} measurements in {elapsed:.1f}s - sufficient for analysis")
                        break
                else:
                    # Very high FPS: exit early after 80% of test duration
                    if elapsed >= (test_duration * 0.80):
                        log.info(f"Very high FPS test ({expected_fps} FPS) collected {len(fps_measurements)} measurements in {elapsed:.1f}s - sufficient for analysis")
                        break

            # Special case for very low FPS: allow early exit with fewer measurements if we have reasonable frame count
            if expected_fps <= 6 and len(fps_measurements) >= 1 and frame_count >= MIN_FRAME_COUNT_LOW_FPS:
                elapsed = test_stopwatch.get_elapsed()
                if elapsed >= (test_duration * MIN_TEST_DURATION_PERCENT):  # At least 60% of test duration
                    log.info(f"Very low FPS test ({expected_fps} FPS) collected {len(fps_measurements)} measurements with {frame_count} frames in {elapsed:.1f}s - acceptable for low FPS analysis")
                    break

    finally:
        # Guard teardown so a failed open()/start() surfaces its real error instead of being
        # masked by a "UVC device is not streaming" raised from stop(). Log (don't mask) the
        # teardown error so CI post-mortem has a trace. Settle before next open().
        try:
            sensor.stop()
        except Exception as e:
            log.debug(f"{stream_name} sensor.stop() failed during teardown: {e}")
        try:
            sensor.close()
        except Exception as e:
            log.debug(f"{stream_name} sensor.close() failed during teardown: {e}")
        _settle()

    # Calculate statistics
    if not fps_measurements:
        return False, 0.0, {"error": f"No FPS measurements collected after {test_stopwatch.get_elapsed():.1f}s (expected {expected_fps} FPS, warmup: {warmup_frames} frames, got {frame_count} total frames)"}

    # For very low FPS (<=6), allow single measurement if we have a minimal reasonable frame count
    # Relaxed from 10 to MIN_FRAME_COUNT_LOW_FPS (default 5) because per-frame measurement now provides limited data
    # in short-duration configuration tests (e.g., 3s) where < 10 frames may be expected (~18 frames max at 6 FPS)
    if expected_fps <= 6 and len(fps_measurements) == 1 and frame_count >= MIN_FRAME_COUNT_LOW_FPS:
        log.info(f"Very low FPS ({expected_fps}): accepting single measurement with {frame_count} frames (threshold {MIN_FRAME_COUNT_LOW_FPS})")
        actual_avg_fps = fps_measurements[0]
        fps_min = fps_max = actual_avg_fps
        fps_std = 0.0
    elif len(fps_measurements) < 2:
        return False, 0.0, {"error": f"Insufficient FPS measurements: {len(fps_measurements)} (need >=2 for statistics, or >=1 with >={MIN_FRAME_COUNT_LOW_FPS} frames for <=6 FPS). Got {frame_count} frames in {test_stopwatch.get_elapsed():.1f}s with warmup={warmup_frames}, interval={measurement_interval}"}
    else:
        actual_avg_fps = sum(fps_measurements) / len(fps_measurements)
        fps_min = min(fps_measurements)
        fps_max = max(fps_measurements)
        fps_std = np.std(fps_measurements) if len(fps_measurements) > 1 else 0.0

    # Calculate deviation from expected FPS
    fps_deviation = abs(actual_avg_fps - expected_fps) / expected_fps
    fps_passed = fps_deviation <= fps_tolerance

    stats = {
        "frame_count": frame_count,
        "test_duration": test_stopwatch.get_elapsed(),
        "expected_fps": expected_fps,
        "width": width,
        "height": height,
        "actual_avg_fps": actual_avg_fps,
        "fps_min": fps_min,
        "fps_max": fps_max,
        "fps_std": fps_std,
        "fps_deviation": fps_deviation,
        "fps_tolerance": fps_tolerance,
        "measurements": fps_measurements,
        "warmup_frames": warmup_frames,
        "measurement_interval": measurement_interval,
        "measurements_count": len(fps_measurements)
    }

    # Extra logging for optimized FPS cases
    if expected_fps <= 30:
        log.info(f"Optimized FPS test completed: {frame_count} frames in {test_stopwatch.get_elapsed():.1f}s, "
              f"{len(fps_measurements)} measurements, avg FPS: {actual_avg_fps:.2f}")
    elif expected_fps <= 60:
        log.info(f"High FPS test completed: {frame_count} frames in {test_stopwatch.get_elapsed():.1f}s, "
              f"{len(fps_measurements)} measurements, avg FPS: {actual_avg_fps:.2f}")

    return fps_passed, actual_avg_fps, stats


# The check_color_fps_accuracy and check_ir_fps_accuracy functions have been consolidated
# into the generic check_stream_fps_accuracy_generic function above to eliminate redundancy


def get_supported_stream_configurations(device, stream_type, format_filter, get_sensor_func, include_resolution=True):
    """
    Generic function to get supported configurations for any stream type - eliminates redundancy

    Args:
        device: RealSense device
        stream_type: rs.stream type
        format_filter: Format to filter by (or None for any format)
        get_sensor_func: Function to get sensor from device
        include_resolution: If True, return (width, height, fps), if False return just fps

    Returns:
        List of configurations or FPS rates
    """
    try:
        sensor = get_sensor_func(device)
    except RuntimeError:
        return []

    if include_resolution:
        supported_configs = set()
        for profile in sensor.profiles:
            if profile.stream_type() == stream_type:
                if format_filter is None or profile.format() == format_filter:
                    vp = profile.as_video_stream_profile()
                    supported_configs.add((vp.width(), vp.height(), vp.fps()))
        return sorted(list(supported_configs), key=lambda x: (x[0] * x[1], x[2]))
    else:
        supported_fps = set()
        for profile in sensor.profiles:
            if profile.stream_type() == stream_type:
                if format_filter is None or profile.format() == format_filter:
                    supported_fps.add(profile.fps())
        return sorted(list(supported_fps))


def get_supported_depth_fps_rates(device):
    """Discover all supported FPS rates for depth streams"""
    return get_supported_stream_configurations(
        device, rs.stream.depth, rs.format.z16,
        lambda d: d.first_depth_sensor(),
        include_resolution=False
    )


def get_supported_depth_configurations(device):
    """Discover all supported resolution and FPS combinations for depth streams"""
    return get_supported_stream_configurations(
        device, rs.stream.depth, rs.format.z16,
        lambda d: d.first_depth_sensor(),
        include_resolution=True
    )


def get_supported_color_fps_rates(device):
    """Discover all supported FPS rates for color/RGB streams"""
    def get_color_sensor_safe(device):
        try:
            return device.first_color_sensor()
        except RuntimeError:
            raise RuntimeError("No color sensor available")

    return get_supported_stream_configurations(
        device, rs.stream.color, None,  # No format filter for color
        get_color_sensor_safe,
        include_resolution=False
    )


def get_supported_color_configurations(device):
    """Discover all supported resolution and FPS combinations for color streams"""
    def get_color_sensor_safe(device):
        try:
            return device.first_color_sensor()
        except RuntimeError:
            raise RuntimeError("No color sensor available")

    return get_supported_stream_configurations(
        device, rs.stream.color, None,  # No format filter for color
        get_color_sensor_safe,
        include_resolution=True
    )


def get_supported_ir_fps_rates(device):
    """Discover all supported FPS rates for IR streams"""
    def get_ir_sensor(device):
        for sensor in device.sensors:
            for profile in sensor.profiles:
                if profile.stream_type() == rs.stream.infrared:
                    return sensor
        raise RuntimeError("No IR sensor found")

    return get_supported_stream_configurations(
        device, rs.stream.infrared, rs.format.y8,
        get_ir_sensor,
        include_resolution=False
    )


def get_supported_ir_configurations(device):
    """Discover all supported resolution and FPS combinations for IR streams"""
    def get_ir_sensor(device):
        for sensor in device.sensors:
            for profile in sensor.profiles:
                if profile.stream_type() == rs.stream.infrared:
                    return sensor
        raise RuntimeError("No IR sensor found")

    return get_supported_stream_configurations(
        device, rs.stream.infrared, rs.format.y8,
        get_ir_sensor,
        include_resolution=True
    )


def get_fps_test_parameters(fps_rate):
    """
    Get optimal test parameters for different FPS rates (optimized for CI)

    Args:
        fps_rate: The FPS rate to test

    Returns:
        Tuple[float, float]: (test_duration, tolerance)
    """
    # Configuration: list of (threshold, (duration, tolerance))
    fps_test_config = [
        (6,   (12.0, 0.35)),  # Very low FPS: extended test time and higher tolerance
        (15,  (10.0, 0.25)),  # Low FPS: increased test time and tolerance
        (30,  (8.0, 0.15)),   # Standard FPS: increased duration for better measurements
        (60,  (6.0, 0.18)),   # High FPS: optimized duration and tolerance
        (90,  (4.0, 0.20)),   # Very high FPS: shorter test with higher tolerance
    ]
    for threshold, params in fps_test_config:
        if fps_rate <= threshold:
            return params
    return (3.0, 0.25)  # Extremely high FPS: quickest test, highest tolerance


def check_stream_fps_accuracy_comprehensive(device, stream_type_name, test_function, get_fps_function, max_fps_rates=None):
    """
    Comprehensive FPS accuracy test for any stream type

    Args:
        device: RealSense device
        stream_type_name: Name of stream type for logging (e.g., "depth", "color", "IR")
        test_function: Function to test FPS for this stream type
        get_fps_function: Function to get supported FPS rates for this stream type
        max_fps_rates: Coverage cap — limits FPS rates tested to an evenly-spaced subset of this
            size. None (default) means test all supported rates.

    Returns:
        Tuple[bool, List[Dict]]: (all_tests_passed, results_list)
    """
    log.info(f"Discovering supported {stream_type_name} FPS rates...")
    supported_fps_rates = get_fps_function(device)

    if not supported_fps_rates:
        log.warning(f"No supported {stream_type_name} FPS rates found!")
        return False, []

    # Coverage tier: limit to a representative subset of FPS rates (None = all supported)
    original_count = len(supported_fps_rates)
    supported_fps_rates = _evenly_spaced_subset(supported_fps_rates, max_fps_rates)
    if len(supported_fps_rates) != original_count:
        log.info(f"Coverage: limiting {stream_type_name} FPS rates from {original_count} to "
                 f"{len(supported_fps_rates)} (cap={max_fps_rates})")

    log.info(f"Found supported {stream_type_name} FPS rates: {supported_fps_rates}")

    # Test results summary
    all_fps_results = []
    all_tests_passed = True

    # Test each supported FPS rate
    for fps_rate in supported_fps_rates:
        log.info(f"\n--- Testing {fps_rate} FPS {stream_type_name} stream accuracy ---")

        # Get optimal test parameters for this FPS rate
        test_duration, tolerance = get_fps_test_parameters(fps_rate)

        try:
            fps_passed, fps_actual, fps_stats = test_function(
                device,
                expected_fps=fps_rate,
                test_duration=test_duration,
                fps_tolerance=tolerance
            )

            if 'error' not in fps_stats:
                # Log detailed results
                log.info(f"{stream_type_name.title()} {fps_rate} FPS Results:")
                log.info(f"  Expected FPS: {fps_stats['expected_fps']}")
                log.info(f"  Actual Average FPS: {fps_stats['actual_avg_fps']:.2f}")
                log.info(f"  FPS Range: {fps_stats['fps_min']:.2f} - {fps_stats['fps_max']:.2f}")
                log.info(f"  FPS Std Dev: {fps_stats['fps_std']:.2f}")
                log.info(f"  FPS Deviation: {fps_stats['fps_deviation']*100:.1f}%")
                log.info(f"  Tolerance: {tolerance*100:.0f}%")
                log.info(f"  Frames Captured: {fps_stats['frame_count']}")
                log.info(f"  Test Duration: {fps_stats['test_duration']:.2f}s")
                log.info(f"  Result: {'PASS' if fps_passed else 'FAIL'}")

                # Store results for summary
                all_fps_results.append({
                    'fps_rate': fps_rate,
                    'passed': fps_passed,
                    'actual_fps': fps_actual,
                    'deviation': fps_stats['fps_deviation'],
                    'tolerance': tolerance,
                    'frame_count': fps_stats['frame_count']
                })

                # Check individual FPS test
                assert fps_passed, f"{fps_rate} FPS {stream_type_name} accuracy test - Expected: {fps_rate} FPS, Got: {fps_actual:.2f} FPS"

                if not fps_passed:
                    all_tests_passed = False

            else:
                log.error(f"Failed to test {fps_rate} FPS {stream_type_name}: {fps_stats['error']}")
                all_tests_passed = False
                assert False, f"{fps_rate} FPS {stream_type_name} test failed: {fps_stats['error']}"

        except Exception as e:
            log.error(f"Exception during {fps_rate} FPS {stream_type_name} test: {e}")
            all_tests_passed = False
            assert False, f"{fps_rate} FPS {stream_type_name} test exception: {e}"

    return all_tests_passed, all_fps_results


def print_fps_test_summary(stream_type_name, supported_fps_rates, all_fps_results, all_tests_passed, product_line):
    """
    Print comprehensive summary for FPS tests with detailed statistics
    """
    log.info(f"\n{'='*70}")
    log.info(f"{stream_type_name.upper()} FPS ACCURACY TEST SUMMARY")
    log.info(f"{'='*70}")
    log.info(f"Device: {product_line}")
    log.info(f"Total FPS rates tested: {len(supported_fps_rates)}")

    if all_fps_results:
        log.info(f"\nDetailed Results:")
        log.info(f"{'FPS':<5} {'Status':<6} {'Actual':<8} {'Deviation':<10} {'Tolerance':<9} {'Frames':<7}")
        log.info(f"{'-'*55}")

        for result in all_fps_results:
            status = "PASS" if result['passed'] else "FAIL"
            log.info(f"{result['fps_rate']:<5} {status:<6} {result['actual_fps']:<8.2f} "
                  f"{result['deviation']*100:<9.1f}% {result['tolerance']*100:<8.0f}% {result['frame_count']:<7}")

        # Calculate comprehensive statistics
        passed_tests = sum(1 for r in all_fps_results if r['passed'])
        failed_tests = len(all_fps_results) - passed_tests

        # FPS accuracy statistics
        actual_fps_values = [r['actual_fps'] for r in all_fps_results if r['actual_fps'] > 0]
        expected_fps_values = [r['fps_rate'] for r in all_fps_results]
        deviation_values = [r['deviation'] for r in all_fps_results if r['deviation'] < 1.0]  # Exclude error cases

        if actual_fps_values:
            avg_actual_fps = sum(actual_fps_values) / len(actual_fps_values)
            min_actual_fps = min(actual_fps_values)
            max_actual_fps = max(actual_fps_values)
            fps_std_dev = np.std(actual_fps_values) if len(actual_fps_values) > 1 else 0.0

            # Expected vs Actual ranges
            min_expected_fps = min(expected_fps_values)
            max_expected_fps = max(expected_fps_values)

            # Deviation statistics
            if deviation_values:
                avg_deviation = sum(deviation_values) / len(deviation_values)
                min_deviation = min(deviation_values)
                max_deviation = max(deviation_values)
                deviation_std = np.std(deviation_values) if len(deviation_values) > 1 else 0.0
            else:
                avg_deviation = min_deviation = max_deviation = deviation_std = 0.0

            # Frame count statistics
            frame_counts = [r['frame_count'] for r in all_fps_results if r['frame_count'] > 0]
            if frame_counts:
                avg_frames = sum(frame_counts) / len(frame_counts)
                min_frames = min(frame_counts)
                max_frames = max(frame_counts)
                total_frames = sum(frame_counts)
            else:
                avg_frames = min_frames = max_frames = total_frames = 0

            log.info(f"\n--- {stream_type_name.upper()} STREAM STATISTICS ---")

            log.info(f"\nTest Results Summary:")
            log.info(f"  Tests Passed: {passed_tests}/{len(all_fps_results)} ({passed_tests/len(all_fps_results)*100:.1f}%)")
            log.info(f"  Tests Failed: {failed_tests}/{len(all_fps_results)} ({failed_tests/len(all_fps_results)*100:.1f}%)")
            log.info(f"  Success Rate: {passed_tests/len(all_fps_results)*100:.1f}%")

            log.info(f"\nFPS Rate Coverage:")
            log.info(f"  Expected FPS Range: {min_expected_fps} - {max_expected_fps} FPS")
            log.info(f"  FPS Rate Count: {len(set(expected_fps_values))} unique rates")
            log.info(f"  Supported Rates: {sorted(set(expected_fps_values))}")

            log.info(f"\nActual FPS Performance:")
            log.info(f"  Average Actual FPS: {avg_actual_fps:.2f}")
            log.info(f"  Actual FPS Range: {min_actual_fps:.2f} - {max_actual_fps:.2f}")
            log.info(f"  Actual FPS Std Dev: {fps_std_dev:.2f}")

            log.info(f"\nAccuracy Statistics:")
            log.info(f"  Average Deviation: {avg_deviation*100:.2f}%")
            log.info(f"  Deviation Range: {min_deviation*100:.2f}% - {max_deviation*100:.2f}%")
            log.info(f"  Deviation Std Dev: {deviation_std*100:.2f}%")

            log.info(f"\nFrame Capture Statistics:")
            log.info(f"  Total Frames Captured: {total_frames:,}")
            log.info(f"  Average Frames/Test: {avg_frames:.1f}")
            log.info(f"  Frame Count Range: {min_frames} - {max_frames}")

            # Performance categories
            excellent_tests = sum(1 for r in all_fps_results if r['passed'] and r['deviation'] <= 0.05)
            good_tests = sum(1 for r in all_fps_results if r['passed'] and 0.05 < r['deviation'] <= 0.10)
            acceptable_tests = sum(1 for r in all_fps_results if r['passed'] and r['deviation'] > 0.10)

            log.info(f"\nPerformance Categories:")
            log.info(f"  Excellent (<=5% deviation): {excellent_tests} tests")
            log.info(f"  Good (5-10% deviation): {good_tests} tests")
            log.info(f"  Acceptable (>10% deviation): {acceptable_tests} tests")
            log.info(f"  Failed: {failed_tests} tests")

        log.info(f"\nOverall {stream_type_name.upper()} Result: {'PASS' if all_tests_passed else 'FAIL'}")

    # Final overall check
    assert all_tests_passed, f"All supported {stream_type_name} FPS rates accuracy test - {len(supported_fps_rates)} rates tested"


def check_stream_configurations_comprehensive(device, stream_type_name, test_function, get_configurations_function,
                                            max_configs=None):
    """
    Test all supported resolution and FPS configurations for a stream type

    Args:
        device: RealSense device
        stream_type_name: Name of stream type for logging (e.g., "depth", "color", "IR")
        test_function: Function to test individual configuration (e.g., check_depth_fps_accuracy)
        get_configurations_function: Function to get supported configurations
        max_configs: Coverage cap — limits configurations tested to an evenly-spaced subset of this
            size. None (default) means test all supported configurations.

    Per-config streaming time and tolerance are derived from the FPS via get_fps_test_parameters
    (same as the FPS-rate and multi-stream paths), so low-FPS modes stream long enough to collect
    a stable, steady-state measurement instead of only startup-burst frames.

    Returns:
        Tuple[bool, List[Dict]]: (all_passed, list_of_results)
    """
    log.info(f"\nTesting all supported {stream_type_name} configurations...")

    # Get all supported configurations
    try:
        supported_configs = get_configurations_function(device)
    except Exception as e:
        log.error(f"Failed to get supported {stream_type_name} configurations: {e}")
        return False, []

    if not supported_configs:
        log.warning(f"No supported {stream_type_name} configurations found")
        return False, []

    log.info(f"Found {len(supported_configs)} {stream_type_name} configurations")

    # Coverage tier: limit to a representative subset of configurations (None = all supported)
    original_count = len(supported_configs)
    supported_configs = _evenly_spaced_subset(supported_configs, max_configs)
    if len(supported_configs) != original_count:
        log.info(f"Coverage: limiting {stream_type_name} configurations from {original_count} to "
                 f"{len(supported_configs)} (cap={max_configs})")

    log.info(f"Testing {len(supported_configs)} {stream_type_name} configurations:")
    for width, height, fps in supported_configs:
        log.info(f"  {width}x{height} @ {fps} FPS")

    all_results = []
    all_passed = True

    for i, (width, height, fps) in enumerate(supported_configs):
        config_name = f"{width}x{height}@{fps}fps"
        log.info(f"\nTesting {stream_type_name} configuration {i+1}/{len(supported_configs)}: {config_name}")

        # Streaming time scales with FPS: low FPS needs a longer stream to gather enough frames
        test_duration, fps_tolerance = get_fps_test_parameters(fps)

        try:
            # Test this specific configuration
            passed, actual_fps, stats = test_function(
                device, fps, width, height, test_duration, fps_tolerance
            )

            # Store results
            result = {
                "width": width,
                "height": height,
                "expected_fps": fps,
                "actual_fps": actual_fps,
                "passed": passed,
                "deviation": stats.get('fps_deviation', 0.0),
                "tolerance": fps_tolerance,
                "frame_count": stats.get('frame_count', 0),
                "config_name": config_name,
                "stats": stats
            }

            all_results.append(result)

            if not passed:
                all_passed = False
                if 'error' in stats:
                    log.error(f"  ERROR: {stats['error']}")
                else:
                    log.error(f"  FAILED: Expected {fps} FPS, got {actual_fps:.1f} FPS "
                          f"(deviation: {stats.get('fps_deviation', 0)*100:.1f}%)")
            else:
                log.info(f"  PASSED: Expected {fps} FPS, got {actual_fps:.1f} FPS "
                      f"(deviation: {stats.get('fps_deviation', 0)*100:.1f}%)")

        except Exception as e:
            log.error(f"  ERROR testing {config_name}: {e}")
            result = {
                "width": width,
                "height": height,
                "expected_fps": fps,
                "actual_fps": 0.0,
                "passed": False,
                "deviation": 1.0,
                "tolerance": fps_tolerance,
                "frame_count": 0,
                "config_name": config_name,
                "stats": {"error": str(e)}
            }
            all_results.append(result)
            all_passed = False

    return all_passed, all_results


def print_configuration_test_summary(stream_type_name, all_results, all_passed, product_line):
    """Print a comprehensive summary of configuration test results with detailed statistics"""
    if not all_results:
        log.warning(f"No {stream_type_name} configuration test results to summarize")
        return

    log.info(f"\n{'='*85}")
    log.info(f"{stream_type_name.upper()} CONFIGURATION TEST SUMMARY - {product_line}")
    log.info(f"{'='*85}")

    # Group results by resolution for better organization
    resolution_groups = {}
    for result in all_results:
        res_key = f"{result['width']}x{result['height']}"
        if res_key not in resolution_groups:
            resolution_groups[res_key] = []
        resolution_groups[res_key].append(result)

    # Print detailed results organized by resolution
    log.info(f"{'Resolution':<12} {'FPS':<5} {'Result':<8} {'Actual FPS':<11} {'Deviation':<10} {'Tolerance':<9} {'Frames':<7}")
    log.info(f"{'-'*85}")

    for resolution in sorted(resolution_groups.keys(), key=lambda x: (int(x.split('x')[0]), int(x.split('x')[1]))):
        fps_results = resolution_groups[resolution]
        fps_results.sort(key=lambda x: x['expected_fps'])

        for i, result in enumerate(fps_results):
            res_display = resolution if i == 0 else ""  # Only show resolution for first FPS in group
            status = "PASS" if result['passed'] else "FAIL"
            actual_fps_str = f"{result['actual_fps']:.1f}" if result['actual_fps'] > 0 else "ERROR"

            log.info(f"{res_display:<12} {result['expected_fps']:<5} {status:<8} {actual_fps_str:<11} "
                  f"{result['deviation']*100:<9.1f}% {result['tolerance']*100:<8.0f}% {result['frame_count']:<7}")

    # Calculate comprehensive statistics
    passed_tests = sum(1 for r in all_results if r['passed'])
    failed_tests = len(all_results) - passed_tests

    # Configuration statistics
    total_configs = len(all_results)
    unique_resolutions = len(resolution_groups)
    unique_fps_rates = len(set(r['expected_fps'] for r in all_results))

    # Resolution analysis
    resolution_sizes = [(r['width'], r['height']) for r in all_results]
    unique_resolution_sizes = set(resolution_sizes)
    resolution_areas = [w * h for w, h in unique_resolution_sizes]
    min_resolution = min(unique_resolution_sizes, key=lambda x: x[0] * x[1]) if unique_resolution_sizes else (0, 0)
    max_resolution = max(unique_resolution_sizes, key=lambda x: x[0] * x[1]) if unique_resolution_sizes else (0, 0)

    # FPS performance statistics
    successful_results = [r for r in all_results if r['passed'] and r['actual_fps'] > 0]
    if successful_results:
        actual_fps_values = [r['actual_fps'] for r in successful_results]
        expected_fps_values = [r['expected_fps'] for r in successful_results]
        deviation_values = [r['deviation'] for r in successful_results]
        frame_counts = [r['frame_count'] for r in successful_results]

        # FPS statistics
        avg_actual_fps = sum(actual_fps_values) / len(actual_fps_values)
        min_actual_fps = min(actual_fps_values)
        max_actual_fps = max(actual_fps_values)
        fps_std_dev = np.std(actual_fps_values) if len(actual_fps_values) > 1 else 0.0

        # Expected FPS range
        min_expected_fps = min(expected_fps_values)
        max_expected_fps = max(expected_fps_values)

        # Deviation statistics
        avg_deviation = sum(deviation_values) / len(deviation_values)
        min_deviation = min(deviation_values)
        max_deviation = max(deviation_values)
        deviation_std = np.std(deviation_values) if len(deviation_values) > 1 else 0.0

        # Frame statistics
        avg_frames = sum(frame_counts) / len(frame_counts)
        min_frames = min(frame_counts)
        max_frames = max(frame_counts)
        total_frames = sum(frame_counts)

        log.info(f"\n--- {stream_type_name.upper()} CONFIGURATION STATISTICS ---")

        log.info(f"\nTest Results Summary:")
        log.info(f"  Configurations Passed: {passed_tests}/{total_configs} ({passed_tests/total_configs*100:.1f}%)")
        log.info(f"  Configurations Failed: {failed_tests}/{total_configs} ({failed_tests/total_configs*100:.1f}%)")
        log.info(f"  Success Rate: {passed_tests/total_configs*100:.1f}%")

        log.info(f"\nConfiguration Coverage:")
        log.info(f"  Total Configurations: {total_configs}")
        log.info(f"  Unique Resolutions: {unique_resolutions}")
        log.info(f"  Unique FPS Rates: {unique_fps_rates}")
        log.info(f"  Resolution Range: {min_resolution[0]}x{min_resolution[1]} to {max_resolution[0]}x{max_resolution[1]}")
        log.info(f"  FPS Rate Range: {min_expected_fps} - {max_expected_fps} FPS")

        # Resolution breakdown
        log.info(f"\nResolution Analysis:")
        log.info(f"  Smallest Resolution: {min_resolution[0]}x{min_resolution[1]} ({min_resolution[0]*min_resolution[1]:,} pixels)")
        log.info(f"  Largest Resolution: {max_resolution[0]}x{max_resolution[1]} ({max_resolution[0]*max_resolution[1]:,} pixels)")
        if resolution_areas:
            avg_resolution_area = sum(resolution_areas) / len(resolution_areas)
            log.info(f"  Average Resolution: {avg_resolution_area:,.0f} pixels")

        log.info(f"\nFPS Performance Statistics:")
        log.info(f"  Average Actual FPS: {avg_actual_fps:.2f}")
        log.info(f"  Actual FPS Range: {min_actual_fps:.2f} - {max_actual_fps:.2f}")
        log.info(f"  Actual FPS Std Dev: {fps_std_dev:.2f}")

        log.info(f"\nAccuracy Statistics:")
        log.info(f"  Average Deviation: {avg_deviation*100:.2f}%")
        log.info(f"  Deviation Range: {min_deviation*100:.2f}% - {max_deviation*100:.2f}%")
        log.info(f"  Deviation Std Dev: {deviation_std*100:.2f}%")

        log.info(f"\nFrame Capture Statistics:")
        log.info(f"  Total Frames Captured: {total_frames:,}")
        log.info(f"  Average Frames/Config: {avg_frames:.1f}")
        log.info(f"  Frame Count Range: {min_frames} - {max_frames}")

        # Performance by resolution category
        resolution_performance = {}
        for resolution, fps_results in resolution_groups.items():
            passed_for_res = sum(1 for r in fps_results if r['passed'])
            total_for_res = len(fps_results)
            success_rate = passed_for_res / total_for_res * 100 if total_for_res > 0 else 0
            resolution_performance[resolution] = {
                'passed': passed_for_res,
                'total': total_for_res,
                'success_rate': success_rate
            }

        log.info(f"\nPerformance by Resolution:")
        log.info(f"{'Resolution':<12} {'Passed':<8} {'Total':<7} {'Success Rate':<12}")
        log.info(f"{'-'*42}")
        for resolution in sorted(resolution_performance.keys(), key=lambda x: (int(x.split('x')[0]), int(x.split('x')[1]))):
            perf = resolution_performance[resolution]
            log.info(f"{resolution:<12} {perf['passed']:<8} {perf['total']:<7} {perf['success_rate']:<11.1f}%")

        # Performance categories
        excellent_tests = sum(1 for r in successful_results if r['deviation'] <= 0.05)
        good_tests = sum(1 for r in successful_results if 0.05 < r['deviation'] <= 0.10)
        acceptable_tests = sum(1 for r in successful_results if r['deviation'] > 0.10)

        log.info(f"\nPerformance Categories:")
        log.info(f"  Excellent (<=5% deviation): {excellent_tests} configurations")
        log.info(f"  Good (5-10% deviation): {good_tests} configurations")
        log.info(f"  Acceptable (>10% deviation): {acceptable_tests} configurations")
        log.info(f"  Failed: {failed_tests} configurations")

    else:
        log.info(f"\nConfiguration Statistics:")
        log.info(f"  Total Configurations Tested: {total_configs}")
        log.info(f"  Unique Resolutions: {unique_resolutions}")
        log.info(f"  Unique FPS Rates: {unique_fps_rates}")
        log.info(f"  Configurations Passed: {passed_tests}/{total_configs}")

    log.info(f"\nOverall {stream_type_name.upper()} Configuration Result: {'PASS' if all_passed else 'FAIL'}")


def check_multistream_fps_accuracy(device, depth_config, color_config, test_duration: float = 5.0, fps_tolerance: float = 0.20):
    """
    Test depth + color multi-stream FPS accuracy

    Args:
        device: RealSense device
        depth_config: Tuple of (width, height, fps) for depth stream
        color_config: Tuple of (width, height, fps) for color stream
        test_duration: How long to test in seconds
        fps_tolerance: Allowed FPS deviation

    Returns:
        Tuple[bool, Dict]: (passed, stats)
    """
    depth_width, depth_height, depth_fps = depth_config
    color_width, color_height, color_fps = color_config

    # Frame counters and timing
    depth_frame_count = 0
    color_frame_count = 0
    depth_fps_measurements = []
    color_fps_measurements = []

    depth_monitor = FPSMonitor(window_size=60)
    color_monitor = FPSMonitor(window_size=60)

    # Adaptive parameters based on the slower FPS (reduced for faster multistream testing)
    min_fps = min(depth_fps, color_fps)
    if min_fps <= 6:
        warmup_frames = 2  # Reduced from 3
        measurement_interval = 2
    elif min_fps <= 15:
        warmup_frames = 8  # Reduced from 10
        measurement_interval = 10
    elif min_fps <= 30:
        warmup_frames = 15  # Reduced from 20
        measurement_interval = 15
    else:
        warmup_frames = 20  # Reduced from 25
        measurement_interval = 20

    # Frame callbacks
    def depth_callback(frame):
        nonlocal depth_frame_count
        current_time = time.time()
        depth_monitor.update(current_time)
        depth_frame_count += 1

        if depth_frame_count > warmup_frames and depth_frame_count % measurement_interval == 0:
            current_fps = depth_monitor.get_current_fps()
            if current_fps > 0:
                depth_fps_measurements.append(current_fps)

    def color_callback(frame):
        nonlocal color_frame_count
        current_time = time.time()
        color_monitor.update(current_time)
        color_frame_count += 1

        if color_frame_count > warmup_frames and color_frame_count % measurement_interval == 0:
            current_fps = color_monitor.get_current_fps()
            if current_fps > 0:
                color_fps_measurements.append(current_fps)

    depth_sensor = None
    color_sensor = None
    try:
        # Get sensors
        depth_sensor = device.first_depth_sensor()
        color_sensor = device.first_color_sensor()

        # Find profiles
        depth_profile = None
        for p in depth_sensor.profiles:
            if (p.fps() == depth_fps and
                p.stream_type() == rs.stream.depth and
                p.format() == rs.format.z16):
                vp = p.as_video_stream_profile()
                if vp.width() == depth_width and vp.height() == depth_height:
                    depth_profile = p
                    break

        color_profile = None
        for p in color_sensor.profiles:
            if (p.fps() == color_fps and
                p.stream_type() == rs.stream.color):
                vp = p.as_video_stream_profile()
                if vp.width() == color_width and vp.height() == color_height:
                    color_profile = p
                    break

        if not depth_profile:
            return False, {"error": f"No depth profile found for {depth_width}x{depth_height}@{depth_fps}fps"}

        if not color_profile:
            return False, {"error": f"No color profile found for {color_width}x{color_height}@{color_fps}fps"}

        # Start streaming both sensors
        depth_sensor.open(depth_profile)
        color_sensor.open(color_profile)

        depth_sensor.start(depth_callback)
        color_sensor.start(color_callback)

        # Wait for test duration
        test_stopwatch = Stopwatch()
        while test_stopwatch.get_elapsed() < test_duration:
            time.sleep(0.1)

    except Exception as e:
        return False, {"error": f"Multi-stream test failed: {str(e)}"}

    finally:
        # Stop and close sensors. Split stop/close so a failing stop() can't skip close() and
        # leak an open sensor. One settle after both are closed (both reopen together on the next
        # combo), so USB endpoints tear down before the next open().
        for name, s in (("depth", depth_sensor), ("color", color_sensor)):
            if s is None:
                continue
            try:
                s.stop()
            except Exception as e:
                log.debug(f"{name} sensor.stop() failed during teardown: {e}")
            try:
                s.close()
            except Exception as e:
                log.debug(f"{name} sensor.close() failed during teardown: {e}")
        _settle()

    # Calculate statistics
    elapsed_time = test_stopwatch.get_elapsed()

    # no_frames marks a stream that never started, as opposed to one merely too slow to measure.
    # Both counts are reported either way: the stream that died is often not the one short of
    # measurements, and naming only that one sends triage after the wrong stream.
    no_frames = depth_frame_count == 0 or color_frame_count == 0
    frame_counts = f"got depth {depth_frame_count}, color {color_frame_count} frames"

    if len(depth_fps_measurements) < 2:
        return False, {"error": f"Insufficient depth measurements: {len(depth_fps_measurements)} ({frame_counts})",
                       "no_frames": no_frames}

    if len(color_fps_measurements) < 2:
        return False, {"error": f"Insufficient color measurements: {len(color_fps_measurements)} ({frame_counts})",
                       "no_frames": no_frames}

    # Calculate FPS statistics
    depth_avg_fps = sum(depth_fps_measurements) / len(depth_fps_measurements)
    color_avg_fps = sum(color_fps_measurements) / len(color_fps_measurements)

    depth_deviation = abs(depth_avg_fps - depth_fps) / depth_fps
    color_deviation = abs(color_avg_fps - color_fps) / color_fps

    depth_passed = depth_deviation <= fps_tolerance
    color_passed = color_deviation <= fps_tolerance
    overall_passed = depth_passed and color_passed

    stats = {
        "test_duration": elapsed_time,
        "depth": {
            "expected_fps": depth_fps,
            "actual_fps": depth_avg_fps,
            "frame_count": depth_frame_count,
            "measurements": len(depth_fps_measurements),
            "deviation": depth_deviation,
            "passed": depth_passed
        },
        "color": {
            "expected_fps": color_fps,
            "actual_fps": color_avg_fps,
            "frame_count": color_frame_count,
            "measurements": len(color_fps_measurements),
            "deviation": color_deviation,
            "passed": color_passed
        },
        "overall_passed": overall_passed,
        "warmup_frames": warmup_frames,
        "measurement_interval": measurement_interval
    }

    return overall_passed, stats


def get_depth_color_combinations(device, max_combinations=None):
    """
    Get all combinations of depth and color configurations for multi-stream testing.
    Includes all possible combinations (any resolution and FPS pairing).

    Args:
        device: RealSense device
        max_combinations: Maximum number of combinations to test

    Returns:
        List of tuples: [(depth_config, color_config), ...] with all possible combinations
    """
    try:
        depth_configs = get_supported_depth_configurations(device)
        color_configs = get_supported_color_configurations(device)
    except Exception as e:
        log.error(f"Failed to get configurations: {e}")
        return []

    if not depth_configs or not color_configs:
        log.warning("No depth or color configurations available for multi-stream testing")
        return []

    # Generate all combinations (removed restrictions for comprehensive testing)
    combinations = []
    for depth_config in depth_configs:
        for color_config in color_configs:
            combinations.append((depth_config, color_config))

    log.info(f"Found {len(combinations)} depth+color combinations (all possible combinations)")

    # Coverage tier: limit to a representative subset of combinations (None = all)
    if max_combinations is not None and len(combinations) > max_combinations:
        log.info(f"Coverage: limiting combinations from {len(combinations)} to {max_combinations}")

        # Prioritize combinations with:
        # 1. Same FPS rates (most common use case)
        # 2. Same resolutions (better performance)
        # 3. Mixed configurations for comprehensive testing

        same_fps_same_res = []
        same_fps_diff_res = []
        diff_fps_same_res = []
        diff_fps_diff_res = []

        for depth_config, color_config in combinations:
            depth_w, depth_h, depth_fps = depth_config
            color_w, color_h, color_fps = color_config

            same_fps = (depth_fps == color_fps)
            same_res = (depth_w == color_w and depth_h == color_h)

            if same_fps and same_res:
                same_fps_same_res.append((depth_config, color_config))
            elif same_fps and not same_res:
                same_fps_diff_res.append((depth_config, color_config))
            elif not same_fps and same_res:
                diff_fps_same_res.append((depth_config, color_config))
            else:
                diff_fps_diff_res.append((depth_config, color_config))

        # Select representative combinations with priority ordering
        selected = []

        # Priority 1: Same FPS, Same Resolution (40% of slots)
        target_same_same = min(len(same_fps_same_res), max_combinations * 2 // 5)
        if same_fps_same_res and target_same_same > 0:
            step = max(1, len(same_fps_same_res) // target_same_same)
            for i in range(0, len(same_fps_same_res), step):
                if len(selected) < target_same_same:
                    selected.append(same_fps_same_res[i])

        # Priority 2: Same FPS, Different Resolution (30% of slots)
        target_same_diff = min(len(same_fps_diff_res), max_combinations * 3 // 10)
        if same_fps_diff_res and target_same_diff > 0:
            step = max(1, len(same_fps_diff_res) // target_same_diff)
            for i in range(0, len(same_fps_diff_res), step):
                if len(selected) < target_same_same + target_same_diff:
                    selected.append(same_fps_diff_res[i])

        # Priority 3: Different FPS, Same Resolution (20% of slots)
        target_diff_same = min(len(diff_fps_same_res), max_combinations // 5)
        if diff_fps_same_res and target_diff_same > 0:
            step = max(1, len(diff_fps_same_res) // target_diff_same)
            for i in range(0, len(diff_fps_same_res), step):
                if len(selected) < target_same_same + target_same_diff + target_diff_same:
                    selected.append(diff_fps_same_res[i])

        # Priority 4: Different FPS, Different Resolution (10% of remaining slots)
        remaining_slots = max_combinations - len(selected)
        if diff_fps_diff_res and remaining_slots > 0:
            step = max(1, len(diff_fps_diff_res) // remaining_slots)
            for i in range(0, len(diff_fps_diff_res), step):
                if len(selected) < max_combinations:
                    selected.append(diff_fps_diff_res[i])

        # Small caps can zero out every proportional bucket above; fall back to an evenly-spaced
        # pick (same strategy as _evenly_spaced_subset) so a low tier (e.g. gating=1) still
        # exercises a representative combo, consistent with the config/FPS tests.
        if not selected:
            selected = list(_evenly_spaced_subset(combinations, max_combinations))

        combinations = selected

        log.info(f"Coverage buckets (available): {len(same_fps_same_res)} same FPS+res, {len(same_fps_diff_res)} same FPS+diff res, "
              f"{len(diff_fps_same_res)} diff FPS+same res, {len(diff_fps_diff_res)} diff FPS+res")
        log.info(f"Final selection: {len(combinations)} combinations")

    return combinations


# Known open defect: on D455, depth throughput degrades while colour also runs at 90 fps.
DUAL_90FPS_DEPTH_DEGRADATION_MIN_RATIO = 0.65


def is_dual_90fps_on_d455(device_name, depth_fps, color_fps):
    """The configuration the known depth degradation is tied to."""
    return 'D455' in device_name and depth_fps == 90 and color_fps == 90


def is_depth_degraded_at_dual_90fps(device_name, depth_fps, color_fps, stats, tolerance):
    """Depth alone falls short of target while colour holds 90 fps, on D455."""
    if not is_dual_90fps_on_d455(device_name, depth_fps, color_fps) or 'error' in stats:
        return False
    try:
        ratio = stats['depth']['actual_fps'] / depth_fps
        color_deviation = stats['color']['deviation']
    except (KeyError, TypeError):
        return False
    return DUAL_90FPS_DEPTH_DEGRADATION_MIN_RATIO <= ratio < (1 - tolerance) and color_deviation <= tolerance


def is_stream_start_failure(stats):
    """One of the pair delivered no frames at all - the stream never started."""
    return stats.get('no_frames', False)


def check_multistream_configurations_comprehensive(device, max_combinations=None):
    """
    Test all depth + color multi-stream configurations.
    Tests all possible combinations of depth and color configurations.

    Args:
        device: RealSense device
        max_combinations: Coverage cap on the number of combinations (None = all)

    Returns:
        Tuple[bool, List[Dict]]: (all_passed, results_list)
    """
    log.info("\nTesting all depth + color multi-stream configurations...")

    device_name = device.get_info(rs.camera_info.name) if device.supports(rs.camera_info.name) else ""

    # Get combinations
    combinations = get_depth_color_combinations(device, max_combinations)

    if not combinations:
        log.warning("No depth + color combinations available for testing")
        return False, []

    log.info(f"Testing {len(combinations)} depth + color combinations:")

    all_results = []
    all_passed = True

    for i, (depth_config, color_config) in enumerate(combinations):
        depth_w, depth_h, depth_fps = depth_config
        color_w, color_h, color_fps = color_config

        config_name = f"D:{depth_w}x{depth_h}@{depth_fps}fps + C:{color_w}x{color_h}@{color_fps}fps"
        log.info(f"\nTesting multi-stream {i+1}/{len(combinations)}: {config_name}")

        try:
            # Adjust test duration and tolerance based on FPS rates using shared logic
            min_fps = min(depth_fps, color_fps)
            test_duration, tolerance = get_fps_test_parameters(min_fps)

            passed, stats = check_multistream_fps_accuracy(
                device, depth_config, color_config, test_duration, tolerance
            )

            known_issue = (not passed) and is_depth_degraded_at_dual_90fps(
                device_name, depth_fps, color_fps, stats, tolerance)
            never_started = (not passed) and is_stream_start_failure(stats)

            result = {
                "depth_config": depth_config,
                "color_config": color_config,
                "config_name": config_name,
                "passed": passed,
                "known_issue": known_issue,
                "never_started": never_started,
                "stats": stats
            }

            all_results.append(result)

            if not passed:
                excused = known_issue or never_started
                if not excused:
                    all_passed = False
                log_fn = log.warning if excused else log.error
                tag = (" (known issue RSDSO-21810, not counted as a CI failure)" if known_issue
                       else " (stream never started, RSDSO-21811 - test will be skipped)" if never_started
                       else "")
                if 'error' in stats:
                    log_fn(f"  ERROR{tag}: {stats['error']}")
                else:
                    depth_stats = stats['depth']
                    color_stats = stats['color']
                    log_fn(f"  FAILED{tag}:")
                    log_fn(f"    Depth: Expected {depth_stats['expected_fps']} FPS, got {depth_stats['actual_fps']:.1f} FPS "
                          f"(deviation: {depth_stats['deviation']*100:.1f}%)")
                    log_fn(f"    Color: Expected {color_stats['expected_fps']} FPS, got {color_stats['actual_fps']:.1f} FPS "
                          f"(deviation: {color_stats['deviation']*100:.1f}%)")
            else:
                depth_stats = stats['depth']
                color_stats = stats['color']
                log.info(f"  PASSED:")
                log.info(f"    Depth: Expected {depth_stats['expected_fps']} FPS, got {depth_stats['actual_fps']:.1f} FPS "
                      f"(deviation: {depth_stats['deviation']*100:.1f}%)")
                log.info(f"    Color: Expected {color_stats['expected_fps']} FPS, got {color_stats['actual_fps']:.1f} FPS "
                      f"(deviation: {color_stats['deviation']*100:.1f}%)")
                if is_dual_90fps_on_d455(device_name, depth_fps, color_fps):
                    log.warning(f"  NOTE: {config_name} PASSED - the known RSDSO-21810 degradation may be fixed; "
                                f"re-check before keeping the allowance")

        except Exception as e:
            log.error(f"  ERROR testing {config_name}: {e}")
            result = {
                "depth_config": depth_config,
                "color_config": color_config,
                "config_name": config_name,
                "passed": False,
                "known_issue": False,
                "never_started": False,
                "stats": {"error": str(e)}
            }
            all_results.append(result)
            all_passed = False

    return all_passed, all_results


def print_multistream_test_summary(all_results, all_passed, product_line):
    """Print comprehensive summary of multi-stream test results"""
    if not all_results:
        log.warning("No multi-stream test results to summarize")
        return

    log.info(f"\n{'='*90}")
    log.info(f"DEPTH + COLOR MULTI-STREAM TEST SUMMARY - {product_line}")
    log.info(f"{'='*90}")

    # Group results by FPS relationship
    same_fps_results = []
    different_fps_results = []

    for result in all_results:
        if 'error' not in result['stats']:
            depth_fps = result['stats']['depth']['expected_fps']
            color_fps = result['stats']['color']['expected_fps']
            if depth_fps == color_fps:
                same_fps_results.append(result)
            else:
                different_fps_results.append(result)

    log.info(f"Total Combinations Tested: {len(all_results)}")
    log.info(f"Same FPS Combinations: {len(same_fps_results)}")
    log.info(f"Different FPS Combinations: {len(different_fps_results)}")

    # Additional categorization by resolution
    same_res_results = []
    different_res_results = []

    for result in all_results:
        if 'error' not in result['stats']:
            depth_config = result['depth_config']
            color_config = result['color_config']
            depth_w, depth_h = depth_config[0], depth_config[1]
            color_w, color_h = color_config[0], color_config[1]

            if depth_w == color_w and depth_h == color_h:
                same_res_results.append(result)
            else:
                different_res_results.append(result)

    log.info(f"Same Resolution Combinations: {len(same_res_results)}")
    log.info(f"Different Resolution Combinations: {len(different_res_results)}")

    # Print detailed results
    log.info(f"\n{'Depth Config':<20} {'Color Config':<20} {'Result':<8} {'Depth FPS':<10} {'Color FPS':<10} {'Status'}")
    log.info(f"{'-'*90}")

    for result in all_results:
        depth_config = result['depth_config']
        color_config = result['color_config']

        depth_str = f"{depth_config[0]}x{depth_config[1]}@{depth_config[2]}"
        color_str = f"{color_config[0]}x{color_config[1]}@{color_config[2]}"

        if result['passed']:
            status = "PASS"
        else:
            status = "KNOWN" if result.get('known_issue') else ("NOSTART" if result.get('never_started') else "FAIL")

        if 'error' in result['stats']:
            log.info(f"{depth_str:<20} {color_str:<20} {status:<8} {'ERROR':<10} {'ERROR':<10} {result['stats']['error']}")
        else:
            depth_actual = result['stats']['depth']['actual_fps']
            color_actual = result['stats']['color']['actual_fps']
            log.info(f"{depth_str:<20} {color_str:<20} {status:<8} {depth_actual:<10.1f} {color_actual:<10.1f}")

    # Calculate statistics
    passed_tests = sum(1 for r in all_results if r['passed'])
    failed_tests = len(all_results) - passed_tests

    successful_results = [r for r in all_results if r['passed'] and 'error' not in r['stats']]

    known_tests = sum(1 for r in all_results if r.get('known_issue'))
    nostart_tests = sum(1 for r in all_results if r.get('never_started'))
    if known_tests or nostart_tests:
        log.info(f"Excluded from pass/fail: {known_tests} known, {nostart_tests} never started")

    if successful_results:
        log.info(f"\n--- MULTI-STREAM STATISTICS ---")
        log.info(f"Success Rate: {passed_tests}/{len(all_results)} ({passed_tests/len(all_results)*100:.1f}%)")

        # FPS performance analysis
        depth_deviations = [r['stats']['depth']['deviation'] for r in successful_results]
        color_deviations = [r['stats']['color']['deviation'] for r in successful_results]

        avg_depth_deviation = sum(depth_deviations) / len(depth_deviations) * 100
        avg_color_deviation = sum(color_deviations) / len(color_deviations) * 100

        log.info(f"Average Depth FPS Deviation: {avg_depth_deviation:.2f}%")
        log.info(f"Average Color FPS Deviation: {avg_color_deviation:.2f}%")

        # Performance by FPS relationship
        if same_fps_results:
            same_fps_passed = sum(1 for r in same_fps_results if r['passed'])
            log.info(f"Same FPS Performance: {same_fps_passed}/{len(same_fps_results)} ({same_fps_passed/len(same_fps_results)*100:.1f}%)")

        if different_fps_results:
            diff_fps_passed = sum(1 for r in different_fps_results if r['passed'])
            log.info(f"Different FPS Performance: {diff_fps_passed}/{len(different_fps_results)} ({diff_fps_passed/len(different_fps_results)*100:.1f}%)")

    log.info(f"\nOverall Multi-Stream Result: {'PASS' if all_passed else 'FAIL'}")


# ============================================================================
# Fixtures
# ============================================================================

@pytest.fixture(scope="module")
def settled_device(test_device):
    """Return (dev, ctx), waiting once for device to reach idle state.

    Module-scoped so the settle-sleep fires once per module under normal runs
    AND re-fires after a --retries-triggered re-instantiation (pytest-retry
    tears down module fixtures between attempts, so the device may have been
    recycled and needs to settle again).
    """
    dev, ctx = test_device
    time.sleep(DEVICE_INIT_SLEEP_SEC)
    return dev, ctx


# Function-scoped (not module): the tier is device-independent (depends only on --context), so a
# module-scoped fixture would trip the per-camera fixture guard on benches with >1 D400 camera.
@pytest.fixture
def coverage_tier(request):
    """Resolve the coverage tier (gating/semi/full) from --context."""
    tier = resolve_coverage_tier(request.config)
    log.info(f"FPS-performance coverage tier: {tier}")
    return tier


# ============================================================================
# Test Functions
# ============================================================================

@pytest.mark.timeout(14400)
def test_depth_configurations(settled_device, coverage_tier):
    """Test depth FPS accuracy for all supported configurations"""
    dev, ctx = settled_device
    product_line = dev.get_info(rs.camera_info.product_line)

    depth_config_tests_passed, depth_config_results = check_stream_configurations_comprehensive(
        dev, "depth", check_depth_fps_accuracy, get_supported_depth_configurations,
        max_configs=TIER_CONFIG_CAP[coverage_tier]
    )

    if depth_config_results:
        print_configuration_test_summary("depth", depth_config_results, depth_config_tests_passed, product_line)

    assert depth_config_tests_passed, \
        f"All supported depth configurations accuracy test - {len(depth_config_results) if depth_config_results else 0} configurations tested"


@pytest.mark.timeout(14400)
def test_color_configurations(settled_device, coverage_tier):
    """Test color/RGB FPS accuracy for all supported configurations"""
    dev, ctx = settled_device
    product_line = dev.get_info(rs.camera_info.product_line)

    color_config_tests_passed, color_config_results = check_stream_configurations_comprehensive(
        dev, "color", check_color_fps_accuracy, get_supported_color_configurations,
        max_configs=TIER_CONFIG_CAP[coverage_tier]
    )

    if color_config_results:
        print_configuration_test_summary("color", color_config_results, color_config_tests_passed, product_line)
        assert color_config_tests_passed, \
            f"All supported color configurations accuracy test - {len(color_config_results)} configurations tested"
    elif not color_config_results:
        # Check if device has no color sensor (like D421, D405)
        product_name = dev.get_info(rs.camera_info.name)
        if 'D421' in product_name or 'D405' in product_name:
            log.info("Device has no color sensor - skipping color configuration tests")
            pytest.skip("Color configuration test skipped - no color sensor on device")
        else:
            log.warning("No color configurations found on device that should have color sensor")
            assert False, "Color sensor expected but no configurations found"


@pytest.mark.timeout(14400)
def test_ir_configurations(settled_device, coverage_tier):
    """Test IR FPS accuracy for all supported configurations"""
    dev, ctx = settled_device
    product_line = dev.get_info(rs.camera_info.product_line)

    ir_config_tests_passed, ir_config_results = check_stream_configurations_comprehensive(
        dev, "IR", check_ir_fps_accuracy, get_supported_ir_configurations,
        max_configs=TIER_CONFIG_CAP[coverage_tier]
    )

    if ir_config_results:
        print_configuration_test_summary("IR", ir_config_results, ir_config_tests_passed, product_line)

    assert ir_config_tests_passed, \
        f"All supported IR configurations accuracy test - {len(ir_config_results) if ir_config_results else 0} configurations tested"


def describe_multistream_failure(result):
    """One-line description of a failed combination, for the assertion message.

    This only ever runs while building a failure message, so it must not raise: an exception
    here would replace the real AssertionError with a confusing secondary one.
    """
    config_name = result.get('config_name', '?')
    stats = result.get('stats', {})
    if 'error' in stats:
        return f"{config_name} -> {stats['error']}"

    try:
        depth_stats = stats['depth']
        color_stats = stats['color']
        return (f"{config_name} -> "
                f"depth {depth_stats['actual_fps']:.1f}/{depth_stats['expected_fps']} fps "
                f"({depth_stats['deviation']*100:.1f}% dev), "
                f"color {color_stats['actual_fps']:.1f}/{color_stats['expected_fps']} fps "
                f"({color_stats['deviation']*100:.1f}% dev)")
    except (KeyError, TypeError, ValueError):
        return f"{config_name} -> unexpected stats shape: {stats}"


MAX_REPORTED_FAILURES = 10


def describe_multistream_failures(all_results):
    """Assertion message naming the failed combinations; known-issue rows are listed separately."""
    def summarize(results):
        shown = "; ".join(describe_multistream_failure(r) for r in results[:MAX_REPORTED_FAILURES])
        extra = len(results) - MAX_REPORTED_FAILURES
        return shown + (f"; ... and {extra} more (see log)" if extra > 0 else "")

    failed = [r for r in all_results if not r['passed'] and not r.get('known_issue') and not r.get('never_started')]
    known = [r for r in all_results if r.get('known_issue')]
    never_started = [r for r in all_results if r.get('never_started')]
    msg = (f"Depth + color multi-stream configurations (all combinations) accuracy test - "
           f"{len(all_results)} combinations tested, {len(failed)} failed: {summarize(failed)}")
    if known:
        msg += f" [{len(known)} known-issue failure(s) excluded: {summarize(known)}]"
    if never_started:
        # Surfaced here too: when a real failure asserts, the skip below it never runs and these
        # would otherwise be missing from the JUnit report entirely.
        msg += f" [{len(never_started)} stream-start failure(s) also detected: {summarize(never_started)}]"
    return msg


@pytest.mark.timeout(14400)
def test_multistream_configurations(settled_device, coverage_tier):
    """Test depth + color multi-stream FPS accuracy for all supported configurations"""
    dev, ctx = settled_device
    product_line = dev.get_info(rs.camera_info.product_line)

    multistream_tests_passed, multistream_results = check_multistream_configurations_comprehensive(
        dev, max_combinations=TIER_MULTISTREAM_CAP[coverage_tier]
    )

    if multistream_results:
        print_multistream_test_summary(multistream_results, multistream_tests_passed, product_line)
        # The assertion message is all that reaches the JUnit report; without the offending
        # combinations, triage means downloading the per-device artifact log.
        assert multistream_tests_passed, describe_multistream_failures(multistream_results)

        # Only skip when nothing streamed at all - that is a rig/link problem and there is no
        # result to report. A partial no-start would otherwise discard the pass signal for every
        # other combination in the run; those stay visible as NOSTART rows and in the log.
        never_started = [r for r in multistream_results if r.get('never_started')]
        if never_started and len(never_started) == len(multistream_results):
            pytest.skip(f"no stream started on any of {len(multistream_results)} combination(s) "
                        f"(RSDSO-21811): " + "; ".join(r['config_name'] for r in never_started))
        if never_started:
            log.warning(f"{len(never_started)}/{len(multistream_results)} combination(s) never started "
                        f"(RSDSO-21811), not counted as failures: "
                        + "; ".join(r['config_name'] for r in never_started))
    else:
        # Check if device has no color sensor (like D421, D405)
        product_name = dev.get_info(rs.camera_info.name)
        if 'D421' in product_name or 'D405' in product_name:
            log.info("Device has no color sensor - skipping multi-stream tests")
            pytest.skip("Multi-stream test skipped - no color sensor on device")
        else:
            log.warning("No multi-stream configurations found on device that should have color sensor")
            assert False, "Multi-stream test failed - no configurations found"


@pytest.mark.timeout(14400)
def test_depth_fps_rates(settled_device, coverage_tier):
    """Test depth FPS accuracy for all supported frame rates"""
    dev, ctx = settled_device
    product_line = dev.get_info(rs.camera_info.product_line)

    depth_tests_passed, depth_results = check_stream_fps_accuracy_comprehensive(
        dev, "depth", check_depth_fps_accuracy, get_supported_depth_fps_rates,
        max_fps_rates=TIER_FPS_CAP[coverage_tier]
    )

    if depth_results:
        print_fps_test_summary("depth", [r['fps_rate'] for r in depth_results], depth_results, depth_tests_passed, product_line)


@pytest.mark.timeout(14400)
def test_color_fps_rates(settled_device, coverage_tier):
    """Test color/RGB FPS accuracy for all supported frame rates"""
    dev, ctx = settled_device
    product_line = dev.get_info(rs.camera_info.product_line)

    color_tests_passed, color_results = check_stream_fps_accuracy_comprehensive(
        dev, "color", check_color_fps_accuracy, get_supported_color_fps_rates,
        max_fps_rates=TIER_FPS_CAP[coverage_tier]
    )

    if color_results:
        print_fps_test_summary("color", [r['fps_rate'] for r in color_results], color_results, color_tests_passed, product_line)
    elif not color_results:
        # Check if device has no color sensor (like D421, D405)
        product_name = dev.get_info(rs.camera_info.name)
        if 'D421' in product_name or 'D405' in product_name:
            log.info("Device has no color sensor - skipping color FPS tests")
            pytest.skip("Color FPS test skipped - no color sensor on device")
        else:
            log.warning("No color FPS rates found on device that should have color sensor")
            assert False, "Color sensor expected but no FPS rates found"


@pytest.mark.timeout(14400)
def test_ir_fps_rates(settled_device, coverage_tier):
    """Test IR FPS accuracy for all supported frame rates"""
    dev, ctx = settled_device
    product_line = dev.get_info(rs.camera_info.product_line)

    ir_tests_passed, ir_results = check_stream_fps_accuracy_comprehensive(
        dev, "IR", check_ir_fps_accuracy, get_supported_ir_fps_rates,
        max_fps_rates=TIER_FPS_CAP[coverage_tier]
    )

    if ir_results:
        print_fps_test_summary("IR", [r['fps_rate'] for r in ir_results], ir_results, ir_tests_passed, product_line)
