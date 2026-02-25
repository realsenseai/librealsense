# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Test configuration: Tests all connected RealSense devices
# Minimal config (* = at least 1 device) then enable all hub ports to discover all devices
#test:device *

"""
Multi-stream operation test for all connected devices.

Tests:
- Simultaneous multi-stream operation (depth + color + IR) on all devices
- Frame drop detection with multiple stream types
- Long duration stress testing
- Stream independence verification

Requires at least 2 devices. Works with any RealSense device types.
See README.md for documentation on writing multi-device tests.
"""

import pyrealsense2 as rs
from rspy import test, log
import time
from collections import defaultdict

# Test configuration
STREAM_DURATION_SEC = 10  # Longer duration for multi-stream stress test
MAX_FRAME_DROP_PERCENTAGE = 5.0  # Allow up to 5% frame drops
STABILIZATION_TIME_SEC = 3  # Time to allow auto-exposure to settle

# Query all connected devices directly via RealSense context
ctx = rs.context()
device_list = ctx.query_devices()
device_count = len(device_list)

log.i(f"\n{'='*80}")
log.i(f"TESTING MULTIPLE CONNECTED DEVICES - Found {device_count} device(s)")
log.i(f"{'='*80}\n")

if device_count == 0:
    # Should not happen as infrastructure requires at least 1 device, but handle gracefully
    with test.closure("No devices found"):
        log.e("No devices found - cannot run any tests")
        test.check(False, "At least one device required")
elif device_count == 1:
    # Single device - run basic verification only
    with test.closure("Single device - basic verification"):
        dev = device_list[0]
        sn = dev.get_info(rs.camera_info.serial_number) if dev.supports(rs.camera_info.serial_number) else "Unknown"
        name = dev.get_info(rs.camera_info.name) if dev.supports(rs.camera_info.name) else "Unknown"
        log.i(f"Found single device: {name} (SN: {sn})")
        log.i("Multi-device streaming test requires at least 2 devices")
        log.i("Performing basic device verification only...")
        
        sensors = dev.query_sensors()
        test.check(len(sensors) > 0, "Device should have sensors")
        log.i(f"Device has {len(sensors)} sensor(s) - basic verification passed")
else:
    # Multiple devices - run full multi-stream test
    log.i(f"Found {device_count} devices - running full multi-stream test")

