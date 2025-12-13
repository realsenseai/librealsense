# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
# Tracked-On: RSDSO-20709

# test:device D400_CAM_SYNC
# test:donotrun:!weekly
# test:timeout 3600

# Test master-slave synchronization across different resolutions and frame rates
# Requirements:
#   - Two D400 cameras connected (D405 excluded as it lacks sync port)
#   - Cameras connected via sync cable
#   - FW version >= 5.15.0.0 for inter_cam_sync_mode support
# 
# Configuration:
#   - Tests all supported depth resolutions at all supported frame rates > 15 fps
#   - Each test includes hardware reset and calibration for clean state
#   - Each test runs for 10 seconds for quick validation
#   - MASTER_SLAVE_OFFSET_THRESHOLD_MAX: Maximum offset for MASTER-SLAVE mode (default: 50 us)
#
# Test flow:
#   For each supported resolution:
#     For each supported frame rate > 15 fps:
#       1. Hardware reset both cameras to clean state (timestamp counters reset to 0)
#       2. Configure both cameras for the target resolution and frame rate
#       3. Set cameras to MASTER-SLAVE mode
#       4. Calibrate slave HW timestamp vs master HW timestamp (specific to this resolution and FPS)
#       5. Stream for 10 seconds and collect timestamp data
#       6. Analyze synchronization quality (offset, standard deviation)
#       7. Verify offset < threshold
#
# Notes:
#   - Hardware timestamp is 32-bit (uint32_t) with microsecond resolution
#   - Hardware reset performed before each test ensures independent measurements
#   - Calibration performed separately for each resolution and frame rate to account for timing differences
#   - Frame alignment uses 30% of frame time as threshold (adjusts per FPS)
#   - Only frame rates > 15 fps tested (excludes 6 and 15 fps)
#   - Test runs weekly only due to extended duration
#   - All calculations use double precision (Python float is 64-bit IEEE 754)

import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy import test, log
import time
import statistics

# Configuration options
CALIBRATION_DURATION = 3.0  # Duration for calibration phase in seconds
SYNC_TEST_DURATION = 10.0  # Duration for sync test per frame rate
MASTER_SLAVE_OFFSET_THRESHOLD_MAX = 50.0  # Maximum offset in microseconds

# Find all connected D400 devices (excluding D405 which lacks sync port)
ctx = rs.context()
devices = ctx.query_devices()

valid_devices = []
for dev in devices:
    product_line = dev.get_info(rs.camera_info.product_line)
    name = dev.get_info(rs.camera_info.name)
    if product_line == "D400" and "D405" not in name:
        valid_devices.append(dev)

if len(valid_devices) < 2:
    log.i(f"Test requires 2 connected D400 cameras (excluding D405), found {len(valid_devices)}, skipping test...")
    test.print_results_and_exit()

master_device = valid_devices[0]
slave_device = valid_devices[1]

master_serial = master_device.get_info(rs.camera_info.serial_number)
slave_serial = slave_device.get_info(rs.camera_info.serial_number)

log.i(f"Master device: {master_device.get_info(rs.camera_info.name)} (SN: {master_serial})")
log.i(f"Slave device: {slave_device.get_info(rs.camera_info.name)} (SN: {slave_serial})")

master_sensor = master_device.first_depth_sensor()
slave_sensor = slave_device.first_depth_sensor()

# Check firmware version support
master_fw_version = rsutils.version(master_device.get_info(rs.camera_info.firmware_version))
slave_fw_version = rsutils.version(slave_device.get_info(rs.camera_info.firmware_version))

if master_fw_version < rsutils.version(5,15,0,0) or slave_fw_version < rsutils.version(5,15,0,0):
    log.i(f"FW version must be >= 5.15.0.0 for INTER_CAM_SYNC_MODE support, skipping test...")
    test.print_results_and_exit()

DEFAULT = 0.0
MASTER = 1.0
SLAVE = 2.0

################################################################################################
# Helper Functions
################################################################################################

