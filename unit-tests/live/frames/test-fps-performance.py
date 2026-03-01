# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# test:donotrun:!weekly
# test:device D400*
# test:timeout 14400
# CI timeout set to 4 hours to accommodate comprehensive testing of all
# supported resolution and FPS combinations across depth, color, and IR streams

"""
RealSense FPS Performance Test
Comprehensive testing of all supported resolutions and frame rates for Depth, Color, and IR streams
Includes multi-stream combination testing for Depth + Color combinations

Code Organization & Redundancy Reduction:
- test_stream_fps_accuracy_generic(): Consolidated function that eliminates ~200 lines of 
  redundant code from the original three separate FPS test functions
- get_supported_stream_configurations(): Generic function that consolidates six "get_supported" 
  functions, reducing ~100 lines of redundant code
- Multi-stream combination testing: Tests all combinations of depth and color configurations
- Overall code reduction: ~300 lines eliminated while adding comprehensive multi-stream testing
"""

from rspy import test, log
from rspy.stopwatch import Stopwatch
import pyrealsense2 as rs
import numpy as np
import platform
import time
import sys
import os
from collections import deque
from typing import List, Tuple, Dict

# Constants for device initialization and testing
DEVICE_INIT_SLEEP_SEC = 3  # Sleep time to allow device to get into idle state
DEFAULT_DEVICE_CREATION_TIMEOUT = 10  # Default timeout for device creation (seconds)
DDS_DEVICE_CREATION_TIMEOUT = 30  # Extended timeout for DDS devices (seconds)
MIN_FRAME_COUNT_LOW_FPS = 5  # Minimum frame count for low FPS tests
MIN_TEST_DURATION_PERCENT = 0.6  # Minimum test duration percentage (60%)

# CI optimization: Detect if running in CI environment
CI_MODE = bool(os.getenv('CI') or os.getenv('CONTINUOUS_INTEGRATION') or os.getenv('GITHUB_ACTIONS'))

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

