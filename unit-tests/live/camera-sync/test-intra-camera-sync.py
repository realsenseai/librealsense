# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# test:device D400* !D405

"""
RealSense Intra-Camera Synchronization Validation Test

Tests timestamp synchronization between depth and color sensors within the SAME camera
when depth sensor operates in inter_cam_sync_mode=MASTER (mode 1). Validates that frame
timestamps from both sensors remain synchronized within a 3ms tolerance.

This test verifies that depth and RGB sensors on a single device produce synchronized
timestamps when global time is enabled on both sensors.

Requires: D400 camera with firmware >= 5.15.0.0 and color sensor support
"""

import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy import test, log
from rspy import tests_wrapper as tw
import time
import threading
from collections import deque

# Test configuration
MASTER = 1.0
DEFAULT = 0.0
SYNC_GAP_THRESHOLD_MS = 3.0  # 3ms threshold (global timestamps are in milliseconds)
TEST_DURATION = 5.0  # seconds
STABILIZATION_TIME = 2.0  # seconds - allow streams to stabilize
TARGET_RESOLUTION = (640, 480)
TARGET_FPS = 30
MIN_FRAME_THRESHOLD = 0.8  # Minimum frame count threshold (80% of expected frames)
MAX_FRAME_DROP_THRESHOLD = 0.05  # Maximum acceptable frame drop ratio (5%)

# Initialize device and sensors
device, _ = test.find_first_device_or_exit()
depth_sensor = device.first_depth_sensor()

# Check for color sensor availability
try:
    color_sensor = device.first_color_sensor()
except RuntimeError:
    log.i("Color sensor not available on this device, skipping test...")
    test.print_results_and_exit()

# Check firmware version
fw_version = rsutils.version(device.get_info(rs.camera_info.firmware_version))
if fw_version < rsutils.version(5,15,0,0):
    log.i(f"Firmware version {fw_version} < 5.15.0.0, skipping test...")
    test.print_results_and_exit()

# Save original option values for cleanup
original_sync_mode = None
original_depth_global_time = None
original_color_global_time = None

try:
    if depth_sensor.supports(rs.option.inter_cam_sync_mode):
        original_sync_mode = depth_sensor.get_option(rs.option.inter_cam_sync_mode)
    if depth_sensor.supports(rs.option.global_time_enabled):
        original_depth_global_time = depth_sensor.get_option(rs.option.global_time_enabled)
    if color_sensor.supports(rs.option.global_time_enabled):
        original_color_global_time = color_sensor.get_option(rs.option.global_time_enabled)
except Exception as e:
    log.w(f"Could not save original option values: {e}")

tw.start_wrapper(device)

################################################################################################
# Frame Collection Classes
################################################################################################

class FrameTimestampCollector:
    """Thread-safe collector for frame timestamps using global time domain."""
    
    def __init__(self, stream_name):
        self.stream_name = stream_name
        self.frames = deque()
        self.lock = threading.Lock()
        self.domain_errors = 0
        
    def callback(self, frame):
        """Callback function to collect frame timestamps (global time)."""
        try:
            # Use global timestamp (frame.timestamp) which is synchronized to host time
            global_timestamp = frame.timestamp  # in milliseconds
            frame_number = frame.get_frame_number()
            timestamp_domain = frame.get_frame_timestamp_domain()
            
            # Metadata may not be available; ignore and keep hw_timestamp as None
            hw_timestamp = None
            try:
                hw_timestamp = frame.get_frame_metadata(rs.frame_metadata_value.frame_timestamp)
            except RuntimeError:
                pass
            
            # Verify we're getting global time domain
            if timestamp_domain != rs.timestamp_domain.global_time:
                with self.lock:
                    self.domain_errors += 1
            
            with self.lock:
                self.frames.append({
                    'global_timestamp': global_timestamp,
                    'hw_timestamp': hw_timestamp,
                    'frame_number': frame_number,
                    'timestamp_domain': timestamp_domain
                })
        except Exception as e:
            log.w(f"Error in {self.stream_name} callback: {e}")
    
    def get_frames(self):
        """Get collected frames as a list."""
        with self.lock:
            return list(self.frames)
    
    def clear_frames(self):
        """Clear collected frames."""
        with self.lock:
            self.frames.clear()
            self.domain_errors = 0
    
    def frame_count(self):
        """Get current frame count."""
        with self.lock:
            return len(self.frames)
    
    def get_domain_errors(self):
        """Get count of frames with wrong timestamp domain."""
        with self.lock:
            return self.domain_errors

