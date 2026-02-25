# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# test:device D400_CAM_SYNC
# test:donotrun:!sync_test
# test:timeout 300

# Test master-slave synchronization with calibrated frame time
# Requirements:
#   - Two D400 cameras connected (D405 excluded as it lacks sync port)
#   - Cameras connected via sync cable
#   - FW version >= 5.15.0.0 for inter_cam_sync_mode support
# 
# Configuration:
#   - CALIBRATION_DURATION: Duration for calibration phase (default: 5 seconds)
#   - DRIFT_TEST_DURATION: Duration in seconds for drift measurement (default: 90 seconds)
#   - ENABLE_DRIFT_PLOT: Enable matplotlib plotting of drift analysis (default: False)
#   - MASTER_SLAVE_OFFSET_THRESHOLD_MAX: Maximum first segment offset for MASTER-SLAVE mode (default: 20 us)
#   - MASTER_SLAVE_DRIFT_RATE_THRESHOLD_MAX: Maximum drift rate for MASTER-SLAVE mode (default: 20 us/minute)
#   - DEFAULT_OFFSET_THRESHOLD_MIN: Minimum first segment offset for DEFAULT mode (default: 100 us)
#   - DEFAULT_DRIFT_RATE_THRESHOLD_MIN: Minimum drift rate for DEFAULT mode (default: 100 us/minute)
#
# Test flow:
#   1. Hardware reset both cameras to clean state -> hardware timestamp counters reset to 0
#   2. Test MASTER-SLAVE mode:
#      - Configure one camera as MASTER, other as SLAVE
#      - Calibration phase (slave HW timestamp vs master HW timestamp in MASTER-SLAVE mode):
#        * Discard first 1 second of frames to allow streams to stabilize
#        * Collect frames simultaneously from both cameras
#        * Align frames based on system timestamps (within 33ms)
#        * Use linear regression to find: slave_hw_ts = slope * master_hw_ts + offset
#        * This captures the relationship between the two hardware clocks
#      - Stream synchronized depth frames at 640x480@30fps
#      - Transform slave HW timestamps to master's domain: slave_in_master_domain = (slave_hw_ts - offset) / slope
#      - Analyze drift as difference between master HW timestamp and transformed slave timestamp
#   3. Test DEFAULT mode:
#      - Configure both cameras to DEFAULT sync mode
#      - Use same calibration from MASTER-SLAVE mode
#      - Perform same data collection and analysis with transformed timestamps
#   4. Compare MASTER-SLAVE vs DEFAULT modes:
#      - Generate 2-panel comparison plot (HW Timestamp Offset and Drift Rate)
#      - Validate MASTER-SLAVE mode meets synchronization thresholds
#      - Validate DEFAULT mode shows expected poor synchronization
#
# Drift Analysis Methodology:
#   - Calibration: Performed once in MASTER-SLAVE mode, used for both tests
#     * Discard first 1 second of frames to allow streams to stabilize
#     * Use linear regression: slave_hw_ts = slope * master_hw_ts + offset
#     * Captures the relationship between the two hardware clocks
#     * Same calibration used for both MASTER-SLAVE and DEFAULT mode tests
#   - Transform slave to master's domain: slave_in_master_domain = (slave_hw_ts - offset) / slope
#   - Frame alignment: Pairs frames based on transformed HW timestamps with 30% of frame time threshold (~10ms at 30fps)
#     * Match quality stored for each aligned pair (HW timestamp difference in master's domain)
#   - Quality filtering: Strict 30% of frame time threshold applied during analysis to ensure high-quality statistics
#   - Outlier removal: IQR method (1.5×IQR) applied to offset values to reduce stability spikes from transient anomalies
#   - HW timestamp offset: Absolute difference between master HW timestamp and slave in master's domain
#     * First segment offset should be near zero in MASTER-SLAVE mode (well synchronized)
#     * DEFAULT mode offset shows how much cameras drift without hardware sync
#   - Segments: Analysis divided into 10-second segments for trend observation
#   - Drift rate: Change in HW timestamp offset per minute
#
# Notes:
#   - Hardware timestamp is 32-bit (uint32_t) with microsecond resolution (1 MHz counter)
#   - Hardware timestamp wraps around after ~4,294 seconds (approximately 71.5 minutes)
#   - Test duration limited to 90 seconds per drift test for quicker validation
#   - Total test time: 1 calibration (10s) + 2 drift tests (180s) = ~3 minutes
#   - Single calibration performed in MASTER-SLAVE mode, used for both drift tests
#   - Calibration uses system timestamps for frame alignment (within 33ms)
#   - Drift measurement uses transformed HW timestamps for frame alignment (30% of frame time = ~10ms at 30fps)
#   - Strict match quality filter (30% of frame time) ensures high-quality statistics
#   - Calibration uses linear regression to account for both offset and clock rate differences
#   - Slope close to 1.0 indicates hardware clocks running at similar rates
#   - Slave timestamps transformed to master's domain for direct comparison
#   - All calculations use double precision (Python float is 64-bit IEEE 754)
#   - Test duration configurable via DRIFT_TEST_DURATION
#   - Test runs nightly only due to extended duration
#   - Pass/fail thresholds configurable via MASTER_SLAVE_*_THRESHOLD_MAX and DEFAULT_*_THRESHOLD_MIN
#   - Comparison validates MASTER-SLAVE mode provides better synchronization than DEFAULT