def create_timestamp_callback(timestamp_list):
    """Create a callback function that captures hardware and system timestamps."""
    def callback(frame):
        depth_frame = None
        if frame.is_frameset():
            depth_frame = frame.as_frameset().get_depth_frame()
        elif frame.is_depth_frame():
            depth_frame = frame
        
        if depth_frame:
            hw_ts = depth_frame.get_frame_metadata(rs.frame_metadata_value.frame_timestamp)
            sys_ts = time.time() * 1e6  # Convert to microseconds
            timestamp_list.append((hw_ts, sys_ts))
    
    return callback

def calibrate_slave_to_master(master_sensor, slave_sensor, master_profile, slave_profile, duration):
    """Calibrate slave hardware timestamp against master hardware timestamp using linear regression."""
    master_timestamps = []
    slave_timestamps = []
    
    master_callback = create_timestamp_callback(master_timestamps)
    slave_callback = create_timestamp_callback(slave_timestamps)
    
    # Start streaming on both cameras
    master_sensor.open(master_profile)
    slave_sensor.open(slave_profile)
    master_sensor.start(master_callback)
    slave_sensor.start(slave_callback)
    
    log.i(f"Calibrating for {duration} seconds...")
    
    # Wait 1 second and discard initial frames to allow streams to stabilize
    time.sleep(1.0)
    master_timestamps.clear()
    slave_timestamps.clear()
    
    # Collect calibration data
    time.sleep(duration)
    
    master_sensor.stop()
    slave_sensor.stop()
    master_sensor.close()
    slave_sensor.close()
    
    if len(master_timestamps) < 10 or len(slave_timestamps) < 10:
        log.e(f"Insufficient frames for calibration: master={len(master_timestamps)}, slave={len(slave_timestamps)}")
        return None, None
    
    # Align frames based on system timestamps (within 33ms)
    frame_time_threshold = 33333.0  # microseconds
    aligned_pairs = []
    
    for master_hw_ts, master_sys_ts in master_timestamps:
        best_match = None
        min_time_diff = float('inf')
        
        for slave_hw_ts, slave_sys_ts in slave_timestamps:
            time_diff = abs(master_sys_ts - slave_sys_ts)
            if time_diff < min_time_diff:
                min_time_diff = time_diff
                best_match = slave_hw_ts
        
        if best_match is not None and min_time_diff <= frame_time_threshold:
            aligned_pairs.append((master_hw_ts, best_match))
    
    if len(aligned_pairs) < 10:
        log.e(f"Insufficient aligned frame pairs for calibration: {len(aligned_pairs)}")
        return None, None
    
    # Extract master and slave HW timestamps
    master_hw_ts_list = [float(master_hw) for master_hw, slave_hw in aligned_pairs]
    slave_hw_ts_list = [float(slave_hw) for master_hw, slave_hw in aligned_pairs]
    
    # Normalize data to avoid numerical precision issues
    master_ref = float(master_hw_ts_list[0])
    slave_ref = float(slave_hw_ts_list[0])
    
    master_norm = [float(m - master_ref) for m in master_hw_ts_list]
    slave_norm = [float(s - slave_ref) for s in slave_hw_ts_list]
    
    # Calculate linear regression: slave_norm = slope * master_norm + offset_norm
    n = float(len(aligned_pairs))
    sum_master = float(sum(master_norm))
    sum_slave = float(sum(slave_norm))
    sum_master_slave = float(sum(m * s for m, s in zip(master_norm, slave_norm)))
    sum_master_sq = float(sum(m * m for m in master_norm))
    
    # Linear regression formulas
    slope = float((n * sum_master_slave - sum_master * sum_slave) / (n * sum_master_sq - sum_master * sum_master))
    offset_norm = float((sum_slave - slope * sum_master) / n)
    
    # Convert back to original scale
    offset = float(slave_ref - slope * master_ref + offset_norm)
    
    log.i(f"Calibration complete: {len(aligned_pairs)} pairs, slope={slope:.6f}, offset={offset:.2f} us")
    
    return offset, slope