def get_common_multi_stream_config(*devs):
    """
    Find a multi-stream configuration that works on all provided devices.
    Returns a list of (stream_type, width, height, format, fps) tuples.
    
    This tries to enable as many stream types as possible:
    - Depth stream
    - Color stream  
    - Infrared streams (1 and 2 if available)
    
    All streams will use the same resolution and FPS for simplicity.
    """
    # Try common resolutions in order of preference
    # 640x360 added as fallback to support safety camera profiles
    target_resolutions = [
        (640, 480, 30),  # Standard VGA resolution
        (640, 360, 30),  # Fallback for safety cameras and other devices
    ]
    target_fps = 30
    
    # Build profile sets for each device
    all_profiles = []
    
    for dev in devs:
        sensors = dev.query_sensors()
        dev_profiles = defaultdict(set)
        
        for sensor in sensors:
            for profile in sensor.get_stream_profiles():
                if profile.is_video_stream_profile():
                    vp = profile.as_video_stream_profile()
                    key = (profile.stream_type(), profile.format())
                    value = (vp.width(), vp.height(), profile.fps())
                    dev_profiles[key].add(value)
        
        all_profiles.append(dev_profiles)
    
    # Build multi-stream configuration by finding common profiles across ALL devices
    stream_configs = []
    
    # Try to add Depth stream
    depth_key = (rs.stream.depth, rs.format.z16)
    if all(depth_key in dev_prof for dev_prof in all_profiles):
        # Find intersection of all devices
        common_depth = all_profiles[0][depth_key]
        for dev_prof in all_profiles[1:]:
            common_depth = common_depth.intersection(dev_prof[depth_key])
        
        # Try each resolution until we find a common one
        for target_width, target_height, target_fps in target_resolutions:
            if (target_width, target_height, target_fps) in common_depth:
                stream_configs.append((rs.stream.depth, target_width, target_height, rs.format.z16, target_fps))
                log.d(f"  Added Depth stream: {target_width}x{target_height} @ {target_fps}fps")
                break
    
    # Try to add Color stream (try multiple formats)
    color_formats = [rs.format.rgb8, rs.format.bgr8, rs.format.rgba8, rs.format.bgra8, rs.format.yuyv]
    for color_format in color_formats:
        color_key = (rs.stream.color, color_format)
        if all(color_key in dev_prof for dev_prof in all_profiles):
            common_color = all_profiles[0][color_key]
            for dev_prof in all_profiles[1:]:
                common_color = common_color.intersection(dev_prof[color_key])
            
            # Try each resolution until we find a common one
            for target_width, target_height, target_fps in target_resolutions:
                if (target_width, target_height, target_fps) in common_color:
                    stream_configs.append((rs.stream.color, target_width, target_height, color_format, target_fps))
                    log.d(f"  Added Color stream: {target_width}x{target_height} @ {target_fps}fps {color_format}")
                    break
            if stream_configs and stream_configs[-1][0] == rs.stream.color:
                # Successfully added color stream, don't try other formats
                break
    
    # Try to add Infrared stream (usually index 1)
    ir_key = (rs.stream.infrared, rs.format.y8)
    if all(ir_key in dev_prof for dev_prof in all_profiles):
        common_ir = all_profiles[0][ir_key]
        for dev_prof in all_profiles[1:]:
            common_ir = common_ir.intersection(dev_prof[ir_key])
        
        # Try each resolution until we find a common one
        for target_width, target_height, target_fps in target_resolutions:
            if (target_width, target_height, target_fps) in common_ir:
                stream_configs.append((rs.stream.infrared, target_width, target_height, rs.format.y8, target_fps))
                log.d(f"  Added Infrared stream: {target_width}x{target_height} @ {target_fps}fps")
                break
    
    # Try to add second Infrared stream if available (index 2)
    # Note: We can't add multiple streams of same type with different indices via simple enable_stream
    # So we'll skip IR2 for now to keep the test simpler
    
    return stream_configs


def setup_pipelines(devs, stream_configs):
    """
    Create and configure pipelines for all devices with the specified stream configurations.
    
    :param devs: List of device objects
    :param stream_configs: List of (stream_type, width, height, format, fps) tuples
    :return: Tuple of (pipes, cfgs, device_info)
    """
    pipes = []
    cfgs = []
    device_info = []
    
    # Setup pipelines for all devices
    for dev in devs:
        sn = dev.get_info(rs.camera_info.serial_number)
        name = dev.get_info(rs.camera_info.name) if dev.supports(rs.camera_info.name) else "Unknown"
        
        pipe = rs.pipeline()
        cfg = rs.config()
        cfg.enable_device(sn)
        
        pipes.append(pipe)
        cfgs.append(cfg)
        device_info.append({'sn': sn, 'name': name})
    
    # Configure all pipelines identically
    log.i(f"Configuring streams:")
    for stream_type, width, height, format, fps in stream_configs:
        for cfg in cfgs:
            cfg.enable_stream(stream_type, width, height, format, fps)
        log.i(f"  - {stream_type} {width}x{height} @ {fps}fps {format}")
    
    return pipes, cfgs, device_info


def stabilize_streams(pipes):
    """
    Allow auto-exposure to stabilize by collecting and discarding initial frames.
    
    :param pipes: List of pipeline objects
    """
    log.i(f"Stabilizing for {STABILIZATION_TIME_SEC} seconds...")
    stabilization_frames = int(STABILIZATION_TIME_SEC * 30)  # Assume ~30fps
    for _ in range(stabilization_frames):
        try:
            for pipe in pipes:
                pipe.wait_for_frames(timeout_ms=5000)
        except Exception as e:
            log.w(f"  Exception during stabilization: {e}")


