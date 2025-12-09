# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# test:donotrun:!weekly
# test:device D400_CAM_SYNC
# test:timeout 300

"""
RealSense Dual-Camera Synchronized Depth FPS Performance Test

Tests FPS accuracy for synchronized depth streaming on two cameras.
Configures cameras in master/slave mode and validates that both cameras
maintain accurate frame rates across all commonly supported depth configurations.

Requires: 2 D400 cameras with matching capabilities
"""

from rspy import test, log
from rspy.stopwatch import Stopwatch
import pyrealsense2 as rs
import numpy as np
import platform
import time
import os
from collections import deque

# Constants for testing
DEVICE_INIT_SLEEP_SEC = 3  # Sleep time to allow device to get into idle state
MIN_FRAME_COUNT_LOW_FPS = 5  # Minimum frame count for low FPS tests
MIN_TEST_DURATION_PERCENT = 0.6  # Minimum test duration percentage (60%)


class FPSMonitor:
    """Monitor and calculate FPS statistics using a sliding window of frame timestamps"""
    
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


# The device starts at D0 (Operational) state, allow time for it to get into idle state
time.sleep(DEVICE_INIT_SLEEP_SEC)


#####################################################################################################
# Initialize both devices for synchronized testing
ctx = rs.context( { "dds" : { "enabled" : False } } )
devs = ctx.devices
if len(devs) == 0:
    # No devices found, try to find a device with DDS enabled
    ctx = rs.context( { "dds" : { "enabled" : True } } )
    devs = ctx.devices

if len(devs) < 2:
    log.e(f"Test requires 2 cameras but only {len(devs)} detected")
    test.check(False, "Insufficient cameras detected for sync test")
    test.print_results_and_exit()

dev1 = devs[0]
dev2 = devs[1]

product_line = dev1.get_info(rs.camera_info.product_line)
log.i(f"Detected {len(devs)} cameras: {dev1.get_info(rs.camera_info.serial_number)} and {dev2.get_info(rs.camera_info.serial_number)}")


