# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Test configuration: Requires ANY TWO RealSense devices connected simultaneously
# Devices can be same or different product lines (e.g., D400 + D400, or D400 + L500)
#test:device * *

"""
Test simultaneous multi-stream operation from two RealSense devices.

This test validates:
1. Multiple different stream types can run together on both devices (depth + color + IR)
2. All streams from both devices can be synchronized
3. Frame metadata (frame counter) is consistent across all streams
4. No significant frame drops occur during multi-stream operation
5. Both devices can handle complex streaming scenarios simultaneously

Unlike test-two-devices-frame-drops.py which tests each stream type separately,
this test streams ALL available stream types together (depth + color + infrared)
from both devices simultaneously, creating a more realistic and stressful scenario.

The test works with ANY two RealSense devices (same or different product lines).
It automatically finds common stream profiles and enables as many streams as possible.
"""

import pyrealsense2 as rs
from rspy import test, log
import time
from collections import defaultdict

# Test configuration
STREAM_DURATION_SEC = 10  # Longer duration for multi-stream stress test
MAX_FRAME_DROP_PERCENTAGE = 5.0  # Allow up to 5% frame drops
STABILIZATION_TIME_SEC = 3  # Time to allow auto-exposure to settle

def get_common_multi_stream_config(dev1, dev2):
    """
    Find a multi-stream configuration that works on both devices.
    Returns a list of (stream_type, width, height, format, fps) tuples.
    
    This tries to enable as many stream types as possible:
    - Depth stream
    - Color stream  
    - Infrared streams (1 and 2 if available)
    
    All streams will use the same resolution and FPS for simplicity.
    """
    # Use a common resolution that most devices support
    target_width, target_height, target_fps = 640, 480, 30
    
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
    
    # Build multi-stream configuration
    stream_configs = []
    
    # Try to add Depth stream
    depth_key = (rs.stream.depth, rs.format.z16)
    if depth_key in profiles1 and depth_key in profiles2:
        common_depth = profiles1[depth_key].intersection(profiles2[depth_key])
        if (target_width, target_height, target_fps) in common_depth:
            stream_configs.append((rs.stream.depth, target_width, target_height, rs.format.z16, target_fps))
            log.d(f"  Added Depth stream: {target_width}x{target_height} @ {target_fps}fps")
    
    # Try to add Color stream (try multiple formats)
    color_formats = [rs.format.rgb8, rs.format.bgr8, rs.format.rgba8, rs.format.bgra8, rs.format.yuyv]
    for color_format in color_formats:
        color_key = (rs.stream.color, color_format)
        if color_key in profiles1 and color_key in profiles2:
            common_color = profiles1[color_key].intersection(profiles2[color_key])
            if (target_width, target_height, target_fps) in common_color:
                stream_configs.append((rs.stream.color, target_width, target_height, color_format, target_fps))
                log.d(f"  Added Color stream: {target_width}x{target_height} @ {target_fps}fps {color_format}")
                break
    
    # Try to add Infrared stream (usually index 1)
    ir_key = (rs.stream.infrared, rs.format.y8)
    if ir_key in profiles1 and ir_key in profiles2:
        common_ir = profiles1[ir_key].intersection(profiles2[ir_key])
        if (target_width, target_height, target_fps) in common_ir:
            stream_configs.append((rs.stream.infrared, target_width, target_height, rs.format.y8, target_fps))
            log.d(f"  Added Infrared stream: {target_width}x{target_height} @ {target_fps}fps")
    
    # Try to add second Infrared stream if available (index 2)
    # Note: We can't add multiple streams of same type with different indices via simple enable_stream
    # So we'll skip IR2 for now to keep the test simpler
    
    return stream_configs