import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy import test, log
import time
import statistics

# Configuration options
CALIBRATION_DURATION = 5.0  # Duration for calibration phase in seconds
DRIFT_TEST_DURATION = 90.0  # Duration in seconds for drift measurement (max ~1200s to stay under HW timestamp wrap-around)
ENABLE_DRIFT_PLOT = False   # Enable drift plotting (requires matplotlib)

# MASTER-SLAVE mode thresholds (expect good synchronization)
MASTER_SLAVE_OFFSET_THRESHOLD_MAX = 20.0  # Maximum first segment offset in microseconds
MASTER_SLAVE_DRIFT_RATE_THRESHOLD_MAX = 20.0  # Maximum drift rate in us/minute

# DEFAULT mode thresholds (expect poor synchronization)
DEFAULT_OFFSET_THRESHOLD_MIN = 100.0  # Minimum first segment offset in microseconds
DEFAULT_DRIFT_RATE_THRESHOLD_MIN = 100.0  # Minimum drift rate in us/minute

if ENABLE_DRIFT_PLOT:
    try:
        import matplotlib.pyplot as plt
        import matplotlib
        matplotlib.use('Agg')  # Use non-interactive backend
    except ImportError:
        log.w("matplotlib not available, drift plotting disabled")
        ENABLE_DRIFT_PLOT = False

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
    """Create a callback function that captures hardware and system timestamps.
    
    Captures both hardware timestamp (from camera) and system timestamp (from host)
    to enable calibration and drift analysis.
    """
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
    """Calibrate slave hardware timestamp against master hardware timestamp.
    
    Starts streaming from both cameras, discards the first 1 second of frames to allow
    streams to stabilize, then collects frames for the specified duration. Aligns frames 
    based on system timestamps and uses linear regression to find the relationship: 
    slave_hw_ts = slope * master_hw_ts + offset
    
    This captures the offset and rate difference between the two hardware clocks.
    Cameras should already be configured in MASTER-SLAVE mode before calibration.
    
    Returns tuple (offset, slope) where:
    - offset: intercept of the linear fit (microseconds)
    - slope: rate of change (should be close to 1.0 if clocks run at same rate)
    
    Note: Data is normalized before regression and all calculations use double precision
          to avoid numerical precision issues with large timestamp values.
    """
    master_timestamps = []
    slave_timestamps = []
    
    master_callback = create_timestamp_callback(master_timestamps)
    slave_callback = create_timestamp_callback(slave_timestamps)
    
    # Start streaming on both cameras
    master_sensor.open(master_profile)
    slave_sensor.open(slave_profile)
    master_sensor.start(master_callback)
    slave_sensor.start(slave_callback)
    
    log.i(f"Calibrating slave HW timestamp against master HW timestamp for {duration} seconds...")
    
    # Wait 1 second and discard initial frames to allow streams to stabilize
    time.sleep(1.0)
    master_timestamps.clear()
    slave_timestamps.clear()
    
    # Collect calibration data for the specified duration
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
    
    # Convert back to original scale: slave = slope * master + offset
    offset = float(slave_ref - slope * master_ref + offset_norm)
    
    # Calculate fit quality (R-squared)
    slave_mean = float(sum_slave / n)
    ss_tot = float(sum((s - slave_mean) ** 2 for s in slave_norm))
    ss_res = float(sum((s - (slope * m + offset_norm)) ** 2 for m, s in zip(master_norm, slave_norm)))
    r_squared = float(1 - (ss_res / ss_tot)) if ss_tot > 0 else 0.0
    
    log.i(f"Calibration complete: {len(aligned_pairs)} aligned frame pairs")
    log.i(f"  Slave = {slope:.6f} * Master + {offset:.2f} us")
    log.i(f"  R-squared: {r_squared:.6f}")
    
    return offset, slope