def test_dual_depth_fps_accuracy(master_device, slave_device, expected_fps: int, width: int = None, height: int = None, test_duration: float = 10.0, fps_tolerance: float = 0.15):
    """
    Test synchronized depth stream FPS accuracy for both cameras simultaneously.
    
    Configures cameras in master/slave mode using inter-camera sync and measures
    FPS performance for both cameras streaming the same profile concurrently.
    
    Args:
        master_device: RealSense device to configure as master (inter_cam_sync_mode=1)
        slave_device: RealSense device to configure as slave (inter_cam_sync_mode=2)
        expected_fps: Target FPS rate for both cameras
        width: Resolution width (optional)
        height: Resolution height (optional)
        test_duration: Test duration in seconds
        fps_tolerance: Allowed FPS deviation ratio (0.15 = 15%)

    Returns:
        Tuple[bool, Dict, Dict]: (both_passed, master_stats_dict, slave_stats_dict)
    """
    # Configure master camera with inter-camera sync mode
    try:
        master_sensor = master_device.first_depth_sensor()
        master_sensor.set_option(rs.option.inter_cam_sync_mode, 1)  # Master mode
        log.i(f"Master camera ({master_device.get_info(rs.camera_info.serial_number)}) configured")
    except Exception as e:
        error_msg = f"Failed to configure master camera: {e}"
        log.e(error_msg)
        return False, {"error": error_msg}, {"error": "Master config failed"}
    
    # Configure slave camera with inter-camera sync mode
    try:
        slave_sensor = slave_device.first_depth_sensor()
        slave_sensor.set_option(rs.option.inter_cam_sync_mode, 2)  # Slave mode
        log.i(f"Slave camera ({slave_device.get_info(rs.camera_info.serial_number)}) configured")
    except Exception as e:
        error_msg = f"Failed to configure slave camera: {e}"
        log.e(error_msg)
        return False, {"error": "Slave config failed"}, {"error": error_msg}
    
    # Find matching profiles for both cameras
    master_profile = None
    slave_profile = None
    
    for p in master_sensor.profiles:
        if (p.fps() == expected_fps and 
            p.stream_type() == rs.stream.depth and 
            p.format() == rs.format.z16):
            if width is not None and height is not None:
                vp = p.as_video_stream_profile()
                if vp.width() == width and vp.height() == height:
                    master_profile = p
                    break
            else:
                master_profile = p
                break
    
    for p in slave_sensor.profiles:
        if (p.fps() == expected_fps and 
            p.stream_type() == rs.stream.depth and 
            p.format() == rs.format.z16):
            if width is not None and height is not None:
                vp = p.as_video_stream_profile()
                if vp.width() == width and vp.height() == height:
                    slave_profile = p
                    break
            else:
                slave_profile = p
                break
    
    if not master_profile:
        error_msg = f"No master profile found with {expected_fps} FPS"
        if width and height:
            error_msg += f" and resolution {width}x{height}"
        log.e(error_msg)
        return False, {"error": error_msg}, {"error": "No slave test (master failed)"}
    
    if not slave_profile:
        error_msg = f"No slave profile found with {expected_fps} FPS"
        if width and height:
            error_msg += f" and resolution {width}x{height}"
        log.e(error_msg)
        return False, {"error": "No master test (slave failed)"}, {"error": error_msg}
    
    # Setup FPS monitors and measurement tracking for both cameras
    master_monitor = FPSMonitor(window_size=60)
    slave_monitor = FPSMonitor(window_size=60)
    
    master_frame_count = 0
    slave_frame_count = 0
    master_fps_measurements = []
    slave_fps_measurements = []
    
    # Adjust warmup and measurement parameters based on FPS rate
    # Lower FPS needs fewer warmup frames and less frequent measurements
    if expected_fps <= 6:
        warmup_frames = 2
        measurement_interval = 1
        log.d(f"Very low FPS mode: warmup={warmup_frames}, interval={measurement_interval}")
    elif expected_fps <= 15:
        warmup_frames = 5
        measurement_interval = 10
    elif expected_fps <= 30:
        warmup_frames = 15
        measurement_interval = 15
    elif expected_fps <= 60:
        warmup_frames = 20
        measurement_interval = 20
    else:
        warmup_frames = 25
        measurement_interval = 25
    
    # Callback functions track frame arrival times and calculate FPS for each camera
    def master_callback(frame):
        nonlocal master_frame_count
        current_time = time.time()
        master_monitor.update(current_time)
        master_frame_count += 1
        
        if master_frame_count > warmup_frames and master_frame_count % measurement_interval == 0:
            current_fps = master_monitor.get_current_fps()
            if current_fps > 0:
                master_fps_measurements.append(current_fps)
    
    def slave_callback(frame):
        nonlocal slave_frame_count
        current_time = time.time()
        slave_monitor.update(current_time)
        slave_frame_count += 1
        
        if slave_frame_count > warmup_frames and slave_frame_count % measurement_interval == 0:
            current_fps = slave_monitor.get_current_fps()
            if current_fps > 0:
                slave_fps_measurements.append(current_fps)
    
    # Start streaming both cameras
    test_stopwatch = Stopwatch()
    try:
        master_sensor.open(master_profile)
        slave_sensor.open(slave_profile)
        
        master_sensor.start(master_callback)
        slave_sensor.start(slave_callback)
        
        log.d(f"Both cameras streaming at {expected_fps} FPS ({width}x{height})...")
        
        # Determine minimum measurements needed
        min_measurements_needed = 2 if expected_fps <= 6 else 3
        
        while test_stopwatch.get_elapsed() < test_duration:
            time.sleep(0.1)
            
            # Early exit if both cameras have sufficient measurements
            if (len(master_fps_measurements) >= min_measurements_needed and 
                len(slave_fps_measurements) >= min_measurements_needed):
                elapsed = test_stopwatch.get_elapsed()
                
                exit_threshold = 0.5 if expected_fps <= 6 else (0.65 if expected_fps <= 15 else (0.70 if expected_fps <= 30 else 0.75))
                if elapsed >= (test_duration * exit_threshold):
                    log.d(f"Both cameras have sufficient measurements - early exit at {elapsed:.1f}s")
                    break
    
    finally:
        try:
            master_sensor.stop()
            master_sensor.close()
        except:
            pass
        try:
            slave_sensor.stop()
            slave_sensor.close()
        except:
            pass
    
    # Calculate FPS statistics for master camera
    # Different handling for very low FPS (<=6) which may have single measurement
    master_stats = {}
    if not master_fps_measurements:
        master_stats = {"error": f"No master FPS measurements (frames: {master_frame_count})"}
        master_passed = False
        master_actual_fps = 0.0
    elif expected_fps <= 6 and len(master_fps_measurements) == 1 and master_frame_count >= MIN_FRAME_COUNT_LOW_FPS:
        master_actual_fps = master_fps_measurements[0]
        master_deviation = abs(master_actual_fps - expected_fps) / expected_fps
        master_passed = master_deviation <= fps_tolerance
        master_stats = {
            "frame_count": master_frame_count,
            "test_duration": test_stopwatch.get_elapsed(),
            "expected_fps": expected_fps,
            "actual_avg_fps": master_actual_fps,
            "fps_deviation": master_deviation,
            "measurements_count": 1
        }
    elif len(master_fps_measurements) < 2:
        master_stats = {"error": f"Insufficient master measurements: {len(master_fps_measurements)}"}
        master_passed = False
        master_actual_fps = 0.0
    else:
        master_actual_fps = sum(master_fps_measurements) / len(master_fps_measurements)
        master_deviation = abs(master_actual_fps - expected_fps) / expected_fps
        master_passed = master_deviation <= fps_tolerance
        master_stats = {
            "frame_count": master_frame_count,
            "test_duration": test_stopwatch.get_elapsed(),
            "expected_fps": expected_fps,
            "actual_avg_fps": master_actual_fps,
            "fps_min": min(master_fps_measurements),
            "fps_max": max(master_fps_measurements),
            "fps_std": np.std(master_fps_measurements),
            "fps_deviation": master_deviation,
            "measurements_count": len(master_fps_measurements)
        }
    
    # Calculate FPS statistics for slave camera using same logic as master
    slave_stats = {}
    if not slave_fps_measurements:
        slave_stats = {"error": f"No slave FPS measurements (frames: {slave_frame_count})"}
        slave_passed = False
        slave_actual_fps = 0.0
    elif expected_fps <= 6 and len(slave_fps_measurements) == 1 and slave_frame_count >= MIN_FRAME_COUNT_LOW_FPS:
        slave_actual_fps = slave_fps_measurements[0]
        slave_deviation = abs(slave_actual_fps - expected_fps) / expected_fps
        slave_passed = slave_deviation <= fps_tolerance
        slave_stats = {
            "frame_count": slave_frame_count,
            "test_duration": test_stopwatch.get_elapsed(),
            "expected_fps": expected_fps,
            "actual_avg_fps": slave_actual_fps,
            "fps_deviation": slave_deviation,
            "measurements_count": 1
        }
    elif len(slave_fps_measurements) < 2:
        slave_stats = {"error": f"Insufficient slave measurements: {len(slave_fps_measurements)}"}
        slave_passed = False
        slave_actual_fps = 0.0
    else:
        slave_actual_fps = sum(slave_fps_measurements) / len(slave_fps_measurements)
        slave_deviation = abs(slave_actual_fps - expected_fps) / expected_fps
        slave_passed = slave_deviation <= fps_tolerance
        slave_stats = {
            "frame_count": slave_frame_count,
            "test_duration": test_stopwatch.get_elapsed(),
            "expected_fps": expected_fps,
            "actual_avg_fps": slave_actual_fps,
            "fps_min": min(slave_fps_measurements),
            "fps_max": max(slave_fps_measurements),
            "fps_std": np.std(slave_fps_measurements),
            "fps_deviation": slave_deviation,
            "measurements_count": len(slave_fps_measurements)
        }
    
    both_passed = master_passed and slave_passed
    
    log.i(f"Master: {master_frame_count} frames, {master_actual_fps:.1f} FPS (expected {expected_fps})")
    log.i(f"Slave: {slave_frame_count} frames, {slave_actual_fps:.1f} FPS (expected {expected_fps})")
    
    return both_passed, master_stats, slave_stats