def test_sync_at_fps(master_sensor, slave_sensor, master_profile, slave_profile, fps, calib_offset, calib_slope):
    """Test synchronization quality at specific frame rate."""
    master_timestamps = []
    slave_timestamps = []
    
    master_callback = create_timestamp_callback(master_timestamps)
    slave_callback = create_timestamp_callback(slave_timestamps)
    
    # Start streaming
    master_sensor.open(master_profile)
    slave_sensor.open(slave_profile)
    master_sensor.start(master_callback)
    slave_sensor.start(slave_callback)
    
    # Wait 1 second then start measurement
    time.sleep(1.0)
    master_timestamps.clear()
    slave_timestamps.clear()
    
    # Collect frames
    time.sleep(SYNC_TEST_DURATION)
    
    master_sensor.stop()
    slave_sensor.stop()
    master_sensor.close()
    slave_sensor.close()
    
    log.i(f"Collected frames: master={len(master_timestamps)}, slave={len(slave_timestamps)}")
    
    # Align frames based on hardware timestamps
    frame_time_us = 1000000.0 / float(fps)  # Frame time in microseconds
    frame_time_threshold = frame_time_us * 0.3  # 30% of frame time
    
    log.i(f"Frame alignment threshold: {frame_time_threshold:.0f} us (30% of {frame_time_us:.0f} us frame time)")
    
    aligned_pairs = []
    
    # Transform all slave HW timestamps to master's domain for comparison
    slave_transformed = [(float((slave_hw - calib_offset) / calib_slope), slave_hw) 
                         for slave_hw, slave_sys in slave_timestamps]
    
    for master_hw_ts, master_sys_ts in master_timestamps:
        best_match = None
        min_time_diff = float('inf')
        best_slave_hw = None
        
        for slave_calib_hw, slave_hw_ts in slave_transformed:
            time_diff = abs(master_hw_ts - slave_calib_hw)
            if time_diff < min_time_diff:
                min_time_diff = time_diff
                best_match = slave_hw_ts
                best_slave_hw = slave_hw_ts
        
        if best_match is not None and min_time_diff <= frame_time_threshold:
            aligned_pairs.append((master_hw_ts, best_slave_hw, min_time_diff))
    
    log.i(f"Aligned {len(aligned_pairs)} frame pairs")
    
    if len(aligned_pairs) < 10:
        log.e(f"Insufficient aligned pairs: {len(aligned_pairs)}")
        return None, None, None
    
    # Extract aligned HW timestamps
    master_hw_timestamps = [float(master_hw) for master_hw, slave_hw, match_qual in aligned_pairs]
    slave_hw_timestamps = [float(slave_hw) for master_hw, slave_hw, match_qual in aligned_pairs]
    match_qualities = [match_qual for master_hw, slave_hw, match_qual in aligned_pairs]
    
    # Transform slave HW timestamps to master's domain
    slave_calib_hw_timestamps = [float((slave_hw - calib_offset) / calib_slope)
                                  for slave_hw in slave_hw_timestamps]
    
    # Calculate offsets
    offsets = [abs(master_hw - slave_calib_hw) 
               for master_hw, slave_calib_hw in zip(master_hw_timestamps, slave_calib_hw_timestamps)]
    
    # Remove outliers using IQR method
    if len(offsets) >= 4:
        sorted_offsets = sorted(offsets)
        q1_idx = len(sorted_offsets) // 4
        q3_idx = (3 * len(sorted_offsets)) // 4
        q1 = sorted_offsets[q1_idx]
        q3 = sorted_offsets[q3_idx]
        iqr = q3 - q1
        lower_bound = q1 - 1.5 * iqr
        upper_bound = q3 + 1.5 * iqr
        offsets = [o for o in offsets if lower_bound <= o <= upper_bound]
    
    if len(offsets) == 0:
        log.e("No valid offsets after outlier removal")
        return None, None, None
    
    median_offset = statistics.median(offsets)
    stdev_offset = statistics.stdev(offsets) if len(offsets) > 1 else 0
    avg_match_quality = sum(match_qualities) / len(match_qualities) if match_qualities else 0
    
    log.i(f"  Median offset: {median_offset:.2f} us")
    log.i(f"  Std deviation: {stdev_offset:.2f} us")
    log.i(f"  Avg match quality: {avg_match_quality:.2f} us")
    
    return median_offset, stdev_offset, avg_match_quality

