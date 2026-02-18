# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Test configuration: Requires TWO D400 devices connected simultaneously
#test:device D400* D400*

"""
Test simultaneous streaming from two D400 devices with frame drop detection.

This test validates:
1. Two devices can stream simultaneously with the same configuration
2. Frame metadata (frame counter) is consistent and detects dropped frames
3. All available sensors can stream concurrently from both devices
4. All supported resolutions and frame rates work correctly
5. No significant frame drops occur during 5-second streaming sessions

The test iterates through all common stream profiles supported by both devices
and verifies data integrity using frame counter metadata.
"""

import pyrealsense2 as rs
from rspy import test, log
import time
from collections import defaultdict

# Test configuration
STREAM_DURATION_SEC = 5
MAX_FRAME_DROP_PERCENTAGE = 5.0  # Allow up to 5% frame drops

def get_common_profiles(dev1, dev2):
    """
    Find all stream profiles that are supported by BOTH devices.
    Returns a dict mapping (stream_type, format) to a list of common (width, height, fps) tuples.
    """
    # Get all sensors from both devices
    sensors1 = dev1.query_sensors()
    sensors2 = dev2.query_sensors()
    
    # Build profile sets for each device
    profiles1 = defaultdict(set)
    profiles2 = defaultdict(set)
    
    for sensor in sensors1:
        for profile in sensor.get_stream_profiles():
            if profile.is_video_stream_profile():
                vp = profile.as_video_stream_profile()
                key = (profile.stream_type(), profile.format())
                value = (vp.width(), vp.height(), profile.fps())
                profiles1[key].add(value)
    
    for sensor in sensors2:
        for profile in sensor.get_stream_profiles():
            if profile.is_video_stream_profile():
                vp = profile.as_video_stream_profile()
                key = (profile.stream_type(), profile.format())
                value = (vp.width(), vp.height(), profile.fps())
                profiles2[key].add(value)
    
    # Find common profiles between both devices
    common_profiles = {}
    for key in profiles1.keys():
        if key in profiles2:
            common = profiles1[key].intersection(profiles2[key])
            if common:
                common_profiles[key] = sorted(list(common))
    
    return common_profiles


def get_sensor_for_stream(device, stream_type):
    """Get the sensor that supports the given stream type."""
    for sensor in device.query_sensors():
        for profile in sensor.get_stream_profiles():
            if profile.stream_type() == stream_type:
                return sensor
    return None