################################################################################################
# Helper Functions
################################################################################################

def enable_global_time(sensor, sensor_name):
    """Enable global time on a sensor if supported."""
    if sensor.supports(rs.option.global_time_enabled):
        sensor.set_option(rs.option.global_time_enabled, 1)
        enabled = sensor.get_option(rs.option.global_time_enabled)
        if enabled != 1:
            log.w(f"Failed to enable global time on {sensor_name}")
            return False
        log.i(f"Global time enabled on {sensor_name}")
        return True
    else:
        log.w(f"{sensor_name} does not support global time option")
        return False

def find_matching_profiles(depth_sensor, color_sensor, resolution=TARGET_RESOLUTION, fps=TARGET_FPS):
    """Find matching stream profiles for depth and color sensors."""
    
    # Find depth profile
    depth_profile = None
    for profile in depth_sensor.profiles:
        if (profile.stream_type() == rs.stream.depth and
            profile.format() == rs.format.z16 and
            profile.fps() == fps and
            profile.as_video_stream_profile().width() == resolution[0] and
            profile.as_video_stream_profile().height() == resolution[1]):
            depth_profile = profile
            break
    
    # Find color profile
    color_profile = None
    for profile in color_sensor.profiles:
        if (profile.stream_type() == rs.stream.color and
            profile.fps() == fps and
            profile.as_video_stream_profile().width() == resolution[0] and
            profile.as_video_stream_profile().height() == resolution[1]):
            color_profile = profile
            break
    
    return depth_profile, color_profile

def analyze_timestamp_synchronization(depth_frames, color_frames, threshold_ms=SYNC_GAP_THRESHOLD_MS):
    """Analyze timestamp synchronization between depth and color frames using global timestamps."""
    
    if len(depth_frames) == 0 or len(color_frames) == 0:
        return {
            'success': False,
            'error': 'No frames collected',
            'max_gap': 0,
            'avg_gap': 0,
            'sync_percentage': 0,
            'total_pairs': 0
        }
    
    # Align frames by closest global timestamps
    aligned_pairs = []
    max_gap = 0
    gaps = []
    
    for depth_frame in depth_frames:
        depth_ts = depth_frame['global_timestamp']
        
        # Find closest color frame
        closest_color_frame = min(color_frames, 
                                key=lambda cf: abs(cf['global_timestamp'] - depth_ts))
        color_ts = closest_color_frame['global_timestamp']
        
        gap = abs(depth_ts - color_ts)
        gaps.append(gap)
        max_gap = max(max_gap, gap)
        
        aligned_pairs.append({
            'depth_ts': depth_ts,
            'color_ts': color_ts,
            'gap': gap,
            'depth_frame_num': depth_frame['frame_number'],
            'color_frame_num': closest_color_frame['frame_number'],
            'depth_hw_ts': depth_frame.get('hw_timestamp'),
            'color_hw_ts': closest_color_frame.get('hw_timestamp')
        })
    
    # Calculate synchronization statistics
    synced_pairs = [pair for pair in aligned_pairs if pair['gap'] <= threshold_ms]
    sync_percentage = (len(synced_pairs) / len(aligned_pairs)) * 100 if aligned_pairs else 0
    avg_gap = sum(gaps) / len(gaps) if gaps else 0
    
    # Calculate 95th percentile gap (more robust than max which can be affected by outliers)
    sorted_gaps = sorted(gaps)
    p95_index = int(len(sorted_gaps) * 0.95)
    p95_gap = sorted_gaps[p95_index] if sorted_gaps else 0
    
    return {
        'success': True,
        'max_gap': max_gap,
        'p95_gap': p95_gap,
        'avg_gap': avg_gap,
        'sync_percentage': sync_percentage,
        'total_pairs': len(aligned_pairs),
        'synced_pairs': len(synced_pairs),
        'aligned_pairs': aligned_pairs[:10]  # First 10 pairs for logging
    }

################################################################################################
# Test Cases
################################################################################################

test.start("Intra-camera depth-color synchronization with MASTER sync mode")

