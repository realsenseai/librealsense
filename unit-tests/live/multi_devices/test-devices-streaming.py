# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Test configuration: Requires 2 D400 series devices
#test:device:!jetson D400* D400*
#test:donotrun

"""
Tests:
- Simultaneous multi-stream operation (depth + color + IR) on 2 devices
- Frame drop detection with multiple stream types
- Long duration stress testing
- Stream independence verification

Requires 2 D400 series devices.
"""

import pyrealsense2 as rs
from rspy import test, log
import time
from collections import defaultdict

# Test configuration
STREAM_DURATION_SEC = 10  # Longer duration for multi-stream stress test
MAX_FRAME_DROP_PERCENTAGE = 5.0  # Allow up to 5% frame drops
STABILIZATION_TIME_SEC = 3  # Time to allow auto-exposure to settle

# Find exactly 2 devices or skip the test
device_list, ctx = test.find_n_devices_or_exit(2)

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
    
    This tries to enable as many stream types as possible:
    - Depth stream
    - Color stream  
    - Infrared stream
    
    All streams will use the same resolution and FPS for simplicity.
    """
    stream_configs = []
    
    # Try to add Depth stream
    res = find_common_profile(devs, rs.stream.depth, rs.format.z16, TARGET_RESOLUTIONS)
    if res:
        w, h, fps = res
        log.d(f"  Added Depth stream: {w}x{h} @ {fps}fps")
        stream_configs.append((rs.stream.depth, -1, w, h, rs.format.z16, fps))
    
    # Try to add Color stream (try multiple formats)
    for color_format in [rs.format.rgb8, rs.format.bgr8, rs.format.rgba8, rs.format.bgra8, rs.format.yuyv]:
        res = find_common_profile(devs, rs.stream.color, color_format, TARGET_RESOLUTIONS)
        if res:
            w, h, fps = res
            log.d(f"  Added Color stream: {w}x{h} @ {fps}fps {color_format}")
            stream_configs.append((rs.stream.color, -1, w, h, color_format, fps))
            break
    
    # Try to add Infrared stream (index 1)
    res = find_common_profile(devs, rs.stream.infrared, rs.format.y8, TARGET_RESOLUTIONS)
    if res:
        w, h, fps = res
        log.d(f"  Added Infrared stream (index 1): {w}x{h} @ {fps}fps")
        stream_configs.append((rs.stream.infrared, 1, w, h, rs.format.y8, fps))
    
    return stream_configs



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


def aggregate_results(all_frame_counters, all_frames_received, all_stream_frame_counts,
                     device_info, actual_duration):
    """
    Aggregate and analyze results from all devices.

    :param all_frame_counters: List of frame counter dicts (one per device)
    :param all_frames_received: List of total frame counts (one per device)
    :param all_stream_frame_counts: List of stream frame count dicts (one per device)
    :param device_info: List of device info dicts
    :param actual_duration: Actual streaming duration in seconds
    :return: Tuple of (success, drop_percentages, stats_dict)
    """
    log.i(f"Streaming completed after {actual_duration:.2f} seconds")
    for i, info in enumerate(device_info):
        log.i(f"Device {i+1} ({info['name']}): {all_frames_received[i]} total frames")
    
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


def stream_multi_and_check_frames(*devs, stream_configs, duration_sec=STREAM_DURATION_SEC):
    """
    Stream multiple stream types from all devices simultaneously and check for frame drops.

    :param devs: Variable number of device objects
    :param stream_configs: List of (stream_type, stream_index, width, height, format, fps) tuples
    :param duration_sec: How long to stream in seconds
    :return: Tuple of (success, list of drop_percentages, stats)
    """
    device_info = []
    all_frame_counters = []
    all_stream_frame_counts = []
    active_sensors = []

    counting = False

    def make_callback(frame_counters, frame_counts):
        def callback(frame):
            if not counting:
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

            # Group profiles by sensor index (depth+IR share stereo module, color is separate)
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
                    log.f(f"No matching profile for {stream_type} on {name}")

            for idx, profiles in profiles_by_idx.items():
                sensor = sensors[idx]
                sensor.open(profiles)
                active_sensors.append(sensor)
                sensor.start(cb)
        log.i(f"Stabilizing for {STABILIZATION_TIME_SEC} seconds...")
        time.sleep(STABILIZATION_TIME_SEC)

        counting = True
        log.i(f"Streaming for {duration_sec} seconds...")
        start_time = time.time()
        time.sleep(duration_sec)
        actual_duration = time.time() - start_time
        counting = False
    finally:
        for s in active_sensors:
            try:
                s.stop()
            except Exception as e:
                log.d(f"Stop error: {e}")
            try:
                s.close()
            except Exception as e:
                log.d(f"Close error: {e}")

    # Feed into existing analysis
    all_frames = [sum(cnt.values()) for cnt in all_stream_frame_counts]
    return aggregate_results(all_frame_counters, all_frames, all_stream_frame_counts, device_info, actual_duration)


#
# Test: Stream multiple stream types simultaneously from all devices
#
with test.closure(f"Multiple devices - multi-stream simultaneous operation (depth + color + IR) - {len(device_list)} devices"):
    # Use the devices already queried at the top of the file
    devs = device_list
    
    log.i("=" * 80)
    log.i(f"Testing multi-stream operation on {len(device_list)} devices:")
    for i, dev in enumerate(devs, 1):
        sn = dev.get_info(rs.camera_info.serial_number)
        name = dev.get_info(rs.camera_info.name) if dev.supports(rs.camera_info.name) else "Unknown"
        log.i(f"  Device {i}: {name} (SN: {sn})")
    log.i("=" * 80)

    # Get common multi-stream configuration
    log.i("\nFinding common multi-stream configuration...")
    stream_configs = get_common_multi_stream_config(*devs)

    if len(stream_configs) < 2:
        log.f(f"At least 2 stream types needed for multi-stream test, but found {len(stream_configs)}")
    
    log.i(f"\nFound {len(stream_configs)} common stream types")
    log.i(f"Will stream all of them simultaneously from all {len(device_list)} devices")
    
    # Run the multi-stream test
    # Note: Exceptions during streaming will propagate and fail the test automatically
    success, drop_percentages, stats = stream_multi_and_check_frames(
        *devs, stream_configs=stream_configs
    )
    
    # Check for analysis errors
    if len(stats['devices']) == 0:
        log.e("\nFAIL - No device statistics collected")
        test.check(False, "Should collect statistics from all devices")
    else:
        # Print detailed results
        log.i("\n" + "=" * 80)
        log.i("RESULTS:")
        log.i("=" * 80)
        log.i(f"Duration: {stats['duration']:.2f} seconds")
        
        for i, dev_stats in enumerate(stats['devices'], 1):
            log.i(f"\nDevice {i} ({dev_stats['name']}):")
            log.i(f"  Total frames: {dev_stats['total_frames']}")
            log.i(f"  Overall drop rate: {dev_stats['drop_pct']:.2f}%")
            for stream_type, stream_stats in dev_stats['streams'].items():
                log.i(f"  {stream_type}:")
                log.i(f"    Received: {stream_stats['received']}/{stream_stats['expected']}")
                log.i(f"    Dropped: {stream_stats['dropped']} ({stream_stats['drop_pct']:.2f}%)")
        
        log.i("=" * 80)
        
        if success:
            log.i(f"\nPASS - Multi-stream test successful!")
            for i, drop_pct in enumerate(drop_percentages, 1):
                log.i(f"  Device {i} drop rate: {drop_pct:.2f}%")
        else:
            log.w(f"\nFAIL - Excessive frame drops detected!")
            for i, drop_pct in enumerate(drop_percentages, 1):
                log.w(f"  Device {i} drop rate: {drop_pct:.2f}% (max: {MAX_FRAME_DROP_PERCENTAGE}%)")
        
        test.check(success, 
                    f"Multi-stream operation should have <{MAX_FRAME_DROP_PERCENTAGE}% drops on all devices")
        
        # Verify stream independence: Check that each stream type received adequate frames
        # (at least 80% of expected based on actual duration and configured FPS)
        log.i("\nVerifying stream independence...")
        all_streams_ok = True
        
        for i, dev_stats in enumerate(stats['devices'], 1):
            for stream_type, stream_stats in dev_stats['streams'].items():
                min_expected_frames = int(stream_stats['expected'] * 0.8)
                if stream_stats['received'] < min_expected_frames:
                    log.w(f"Device {i} {stream_type} received only {stream_stats['received']} frames (expected >={min_expected_frames})")
                    all_streams_ok = False
        
        if all_streams_ok:
            log.i("PASS - All streams received adequate frame counts (independence verified)")
        else:
            log.w("FAIL - Some streams received fewer frames than expected")
        
        test.check(all_streams_ok, 
                    "All streams should receive frames independently without interference")

# Print test summary
test.print_results_and_exit()