def collect_drift_data_calibrated(master_sensor, slave_sensor, master_profile, slave_profile,
                                   calib_offset, calib_slope,
                                   duration, mode_name):
    """Collect timestamp data for drift analysis using transformed slave HW timestamps.
    
    Collects HW timestamps from both cameras, aligns frames based on transformed hardware timestamps
    with 30% of frame time threshold (~10ms at 30fps), and transforms slave HW timestamps to master's
    timestamp domain using the inverse transformation: slave_in_master_domain = (slave_hw_ts - offset) / slope
    
    Frame alignment uses 30% of frame time as threshold for pairing frames based on HW timestamps.
    Match quality is stored for each pair to enable filtering during analysis.
    
    This allows direct comparison of timestamps in the same domain (master's HW clock).
    All calculations use double precision for numerical stability.
    
    Returns:
    - master_hw_timestamps: List of master HW timestamps
    - slave_calib_hw_timestamps: List of slave HW timestamps transformed to master's domain
    - match_qualities: List of match quality (HW timestamp difference in microseconds)
    - min_expected_frames: Minimum expected frame count
    """
    master_timestamps = []
    slave_timestamps = []
    
    master_callback = create_timestamp_callback(master_timestamps)
    slave_callback = create_timestamp_callback(slave_timestamps)
    
    # Start streaming
    master_sensor.open(master_profile)
    slave_sensor.open(slave_profile)
    master_sensor.start(master_callback)
    slave_sensor.start(slave_callback)
    
    # Wait 1 second then start drift measurement
    log.i(f"Waiting 1 second before starting drift measurement in {mode_name} mode...")
    time.sleep(1.0)
    master_timestamps.clear()
    slave_timestamps.clear()
    log.i(f"Starting drift measurement in {mode_name} mode")
    
    # Collect frames for the configured duration
    expected_frames = int(duration * 30 * 1.1)  # 30fps with 10% margin
    log.i(f"Collecting frames for {duration} seconds (expecting ~{expected_frames} frames)...")
    start_time = time.time()
    last_log_time = start_time
    
    while (time.time() - start_time) < duration:
        # Log progress periodically
        progress_interval = max(15.0, duration / 6)
        if (time.time() - last_log_time) > progress_interval:
            elapsed = time.time() - start_time
            log.i(f"  Progress: {elapsed:.0f}s - master={len(master_timestamps)}, slave={len(slave_timestamps)} frames")
            last_log_time = time.time()
        time.sleep(0.1)
    
    master_sensor.stop()
    slave_sensor.stop()
    master_sensor.close()
    slave_sensor.close()
    
    # Align frames based on hardware timestamps
    # Use 30% of frame time as threshold (at 30fps, frame time is 33.33ms, so 30% = 10ms)
    frame_time_us = 1000000.0 / 30.0  # Frame time in microseconds at 30fps
    frame_time_threshold = frame_time_us * 0.3  # 30% of frame time
    
    log.i(f"Frame alignment threshold: {frame_time_threshold:.0f} us (30% of frame time)")
    
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
            # Store (master_hw, slave_hw, match_quality_us)
            aligned_pairs.append((master_hw_ts, best_slave_hw, min_time_diff))
    
    log.i(f"Aligned {len(aligned_pairs)} frame pairs (master={len(master_timestamps)}, slave={len(slave_timestamps)})")
    
    # Extract aligned HW timestamps and match quality
    master_hw_timestamps = [float(master_hw) for master_hw, slave_hw, match_qual in aligned_pairs]
    slave_hw_timestamps = [float(slave_hw) for master_hw, slave_hw, match_qual in aligned_pairs]
    match_qualities = [match_qual for master_hw, slave_hw, match_qual in aligned_pairs]
    
    # Transform slave HW timestamps to master's timestamp domain
    # Since: slave = slope * master + offset
    # We get: slave_in_master_domain = (slave - offset) / slope
    slave_calib_hw_timestamps = [float((slave_hw - calib_offset) / calib_slope)
                                  for slave_hw in slave_hw_timestamps]
    
    # Calculate average match quality
    avg_match_quality = sum(match_qualities) / len(match_qualities) if match_qualities else 0
    max_match_quality = max(match_qualities) if match_qualities else 0
    log.i(f"Frame alignment quality: avg={avg_match_quality:.2f} us, max={max_match_quality:.2f} us")
    
    # Verify frame collection
    total_duration = time.time() - start_time
    min_expected_frames = int(duration * 30 * 0.75)  # At least 75% of expected frames
    log.i(f"Collected frames over {total_duration:.1f}s: master={len(master_hw_timestamps)}, slave={len(slave_calib_hw_timestamps)}")
    
    return master_hw_timestamps, slave_calib_hw_timestamps, match_qualities, min_expected_frames