def optimize_for_ci(configurations, max_configs=8):
    """
    Optimize configuration list for CI by limiting the number of configurations tested
    
    Args:
        configurations: List of (width, height, fps) configurations
        max_configs: Maximum number of configurations to test in CI mode
        
    Returns:
        List of configurations (limited if in CI mode)
    """
    if not CI_MODE or len(configurations) <= max_configs:
        return configurations
    
    # In CI mode, select a representative subset:
    # - Lowest resolution configuration
    # - Highest resolution configuration  
    # - Medium resolution configurations
    # - Various FPS rates
    
    log.i(f"CI mode detected: limiting configurations from {len(configurations)} to {max_configs}")
    
    # Sort by resolution area, then by FPS
    sorted_configs = sorted(configurations, key=lambda x: (x[0] * x[1], x[2]))
    
    # Select representative configurations
    selected = []
    
    # Always include lowest and highest resolution
    if sorted_configs:
        selected.append(sorted_configs[0])  # Lowest resolution
        if len(sorted_configs) > 1:
            selected.append(sorted_configs[-1])  # Highest resolution
    
    # Select middle configurations to reach max_configs
    remaining_slots = max_configs - len(selected)
    if remaining_slots > 0 and len(sorted_configs) > 2:
        # Select evenly spaced configurations from the middle
        middle_configs = sorted_configs[1:-1]
        if middle_configs:
            step = max(1, len(middle_configs) // remaining_slots)
            for i in range(0, len(middle_configs), step):
                if len(selected) < max_configs:
                    selected.append(middle_configs[i])
    
    return selected


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

    return first_frame_time


# The device starts at D0 (Operational) state, allow time for it to get into idle state
time.sleep(DEVICE_INIT_SLEEP_SEC)


#####################################################################################################
test.start("Testing device creation time on " + platform.system() + " OS")
device_creation_stopwatch = Stopwatch()
ctx = rs.context( { "dds" : { "enabled" : False } } )
devs = ctx.devices
if len(devs) == 0:
    # No devices found, try to find a device with DDS enabled
    device_creation_stopwatch.reset()
    ctx = rs.context( { "dds" : { "enabled" : True } } )
    devs = ctx.devices
dev = devs[0]
device_creation_time = device_creation_stopwatch.get_elapsed()
is_dds = dev.supports(rs.camera_info.connection_type) and dev.get_info(rs.camera_info.connection_type) == "DDS"
max_time_for_device_creation = DEFAULT_DEVICE_CREATION_TIMEOUT if not is_dds else DDS_DEVICE_CREATION_TIMEOUT  # currently, DDS devices take longer time to complete
print("Device creation time is: {:.3f} [sec] max allowed is: {:.1f} [sec] ".format(device_creation_time, max_time_for_device_creation))
test.check(device_creation_time < max_time_for_device_creation)
test.finish()

product_line = dev.get_info(rs.camera_info.product_line)
max_delay_for_depth_frame = 1
max_delay_for_color_frame = 1

# Log CI optimization status
if CI_MODE:
    log.i("CI mode detected - using optimized test parameters:")
    log.i("  - Reduced test durations (3-8 seconds per test)")
    log.i("  - Limited configuration count (max 8 per stream)")
    log.i("  - Limited FPS rates (max 6 per stream)")
    log.i("  - Test timeout: 10 minutes")
else:
    log.i("Full test mode - comprehensive testing of all configurations")
    log.i("  - Standard test durations (3-8 seconds per test)")
    log.i("  - All supported configurations tested")
    log.i("  - Test timeout: 10 minutes")

# Start overall test timing
overall_test_start_time = time.time()


#####################################################################################################
test.start("Testing first depth frame delay on " + product_line + " device - "+ platform.system() + " OS")
ds = dev.first_depth_sensor()
dp = next(p for p in
          ds.profiles if p.fps() == 30
          and p.stream_type() == rs.stream.depth
          and p.format() == rs.format.z16
          and p.is_default())
first_depth_frame_delay = time_to_first_frame(ds, dp, max_delay_for_depth_frame)
print("Time until first depth frame is: {:.3f} [sec] max allowed is: {:.1f} [sec] ".format(first_depth_frame_delay, max_delay_for_depth_frame))
test.check(first_depth_frame_delay < max_delay_for_depth_frame)
test.finish()


#####################################################################################################
test.start("Testing first color frame delay on " + product_line + " device - "+ platform.system() + " OS")
product_name = dev.get_info(rs.camera_info.name)
cs = None
try:
    cs = dev.first_color_sensor()
except RuntimeError as rte:
    if 'D421' not in product_name and 'D405' not in product_name: # Cameras with no color sensor may fail.
        test.unexpected_exception()

if cs:
    cp = next(p for p in
              cs.profiles if p.fps() == 30
              and p.stream_type() == rs.stream.color
              and p.format() == rs.format.rgb8
              and p.is_default())
    first_color_frame_delay = time_to_first_frame(cs, cp, max_delay_for_color_frame)
    print("Time until first color frame is: {:.3f} [sec] max allowed is: {:.1f} [sec] ".format(first_color_frame_delay, max_delay_for_color_frame))
    test.check(first_color_frame_delay < max_delay_for_color_frame)
test.finish()


def test_depth_fps_accuracy(device, expected_fps: int, width: int = None, height: int = None, test_duration: float = 10.0, fps_tolerance: float = 0.1):
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
    return test_stream_fps_accuracy_generic(
        device, "depth", rs.stream.depth, rs.format.z16, 
        lambda d: d.first_depth_sensor(),
        expected_fps, width, height, test_duration, fps_tolerance
    )


def test_color_fps_accuracy(device, expected_fps: int, width: int = None, height: int = None, test_duration: float = 10.0, fps_tolerance: float = 0.1):
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
    
    return test_stream_fps_accuracy_generic(
        device, "color", rs.stream.color, [rs.format.rgb8, rs.format.bgr8, rs.format.yuyv],
        get_color_sensor,
        expected_fps, width, height, test_duration, fps_tolerance
    )


def test_ir_fps_accuracy(device, expected_fps: int, width: int = None, height: int = None, test_duration: float = 10.0, fps_tolerance: float = 0.1):
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
    
    return test_stream_fps_accuracy_generic(
        device, "IR", rs.stream.infrared, rs.format.y8,
        get_ir_sensor,
        expected_fps, width, height, test_duration, fps_tolerance
    )


def test_stream_fps_accuracy_generic(device, stream_name, stream_type, formats, get_sensor_func, expected_fps: int, width: int = None, height: int = None, test_duration: float = 10.0, fps_tolerance: float = 0.1):
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
        log.e(error_msg)
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
        log.i(f"Very low FPS mode for {expected_fps} FPS: warmup={warmup_frames}, interval={measurement_interval} (per-frame measurement)")
    elif expected_fps <= 15:
        warmup_frames = 5      # Reduced warmup for slow FPS
        measurement_interval = 10
        log.i(f"Medium-low FPS mode for {expected_fps} FPS: warmup={warmup_frames}, interval={measurement_interval}")
    elif expected_fps <= 30:
        warmup_frames = 15     # Reduced warmup for 30 FPS
        measurement_interval = 15  # More frequent measurements for better statistics
        log.i(f"Standard FPS mode for {expected_fps} FPS: warmup={warmup_frames}, interval={measurement_interval}")
    elif expected_fps <= 60:
        warmup_frames = 20     # Moderate warmup for 60 FPS
        measurement_interval = 20  # Optimized for sufficient measurements
        log.i(f"High FPS mode for {expected_fps} FPS: warmup={warmup_frames}, interval={measurement_interval}")
    else:
        warmup_frames = 25     # Higher warmup for very high FPS (90+)
        measurement_interval = 25  # Balance between frequency and performance
        log.i(f"Very high FPS mode for {expected_fps} FPS: warmup={warmup_frames}, interval={measurement_interval}")
    
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
                        log.i(f"Very low FPS test ({expected_fps} FPS) collected {len(fps_measurements)} measurements in {elapsed:.1f}s - sufficient for analysis")
                        break
                elif expected_fps <= 15:
                    # Low FPS: exit early after 65% of test duration
                    if elapsed >= (test_duration * 0.65):
                        log.i(f"Medium-low FPS test ({expected_fps} FPS) collected {len(fps_measurements)} measurements in {elapsed:.1f}s - sufficient for analysis")
                        break
                elif expected_fps <= 30:
                    # Standard FPS: exit early after 70% of test duration
                    if elapsed >= (test_duration * 0.70):
                        log.i(f"Standard FPS test ({expected_fps} FPS) collected {len(fps_measurements)} measurements in {elapsed:.1f}s - sufficient for analysis")
                        break
                elif expected_fps <= 60:
                    # High FPS: exit early after 75% of test duration
                    if elapsed >= (test_duration * 0.75):
                        log.i(f"High FPS test ({expected_fps} FPS) collected {len(fps_measurements)} measurements in {elapsed:.1f}s - sufficient for analysis")
                        break
                else:
                    # Very high FPS: exit early after 80% of test duration
                    if elapsed >= (test_duration * 0.80):
                        log.i(f"Very high FPS test ({expected_fps} FPS) collected {len(fps_measurements)} measurements in {elapsed:.1f}s - sufficient for analysis")
                        break
            
            # Special case for very low FPS: allow early exit with fewer measurements if we have reasonable frame count
            if expected_fps <= 6 and len(fps_measurements) >= 1 and frame_count >= MIN_FRAME_COUNT_LOW_FPS:
                elapsed = test_stopwatch.get_elapsed()
                if elapsed >= (test_duration * MIN_TEST_DURATION_PERCENT):  # At least 60% of test duration
                    log.i(f"Very low FPS test ({expected_fps} FPS) collected {len(fps_measurements)} measurements with {frame_count} frames in {elapsed:.1f}s - acceptable for low FPS analysis")
                    break
            
    finally:
        sensor.stop()
        sensor.close()
    
    # Calculate statistics
    if not fps_measurements:
        return False, 0.0, {"error": f"No FPS measurements collected after {test_stopwatch.get_elapsed():.1f}s (expected {expected_fps} FPS, warmup: {warmup_frames} frames, got {frame_count} total frames)"}
    
    # For very low FPS (<=6), allow single measurement if we have a minimal reasonable frame count
    # Relaxed from 10 to MIN_FRAME_COUNT_LOW_FPS (default 5) because per-frame measurement now provides limited data
    # in short-duration configuration tests (e.g., 3s) where <10 frames may be expected (~18 frames max at 6 FPS)
    if expected_fps <= 6 and len(fps_measurements) == 1 and frame_count >= MIN_FRAME_COUNT_LOW_FPS:
        log.i(f"Very low FPS ({expected_fps}): accepting single measurement with {frame_count} frames (threshold {MIN_FRAME_COUNT_LOW_FPS})")
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
        log.i(f"Optimized FPS test completed: {frame_count} frames in {test_stopwatch.get_elapsed():.1f}s, "
              f"{len(fps_measurements)} measurements, avg FPS: {actual_avg_fps:.2f}")
    elif expected_fps <= 60:
        log.i(f"High FPS test completed: {frame_count} frames in {test_stopwatch.get_elapsed():.1f}s, "
              f"{len(fps_measurements)} measurements, avg FPS: {actual_avg_fps:.2f}")
    
    return fps_passed, actual_avg_fps, stats


# The test_color_fps_accuracy and test_ir_fps_accuracy functions have been consolidated 
# into the generic test_stream_fps_accuracy_generic function above to eliminate redundancy


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
        (6,   (15.0, 0.35)),  # Very low FPS: extended test time and higher tolerance
        (15,  (10.0, 0.25)),  # Low FPS: increased test time and tolerance
        (30,  (8.0, 0.15)),   # Standard FPS: increased duration for better measurements
        (60,  (6.0, 0.18)),   # High FPS: optimized duration and tolerance
        (90,  (4.0, 0.20)),   # Very high FPS: shorter test with higher tolerance
    ]
    for threshold, params in fps_test_config:
        if fps_rate <= threshold:
            return params
    return (3.0, 0.25)  # Extremely high FPS: quickest test, highest tolerance