def get_supported_stream_configurations(device, stream_type, format_filter, get_sensor_func, include_resolution=True):
    """
    Discover all supported configurations for a stream type.
    
    Args:
        device: RealSense device
        stream_type: rs.stream type constant
        format_filter: Format to filter by (None accepts any)
        get_sensor_func: Function that returns the sensor
        include_resolution: True returns (width, height, fps), False returns fps only
        
    Returns:
        List of configuration tuples or FPS rates
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


def get_supported_depth_configurations(device):
    """Get all supported depth stream configurations (resolution + FPS combinations)"""
    return get_supported_stream_configurations(
        device, rs.stream.depth, rs.format.z16, 
        lambda d: d.first_depth_sensor(), 
        include_resolution=True
    )


def get_fps_test_parameters(fps_rate):
    """
    Determine optimal test duration and tolerance based on FPS rate.
    Lower FPS rates get longer test durations and higher tolerance.
    
    Args:
        fps_rate: Target FPS rate
    
    Returns:
        Tuple[float, float]: (test_duration_seconds, tolerance_ratio)
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


def test_dual_stream_configurations_comprehensive(master_device, slave_device, stream_type_name, test_function, get_configurations_function, 
                                            test_duration=3.0, fps_tolerance=0.20):
    """
    Test all commonly supported configurations for synchronized dual-camera streaming.
    
    Discovers configurations supported by both cameras, then tests each common
    configuration by streaming both cameras simultaneously in master/slave mode.
    Only tests profiles that both cameras support to ensure compatibility.
    
    Args:
        master_device: RealSense master device
        slave_device: RealSense slave device
        stream_type_name: Stream name for logging (e.g., "depth")
        test_function: Function to test dual-camera configuration
        get_configurations_function: Function to discover camera configurations
        test_duration: Duration per configuration test in seconds
        fps_tolerance: Allowed FPS deviation ratio
        
    Returns:
        Tuple[bool, List[Dict]]: (all_tests_passed, results_list_for_common_configs)
    """
    log.i(f"\nTesting all supported {stream_type_name} configurations on both cameras...")
    
    # Get supported configurations from both cameras to find common profiles
    try:
        master_configs = get_configurations_function(master_device)
        slave_configs = get_configurations_function(slave_device)
    except Exception as e:
        log.e(f"Failed to get supported {stream_type_name} configurations: {e}")
        return False, []
    
    if not master_configs:
        log.w(f"No supported {stream_type_name} configurations found on master camera")
        return False, []
    
    if not slave_configs:
        log.w(f"No supported {stream_type_name} configurations found on slave camera")
        return False, []
    
    # Find intersection of supported profiles - only test configurations both cameras support
    # This prevents attempting to stream profiles that one camera doesn't have
    master_configs_set = set(master_configs)
    slave_configs_set = set(slave_configs)
    supported_configs = sorted(list(master_configs_set & slave_configs_set), key=lambda x: (x[0] * x[1], x[2]))
    
    if not supported_configs:
        log.e(f"No common {stream_type_name} configurations found between cameras")
        log.e(f"Master has {len(master_configs)} configs, Slave has {len(slave_configs)} configs, but none match")
        return False, []
    
    log.i(f"Found {len(supported_configs)} common {stream_type_name} configurations (Master: {len(master_configs)}, Slave: {len(slave_configs)})")
    
    log.i(f"Testing {len(supported_configs)} {stream_type_name} configurations on both cameras:")
    for width, height, fps in supported_configs:
        log.i(f"  {width}x{height} @ {fps} FPS")
    
    all_results = []
    all_passed = True
    
    for i, (width, height, fps) in enumerate(supported_configs):
        config_name = f"{width}x{height}@{fps}fps"
        log.i(f"\nTesting {stream_type_name} configuration {i+1}/{len(supported_configs)}: {config_name}")
        
        try:
            # Test this specific configuration on both cameras
            both_passed, master_stats, slave_stats = test_function(
                master_device, slave_device, fps, width, height, test_duration, fps_tolerance
            )
            
            # Skip if profile not found on either camera (mismatched capabilities)
            if 'error' in master_stats and ('No master profile found' in master_stats['error'] or 
                                            'No slave test' in master_stats['error']):
                log.w(f"  SKIPPED: Profile not available on master camera")
                continue
            
            if 'error' in slave_stats and ('No slave profile found' in slave_stats['error'] or 
                                           'No master test' in slave_stats['error']):
                log.w(f"  SKIPPED: Profile not available on slave camera")
                continue
            
            # Store results for both cameras
            result = {
                "width": width,
                "height": height, 
                "expected_fps": fps,
                "master_actual_fps": master_stats.get('actual_avg_fps', 0.0),
                "slave_actual_fps": slave_stats.get('actual_avg_fps', 0.0),
                "passed": both_passed,
                "master_deviation": master_stats.get('fps_deviation', 1.0),
                "slave_deviation": slave_stats.get('fps_deviation', 1.0),
                "tolerance": fps_tolerance,
                "master_frame_count": master_stats.get('frame_count', 0),
                "slave_frame_count": slave_stats.get('frame_count', 0),
                "config_name": config_name,
                "master_stats": master_stats,
                "slave_stats": slave_stats
            }
            
            all_results.append(result)
            
            if not both_passed:
                all_passed = False
                if 'error' in master_stats:
                    log.e(f"  MASTER ERROR: {master_stats['error']}")
                else:
                    log.e(f"  MASTER: Expected {fps} FPS, got {master_stats.get('actual_avg_fps', 0):.1f} FPS "
                          f"(deviation: {master_stats.get('fps_deviation', 0)*100:.1f}%)")
                
                if 'error' in slave_stats:
                    log.e(f"  SLAVE ERROR: {slave_stats['error']}")
                else:
                    log.e(f"  SLAVE: Expected {fps} FPS, got {slave_stats.get('actual_avg_fps', 0):.1f} FPS "
                          f"(deviation: {slave_stats.get('fps_deviation', 0)*100:.1f}%)")
            else:
                log.i(f"  PASSED - Master: {master_stats.get('actual_avg_fps', 0):.1f} FPS, "
                      f"Slave: {slave_stats.get('actual_avg_fps', 0):.1f} FPS")
                
        except Exception as e:
            log.e(f"  ERROR testing {config_name}: {e}")
            result = {
                "width": width,
                "height": height,
                "expected_fps": fps,
                "master_actual_fps": 0.0,
                "slave_actual_fps": 0.0,
                "passed": False,
                "master_deviation": 1.0,
                "slave_deviation": 1.0,
                "tolerance": fps_tolerance,
                "master_frame_count": 0,
                "slave_frame_count": 0,
                "config_name": config_name,
                "master_stats": {"error": str(e)},
                "slave_stats": {"error": str(e)}
            }
            all_results.append(result)
            all_passed = False
    
    return all_passed, all_results


#####################################################################################################
test.start("Testing synchronized depth FPS accuracy for all supported configurations on 2x " + product_line + " devices - " + platform.system() + " OS")

depth_config_tests_passed, depth_config_results = test_dual_stream_configurations_comprehensive(
    dev1, dev2, "depth", test_dual_depth_fps_accuracy, get_supported_depth_configurations
)

test.check(depth_config_tests_passed, f"All supported depth configurations sync test - {len(depth_config_results) if depth_config_results else 0} configurations tested on both cameras")
test.finish()

#####################################################################################################
# Run second test with reversed master/slave roles to validate both cameras in each role
test.start("Testing synchronized depth FPS accuracy for all supported configurations on 2x " + product_line + " devices - " + platform.system() + " OS")

depth_config_tests_passed, depth_config_results = test_dual_stream_configurations_comprehensive(
    dev2, dev1, "depth", test_dual_depth_fps_accuracy, get_supported_depth_configurations
)

test.check(depth_config_tests_passed, f"All supported depth configurations sync test - {len(depth_config_results) if depth_config_results else 0} configurations tested on both cameras")
test.finish()


#####################################################################################################
test.print_results_and_exit()