def analyze_drift_segment_calibrated(master_hw_seg, slave_calib_hw_seg, match_qual_seg):
    """Analyze drift for a single segment using timestamps in master's domain.
    
    Both master and slave timestamps are in the same domain (master's HW clock).
    Computes HW timestamp offset as the absolute difference between them.
    Filters out poorly matched pairs to avoid outliers.
    
    Uses a strict 30% of frame time match quality threshold (~10ms at 30fps) for statistics.
    This ensures only well-synchronized frames contribute to drift measurements,
    even in long tests where DEFAULT mode may accumulate drift.
    
    Additional outlier removal using IQR method (1.5×IQR) is applied to offset values
    to reduce spikes in stability measurements caused by transient anomalies.
    
    Timestamps are already aligned (same frame indices).
    """
    if len(master_hw_seg) != len(slave_calib_hw_seg) or len(master_hw_seg) == 0:
        return None, None
    
    # Filter out poorly matched pairs with strict threshold for high-quality statistics
    # 30% of frame time at 30fps (same as alignment threshold)
    match_quality_threshold = 10000.0  # 30% of frame time: ~10ms at 30fps in microseconds
    filtered_pairs = [(m, s) for m, s, q in zip(master_hw_seg, slave_calib_hw_seg, match_qual_seg) 
                      if q <= match_quality_threshold]
    
    if len(filtered_pairs) == 0:
        return None, None
    
    # Calculate offset between master HW timestamp and calibrated slave HW timestamp
    offsets = [abs(master_hw - slave_calib_hw) 
               for master_hw, slave_calib_hw in filtered_pairs]
    
    # Remove outliers using IQR method to reduce spikes in stability chart
    if len(offsets) >= 4:  # Need at least 4 points for quartile calculation
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
        return None, None
    
    hw_ts_offset = statistics.median(offsets)
    seg_stdev = statistics.stdev(offsets) if len(offsets) > 1 else 0
    
    return hw_ts_offset, seg_stdev

def analyze_drift_data_calibrated(master_hw_timestamps, slave_calib_hw_timestamps, match_qualities, segment_duration, mode_name):
    """Analyze drift data across multiple segments using master and calibrated slave HW timestamps.
    
    Divides collected data into segments, analyzes each segment, and calculates drift rate
    showing how HW timestamp offset changes over time.
    """
    analysis_start_time = time.time()
    
    num_segments = int(DRIFT_TEST_DURATION / segment_duration)
    frames_per_segment = len(master_hw_timestamps) // num_segments
    
    segment_stdevs = []
    segment_medians = []
    segment_drift_rates = []
    
    log.i(f"\nDrift analysis for {mode_name} mode over {num_segments} segments:")
    
    for seg in range(num_segments):
        start_idx = seg * frames_per_segment
        end_idx = start_idx + frames_per_segment
        
        master_hw_seg = master_hw_timestamps[start_idx:end_idx]
        slave_calib_hw_seg = slave_calib_hw_timestamps[start_idx:end_idx]
        match_qual_seg = match_qualities[start_idx:end_idx]
        
        hw_ts_offset, seg_stdev = analyze_drift_segment_calibrated(master_hw_seg, slave_calib_hw_seg, match_qual_seg)
        
        if hw_ts_offset is not None:
            segment_medians.append(hw_ts_offset)
            segment_stdevs.append(seg_stdev)
            
            # Calculate drift rate for this segment (us/minute)
            if seg > 0:
                seg_drift = abs(hw_ts_offset - segment_medians[0])
                seg_elapsed_time = (seg + 1) * segment_duration
                seg_drift_rate = (seg_drift / seg_elapsed_time) * 60.0
                segment_drift_rates.append(seg_drift_rate)
            
            seg_start_time = seg * segment_duration
            seg_end_time = (seg + 1) * segment_duration
            log.i(f"  Segment {seg+1} ({seg_start_time:.0f}-{seg_end_time:.0f}s): hw_ts_offset={hw_ts_offset:.2f} us, stdev={seg_stdev:.2f} us")
    
    analysis_time = time.time() - analysis_start_time
    log.i(f"Analysis completed in {analysis_time:.2f} seconds")
    
    return segment_medians, segment_stdevs, segment_drift_rates