# Setup frame collectors
depth_collector = FrameTimestampCollector("depth")
color_collector = FrameTimestampCollector("color")

try:
    # Find matching profiles
    depth_profile, color_profile = find_matching_profiles(depth_sensor, color_sensor)
    
    if not depth_profile or not color_profile:
        test.fail("Could not find matching profiles for depth and color streams")
    
    log.i(f"Using profiles: Depth {TARGET_RESOLUTION[0]}x{TARGET_RESOLUTION[1]}@{TARGET_FPS}fps, "
          f"Color {TARGET_RESOLUTION[0]}x{TARGET_RESOLUTION[1]}@{TARGET_FPS}fps")
    
    # Enable global time on both sensors for synchronized timestamps
    log.i("Enabling global time on both sensors...")
    depth_global_ok = enable_global_time(depth_sensor, "depth sensor")
    color_global_ok = enable_global_time(color_sensor, "color sensor")
    
    if not depth_global_ok or not color_global_ok:
        log.w("Global time not fully enabled on both sensors, test may be less accurate")
    
    # Set device to MASTER sync mode (inter_cam_sync_mode = 1)
    log.i("Setting depth sensor to MASTER sync mode (inter_cam_sync_mode=1)...")
    depth_sensor.set_option(rs.option.inter_cam_sync_mode, MASTER)
    test.check_equal(depth_sensor.get_option(rs.option.inter_cam_sync_mode), MASTER)
    
    # Configure and start streaming
    depth_sensor.open(depth_profile)
    color_sensor.open(color_profile)
    
    depth_sensor.start(depth_collector.callback)
    color_sensor.start(color_collector.callback)
    
    log.i("Started intra-camera multi-stream capture")
    
    # Allow streams to stabilize (longer stabilization for better results)
    log.i(f"Stabilization period: {STABILIZATION_TIME} seconds...")
    time.sleep(STABILIZATION_TIME)
    depth_collector.clear_frames()
    color_collector.clear_frames()
    
    # Collect synchronized data
    log.i(f"Collecting synchronized frames for {TEST_DURATION} seconds...")
    time.sleep(TEST_DURATION)
    
    # Stop streaming
    depth_sensor.stop()
    color_sensor.stop()
    depth_sensor.close()
    color_sensor.close()
    
    # Get collected frames
    depth_frames = depth_collector.get_frames()
    color_frames = color_collector.get_frames()
    
    log.i(f"Collected {len(depth_frames)} depth frames and {len(color_frames)} color frames")
    
    # Check for timestamp domain errors
    depth_domain_errors = depth_collector.get_domain_errors()
    color_domain_errors = color_collector.get_domain_errors()
    if depth_domain_errors > 0 or color_domain_errors > 0:
        log.w(f"Timestamp domain errors - Depth: {depth_domain_errors}, Color: {color_domain_errors}")
    
    # Validate timestamp domain on collected frames
    if depth_frames:
        first_depth_domain = depth_frames[0].get('timestamp_domain')
        test.check(first_depth_domain == rs.timestamp_domain.global_time,
                   f"Depth frames should have global_time domain (got {first_depth_domain})")
    if color_frames:
        first_color_domain = color_frames[0].get('timestamp_domain')
        test.check(first_color_domain == rs.timestamp_domain.global_time,
                   f"Color frames should have global_time domain (got {first_color_domain})")
    
    # Validate minimum frame count
    expected_min_frames = int(TEST_DURATION * TARGET_FPS * MIN_FRAME_THRESHOLD)
    test.check(len(depth_frames) >= expected_min_frames, 
               f"Sufficient depth frames collected ({len(depth_frames)} >= {expected_min_frames})")
    test.check(len(color_frames) >= expected_min_frames, 
               f"Sufficient color frames collected ({len(color_frames)} >= {expected_min_frames})")
    
    # Analyze timestamp synchronization
    sync_results = analyze_timestamp_synchronization(depth_frames, color_frames)
    
    if not sync_results['success']:
        test.fail(f"Synchronization analysis failed: {sync_results['error']}")
    
    # Log synchronization statistics
    log.i(f"Synchronization analysis results:")
    log.i(f"  Max timestamp gap: {sync_results['max_gap']:.3f} ms")
    log.i(f"  95th percentile gap: {sync_results['p95_gap']:.3f} ms")
    log.i(f"  Average timestamp gap: {sync_results['avg_gap']:.3f} ms")
    log.i(f"  Sync percentage: {sync_results['sync_percentage']:.1f}%")
    log.i(f"  Total frame pairs: {sync_results['total_pairs']}")
    log.i(f"  Synced pairs (within {SYNC_GAP_THRESHOLD_MS}ms): {sync_results['synced_pairs']}")
    
    # Log first few aligned pairs for debugging
    log.i("First few aligned frame pairs:")
    for i, pair in enumerate(sync_results['aligned_pairs']):
        hw_info = ""
        if pair.get('depth_hw_ts') and pair.get('color_hw_ts'):
            # Hardware timestamps are reported in microseconds; convert to milliseconds for logging
            hw_gap = abs(pair['depth_hw_ts'] - pair['color_hw_ts']) / 1000.0
            hw_info = f", HW gap: {hw_gap:.3f}ms"
        log.i(f"  Pair {i+1}: Gap {pair['gap']:.3f}ms, "
              f"Depth#{pair['depth_frame_num']}, Color#{pair['color_frame_num']}{hw_info}")
    
    # Validate synchronization requirements
    # Use 95th percentile instead of max to be robust against occasional outliers
    test.check(sync_results['p95_gap'] <= SYNC_GAP_THRESHOLD_MS, 
               f"95th percentile timestamp gap {sync_results['p95_gap']:.3f}ms <= {SYNC_GAP_THRESHOLD_MS}ms threshold")
    
    if sync_results['max_gap'] > SYNC_GAP_THRESHOLD_MS:
        log.w(f"Note: Max gap ({sync_results['max_gap']:.3f}ms) exceeds threshold but p95 is within bounds")
    
    test.check(sync_results['sync_percentage'] >= 95.0, 
               f"Synchronization percentage {sync_results['sync_percentage']:.1f}% >= 95%")
    
    # Validate frame continuity per sensor (sensors have independent frame counters)
    # Check that frames are incrementing properly within each sensor's stream
    depth_frame_nums = [f['frame_number'] for f in depth_frames]
    color_frame_nums = [f['frame_number'] for f in color_frames]
    
    def check_frame_continuity(frame_nums, sensor_name):
        """Check that frame numbers are incrementing (allowing for occasional drops)."""
        if len(frame_nums) < 2:
            return True, 0
        drops = 0
        for i in range(1, len(frame_nums)):
            diff = frame_nums[i] - frame_nums[i-1]
            if diff != 1:
                drops += 1
        return drops <= len(frame_nums) * MAX_FRAME_DROP_THRESHOLD, drops
    
    depth_continuous, depth_drops = check_frame_continuity(depth_frame_nums, "depth")
    color_continuous, color_drops = check_frame_continuity(color_frame_nums, "color")
    
    log.i(f"Frame continuity analysis:")
    log.i(f"  Depth sensor: {len(depth_frames)} frames, {depth_drops} discontinuities")
    log.i(f"  Color sensor: {len(color_frames)} frames, {color_drops} discontinuities")
    
    test.check(depth_continuous, 
               f"Depth frame continuity: {depth_drops} drops <= 5% of {len(depth_frames)} frames")
    test.check(color_continuous, 
               f"Color frame continuity: {color_drops} drops <= 5% of {len(color_frames)} frames")

except Exception as e:
    test.fail(f"Intra-camera synchronization test failed: {e}")
    log.e(f"Exception: {e}")

finally:
    # Restore original option values
    try:
        if original_sync_mode is not None:
            depth_sensor.set_option(rs.option.inter_cam_sync_mode, original_sync_mode)
        if original_depth_global_time is not None and depth_sensor.supports(rs.option.global_time_enabled):
            depth_sensor.set_option(rs.option.global_time_enabled, original_depth_global_time)
        if original_color_global_time is not None and color_sensor.supports(rs.option.global_time_enabled):
            color_sensor.set_option(rs.option.global_time_enabled, original_color_global_time)
    except Exception as e:
        log.w(f"Error restoring original options: {e}")

test.finish()

################################################################################################
# Cleanup
################################################################################################

tw.stop_wrapper(device)
test.print_results_and_exit()