def test_stream_fps_accuracy_comprehensive(device, stream_type_name, test_function, get_fps_function):
    """
    Comprehensive FPS accuracy test for any stream type
    
    Args:
        device: RealSense device
        stream_type_name: Name of stream type for logging (e.g., "depth", "color", "IR")
        test_function: Function to test FPS for this stream type
        get_fps_function: Function to get supported FPS rates for this stream type
    
    Returns:
        Tuple[bool, List[Dict]]: (all_tests_passed, results_list)
    """
    log.i(f"Discovering supported {stream_type_name} FPS rates...")
    supported_fps_rates = get_fps_function(device)
    
    if not supported_fps_rates:
        log.w(f"No supported {stream_type_name} FPS rates found!")
        return False, []
    
    # Optimize for CI if needed
    if CI_MODE and len(supported_fps_rates) > 6:
        # In CI mode, limit to key FPS rates for faster testing
        original_count = len(supported_fps_rates)
        # Keep low, medium, and high FPS rates
        supported_fps_rates = supported_fps_rates[:2] + supported_fps_rates[-2:]  # First 2 and last 2
        # Add one from middle if available
        if original_count > 4:
            mid_index = original_count // 2
            if mid_index not in [0, 1, original_count-2, original_count-1]:
                supported_fps_rates.append(get_fps_function(device)[mid_index])
        supported_fps_rates = sorted(list(set(supported_fps_rates)))
        log.i(f"CI optimization: limiting FPS rates from {original_count} to {len(supported_fps_rates)}")
    
    log.i(f"Found supported {stream_type_name} FPS rates: {supported_fps_rates}")
    
    # Test results summary
    all_fps_results = []
    all_tests_passed = True
    
    # Test each supported FPS rate
    for fps_rate in supported_fps_rates:
        log.i(f"\n--- Testing {fps_rate} FPS {stream_type_name} stream accuracy ---")
        
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
                log.i(f"{stream_type_name.title()} {fps_rate} FPS Results:")
                log.i(f"  Expected FPS: {fps_stats['expected_fps']}")
                log.i(f"  Actual Average FPS: {fps_stats['actual_avg_fps']:.2f}")
                log.i(f"  FPS Range: {fps_stats['fps_min']:.2f} - {fps_stats['fps_max']:.2f}")
                log.i(f"  FPS Std Dev: {fps_stats['fps_std']:.2f}")
                log.i(f"  FPS Deviation: {fps_stats['fps_deviation']*100:.1f}%")
                log.i(f"  Tolerance: {tolerance*100:.0f}%")
                log.i(f"  Frames Captured: {fps_stats['frame_count']}")
                log.i(f"  Test Duration: {fps_stats['test_duration']:.2f}s")
                log.i(f"  Result: {'PASS' if fps_passed else 'FAIL'}")
                
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
                test.check(fps_passed, f"{fps_rate} FPS {stream_type_name} accuracy test - Expected: {fps_rate} FPS, Got: {fps_actual:.2f} FPS")
                
                if not fps_passed:
                    all_tests_passed = False
                    
            else:
                log.e(f"Failed to test {fps_rate} FPS {stream_type_name}: {fps_stats['error']}")
                all_tests_passed = False
                test.check(False, f"{fps_rate} FPS {stream_type_name} test failed: {fps_stats['error']}")
                
        except Exception as e:
            log.e(f"Exception during {fps_rate} FPS {stream_type_name} test: {e}")
            all_tests_passed = False
            test.check(False, f"{fps_rate} FPS {stream_type_name} test exception: {e}")
    
    return all_tests_passed, all_fps_results