def collect_frames(pipes, duration_sec):
    """
    Collect frames from all pipelines for the specified duration.
    
    :param pipes: List of pipeline objects
    :param duration_sec: How long to stream in seconds
    :return: Tuple of (all_frame_counters, all_framesets_received, all_stream_frame_counts, actual_duration)
    """
    all_frame_counters = [defaultdict(list) for _ in pipes]
    all_framesets_received = [0] * len(pipes)
    all_stream_frame_counts = [defaultdict(int) for _ in pipes]
    
    log.i(f"Streaming for {duration_sec} seconds...")
    start_time = time.time()
    
    while time.time() - start_time < duration_sec:
        try:
            for i, pipe in enumerate(pipes):
                frameset = pipe.wait_for_frames(timeout_ms=5000)
                all_framesets_received[i] += 1
                
                for frame in frameset:
                    stream_type = frame.get_profile().stream_type()
                    all_stream_frame_counts[i][stream_type] += 1
                    
                    if frame.supports_frame_metadata(rs.frame_metadata_value.frame_counter):
                        counter = frame.get_frame_metadata(rs.frame_metadata_value.frame_counter)
                        all_frame_counters[i][stream_type].append(counter)
                    
        except Exception as e:
            log.w(f"  Exception during streaming: {e}")
            break
    
    actual_duration = time.time() - start_time
    return all_frame_counters, all_framesets_received, all_stream_frame_counts, actual_duration


def analyze_device_drops(frame_counters, stream_frame_counts, device_name):
    """
    Analyze frame drops for a single device across all streams.
    
    :param frame_counters: Dict of stream_type -> list of frame counters
    :param stream_frame_counts: Dict of stream_type -> total frame count
    :param device_name: Name/identifier for logging
    :return: Tuple of (overall_drop_percentage, per_stream_stats_dict)
    """
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