def calculate_drift_summary(segment_medians, mode_name):
    """Calculate and log drift summary statistics.
    
    Computes total drift as the change in HW timestamp offset from first to last segment,
    and expresses it as drift rate in microseconds per minute.
    """
    if len(segment_medians) >= 2:
        first_hw_ts_offset = segment_medians[0]
        last_hw_ts_offset = segment_medians[-1]
        total_drift = abs(last_hw_ts_offset - first_hw_ts_offset)
        drift_per_minute = (total_drift / DRIFT_TEST_DURATION) * 60.0
        
        log.i(f"\nDrift summary for {mode_name} mode:")
        log.i(f"  First segment HW timestamp offset: {first_hw_ts_offset:.2f} us")
        log.i(f"  Last segment HW timestamp offset: {last_hw_ts_offset:.2f} us")
        log.i(f"  Total drift: {total_drift:.2f} us over {DRIFT_TEST_DURATION:.0f}s")
        log.i(f"  Drift rate: {drift_per_minute:.2f} us/minute")
        
        return drift_per_minute
    
    return None

def generate_comparison_plot(master_slave_results, default_results):
    """Generate a 2-panel comparison plot between MASTER-SLAVE and DEFAULT modes"""
    try:
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))
        
        segment_duration = master_slave_results['segment_duration']
        num_segments_ms = len(master_slave_results['hw_ts_offsets'])
        num_segments_def = len(default_results['hw_ts_offsets'])
        
        # Plot 1: HW Timestamp Offset Over Time
        times_ms = [(seg + 0.5) * segment_duration for seg in range(num_segments_ms)]
        times_def = [(seg + 0.5) * segment_duration for seg in range(num_segments_def)]
        
        ax1.plot(times_ms, master_slave_results['hw_ts_offsets'], 'b-o', linewidth=2, markersize=6, label='MASTER-SLAVE')
        ax1.plot(times_def, default_results['hw_ts_offsets'], 'r-s', linewidth=2, markersize=6, label='DEFAULT')
        ax1.set_xlabel('Time (seconds)', fontsize=12)
        ax1.set_ylabel('HW Timestamp Offset (us)', fontsize=12)
        ax1.set_title('HW Timestamp Offset Over Time (Calibrated)', fontsize=14, fontweight='bold')
        ax1.grid(True, alpha=0.3)
        ax1.legend()
        
        # Plot 2: Drift Rate Over Time
        drift_times_ms = [(seg + 1.5) * segment_duration for seg in range(len(master_slave_results['drift_rates']))]
        drift_times_def = [(seg + 1.5) * segment_duration for seg in range(len(default_results['drift_rates']))]
        
        ax2.plot(drift_times_ms, master_slave_results['drift_rates'], 'b-o', linewidth=2, markersize=6, label='MASTER-SLAVE')
        ax2.plot(drift_times_def, default_results['drift_rates'], 'r-s', linewidth=2, markersize=6, label='DEFAULT')
        ax2.set_xlabel('Time (seconds)', fontsize=12)
        ax2.set_ylabel('Drift Rate (us/minute)', fontsize=12)
        ax2.set_title('Drift Rate Over Time', fontsize=14, fontweight='bold')
        ax2.grid(True, alpha=0.3)
        ax2.legend()
        
        plt.tight_layout()
        plot_filename = f'sync_drift_calibrated_comparison_{int(time.time())}.png'
        plt.savefig(plot_filename, dpi=150)
        log.i(f"Comparison plot saved to: {plot_filename}")
        plt.close()
        
        return True
    except Exception as e:
        log.w(f"Failed to generate comparison plot: {e}")
        return False

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