def print_fps_test_summary(stream_type_name, supported_fps_rates, all_fps_results, all_tests_passed, product_line):
    """
    Print comprehensive summary for FPS tests with detailed statistics
    """
    log.i(f"\n{'='*70}")
    log.i(f"{stream_type_name.upper()} FPS ACCURACY TEST SUMMARY")
    log.i(f"{'='*70}")
    log.i(f"Device: {product_line}")
    log.i(f"Total FPS rates tested: {len(supported_fps_rates)}")
    
    if all_fps_results:
        log.i(f"\nDetailed Results:")
        log.i(f"{'FPS':<5} {'Status':<6} {'Actual':<8} {'Deviation':<10} {'Tolerance':<9} {'Frames':<7}")
        log.i(f"{'-'*55}")
        
        for result in all_fps_results:
            status = "PASS" if result['passed'] else "FAIL"
            log.i(f"{result['fps_rate']:<5} {status:<6} {result['actual_fps']:<8.2f} "
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
            
            log.i(f"\n--- {stream_type_name.upper()} STREAM STATISTICS ---")
            
            log.i(f"\nTest Results Summary:")
            log.i(f"  Tests Passed: {passed_tests}/{len(all_fps_results)} ({passed_tests/len(all_fps_results)*100:.1f}%)")
            log.i(f"  Tests Failed: {failed_tests}/{len(all_fps_results)} ({failed_tests/len(all_fps_results)*100:.1f}%)")
            log.i(f"  Success Rate: {passed_tests/len(all_fps_results)*100:.1f}%")
            
            log.i(f"\nFPS Rate Coverage:")
            log.i(f"  Expected FPS Range: {min_expected_fps} - {max_expected_fps} FPS")
            log.i(f"  FPS Rate Count: {len(set(expected_fps_values))} unique rates")
            log.i(f"  Supported Rates: {sorted(set(expected_fps_values))}")
            
            log.i(f"\nActual FPS Performance:")
            log.i(f"  Average Actual FPS: {avg_actual_fps:.2f}")
            log.i(f"  Actual FPS Range: {min_actual_fps:.2f} - {max_actual_fps:.2f}")
            log.i(f"  Actual FPS Std Dev: {fps_std_dev:.2f}")
            
            log.i(f"\nAccuracy Statistics:")
            log.i(f"  Average Deviation: {avg_deviation*100:.2f}%")
            log.i(f"  Deviation Range: {min_deviation*100:.2f}% - {max_deviation*100:.2f}%")
            log.i(f"  Deviation Std Dev: {deviation_std*100:.2f}%")
            
            log.i(f"\nFrame Capture Statistics:")
            log.i(f"  Total Frames Captured: {total_frames:,}")
            log.i(f"  Average Frames/Test: {avg_frames:.1f}")
            log.i(f"  Frame Count Range: {min_frames} - {max_frames}")
            
            # Performance categories
            excellent_tests = sum(1 for r in all_fps_results if r['passed'] and r['deviation'] <= 0.05)
            good_tests = sum(1 for r in all_fps_results if r['passed'] and 0.05 < r['deviation'] <= 0.10)
            acceptable_tests = sum(1 for r in all_fps_results if r['passed'] and r['deviation'] > 0.10)
            
            log.i(f"\nPerformance Categories:")
            log.i(f"  Excellent (<=5% deviation): {excellent_tests} tests")
            log.i(f"  Good (5-10% deviation): {good_tests} tests")
            log.i(f"  Acceptable (>10% deviation): {acceptable_tests} tests")
            log.i(f"  Failed: {failed_tests} tests")
            
        log.i(f"\nOverall {stream_type_name.upper()} Result: {'PASS' if all_tests_passed else 'FAIL'}")
    
    # Final overall check
    test.check(all_tests_passed, f"All supported {stream_type_name} FPS rates accuracy test - {len(supported_fps_rates)} rates tested")


def test_stream_configurations_comprehensive(device, stream_type_name, test_function, get_configurations_function, 
                                            test_duration=3.0, fps_tolerance=0.20):
    """
    Test all supported resolution and FPS configurations for a stream type
    
    Args:
        device: RealSense device
        stream_type_name: Name of stream type for logging (e.g., "depth", "color", "IR")
        test_function: Function to test individual configuration (e.g., test_depth_fps_accuracy)
        get_configurations_function: Function to get supported configurations
        test_duration: How long to test each configuration
        fps_tolerance: Allowed FPS deviation
        
    Returns:
        Tuple[bool, List[Dict]]: (all_passed, list_of_results)
    """
    log.i(f"\nTesting all supported {stream_type_name} configurations...")
    
    # Get all supported configurations
    try:
        supported_configs = get_configurations_function(device)
    except Exception as e:
        log.e(f"Failed to get supported {stream_type_name} configurations: {e}")
        return False, []
    
    if not supported_configs:
        log.w(f"No supported {stream_type_name} configurations found")
        return False, []
    
    log.i(f"Found {len(supported_configs)} {stream_type_name} configurations")
    
    # Optimize for CI if needed
    if CI_MODE:
        supported_configs = optimize_for_ci(supported_configs, max_configs=8)
        log.i(f"CI optimization: testing {len(supported_configs)} configurations")
    
    log.i(f"Testing {len(supported_configs)} {stream_type_name} configurations:")
    for width, height, fps in supported_configs:
        log.i(f"  {width}x{height} @ {fps} FPS")
    
    all_results = []
    all_passed = True
    
    for i, (width, height, fps) in enumerate(supported_configs):
        config_name = f"{width}x{height}@{fps}fps"
        log.i(f"\nTesting {stream_type_name} configuration {i+1}/{len(supported_configs)}: {config_name}")
        
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
                    log.e(f"  ERROR: {stats['error']}")
                else:
                    log.e(f"  FAILED: Expected {fps} FPS, got {actual_fps:.1f} FPS "
                          f"(deviation: {stats.get('fps_deviation', 0)*100:.1f}%)")
            else:
                log.i(f"  PASSED: Expected {fps} FPS, got {actual_fps:.1f} FPS "
                      f"(deviation: {stats.get('fps_deviation', 0)*100:.1f}%)")
                
        except Exception as e:
            log.e(f"  ERROR testing {config_name}: {e}")
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
        log.w(f"No {stream_type_name} configuration test results to summarize")
        return
    
    log.i(f"\n{'='*85}")
    log.i(f"{stream_type_name.upper()} CONFIGURATION TEST SUMMARY - {product_line}")
    log.i(f"{'='*85}")
    
    # Group results by resolution for better organization
    resolution_groups = {}
    for result in all_results:
        res_key = f"{result['width']}x{result['height']}"
        if res_key not in resolution_groups:
            resolution_groups[res_key] = []
        resolution_groups[res_key].append(result)
    
    # Print detailed results organized by resolution
    log.i(f"{'Resolution':<12} {'FPS':<5} {'Result':<8} {'Actual FPS':<11} {'Deviation':<10} {'Tolerance':<9} {'Frames':<7}")
    log.i(f"{'-'*85}")
    
    for resolution in sorted(resolution_groups.keys(), key=lambda x: (int(x.split('x')[0]), int(x.split('x')[1]))):
        fps_results = resolution_groups[resolution]
        fps_results.sort(key=lambda x: x['expected_fps'])
        
        for i, result in enumerate(fps_results):
            res_display = resolution if i == 0 else ""  # Only show resolution for first FPS in group
            status = "PASS" if result['passed'] else "FAIL"
            actual_fps_str = f"{result['actual_fps']:.1f}" if result['actual_fps'] > 0 else "ERROR"
            
            log.i(f"{res_display:<12} {result['expected_fps']:<5} {status:<8} {actual_fps_str:<11} "
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
        
        log.i(f"\n--- {stream_type_name.upper()} CONFIGURATION STATISTICS ---")
        
        log.i(f"\nTest Results Summary:")
        log.i(f"  Configurations Passed: {passed_tests}/{total_configs} ({passed_tests/total_configs*100:.1f}%)")
        log.i(f"  Configurations Failed: {failed_tests}/{total_configs} ({failed_tests/total_configs*100:.1f}%)")
        log.i(f"  Success Rate: {passed_tests/total_configs*100:.1f}%")
        
        log.i(f"\nConfiguration Coverage:")
        log.i(f"  Total Configurations: {total_configs}")
        log.i(f"  Unique Resolutions: {unique_resolutions}")  
        log.i(f"  Unique FPS Rates: {unique_fps_rates}")
        log.i(f"  Resolution Range: {min_resolution[0]}x{min_resolution[1]} to {max_resolution[0]}x{max_resolution[1]}")
        log.i(f"  FPS Rate Range: {min_expected_fps} - {max_expected_fps} FPS")
        
        # Resolution breakdown
        log.i(f"\nResolution Analysis:")
        log.i(f"  Smallest Resolution: {min_resolution[0]}x{min_resolution[1]} ({min_resolution[0]*min_resolution[1]:,} pixels)")
        log.i(f"  Largest Resolution: {max_resolution[0]}x{max_resolution[1]} ({max_resolution[0]*max_resolution[1]:,} pixels)")
        if resolution_areas:
            avg_resolution_area = sum(resolution_areas) / len(resolution_areas)
            log.i(f"  Average Resolution: {avg_resolution_area:,.0f} pixels")
        
        log.i(f"\nFPS Performance Statistics:")
        log.i(f"  Average Actual FPS: {avg_actual_fps:.2f}")
        log.i(f"  Actual FPS Range: {min_actual_fps:.2f} - {max_actual_fps:.2f}")
        log.i(f"  Actual FPS Std Dev: {fps_std_dev:.2f}")
        
        log.i(f"\nAccuracy Statistics:")
        log.i(f"  Average Deviation: {avg_deviation*100:.2f}%")
        log.i(f"  Deviation Range: {min_deviation*100:.2f}% - {max_deviation*100:.2f}%")
        log.i(f"  Deviation Std Dev: {deviation_std*100:.2f}%")
        
        log.i(f"\nFrame Capture Statistics:")
        log.i(f"  Total Frames Captured: {total_frames:,}")
        log.i(f"  Average Frames/Config: {avg_frames:.1f}")
        log.i(f"  Frame Count Range: {min_frames} - {max_frames}")
        
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
        
        log.i(f"\nPerformance by Resolution:")
        log.i(f"{'Resolution':<12} {'Passed':<8} {'Total':<7} {'Success Rate':<12}")
        log.i(f"{'-'*42}")
        for resolution in sorted(resolution_performance.keys(), key=lambda x: (int(x.split('x')[0]), int(x.split('x')[1]))):
            perf = resolution_performance[resolution]
            log.i(f"{resolution:<12} {perf['passed']:<8} {perf['total']:<7} {perf['success_rate']:<11.1f}%")
        
        # Performance categories
        excellent_tests = sum(1 for r in successful_results if r['deviation'] <= 0.05)
        good_tests = sum(1 for r in successful_results if 0.05 < r['deviation'] <= 0.10)
        acceptable_tests = sum(1 for r in successful_results if r['deviation'] > 0.10)
        
        log.i(f"\nPerformance Categories:")
        log.i(f"  Excellent (<=5% deviation): {excellent_tests} configurations")
        log.i(f"  Good (5-10% deviation): {good_tests} configurations")
        log.i(f"  Acceptable (>10% deviation): {acceptable_tests} configurations")
        log.i(f"  Failed: {failed_tests} configurations")
        
    else:
        log.i(f"\nConfiguration Statistics:")
        log.i(f"  Total Configurations Tested: {total_configs}")
        log.i(f"  Unique Resolutions: {unique_resolutions}")  
        log.i(f"  Unique FPS Rates: {unique_fps_rates}")
        log.i(f"  Configurations Passed: {passed_tests}/{total_configs}")
        
    log.i(f"\nOverall {stream_type_name.upper()} Configuration Result: {'PASS' if all_passed else 'FAIL'}")


def test_multistream_fps_accuracy(device, depth_config, color_config, test_duration: float = 5.0, fps_tolerance: float = 0.20):
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
        # Stop and close sensors
        try:
            depth_sensor.stop()
            depth_sensor.close()
        except:
            pass
        try:
            color_sensor.stop()
            color_sensor.close()
        except:
            pass
    
    # Calculate statistics
    elapsed_time = test_stopwatch.get_elapsed()
    
    # Check if we have sufficient measurements
    if len(depth_fps_measurements) < 2:
        return False, {"error": f"Insufficient depth measurements: {len(depth_fps_measurements)} (got {depth_frame_count} frames)"}
    
    if len(color_fps_measurements) < 2:
        return False, {"error": f"Insufficient color measurements: {len(color_fps_measurements)} (got {color_frame_count} frames)"}
    
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


def get_depth_color_combinations(device, max_combinations=50):
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
        log.e(f"Failed to get configurations: {e}")
        return []
    
    if not depth_configs or not color_configs:
        log.w("No depth or color configurations available for multi-stream testing")
        return []
    
    # Generate all combinations (removed restrictions for comprehensive testing)
    combinations = []
    for depth_config in depth_configs:
        for color_config in color_configs:
            combinations.append((depth_config, color_config))
    
    log.i(f"Found {len(combinations)} depth+color combinations (all possible combinations)")
    
    # Optimize for CI if needed
    if CI_MODE and len(combinations) > max_combinations:
        log.i(f"CI mode: limiting combinations from {len(combinations)} to {max_combinations}")
        
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
        
        combinations = selected
        
        log.i(f"CI optimization selected: {len(same_fps_same_res)} same FPS+res, {len(same_fps_diff_res)} same FPS+diff res, "
              f"{len(diff_fps_same_res)} diff FPS+same res, {len(diff_fps_diff_res)} diff FPS+res")
        log.i(f"Final selection: {len(selected)} combinations")
    
    return combinations


def test_multistream_configurations_comprehensive(device):
    """
    Test all depth + color multi-stream configurations.
    Tests all possible combinations of depth and color configurations.
    
    Args:
        device: RealSense device
        
    Returns:
        Tuple[bool, List[Dict]]: (all_passed, results_list)
    """
    log.i("\nTesting all depth + color multi-stream configurations...")
    
    # Get combinations
    combinations = get_depth_color_combinations(device)
    
    if not combinations:
        log.w("No depth + color combinations available for testing")
        return False, []
    
    log.i(f"Testing {len(combinations)} depth + color combinations:")
    
    all_results = []
    all_passed = True
    
    for i, (depth_config, color_config) in enumerate(combinations):
        depth_w, depth_h, depth_fps = depth_config
        color_w, color_h, color_fps = color_config
        
        config_name = f"D:{depth_w}x{depth_h}@{depth_fps}fps + C:{color_w}x{color_h}@{color_fps}fps"
        log.i(f"\nTesting multi-stream {i+1}/{len(combinations)}: {config_name}")
        
        try:
            # Adjust test duration and tolerance based on FPS rates using shared logic
            min_fps = min(depth_fps, color_fps)
            test_duration, tolerance = get_fps_test_parameters(min_fps)
            
            passed, stats = test_multistream_fps_accuracy(
                device, depth_config, color_config, test_duration, tolerance
            )
            
            result = {
                "depth_config": depth_config,
                "color_config": color_config,
                "config_name": config_name,
                "passed": passed,
                "stats": stats
            }
            
            all_results.append(result)
            
            if not passed:
                all_passed = False
                if 'error' in stats:
                    log.e(f"  ERROR: {stats['error']}")
                else:
                    depth_stats = stats['depth']
                    color_stats = stats['color']
                    log.e(f"  FAILED:")
                    log.e(f"    Depth: Expected {depth_stats['expected_fps']} FPS, got {depth_stats['actual_fps']:.1f} FPS "
                          f"(deviation: {depth_stats['deviation']*100:.1f}%)")
                    log.e(f"    Color: Expected {color_stats['expected_fps']} FPS, got {color_stats['actual_fps']:.1f} FPS "
                          f"(deviation: {color_stats['deviation']*100:.1f}%)")
            else:
                depth_stats = stats['depth']
                color_stats = stats['color']
                log.i(f"  PASSED:")
                log.i(f"    Depth: Expected {depth_stats['expected_fps']} FPS, got {depth_stats['actual_fps']:.1f} FPS "
                      f"(deviation: {depth_stats['deviation']*100:.1f}%)")
                log.i(f"    Color: Expected {color_stats['expected_fps']} FPS, got {color_stats['actual_fps']:.1f} FPS "
                      f"(deviation: {color_stats['deviation']*100:.1f}%)")
                
        except Exception as e:
            log.e(f"  ERROR testing {config_name}: {e}")
            result = {
                "depth_config": depth_config,
                "color_config": color_config,
                "config_name": config_name,
                "passed": False,
                "stats": {"error": str(e)}
            }
            all_results.append(result)
            all_passed = False
    
    return all_passed, all_results


def print_multistream_test_summary(all_results, all_passed, product_line):
    """Print comprehensive summary of multi-stream test results"""
    if not all_results:
        log.w("No multi-stream test results to summarize")
        return
    
    log.i(f"\n{'='*90}")
    log.i(f"DEPTH + COLOR MULTI-STREAM TEST SUMMARY - {product_line}")
    log.i(f"{'='*90}")
    
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
    
    log.i(f"Total Combinations Tested: {len(all_results)}")
    log.i(f"Same FPS Combinations: {len(same_fps_results)}")
    log.i(f"Different FPS Combinations: {len(different_fps_results)}")
    
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
    
    log.i(f"Same Resolution Combinations: {len(same_res_results)}")
    log.i(f"Different Resolution Combinations: {len(different_res_results)}")
    
    # Print detailed results
    log.i(f"\n{'Depth Config':<20} {'Color Config':<20} {'Result':<8} {'Depth FPS':<10} {'Color FPS':<10} {'Status'}")
    log.i(f"{'-'*90}")
    
    for result in all_results:
        depth_config = result['depth_config']
        color_config = result['color_config']
        
        depth_str = f"{depth_config[0]}x{depth_config[1]}@{depth_config[2]}"
        color_str = f"{color_config[0]}x{color_config[1]}@{color_config[2]}"
        
        status = "PASS" if result['passed'] else "FAIL"
        
        if 'error' in result['stats']:
            log.i(f"{depth_str:<20} {color_str:<20} {status:<8} {'ERROR':<10} {'ERROR':<10} {result['stats']['error']}")
        else:
            depth_actual = result['stats']['depth']['actual_fps']
            color_actual = result['stats']['color']['actual_fps']
            log.i(f"{depth_str:<20} {color_str:<20} {status:<8} {depth_actual:<10.1f} {color_actual:<10.1f}")
    
    # Calculate statistics
    passed_tests = sum(1 for r in all_results if r['passed'])
    failed_tests = len(all_results) - passed_tests
    
    successful_results = [r for r in all_results if r['passed'] and 'error' not in r['stats']]
    
    if successful_results:
        log.i(f"\n--- MULTI-STREAM STATISTICS ---")
        log.i(f"Success Rate: {passed_tests}/{len(all_results)} ({passed_tests/len(all_results)*100:.1f}%)")
        
        # FPS performance analysis
        depth_deviations = [r['stats']['depth']['deviation'] for r in successful_results]
        color_deviations = [r['stats']['color']['deviation'] for r in successful_results]
        
        avg_depth_deviation = sum(depth_deviations) / len(depth_deviations) * 100
        avg_color_deviation = sum(color_deviations) / len(color_deviations) * 100
        
        log.i(f"Average Depth FPS Deviation: {avg_depth_deviation:.2f}%")
        log.i(f"Average Color FPS Deviation: {avg_color_deviation:.2f}%")
        
        # Performance by FPS relationship
        if same_fps_results:
            same_fps_passed = sum(1 for r in same_fps_results if r['passed'])
            log.i(f"Same FPS Performance: {same_fps_passed}/{len(same_fps_results)} ({same_fps_passed/len(same_fps_results)*100:.1f}%)")
        
        if different_fps_results:
            diff_fps_passed = sum(1 for r in different_fps_results if r['passed'])
            log.i(f"Different FPS Performance: {diff_fps_passed}/{len(different_fps_results)} ({diff_fps_passed/len(different_fps_results)*100:.1f}%)")
    
    log.i(f"\nOverall Multi-Stream Result: {'PASS' if all_passed else 'FAIL'}")


#####################################################################################################
test.start("Testing depth FPS accuracy for all supported configurations on " + product_line + " device - " + platform.system() + " OS")

depth_config_tests_passed, depth_config_results = test_stream_configurations_comprehensive(
    dev, "depth", test_depth_fps_accuracy, get_supported_depth_configurations
)

if depth_config_results:
    print_configuration_test_summary("depth", depth_config_results, depth_config_tests_passed, product_line)

test.check(depth_config_tests_passed, f"All supported depth configurations accuracy test - {len(depth_config_results) if depth_config_results else 0} configurations tested")
test.finish()


#####################################################################################################
test.start("Testing color/RGB FPS accuracy for all supported configurations on " + product_line + " device - " + platform.system() + " OS")

color_config_tests_passed, color_config_results = test_stream_configurations_comprehensive(
    dev, "color", test_color_fps_accuracy, get_supported_color_configurations
)

if color_config_results:
    print_configuration_test_summary("color", color_config_results, color_config_tests_passed, product_line)
    test.check(color_config_tests_passed, f"All supported color configurations accuracy test - {len(color_config_results)} configurations tested")
elif not color_config_results:
    # Check if device has no color sensor (like D421, D405)
    product_name = dev.get_info(rs.camera_info.name)
    if 'D421' in product_name or 'D405' in product_name:
        log.i("Device has no color sensor - skipping color configuration tests")
        test.check(True, "Color configuration test skipped - no color sensor on device")
    else:
        log.w("No color configurations found on device that should have color sensor")
        test.check(False, "Color sensor expected but no configurations found")

test.finish()


#####################################################################################################
test.start("Testing IR FPS accuracy for all supported configurations on " + product_line + " device - " + platform.system() + " OS")

ir_config_tests_passed, ir_config_results = test_stream_configurations_comprehensive(
    dev, "IR", test_ir_fps_accuracy, get_supported_ir_configurations
)

if ir_config_results:
    print_configuration_test_summary("IR", ir_config_results, ir_config_tests_passed, product_line)

test.check(ir_config_tests_passed, f"All supported IR configurations accuracy test - {len(ir_config_results) if ir_config_results else 0} configurations tested")
test.finish()


#####################################################################################################
test.start("Testing depth + color multi-stream FPS accuracy for all supported configurations on " + product_line + " device - " + platform.system() + " OS")

multistream_tests_passed, multistream_results = test_multistream_configurations_comprehensive(dev)

if multistream_results:
    print_multistream_test_summary(multistream_results, multistream_tests_passed, product_line)
    test.check(multistream_tests_passed, f"Depth + color multi-stream configurations (all combinations) accuracy test - {len(multistream_results)} combinations tested")
else:
    # Check if device has no color sensor (like D421, D405)
    product_name = dev.get_info(rs.camera_info.name)
    if 'D421' in product_name or 'D405' in product_name:
        log.i("Device has no color sensor - skipping multi-stream tests")
        test.check(True, "Multi-stream test skipped - no color sensor on device")
    else:
        log.w("No multi-stream configurations found on device that should have color sensor")
        test.check(False, "Multi-stream test failed - no configurations found")

test.finish()


#####################################################################################################
test.start("Testing depth FPS accuracy for all supported frame rates on " + product_line + " device - " + platform.system() + " OS")

depth_tests_passed, depth_results = test_stream_fps_accuracy_comprehensive(
    dev, "depth", test_depth_fps_accuracy, get_supported_depth_fps_rates
)

if depth_results:
    print_fps_test_summary("depth", [r['fps_rate'] for r in depth_results], depth_results, depth_tests_passed, product_line)

test.finish()


#####################################################################################################
test.start("Testing color/RGB FPS accuracy for all supported frame rates on " + product_line + " device - " + platform.system() + " OS")

color_tests_passed, color_results = test_stream_fps_accuracy_comprehensive(
    dev, "color", test_color_fps_accuracy, get_supported_color_fps_rates
)

if color_results:
    print_fps_test_summary("color", [r['fps_rate'] for r in color_results], color_results, color_tests_passed, product_line)
elif not color_results:
    # Check if device has no color sensor (like D421, D405)
    product_name = dev.get_info(rs.camera_info.name)
    if 'D421' in product_name or 'D405' in product_name:
        log.i("Device has no color sensor - skipping color FPS tests")
        test.check(True, "Color FPS test skipped - no color sensor on device")
    else:
        log.w("No color FPS rates found on device that should have color sensor")
        test.check(False, "Color sensor expected but no FPS rates found")

test.finish()


#####################################################################################################
test.start("Testing IR FPS accuracy for all supported frame rates on " + product_line + " device - " + platform.system() + " OS")

ir_tests_passed, ir_results = test_stream_fps_accuracy_comprehensive(
    dev, "IR", test_ir_fps_accuracy, get_supported_ir_fps_rates
)

if ir_results:
    print_fps_test_summary("IR", [r['fps_rate'] for r in ir_results], ir_results, ir_tests_passed, product_line)

test.finish()


#####################################################################################################
# Overall comprehensive summary
test.start("Overall FPS Accuracy Test Summary for " + product_line + " device")

log.i(f"\n{'='*80}")
log.i("COMPREHENSIVE FPS ACCURACY TEST SUMMARY")
log.i(f"{'='*80}")
log.i(f"Device: {product_line}")
log.i(f"Platform: {platform.system()}")

# Summary of all stream types - FPS-only tests
log.i(f"\n--- FPS Rate Testing Summary ---")
all_stream_results = []
if depth_results:
    all_stream_results.append(("Depth", depth_tests_passed, len(depth_results)))
if color_results:
    all_stream_results.append(("Color", color_tests_passed, len(color_results)))
if ir_results:
    all_stream_results.append(("IR", ir_tests_passed, len(ir_results)))

log.i(f"{'Stream':<8} {'Result':<8} {'FPS Tests':<10}")
log.i(f"{'-'*30}")

fps_tests_passed = True
total_fps_tests = 0

for stream_name, passed, test_count in all_stream_results:
    status = "PASS" if passed else "FAIL"
    log.i(f"{stream_name:<8} {status:<8} {test_count:<10}")
    total_fps_tests += test_count
    if not passed:
        fps_tests_passed = False

# Summary of all stream types - Full configuration tests
log.i(f"\n--- Full Configuration Testing Summary ---")
all_config_results = []
if depth_config_results:
    all_config_results.append(("Depth", depth_config_tests_passed, len(depth_config_results)))
if color_config_results:
    all_config_results.append(("Color", color_config_tests_passed, len(color_config_results)))
if ir_config_results:
    all_config_results.append(("IR", ir_config_tests_passed, len(ir_config_results)))

# Add multi-stream results
multistream_test_count = 0
if multistream_results:
    all_config_results.append(("Multi-Stream", multistream_tests_passed, len(multistream_results)))
    multistream_test_count = len(multistream_results)

log.i(f"{'Stream':<12} {'Result':<8} {'Config Tests':<12}")
log.i(f"{'-'*36}")

config_tests_passed = True
total_config_tests = 0

for stream_name, passed, test_count in all_config_results:
    status = "PASS" if passed else "FAIL"
    log.i(f"{stream_name:<12} {status:<8} {test_count:<12}")
    total_config_tests += test_count
    if not passed:
        config_tests_passed = False

# Calculate comprehensive cross-stream statistics
if depth_results or color_results or ir_results or depth_config_results or color_config_results or ir_config_results:
    log.i(f"\n--- CROSS-STREAM ANALYSIS ---")
    
    # Combine all FPS results for cross-stream analysis
    all_fps_test_results = []
    if depth_results:
        all_fps_test_results.extend([(r, "Depth") for r in depth_results])
    if color_results:
        all_fps_test_results.extend([(r, "Color") for r in color_results])
    if ir_results:
        all_fps_test_results.extend([(r, "IR") for r in ir_results])
    
    # Combine all configuration results
    all_config_test_results = []
    if depth_config_results:
        all_config_test_results.extend([(r, "Depth") for r in depth_config_results])
    if color_config_results:
        all_config_test_results.extend([(r, "Color") for r in color_config_results])
    if ir_config_results:
        all_config_test_results.extend([(r, "IR") for r in ir_config_results])
    
    if all_fps_test_results:
        # FPS-only test statistics across all streams
        all_fps_rates = set()
        stream_fps_coverage = {}
        
        for result, stream_type in all_fps_test_results:
            all_fps_rates.add(result['fps_rate'])
            if stream_type not in stream_fps_coverage:
                stream_fps_coverage[stream_type] = set()
            stream_fps_coverage[stream_type].add(result['fps_rate'])
        
        log.i(f"\nFPS Rate Coverage Analysis:")
        log.i(f"  Total Unique FPS Rates: {len(all_fps_rates)}")
        log.i(f"  FPS Rates Tested: {sorted(all_fps_rates)}")
        
        for stream_type, fps_rates in stream_fps_coverage.items():
            coverage_percent = len(fps_rates) / len(all_fps_rates) * 100 if all_fps_rates else 0
            log.i(f"  {stream_type} Stream Coverage: {len(fps_rates)}/{len(all_fps_rates)} rates ({coverage_percent:.1f}%)")
    
    if all_config_test_results:
        # Configuration test statistics across all streams
        all_resolutions = set()
        all_config_fps_rates = set()
        stream_config_coverage = {}
        total_configs_by_stream = {}
        
        for result, stream_type in all_config_test_results:
            resolution = (result['width'], result['height'])
            all_resolutions.add(resolution)
            all_config_fps_rates.add(result['expected_fps'])
            
            if stream_type not in stream_config_coverage:
                stream_config_coverage[stream_type] = {'resolutions': set(), 'fps_rates': set()}
                total_configs_by_stream[stream_type] = 0
            
            stream_config_coverage[stream_type]['resolutions'].add(resolution)
            stream_config_coverage[stream_type]['fps_rates'].add(result['expected_fps'])
            total_configs_by_stream[stream_type] += 1
        
        log.i(f"\nConfiguration Coverage Analysis:")
        log.i(f"  Total Unique Resolutions: {len(all_resolutions)}")
        log.i(f"  Total Unique FPS Rates: {len(all_config_fps_rates)}")
        log.i(f"  Total Configuration Combinations: {sum(total_configs_by_stream.values())}")
        
        # Show resolution ranges
        if all_resolutions:
            min_res = min(all_resolutions, key=lambda x: x[0] * x[1])
            max_res = max(all_resolutions, key=lambda x: x[0] * x[1])
            log.i(f"  Resolution Range: {min_res[0]}x{min_res[1]} to {max_res[0]}x{max_res[1]}")
        
        for stream_type, coverage in stream_config_coverage.items():
            res_count = len(coverage['resolutions'])
            fps_count = len(coverage['fps_rates'])
            config_count = total_configs_by_stream[stream_type]
            log.i(f"  {stream_type} Stream: {config_count} configs ({res_count} resolutions {fps_count} FPS rates)")
    
    # Performance comparison across streams
    if all_fps_test_results:
        log.i(f"\nStream Performance Comparison:")
        log.i(f"{'Stream':<8} {'Tests':<6} {'Pass Rate':<10} {'Avg Deviation':<13} {'Best FPS':<9} {'Worst FPS':<10}")
        log.i(f"{'-'*65}")
        
        for stream_type in ["Depth", "Color", "IR"]:
            stream_results = [r for r, s in all_fps_test_results if s == stream_type]
            if stream_results:
                passed_count = sum(1 for r in stream_results if r['passed'])
                pass_rate = passed_count / len(stream_results) * 100
                
                valid_deviations = [r['deviation'] for r in stream_results if r['deviation'] < 1.0]
                avg_deviation = sum(valid_deviations) / len(valid_deviations) * 100 if valid_deviations else 0
                
                valid_fps = [r['actual_fps'] for r in stream_results if r['actual_fps'] > 0]
                best_fps = max(valid_fps) if valid_fps else 0
                worst_fps = min(valid_fps) if valid_fps else 0
                
                log.i(f"{stream_type:<8} {len(stream_results):<6} {pass_rate:<9.1f}% {avg_deviation:<12.1f}% {best_fps:<8.1f} {worst_fps:<10.1f}")

if not all_stream_results and not all_config_results:
    log.e("No stream types could be tested!")
    overall_passed = False
else:
    overall_passed = fps_tests_passed and config_tests_passed

log.i(f"\nOverall Results:")
log.i(f"  Stream types tested: {max(len(all_stream_results), len(all_config_results))}")
log.i(f"  FPS rate tests: {total_fps_tests} ({'PASS' if fps_tests_passed else 'FAIL'})")
log.i(f"  Single-stream configuration tests: {total_config_tests - multistream_test_count} ({'PASS' if config_tests_passed else 'FAIL'})")
if multistream_test_count > 0:
    log.i(f"  Multi-stream configuration tests: {multistream_test_count} ({'PASS' if multistream_tests_passed else 'FAIL'})")
log.i(f"  Total configuration tests: {total_config_tests}")
log.i(f"  Grand total tests: {total_fps_tests + total_config_tests}")
log.i(f"  Overall result: {'PASS' if overall_passed else 'FAIL'}")

# Calculate and display total test execution time
overall_test_end_time = time.time()
total_test_duration = overall_test_end_time - overall_test_start_time
log.i(f"\n{'='*80}")
log.i("TOTAL TEST EXECUTION TIME")
log.i(f"{'='*80}")
log.i(f"Total execution time: {format_duration(total_test_duration)}")

# Performance summary
tests_per_second = (total_fps_tests + total_config_tests) / total_test_duration if total_test_duration > 0 else 0
log.i(f"Average test throughput: {tests_per_second:.2f} tests/second")
log.i(f"Time per test (average): {total_test_duration/(total_fps_tests + total_config_tests):.2f} seconds/test")

test.check(overall_passed, f"Comprehensive FPS accuracy test - {total_fps_tests} FPS tests + {total_config_tests} configuration tests (including {multistream_test_count} multi-stream tests)")

test.finish()


#####################################################################################################
test.print_results_and_exit()