def aggregate_results(all_frame_counters, all_framesets_received, all_stream_frame_counts, 
                     device_info, actual_duration):
    """
    Aggregate and analyze results from all devices.
    
    :param all_frame_counters: List of frame counter dicts (one per device)
    :param all_framesets_received: List of frameset counts (one per device)
    :param all_stream_frame_counts: List of stream frame count dicts (one per device)
    :param device_info: List of device info dicts
    :param actual_duration: Actual streaming duration in seconds
    :return: Tuple of (success, drop_percentages, stats_dict)
    """
    log.i(f"Streaming completed after {actual_duration:.2f} seconds")
    for i, info in enumerate(device_info):
        log.i(f"Device {i+1} ({info['name']}): {all_framesets_received[i]} framesets")
    
    # Log per-stream frame counts for all devices
    for i, (info, stream_counts) in enumerate(zip(device_info, all_stream_frame_counts)):
        log.d(f"Device {i+1} frame counts by stream:")
        for stream_type, count in stream_counts.items():
            log.d(f"  {stream_type}: {count} frames")
    
    # Analyze drops for all devices
    drop_percentages = []
    all_stats = []
    
    for i, (frame_counters, stream_counts, info) in enumerate(zip(all_frame_counters, all_stream_frame_counts, device_info)):
        drop_pct, stream_stats = analyze_device_drops(frame_counters, stream_counts, f"Dev{i+1}({info['sn']})")
        drop_percentages.append(drop_pct)
        
        dev_stats = {
            'name': info['name'],
            'sn': info['sn'],
            'framesets': all_framesets_received[i],
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


def stream_multi_and_check_frames(*devs, stream_configs, duration_sec=STREAM_DURATION_SEC):
    """
    Stream multiple stream types from all devices simultaneously and check for frame drops.
    
    :param devs: Variable number of device objects
    :param stream_configs: List of (stream_type, width, height, format, fps) tuples
    :param duration_sec: How long to stream in seconds
    :return: Tuple of (success, list of drop_percentages, stats)
    """
    # Setup phase: Create and configure pipelines
    pipes, cfgs, device_info = setup_pipelines(devs, stream_configs)
    
    try:
        # Start all pipelines
        for i, (pipe, cfg, info) in enumerate(zip(pipes, cfgs, device_info)):
            log.d(f"Starting pipeline on {info['name']} (SN: {info['sn']})...")
            pipe.start(cfg)
        
        # Stabilization phase: Allow auto-exposure to settle
        stabilize_streams(pipes)
        
        # Collection phase: Stream and collect frame data
        all_frame_counters, all_framesets_received, all_stream_frame_counts, actual_duration = \
            collect_frames(pipes, duration_sec)
        
        # Analysis phase: Aggregate results and analyze drops
        success, drop_percentages, stats = aggregate_results(
            all_frame_counters, all_framesets_received, all_stream_frame_counts,
            device_info, actual_duration
        )
        
        return success, drop_percentages, stats
        
    finally:
        for pipe in pipes:
            try:
                pipe.stop()
            except:
                pass


#
# Test: Stream multiple stream types simultaneously from all devices
#
if device_count >= 2:
    with test.closure(f"Multiple devices - multi-stream simultaneous operation (depth + color + IR) - {device_count} devices"):
        # Use the devices already queried at the top of the file
        devs = [device_list[i] for i in range(device_count)]
        
        log.i("=" * 80)
        log.i(f"Testing multi-stream operation on {device_count} devices:")
        for i, dev in enumerate(devs, 1):
            sn = dev.get_info(rs.camera_info.serial_number)
            name = dev.get_info(rs.camera_info.name) if dev.supports(rs.camera_info.name) else "Unknown"
            log.i(f"  Device {i}: {name} (SN: {sn})")
    log.i("=" * 80)
        
    # Get common multi-stream configuration
    log.i("\nFinding common multi-stream configuration...")
    stream_configs = get_common_multi_stream_config(*devs)
    
    if len(stream_configs) < 2:
        log.w(f"Insufficient common streams found ({len(stream_configs)})")
        log.w("At least 2 stream types needed for multi-stream test")
        test.check(False, "Devices should support at least 2 common stream types")
    else:
        log.i(f"\nFound {len(stream_configs)} common stream types")
        log.i(f"Will stream all of them simultaneously from all {device_count} devices")
        
        # Run the multi-stream test
        success, drop_percentages, stats = stream_multi_and_check_frames(
            *devs, stream_configs=stream_configs
        )
        
        # Print detailed results
        log.i("\n" + "=" * 80)
        log.i("RESULTS:")
        log.i("=" * 80)
        log.i(f"Duration: {stats['duration']:.2f} seconds")
        
        for i, dev_stats in enumerate(stats['devices'], 1):
            log.i(f"\nDevice {i} ({dev_stats['name']}):")
            log.i(f"  Total framesets: {dev_stats['framesets']}")
            log.i(f"  Overall drop rate: {dev_stats['drop_pct']:.2f}%")
            for stream_type, stream_stats in dev_stats['streams'].items():
                log.i(f"  {stream_type}:")
                log.i(f"    Received: {stream_stats['received']}/{stream_stats['expected']}")
                log.i(f"    Dropped: {stream_stats['dropped']} ({stream_stats['drop_pct']:.2f}%)")
        
        log.i("=" * 80)
        
        if success:
            log.i(f"\n✓ PASS - Multi-stream test successful!")
            for i, drop_pct in enumerate(drop_percentages, 1):
                log.i(f"  Device {i} drop rate: {drop_pct:.2f}%")
        else:
            log.w(f"\n✗ FAIL - Excessive frame drops detected!")
            for i, drop_pct in enumerate(drop_percentages, 1):
                log.w(f"  Device {i} drop rate: {drop_pct:.2f}% (max: {MAX_FRAME_DROP_PERCENTAGE}%)")
        
        test.check(success, 
                    f"Multi-stream operation should have <{MAX_FRAME_DROP_PERCENTAGE}% drops on all devices")
        
        # Verify stream independence: Check that each stream type received adequate frames
        # (at least 80% of expected for a 10-second test at 30fps = ~240 frames)
        log.i("\nVerifying stream independence...")
        min_expected_frames = int(STREAM_DURATION_SEC * 30 * 0.8)
        all_streams_ok = True
        
        for i, dev_stats in enumerate(stats['devices'], 1):
            for stream_type, stream_stats in dev_stats['streams'].items():
                if stream_stats['received'] < min_expected_frames:
                    log.w(f"Device {i} {stream_type} received only {stream_stats['received']} frames (expected ~{min_expected_frames})")
                    all_streams_ok = False
        
        if all_streams_ok:
            log.i("✓ All streams received adequate frame counts (independence verified)")
        else:
            log.w("✗ Some streams received fewer frames than expected")
        
        test.check(all_streams_ok, 
                    "All streams should receive frames independently without interference")

# Print test summary
test.print_results_and_exit()