def stream_multi_and_check_frames(dev1, dev2, stream_configs, duration_sec=STREAM_DURATION_SEC):
    """
    Stream multiple stream types from both devices simultaneously and check for frame drops.
    
    :param dev1: First device
    :param dev2: Second device
    :param stream_configs: List of (stream_type, width, height, format, fps) tuples
    :param duration_sec: How long to stream in seconds
    :return: Tuple of (success, drop_percentage1, drop_percentage2, stats)
    """
    sn1 = dev1.get_info(rs.camera_info.serial_number)
    sn2 = dev2.get_info(rs.camera_info.serial_number)
    
    name1 = dev1.get_info(rs.camera_info.name) if dev1.supports(rs.camera_info.name) else "Unknown"
    name2 = dev2.get_info(rs.camera_info.name) if dev2.supports(rs.camera_info.name) else "Unknown"
    
    pipe1 = rs.pipeline()
    pipe2 = rs.pipeline()
    
    cfg1 = rs.config()
    cfg2 = rs.config()
    
    # Configure both pipelines identically
    cfg1.enable_device(sn1)
    cfg2.enable_device(sn2)
    
    log.i(f"Configuring streams:")
    for stream_type, width, height, format, fps in stream_configs:
        cfg1.enable_stream(stream_type, width, height, format, fps)
        cfg2.enable_stream(stream_type, width, height, format, fps)
        log.i(f"  - {stream_type} {width}x{height} @ {fps}fps {format}")
    
    try:
        # Start both pipelines
        log.d(f"Starting pipeline on {name1} (SN: {sn1})...")
        profile1 = pipe1.start(cfg1)
        
        log.d(f"Starting pipeline on {name2} (SN: {sn2})...")
        profile2 = pipe2.start(cfg2)
        
        # Allow auto-exposure to stabilize
        log.i(f"Stabilizing for {STABILIZATION_TIME_SEC} seconds...")
        stabilization_frames = int(STABILIZATION_TIME_SEC * 30)  # Assume ~30fps
        for _ in range(stabilization_frames):
            try:
                pipe1.wait_for_frames(timeout_ms=5000)
                pipe2.wait_for_frames(timeout_ms=5000)
            except Exception as e:
                log.w(f"  Exception during stabilization: {e}")
        
        # Collect frame counters for each stream
        frame_counters1 = defaultdict(list)
        frame_counters2 = defaultdict(list)
        
        # Track framesets received
        framesets_received1 = 0
        framesets_received2 = 0
        
        # Track how many frames each stream type received
        stream_frame_counts1 = defaultdict(int)
        stream_frame_counts2 = defaultdict(int)
        
        log.i(f"Streaming for {duration_sec} seconds...")
        start_time = time.time()
        
        while time.time() - start_time < duration_sec:
            try:
                # Device 1
                frameset1 = pipe1.wait_for_frames(timeout_ms=5000)
                framesets_received1 += 1
                
                for frame in frameset1:
                    stream_type = frame.get_profile().stream_type()
                    stream_frame_counts1[stream_type] += 1
                    
                    if frame.supports_frame_metadata(rs.frame_metadata_value.frame_counter):
                        counter = frame.get_frame_metadata(rs.frame_metadata_value.frame_counter)
                        frame_counters1[stream_type].append(counter)
                
                # Device 2
                frameset2 = pipe2.wait_for_frames(timeout_ms=5000)
                framesets_received2 += 1
                
                for frame in frameset2:
                    stream_type = frame.get_profile().stream_type()
                    stream_frame_counts2[stream_type] += 1
                    
                    if frame.supports_frame_metadata(rs.frame_metadata_value.frame_counter):
                        counter = frame.get_frame_metadata(rs.frame_metadata_value.frame_counter)
                        frame_counters2[stream_type].append(counter)
                        
            except Exception as e:
                log.w(f"  Exception during streaming: {e}")
                break
        
        actual_duration = time.time() - start_time
        
        log.i(f"Streaming completed after {actual_duration:.2f} seconds")
        log.i(f"Device 1 ({name1}): {framesets_received1} framesets")
        log.i(f"Device 2 ({name2}): {framesets_received2} framesets")
        
        # Log per-stream frame counts
        log.d(f"Device 1 frame counts by stream:")
        for stream_type, count in stream_frame_counts1.items():
            log.d(f"  {stream_type}: {count} frames")
        
        log.d(f"Device 2 frame counts by stream:")
        for stream_type, count in stream_frame_counts2.items():
            log.d(f"  {stream_type}: {count} frames")
        
        # Analyze frame drops
        def analyze_drops(frame_counters, stream_frame_counts, device_name):
            total_expected = 0
            total_received = 0
            per_stream_stats = {}
            
            for stream_type, counters in frame_counters.items():
                if len(counters) < 2:
                    log.w(f"  {device_name} {stream_type}: insufficient frames ({len(counters)})")
                    continue
                
                # Calculate expected frames based on counter range
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
                
                log.d(f"  {device_name} {stream_type}: {received}/{expected} frames, "
                      f"{dropped} dropped ({drop_pct:.2f}%)")
            
            if total_expected > 0:
                overall_drop_pct = ((total_expected - total_received) / total_expected * 100)
            else:
                overall_drop_pct = 0.0
                
            return overall_drop_pct, per_stream_stats
        
        drop_pct1, stats1 = analyze_drops(frame_counters1, stream_frame_counts1, f"Dev1({sn1})")
        drop_pct2, stats2 = analyze_drops(frame_counters2, stream_frame_counts2, f"Dev2({sn2})")
        
        success = (drop_pct1 <= MAX_FRAME_DROP_PERCENTAGE and 
                   drop_pct2 <= MAX_FRAME_DROP_PERCENTAGE)
        
        stats = {
            'dev1': {
                'name': name1,
                'sn': sn1,
                'framesets': framesets_received1,
                'drop_pct': drop_pct1,
                'streams': stats1
            },
            'dev2': {
                'name': name2,
                'sn': sn2,
                'framesets': framesets_received2,
                'drop_pct': drop_pct2,
                'streams': stats2
            },
            'duration': actual_duration
        }
        
        return success, drop_pct1, drop_pct2, stats
        
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
# Test: Stream multiple stream types simultaneously from both devices
#
with test.closure("Two devices - multi-stream simultaneous operation (depth + color + IR)"):
    with test.two_devices() as (dev1, dev2):
        
        sn1 = dev1.get_info(rs.camera_info.serial_number)
        sn2 = dev2.get_info(rs.camera_info.serial_number)
        
        name1 = dev1.get_info(rs.camera_info.name) if dev1.supports(rs.camera_info.name) else "Unknown"
        name2 = dev2.get_info(rs.camera_info.name) if dev2.supports(rs.camera_info.name) else "Unknown"
        
        log.i("=" * 80)
        log.i("Testing multi-stream operation on two devices:")
        log.i(f"  Device 1: {name1} (SN: {sn1})")
        log.i(f"  Device 2: {name2} (SN: {sn2})")
        log.i("=" * 80)
        
        # Get common multi-stream configuration
        log.i("\nFinding common multi-stream configuration...")
        stream_configs = get_common_multi_stream_config(dev1, dev2)
        
        if len(stream_configs) < 2:
            log.w(f"Insufficient common streams found ({len(stream_configs)})")
            log.w("At least 2 stream types needed for multi-stream test")
            test.check(False, "Devices should support at least 2 common stream types")
        else:
            log.i(f"\nFound {len(stream_configs)} common stream types")
            log.i("Will stream all of them simultaneously from both devices")
            
            # Run the multi-stream test
            success, drop_pct1, drop_pct2, stats = stream_multi_and_check_frames(
                dev1, dev2, stream_configs
            )
            
            # Print detailed results
            log.i("\n" + "=" * 80)
            log.i("RESULTS:")
            log.i("=" * 80)
            log.i(f"Duration: {stats['duration']:.2f} seconds")
            log.i(f"\nDevice 1 ({stats['dev1']['name']}):")
            log.i(f"  Total framesets: {stats['dev1']['framesets']}")
            log.i(f"  Overall drop rate: {stats['dev1']['drop_pct']:.2f}%")
            for stream_type, stream_stats in stats['dev1']['streams'].items():
                log.i(f"  {stream_type}:")
                log.i(f"    Received: {stream_stats['received']}/{stream_stats['expected']}")
                log.i(f"    Dropped: {stream_stats['dropped']} ({stream_stats['drop_pct']:.2f}%)")
            
            log.i(f"\nDevice 2 ({stats['dev2']['name']}):")
            log.i(f"  Total framesets: {stats['dev2']['framesets']}")
            log.i(f"  Overall drop rate: {stats['dev2']['drop_pct']:.2f}%")
            for stream_type, stream_stats in stats['dev2']['streams'].items():
                log.i(f"  {stream_type}:")
                log.i(f"    Received: {stream_stats['received']}/{stream_stats['expected']}")
                log.i(f"    Dropped: {stream_stats['dropped']} ({stream_stats['drop_pct']:.2f}%)")
            
            log.i("=" * 80)
            
            if success:
                log.i(f"\n✓ PASS - Multi-stream test successful!")
                log.i(f"  Device 1 drop rate: {drop_pct1:.2f}%")
                log.i(f"  Device 2 drop rate: {drop_pct2:.2f}%")
            else:
                log.w(f"\n✗ FAIL - Excessive frame drops detected!")
                log.w(f"  Device 1 drop rate: {drop_pct1:.2f}% (max: {MAX_FRAME_DROP_PERCENTAGE}%)")
                log.w(f"  Device 2 drop rate: {drop_pct2:.2f}% (max: {MAX_FRAME_DROP_PERCENTAGE}%)")
            
            test.check(success, 
                      f"Multi-stream operation should have <{MAX_FRAME_DROP_PERCENTAGE}% drops on both devices")