################################################################################################
# Test Execution
################################################################################################

test.start("Perform hardware reset on both devices")
log.i("Resetting master device...")
master_device.hardware_reset()
log.i("Resetting slave device...")
slave_device.hardware_reset()

# Wait for devices to come back online after reset
log.i("Waiting for devices to reinitialize...")
time.sleep(5.0)

# Re-query devices after reset
ctx = rs.context()
devices = ctx.query_devices()

valid_devices = []
for dev in devices:
    product_line = dev.get_info(rs.camera_info.product_line)
    name = dev.get_info(rs.camera_info.name)
    if product_line == "D400" and "D405" not in name:
        valid_devices.append(dev)

if len(valid_devices) < 2:
    log.e(f"After reset, only {len(valid_devices)} devices found, expected 2")
    test.check(False, "Devices not found after reset")
    test.print_results_and_exit()

# Re-assign devices by serial number to maintain master/slave roles
master_device = None
slave_device = None
for dev in valid_devices:
    sn = dev.get_info(rs.camera_info.serial_number)
    if sn == master_serial:
        master_device = dev
    elif sn == slave_serial:
        slave_device = dev

if not master_device or not slave_device:
    log.e("Could not find master or slave device after reset")
    test.check(False, "Device assignment failed after reset")
    test.print_results_and_exit()

master_sensor = master_device.first_depth_sensor()
slave_sensor = slave_device.first_depth_sensor()

log.i(f"Devices reinitialized successfully")
test.finish()

################################################################################################

test.start("Discover all supported depth resolutions and frame rates")

# Find all resolutions supported by both cameras
master_resolutions = {}
for profile in master_sensor.profiles:
    if profile.stream_type() == rs.stream.depth:
        vsp = profile.as_video_stream_profile()
        res = (vsp.width(), vsp.height())
        if res not in master_resolutions:
            master_resolutions[res] = set()
        master_resolutions[res].add(profile.fps())

slave_resolutions = {}
for profile in slave_sensor.profiles:
    if profile.stream_type() == rs.stream.depth:
        vsp = profile.as_video_stream_profile()
        res = (vsp.width(), vsp.height())
        if res not in slave_resolutions:
            slave_resolutions[res] = set()
        slave_resolutions[res].add(profile.fps())

# Find common resolutions
common_resolutions = sorted(list(set(master_resolutions.keys()) & set(slave_resolutions.keys())))

if len(common_resolutions) == 0:
    log.e("No common depth resolutions found on both cameras")
    test.check(False, "No common resolutions available")
    test.print_results_and_exit()

log.i(f"Found {len(common_resolutions)} common resolutions: {common_resolutions}")
test.finish()

################################################################################################