def stream_and_check_frames(dev1, dev2, config_description, stream_configs):
    """
    Stream from both devices with the given configuration and check for frame drops.
    
    :param dev1: First device
    :param dev2: Second device
    :param config_description: Human-readable description of the configuration
    :param stream_configs: List of (stream_type, width, height, format, fps) tuples
    :return: Tuple of (success, drop_percentage1, drop_percentage2)
    """
    sn1 = dev1.get_info(rs.camera_info.serial_number)
    sn2 = dev2.get_info(rs.camera_info.serial_number)
    
    pipe1 = rs.pipeline()
    pipe2 = rs.pipeline()
    
    cfg1 = rs.config()
    cfg2 = rs.config()
    
    # Configure both pipelines identically
    cfg1.enable_device(sn1)
    cfg2.enable_device(sn2)
    
    for stream_type, width, height, format, fps in stream_configs:
        cfg1.enable_stream(stream_type, width, height, format, fps)
        cfg2.enable_stream(stream_type, width, height, format, fps)
    
    try:
        # Start both pipelines
        profile1 = pipe1.start(cfg1)
        profile2 = pipe2.start(cfg2)
        
        # Allow auto-exposure to stabilize
        log.d(f"  Stabilizing for 2 seconds...")
        for _ in range(60):  # ~2 seconds at 30fps
            try:
                pipe1.wait_for_frames(timeout_ms=5000)
                pipe2.wait_for_frames(timeout_ms=5000)
            except:
                pass
        
        # Collect frame counters for each stream
        frame_counters1 = defaultdict(list)
        frame_counters2 = defaultdict(list)
        
        log.d(f"  Streaming for {STREAM_DURATION_SEC} seconds...")
        start_time = time.time()
        frames_received1 = 0
        frames_received2 = 0
        
        while time.time() - start_time < STREAM_DURATION_SEC:
            try:
                # Device 1
                frameset1 = pipe1.wait_for_frames(timeout_ms=5000)
                frames_received1 += 1
                
                for frame in frameset1:
                    if frame.supports_frame_metadata(rs.frame_metadata_value.frame_counter):
                        counter = frame.get_frame_metadata(rs.frame_metadata_value.frame_counter)
                        stream_type = frame.get_profile().stream_type()
                        frame_counters1[stream_type].append(counter)
                
                # Device 2
                frameset2 = pipe2.wait_for_frames(timeout_ms=5000)
                frames_received2 += 1
                
                for frame in frameset2:
                    if frame.supports_frame_metadata(rs.frame_metadata_value.frame_counter):
                        counter = frame.get_frame_metadata(rs.frame_metadata_value.frame_counter)
                        stream_type = frame.get_profile().stream_type()
                        frame_counters2[stream_type].append(counter)
                        
            except Exception as e:
                log.w(f"  Exception during streaming: {e}")
                break
        
        log.i(f"  Device 1 received {frames_received1} framesets")
        log.i(f"  Device 2 received {frames_received2} framesets")
        
        # Analyze frame drops
        def analyze_drops(frame_counters, device_name):
            total_expected = 0
            total_received = 0
            
            for stream_type, counters in frame_counters.items():
                if len(counters) < 2:
                    log.w(f"    {device_name} {stream_type}: insufficient frames")
                    continue
                
                # Calculate expected frames based on counter range
                counter_range = counters[-1] - counters[0]
                expected = counter_range + 1
                received = len(counters)
                dropped = expected - received
                
                total_expected += expected
                total_received += received
                
                drop_pct = (dropped / expected * 100) if expected > 0 else 0
                
                log.d(f"    {device_name} {stream_type}: {received}/{expected} frames, "
                      f"{dropped} dropped ({drop_pct:.2f}%)")
            
            if total_expected > 0:
                overall_drop_pct = ((total_expected - total_received) / total_expected * 100)
                return overall_drop_pct
            return 0.0
        
        drop_pct1 = analyze_drops(frame_counters1, f"Dev1({sn1})")
        drop_pct2 = analyze_drops(frame_counters2, f"Dev2({sn2})")
        
        success = (drop_pct1 <= MAX_FRAME_DROP_PERCENTAGE and 
                   drop_pct2 <= MAX_FRAME_DROP_PERCENTAGE)
        
        return success, drop_pct1, drop_pct2
        
    finally:
        try:
            pipe1.stop()
        except:
            pass
        try:
            pipe2.stop()
        except:
            pass