test.start("Reset both devices to DEFAULT sync mode")
master_sensor.set_option(rs.option.inter_cam_sync_mode, DEFAULT)
slave_sensor.set_option(rs.option.inter_cam_sync_mode, DEFAULT)
test.check_equal(master_sensor.get_option(rs.option.inter_cam_sync_mode), DEFAULT)
test.check_equal(slave_sensor.get_option(rs.option.inter_cam_sync_mode), DEFAULT)
test.finish()

################################################################################################

test.start("Get standard depth profile (640x480 @ 30fps)")
master_profile = next((p for p in master_sensor.profiles 
                       if p.stream_type() == rs.stream.depth 
                       and p.as_video_stream_profile().width() == 640
                       and p.as_video_stream_profile().height() == 480
                       and p.fps() == 30), None)

slave_profile = next((p for p in slave_sensor.profiles 
                      if p.stream_type() == rs.stream.depth 
                      and p.as_video_stream_profile().width() == 640
                      and p.as_video_stream_profile().height() == 480
                      and p.fps() == 30), None)

if not master_profile or not slave_profile:
    log.e("Could not find standard 640x480@30fps depth profile on both devices")
    test.check(False, "Standard profile not available")
    test.print_results_and_exit()

test.finish()

################################################################################################

test.start("Configure master-slave mode")
master_sensor.set_option(rs.option.inter_cam_sync_mode, MASTER)
slave_sensor.set_option(rs.option.inter_cam_sync_mode, SLAVE)
test.check_equal(master_sensor.get_option(rs.option.inter_cam_sync_mode), MASTER)
test.check_equal(slave_sensor.get_option(rs.option.inter_cam_sync_mode), SLAVE)
test.finish()

################################################################################################

test.start(f"Calibrate slave HW timestamp vs master HW timestamp in MASTER-SLAVE mode ({CALIBRATION_DURATION} seconds)")

# Calibrate slave against master
calib_offset_ms, calib_slope_ms = calibrate_slave_to_master(master_sensor, slave_sensor, 
                                                             master_profile, slave_profile, 
                                                             CALIBRATION_DURATION)
if calib_offset_ms is None or calib_slope_ms is None:
    log.e("Calibration failed")
    test.check(False, "Slave-to-master calibration failed")
    test.print_results_and_exit()

test.finish()

################################################################################################

test.start(f"Measure timestamp drift in MASTER-SLAVE mode over {DRIFT_TEST_DURATION:.0f} seconds")

# Collect drift data using calibrated slave HW timestamps
master_hw_timestamps, slave_calib_hw_timestamps, match_qualities, min_expected_frames = collect_drift_data_calibrated(
    master_sensor, slave_sensor, master_profile, slave_profile,
    calib_offset_ms, calib_slope_ms,
    DRIFT_TEST_DURATION, "MASTER-SLAVE")

# Verify frame collection
test.check(len(master_hw_timestamps) >= min_expected_frames, 
          f"Master should receive at least {min_expected_frames} frames in {DRIFT_TEST_DURATION}s, got {len(master_hw_timestamps)}")
test.check(len(slave_calib_hw_timestamps) >= min_expected_frames, 
          f"Slave should receive at least {min_expected_frames} frames in {DRIFT_TEST_DURATION}s, got {len(slave_calib_hw_timestamps)}")

master_slave_results = None