# Test each resolution and frame rate combination
for res in common_resolutions:
    width, height = res
    
    # Find common frame rates for this resolution
    common_fps = sorted(list(master_resolutions[res] & slave_resolutions[res]))
    
    # Only test frame rates > 15 fps
    common_fps = [fps for fps in common_fps if fps > 15]
    
    if len(common_fps) == 0:
        log.i(f"No frame rates > 15 fps found for {width}x{height}, skipping resolution")
        continue
    
    log.i(f"Testing {width}x{height} at {len(common_fps)} frame rates: {common_fps} fps")
    
    for fps in common_fps:
        test.start(f"Test synchronization at {width}x{height} @ {fps}fps")
        
        # Hardware reset before each test to ensure clean state
        log.i(f"Resetting devices for {width}x{height}@{fps}fps test...")
        master_device.hardware_reset()
        slave_device.hardware_reset()
        time.sleep(5.0)
        
        # Re-query devices after reset
        ctx = rs.context()
        devices = ctx.query_devices()
        
        valid_devices = []
        for dev in devices:
            product_line = dev.get_info(rs.camera_info.product_line)
            name = dev.get_info(rs.camera_info.name)
            if product_line == "D400" and "D405" not in name:
                valid_devices.append(dev)
        
        if len(valid_devices) < 2:
            log.e(f"After reset, only {len(valid_devices)} devices found for {width}x{height}@{fps}fps test")
            test.check(False, f"Devices not found after reset for {width}x{height}@{fps}fps test")
            test.finish()
            continue
        
        # Re-assign devices by serial number
        master_device = None
        slave_device = None
        for dev in valid_devices:
            sn = dev.get_info(rs.camera_info.serial_number)
            if sn == master_serial:
                master_device = dev
            elif sn == slave_serial:
                slave_device = dev
        
        if not master_device or not slave_device:
            log.e(f"Could not find devices after reset for {width}x{height}@{fps}fps test")
            test.check(False, f"Device assignment failed for {width}x{height}@{fps}fps test")
            test.finish()
            continue
        
        master_sensor = master_device.first_depth_sensor()
        slave_sensor = slave_device.first_depth_sensor()
        
        # Get profiles for this resolution and frame rate
        master_profile = next((p for p in master_sensor.profiles 
                               if p.stream_type() == rs.stream.depth 
                               and p.as_video_stream_profile().width() == width
                               and p.as_video_stream_profile().height() == height
                               and p.fps() == fps), None)
        
        slave_profile = next((p for p in slave_sensor.profiles 
                              if p.stream_type() == rs.stream.depth 
                              and p.as_video_stream_profile().width() == width
                              and p.as_video_stream_profile().height() == height
                              and p.fps() == fps), None)
        
        if not master_profile or not slave_profile:
            log.e(f"Could not find {width}x{height}@{fps}fps profile on both cameras")
            test.check(False, f"Profile not available for {width}x{height}@{fps}fps")
            test.finish()
            test.finish()
            continue
        
        # Configure master-slave mode
        master_sensor.set_option(rs.option.inter_cam_sync_mode, MASTER)
        slave_sensor.set_option(rs.option.inter_cam_sync_mode, SLAVE)
        
        # Calibrate for this specific resolution and fps
        calib_offset, calib_slope = calibrate_slave_to_master(master_sensor, slave_sensor, 
                                                              master_profile, slave_profile, 
                                                              CALIBRATION_DURATION)
        
        if calib_offset is None or calib_slope is None:
            log.e(f"Calibration failed for {width}x{height}@{fps}fps")
            test.check(False, f"Calibration failed for {width}x{height}@{fps}fps")
            test.finish()
            continue
        
        # Test synchronization
        median_offset, stdev_offset, avg_match_quality = test_sync_at_fps(
            master_sensor, slave_sensor, master_profile, slave_profile, 
            fps, calib_offset, calib_slope)
        
        if median_offset is None:
            log.e(f"Synchronization test failed for {width}x{height}@{fps}fps")
            test.check(False, f"Synchronization test failed for {width}x{height}@{fps}fps")
        else:
            # Verify offset threshold
            test.check(median_offset < MASTER_SLAVE_OFFSET_THRESHOLD_MAX,
                      f"{width}x{height}@{fps}fps: Median offset should be < {MASTER_SLAVE_OFFSET_THRESHOLD_MAX} us, got {median_offset:.2f} us")
            
            log.i(f"✓ {width}x{height}@{fps}fps: offset={median_offset:.2f}±{stdev_offset:.2f} us, match_quality={avg_match_quality:.2f} us")
        
        test.finish()

################################################################################################
test.print_results_and_exit()