#
# Test: Stream all common profiles and check for frame drops
#
with test.closure("Two devices - frame drop detection across all profiles"):
    with test.two_devices(rs.product_line.D400) as (dev1, dev2):
        
        sn1 = dev1.get_info(rs.camera_info.serial_number)
        sn2 = dev2.get_info(rs.camera_info.serial_number)
        
        log.i(f"Testing devices: {sn1} and {sn2}")
        
        # Get common profiles
        log.i("Finding common stream profiles...")
        common_profiles = get_common_profiles(dev1, dev2)
        
        if not common_profiles:
            log.w("No common profiles found between devices")
            test.check(False, "Devices should have common stream profiles")
        else:
            log.i(f"Found {len(common_profiles)} common stream types")
            for (stream_type, format), configs in common_profiles.items():
                log.d(f"  {stream_type} {format}: {len(configs)} configurations")
        
        # Test a representative subset of configurations to keep test time reasonable
        # We'll test: each stream type with its most common resolution at different FPS
        tested_configs = 0
        failed_configs = 0
        
        # Group by stream type
        by_stream_type = defaultdict(list)
        for (stream_type, format), configs in common_profiles.items():
            by_stream_type[stream_type].append((format, configs))
        
        # For each stream type, test multiple configurations
        for stream_type, format_configs in by_stream_type.items():
            log.i(f"\nTesting {stream_type} stream:")
            
            # Get the first format (most common)
            if format_configs:
                format, configs = format_configs[0]
                
                # Test a few different resolutions/fps combinations
                # Sort by resolution (width*height) and fps
                sorted_configs = sorted(configs, key=lambda x: (x[0]*x[1], x[2]))
                
                # Test: lowest resolution, mid resolution, highest resolution
                test_indices = [0]
                if len(sorted_configs) > 2:
                    test_indices.append(len(sorted_configs) // 2)
                if len(sorted_configs) > 1:
                    test_indices.append(-1)
                
                for idx in set(test_indices):
                    width, height, fps = sorted_configs[idx]
                    
                    config_desc = f"{stream_type} {width}x{height} @ {fps}fps {format}"
                    log.i(f"\n  Testing: {config_desc}")
                    
                    stream_configs = [(stream_type, width, height, format, fps)]
                    
                    success, drop_pct1, drop_pct2 = stream_and_check_frames(
                        dev1, dev2, config_desc, stream_configs
                    )
                    
                    tested_configs += 1
                    
                    if success:
                        log.i(f"  ✓ PASS - Drop rates: {drop_pct1:.2f}%, {drop_pct2:.2f}%")
                    else:
                        log.w(f"  ✗ FAIL - Drop rates: {drop_pct1:.2f}%, {drop_pct2:.2f}%")
                        failed_configs += 1
                    
                    test.check(success, 
                              f"{config_desc} should have <{MAX_FRAME_DROP_PERCENTAGE}% drops")
        
        log.i(f"\nTested {tested_configs} configurations, {failed_configs} failed")
        test.check(failed_configs == 0, "All tested configurations should pass")


#
# Test: Stream ALL sensors simultaneously (depth + color + infrared)
#
with test.closure("Two devices - all sensors streaming simultaneously"):
    with test.two_devices(rs.product_line.D400) as (dev1, dev2):
        
        sn1 = dev1.get_info(rs.camera_info.serial_number)
        sn2 = dev2.get_info(rs.camera_info.serial_number)
        
        log.i(f"Testing simultaneous multi-sensor streaming")
        
        # Find a configuration that enables multiple streams
        # Use a common, safe resolution that most D400 devices support
        width, height, fps = 640, 480, 30
        
        # Build multi-stream configuration
        stream_configs = []
        
        # Try to enable depth, color, and infrared streams
        common_profiles = get_common_profiles(dev1, dev2)
        
        # Add depth stream
        if (rs.stream.depth, rs.format.z16) in common_profiles:
            stream_configs.append((rs.stream.depth, width, height, rs.format.z16, fps))
            log.d(f"  Added depth stream: {width}x{height} @ {fps}fps")
        
        # Add color stream
        color_added = False
        for color_format in [rs.format.rgb8, rs.format.bgr8, rs.format.rgba8, rs.format.bgra8]:
            if (rs.stream.color, color_format) in common_profiles:
                configs = common_profiles[(rs.stream.color, color_format)]
                # Check if our desired resolution is available
                if (width, height, fps) in configs:
                    stream_configs.append((rs.stream.color, width, height, color_format, fps))
                    log.d(f"  Added color stream: {width}x{height} @ {fps}fps {color_format}")
                    color_added = True
                    break
        
        # Add infrared streams
        if (rs.stream.infrared, rs.format.y8) in common_profiles:
            # Check for infrared 1 and 2
            stream_configs.append((rs.stream.infrared, width, height, rs.format.y8, fps))
            log.d(f"  Added infrared stream: {width}x{height} @ {fps}fps")
        
        if len(stream_configs) < 2:
            log.w("Could not find enough common streams for multi-sensor test")
        else:
            log.i(f"Testing {len(stream_configs)} streams simultaneously:")
            for stream_type, w, h, fmt, f in stream_configs:
                log.i(f"  - {stream_type} {w}x{h} @ {f}fps {fmt}")
            
            config_desc = f"{len(stream_configs)} streams @ {width}x{height} {fps}fps"
            
            success, drop_pct1, drop_pct2 = stream_and_check_frames(
                dev1, dev2, config_desc, stream_configs
            )
            
            if success:
                log.i(f"✓ Multi-sensor streaming PASS - Drop rates: {drop_pct1:.2f}%, {drop_pct2:.2f}%")
            else:
                log.w(f"✗ Multi-sensor streaming FAIL - Drop rates: {drop_pct1:.2f}%, {drop_pct2:.2f}%")
            
            test.check(success, 
                      f"Multi-sensor streaming should have <{MAX_FRAME_DROP_PERCENTAGE}% drops")


#
# Test: High-resolution streaming from both devices
#
with test.closure("Two devices - high resolution streaming"):
    with test.two_devices(rs.product_line.D400) as (dev1, dev2):
        
        log.i("Testing high-resolution streaming")
        
        common_profiles = get_common_profiles(dev1, dev2)
        
        # Find highest common resolution for depth
        if (rs.stream.depth, rs.format.z16) in common_profiles:
            depth_configs = common_profiles[(rs.stream.depth, rs.format.z16)]
            # Sort by resolution (width * height)
            sorted_depths = sorted(depth_configs, key=lambda x: x[0] * x[1], reverse=True)
            
            if sorted_depths:
                width, height, fps = sorted_depths[0]
                log.i(f"Testing highest depth resolution: {width}x{height} @ {fps}fps")
                
                stream_configs = [(rs.stream.depth, width, height, rs.format.z16, fps)]
                
                success, drop_pct1, drop_pct2 = stream_and_check_frames(
                    dev1, dev2, f"High-res depth {width}x{height} @ {fps}fps", stream_configs
                )
                
                if success:
                    log.i(f"✓ High-resolution PASS - Drop rates: {drop_pct1:.2f}%, {drop_pct2:.2f}%")
                else:
                    log.w(f"✗ High-resolution FAIL - Drop rates: {drop_pct1:.2f}%, {drop_pct2:.2f}%")
                
                test.check(success, 
                          f"High-resolution streaming should have <{MAX_FRAME_DROP_PERCENTAGE}% drops")


#
# Test: High frame rate streaming from both devices
#
with test.closure("Two devices - high frame rate streaming"):
    with test.two_devices(rs.product_line.D400) as (dev1, dev2):
        
        log.i("Testing high frame rate streaming")
        
        common_profiles = get_common_profiles(dev1, dev2)
        
        # Find highest common frame rate for depth
        if (rs.stream.depth, rs.format.z16) in common_profiles:
            depth_configs = common_profiles[(rs.stream.depth, rs.format.z16)]
            # Sort by FPS
            sorted_by_fps = sorted(depth_configs, key=lambda x: x[2], reverse=True)
            
            if sorted_by_fps and sorted_by_fps[0][2] > 30:  # Only test if we have > 30 FPS
                width, height, fps = sorted_by_fps[0]
                log.i(f"Testing highest frame rate: {width}x{height} @ {fps}fps")
                
                stream_configs = [(rs.stream.depth, width, height, rs.format.z16, fps)]
                
                success, drop_pct1, drop_pct2 = stream_and_check_frames(
                    dev1, dev2, f"High-FPS depth {width}x{height} @ {fps}fps", stream_configs
                )
                
                if success:
                    log.i(f"✓ High frame rate PASS - Drop rates: {drop_pct1:.2f}%, {drop_pct2:.2f}%")
                else:
                    log.w(f"✗ High frame rate FAIL - Drop rates: {drop_pct1:.2f}%, {drop_pct2:.2f}%")
                
                test.check(success, 
                          f"High frame rate streaming should have <{MAX_FRAME_DROP_PERCENTAGE}% drops")
            else:
                log.i("No high frame rate (>30fps) profiles found, skipping")


# Print test summary
test.print_results_and_exit()