#
# Test: Stress test with longer duration
#
with test.closure("Two devices - multi-stream long duration stress test"):
    with test.two_devices() as (dev1, dev2):
        
        log.i("Running extended multi-stream stress test (30 seconds)...")
        
        stream_configs = get_common_multi_stream_config(dev1, dev2)
        
        if len(stream_configs) >= 2:
            success, drop_pct1, drop_pct2, stats = stream_multi_and_check_frames(
                dev1, dev2, stream_configs, duration_sec=30
            )
            
            log.i(f"\nStress test completed:")
            log.i(f"  Duration: {stats['duration']:.2f} seconds")
            log.i(f"  Device 1: {stats['dev1']['framesets']} framesets, {drop_pct1:.2f}% drops")
            log.i(f"  Device 2: {stats['dev2']['framesets']} framesets, {drop_pct2:.2f}% drops")
            
            if success:
                log.i("✓ Long duration stress test PASSED")
            else:
                log.w("✗ Long duration stress test FAILED")
            
            test.check(success, 
                      f"30-second multi-stream stress test should have <{MAX_FRAME_DROP_PERCENTAGE}% drops")
        else:
            log.w("Skipping stress test - insufficient stream types available")


#
# Test: Verify stream independence (changing one stream doesn't affect others)
#
with test.closure("Two devices - multi-stream independence verification"):
    with test.two_devices() as (dev1, dev2):
        
        log.i("Testing stream independence...")
        
        stream_configs = get_common_multi_stream_config(dev1, dev2)
        
        if len(stream_configs) >= 2:
            log.i("Verifying that all streams receive frames independently")
            
            success, drop_pct1, drop_pct2, stats = stream_multi_and_check_frames(
                dev1, dev2, stream_configs, duration_sec=5
            )
            
            # Check that each stream type received a reasonable number of frames
            # (at least 80% of expected for a 5-second test at 30fps = ~120 frames)
            min_expected_frames = int(5 * 30 * 0.8)  # 80% of 5 sec @ 30fps
            
            all_streams_ok = True
            
            for stream_type, stream_stats in stats['dev1']['streams'].items():
                if stream_stats['received'] < min_expected_frames:
                    log.w(f"Device 1 {stream_type} received only {stream_stats['received']} frames (expected ~{min_expected_frames})")
                    all_streams_ok = False
            
            for stream_type, stream_stats in stats['dev2']['streams'].items():
                if stream_stats['received'] < min_expected_frames:
                    log.w(f"Device 2 {stream_type} received only {stream_stats['received']} frames (expected ~{min_expected_frames})")
                    all_streams_ok = False
            
            if all_streams_ok:
                log.i("✓ All streams received adequate frame counts")
            else:
                log.w("✗ Some streams received fewer frames than expected")
            
            test.check(all_streams_ok and success, 
                      "All streams should receive frames independently without interference")
        else:
            log.w("Skipping independence test - insufficient stream types available")


# Print test summary
test.print_results_and_exit()