if len(master_hw_timestamps) >= 100 and len(slave_calib_hw_timestamps) >= 100:
    segment_duration = 10.0  # seconds
    
    # Analyze drift using master HW timestamp and calibrated slave HW timestamp
    segment_medians, segment_stdevs, segment_drift_rates = analyze_drift_data_calibrated(
        master_hw_timestamps, slave_calib_hw_timestamps, match_qualities, segment_duration, "MASTER-SLAVE")
    
    # Calculate summary
    drift_per_minute = calculate_drift_summary(segment_medians, "MASTER-SLAVE")
    
    if drift_per_minute is not None and len(segment_medians) >= 2:
        # Check MASTER-SLAVE mode thresholds
        first_offset = segment_medians[0]
        
        test.check(first_offset < MASTER_SLAVE_OFFSET_THRESHOLD_MAX,
                  f"MASTER-SLAVE first segment offset should be < {MASTER_SLAVE_OFFSET_THRESHOLD_MAX} us, got {first_offset:.2f} us")
        test.check(drift_per_minute < MASTER_SLAVE_DRIFT_RATE_THRESHOLD_MAX,
                  f"MASTER-SLAVE drift rate should be < {MASTER_SLAVE_DRIFT_RATE_THRESHOLD_MAX} us/minute, got {drift_per_minute:.2f} us/minute")
        
        # Store MASTER-SLAVE results for comparison
        master_slave_results = {
            'hw_ts_offsets': segment_medians,
            'stdevs': segment_stdevs,
            'drift_rates': segment_drift_rates,
            'drift_rate': drift_per_minute,
            'segment_duration': segment_duration
        }

test.finish()

################################################################################################

test.start("Configure DEFAULT mode")
master_sensor.set_option(rs.option.inter_cam_sync_mode, DEFAULT)
slave_sensor.set_option(rs.option.inter_cam_sync_mode, DEFAULT)
test.finish()

################################################################################################

test.start(f"Measure timestamp drift in DEFAULT mode over {DRIFT_TEST_DURATION:.0f} seconds for comparison")

# Collect drift data using same calibration from MASTER-SLAVE mode
master_hw_timestamps_default, slave_calib_hw_timestamps_default, match_qualities_default, min_expected_frames_default = collect_drift_data_calibrated(
    master_sensor, slave_sensor, master_profile, slave_profile,
    calib_offset_ms, calib_slope_ms,
    DRIFT_TEST_DURATION, "DEFAULT")

# Verify frame collection
test.check(len(master_hw_timestamps_default) >= min_expected_frames_default,
          f"Master should receive at least {min_expected_frames_default} frames in {DRIFT_TEST_DURATION}s, got {len(master_hw_timestamps_default)}")
test.check(len(slave_calib_hw_timestamps_default) >= min_expected_frames_default,
          f"Slave should receive at least {min_expected_frames_default} frames in {DRIFT_TEST_DURATION}s, got {len(slave_calib_hw_timestamps_default)}")

default_results = None

if len(master_hw_timestamps_default) >= 100 and len(slave_calib_hw_timestamps_default) >= 100:
    segment_duration = 10.0  # seconds
    
    # Analyze drift using master HW timestamp and calibrated slave HW timestamp
    segment_medians_default, segment_stdevs_default, segment_drift_rates_default = analyze_drift_data_calibrated(
        master_hw_timestamps_default, slave_calib_hw_timestamps_default, match_qualities_default, segment_duration, "DEFAULT")
    
    # Calculate summary
    drift_per_minute_default = calculate_drift_summary(segment_medians_default, "DEFAULT")
    
    if drift_per_minute_default is not None and len(segment_medians_default) >= 2:
        # Check DEFAULT mode thresholds (should show significant drift)
        first_offset_default = segment_medians_default[0]
        
        test.check(first_offset_default > DEFAULT_OFFSET_THRESHOLD_MIN,
                  f"DEFAULT first segment offset should be > {DEFAULT_OFFSET_THRESHOLD_MIN} us, got {first_offset_default:.2f} us")
        test.check(drift_per_minute_default > DEFAULT_DRIFT_RATE_THRESHOLD_MIN,
                  f"DEFAULT drift rate should be > {DEFAULT_DRIFT_RATE_THRESHOLD_MIN} us/minute, got {drift_per_minute_default:.2f} us/minute")
        
        # Store DEFAULT results for comparison
        default_results = {
            'hw_ts_offsets': segment_medians_default,
            'stdevs': segment_stdevs_default,
            'drift_rates': segment_drift_rates_default,
            'drift_rate': drift_per_minute_default,
            'segment_duration': segment_duration
        }

test.finish()

################################################################################################

# Generate comparison plot if both tests completed successfully
if ENABLE_DRIFT_PLOT and master_slave_results and default_results:
    test.start("Generate comparison plot")
    success = generate_comparison_plot(master_slave_results, default_results)
    test.finish()

################################################################################################
test.print_results_and_exit()
