# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
#
# test:donotrun # until fw issue resolved
# test:device each(D400*)
# test:donotrun:!nightly
# test:timeout 300

# Validates depth auto-exposure (AE) robustness while streaming.
#
# Test flow:
# 1. Baseline streaming test - measure frame timing with AE enabled but no toggling
# 2. Rapid AE toggle test - toggle AE on/off repeatedly and measure frame timing stability
# 3. Manual-to-AE test - switch from long manual exposure (2x frame time) to AE and verify recovery
#
# All tests verify:
# - Camera continues streaming without stalls
# - Frame timing spikes (gaps > 110% of expected frame time) stay below acceptable threshold
# - Average frame time remains close to expected value
# - AUTO_EXPOSURE metadata matches the expected AE state
#
# Timing notes:
# - `frame_timestamps` are host timestamps in milliseconds: `time.time() * 1000.0`
# - A "spike" is an inter-frame gap exceeding 110% of the expected frame time
# - Spike rate threshold is configurable via `MAX_ACCEPTABLE_SPIKE_RATE_PERCENT`

import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy import test, log
import time

# Configuration: acceptable spike rate percentage
# A "spike" is a gap exceeding 110% of expected frame time
MAX_ACCEPTABLE_SPIKE_RATE_PERCENT = float(4.0)      # Maximum acceptable percentage of frames with timing spikes

# Configuration: test iterations
NUM_MANUAL_TO_AE_ITERATIONS = 20                    # Number of manual->AE toggle iterations in third test

device, ctx = test.find_first_device_or_exit()
depth_sensor = device.first_depth_sensor()
fw_version = rsutils.version(device.get_info(rs.camera_info.firmware_version))

if fw_version < rsutils.version(5, 15, 0, 0):
    log.i(f"FW version {fw_version} does not support DEPTH_AUTO_EXPOSURE_MODE option, skipping test...")
    test.print_results_and_exit()

# StreamingContext: collects frame timestamps and metadata during streaming
class StreamingContext:
    def __init__(self):
        self.frame_count = 0
        self.frame_timestamps = []  # Host timestamps in milliseconds
        self.frame_ae_metadata = []  # AUTO_EXPOSURE metadata values
    
    def frame_callback(self, frame):
        self.frame_count += 1
        self.frame_timestamps.append(time.time() * 1000.0)
        if frame.supports_frame_metadata(rs.frame_metadata_value.auto_exposure):
            ae_metadata = frame.get_frame_metadata(rs.frame_metadata_value.auto_exposure)
            self.frame_ae_metadata.append(ae_metadata)
        else:
            self.frame_ae_metadata.append(None)


def verify_ae_metadata(context, start_frame, end_frame, expected_ae_state):
    """
    Verify AUTO_EXPOSURE metadata matches expected state for a range of frames.
    
    Args:
        context: StreamingContext with collected metadata
        start_frame: First frame index to check (inclusive)
        end_frame: Last frame index to check (exclusive)
        expected_ae_state: True if AE should be enabled, False if disabled
    
    Returns:
        Number of frames with mismatched metadata
    """
    metadata_errors = 0
    for i in range(start_frame, end_frame):
        if i < len(context.frame_ae_metadata) and context.frame_ae_metadata[i] is not None:
            ae_metadata = context.frame_ae_metadata[i]
            ae_enabled_in_metadata = (ae_metadata != 0)
            if ae_enabled_in_metadata != expected_ae_state:
                metadata_errors += 1
                log.d(f"Frame {i}: AE metadata mismatch - expected {expected_ae_state}, metadata shows {ae_enabled_in_metadata}")
    return metadata_errors


def calculate_max_frame_gap(context, start_idx=0):
    """
    Calculate maximum inter-frame gap in milliseconds.
    
    Args:
        context: StreamingContext with frame_timestamps
        start_idx: Start calculating from this frame index (default 0)
    
    Returns:
        Maximum gap in milliseconds
    """
    max_gap_ms = 0.0
    if len(context.frame_timestamps) > 1:
        for i in range(max(1, start_idx), len(context.frame_timestamps)):
            if i > 0:
                gap = context.frame_timestamps[i] - context.frame_timestamps[i-1]
                if gap > max_gap_ms:
                    max_gap_ms = gap
    return max_gap_ms


def count_gap_spikes(context, frame_time_ms, spike_threshold_percent=10.0, start_idx=0):
    """
    Count the number of inter-frame gaps that exceed the expected frame time by more than spike_threshold_percent.
    
    Args:
        context: StreamingContext with frame_timestamps
        frame_time_ms: Expected frame time in milliseconds
        spike_threshold_percent: Percentage above frame_time_ms to consider a spike (default 10%)
        start_idx: Start counting from this frame index
    
    Returns:
        Number of gaps exceeding the threshold
    """
    spike_count = 0
    threshold_ms = frame_time_ms * (1.0 + spike_threshold_percent / 100.0)
    
    if len(context.frame_timestamps) > 1:
        for i in range(max(1, start_idx + 1), len(context.frame_timestamps)):
            gap = context.frame_timestamps[i] - context.frame_timestamps[i-1]
            if gap > threshold_ms:
                spike_count += 1
    
    return spike_count


def calculate_average_frame_time(context, start_idx=0, frame_time_ms=None, spike_threshold_percent=10.0):
    """
    Calculate the average inter-frame time in milliseconds, optionally excluding spike gaps.
    
    Args:
        context: StreamingContext with frame_timestamps
        start_idx: Start calculating from this frame index
        frame_time_ms: Expected frame time in ms. If provided, gaps exceeding spike threshold are excluded
        spike_threshold_percent: Percentage above frame_time_ms to consider a spike (default 10%)
    
    Returns:
        Average frame time in milliseconds, or 0 if insufficient data
    """
    if len(context.frame_timestamps) < 2:
        return 0.0
    
    total_time = 0.0
    count = 0
    
    # Calculate spike threshold if frame_time_ms is provided
    spike_threshold_ms = None
    if frame_time_ms is not None:
        spike_threshold_ms = frame_time_ms * (1.0 + spike_threshold_percent / 100.0)
    
    for i in range(max(1, start_idx + 1), len(context.frame_timestamps)):
        gap = context.frame_timestamps[i] - context.frame_timestamps[i-1]
        
        # Skip spike gaps if threshold is set
        if spike_threshold_ms is not None and gap > spike_threshold_ms:
            continue
            
        total_time += gap
        count += 1
    
    return total_time / count if count > 0 else 0.0


def wait_for_frames(context, target_frames, timeout=10.0):
    """
    Wait until the specified number of new frames have been received.
    Returns True if frames received, False if timeout.
    """
    frames_before = context.frame_count
    start_time = time.time()
    
    while (context.frame_count - frames_before) < target_frames:
        elapsed = time.time() - start_time
        if elapsed > timeout:
            frames_received = context.frame_count - frames_before
            log.e(f"Timeout waiting for {target_frames} frames")
            log.e(f"Waited {elapsed:.1f}s but only got {frames_received}/{target_frames} frames")
            return False
        time.sleep(0.01)
    
    return True


def toggle_ae_while_streaming(depth_sensor, num_toggles=5, frames_per_state=10, frames_between_toggles=30):
    """
    Toggle AE on/off while streaming and verify camera stability.
    
    Toggles AE state multiple times, collecting frames between each toggle for stabilization
    and verifying metadata correctness and frame timing stability.
    
    Args:
        depth_sensor: Depth sensor to test
        num_toggles: Number of AE state toggles to perform
        frames_per_state: Number of frames to collect for metadata verification in each state
        frames_between_toggles: Number of frames to wait between toggles for stabilization
    
    Returns:
        Tuple of (success, total_frames, metadata_errors, spike_count, spike_rate_percent)
    """
    # Select 30fps depth profile
    depth_profile = next(
        (p for p in depth_sensor.get_stream_profiles()
         if p.stream_type() == rs.stream.depth and p.fps() == 30),
        depth_sensor.get_stream_profiles()[0]  # Fallback to first profile
    )
    context = StreamingContext()
    
    depth_sensor.open(depth_profile)
    depth_sensor.start(lambda frame: context.frame_callback(frame))
    
    # Start with AE enabled
    current_ae_state = True
    depth_sensor.set_option(rs.option.enable_auto_exposure, current_ae_state)
    
    # Give initial time for streaming to stabilize
    time.sleep(1.0)
    log.d(f"Initial streaming started, received {context.frame_count} frames")
    
    metadata_errors = 0
    toggle_count = 0
    
    try:
        for toggle_idx in range(num_toggles):
            # Toggle AE state first (except for the initial iteration where we start with AE=True)
            if toggle_idx > 0:
                current_ae_state = not current_ae_state
                depth_sensor.set_option(rs.option.enable_auto_exposure, current_ae_state)
                toggle_count += 1
                log.d(f"Toggled AE to {current_ae_state} (toggle {toggle_count}/{num_toggles})")
                
                # Let camera run for some frames between toggles to stabilize
                frames_before_stabilize = context.frame_count
                log.d(f"Waiting {frames_between_toggles} frames for camera to stabilize after toggle...")
                if not wait_for_frames(context, frames_between_toggles):
                    log.e(f"Failed to collect {frames_between_toggles} stabilization frames after toggling to AE={current_ae_state}")
                    try:
                        depth_sensor.stop()
                        depth_sensor.close()
                    except RuntimeError:
                        pass
                    return False, context.frame_count, metadata_errors, 0, 0.0
                log.d(f"Collected {context.frame_count - frames_before_stabilize} stabilization frames")
            
            # Now wait for frames in current state
            frames_before = context.frame_count
            log.d(f"Toggle {toggle_idx}: waiting for {frames_per_state} frames with AE={current_ae_state}, currently have {context.frame_count} total")
            
            if not wait_for_frames(context, frames_per_state):
                log.e(f"Failed in AE state {current_ae_state} (toggle {toggle_idx})")
                log.e(f"Total frames collected: {context.frame_count} (expected {num_toggles * frames_per_state})")
                try:
                    depth_sensor.stop()
                    depth_sensor.close()
                except RuntimeError:
                    pass
                return False, context.frame_count, metadata_errors, 0, 0.0
            
            log.d(f"Toggle {toggle_idx}: collected {context.frame_count - frames_before} frames, total now {context.frame_count}")
            
            # Check metadata for frames received in this state
            metadata_errors += verify_ae_metadata(context, frames_before, context.frame_count, current_ae_state)
    
    finally:
        try:
            depth_sensor.stop()
            depth_sensor.close()
        except RuntimeError:
            pass  # Sensor may have already stopped

    # Count frame gap spikes (gaps exceeding 10% of frame time)
    fps = depth_profile.fps()
    frame_time_ms = 1000.0 / fps
    spike_count = count_gap_spikes(context, frame_time_ms, spike_threshold_percent=10.0)
    avg_frame_time_ms = calculate_average_frame_time(context, frame_time_ms=frame_time_ms, spike_threshold_percent=10.0)
    
    # Calculate spike rate
    total_gaps = max(1, len(context.frame_timestamps) - 1)
    spike_rate_percent = (spike_count / total_gaps) * 100.0

    log.i(
        f"Received {context.frame_count} total frames, avg frame time: {avg_frame_time_ms:.2f}ms (expected: {frame_time_ms:.2f}ms), "
        f"{spike_count} gap spikes (>{frame_time_ms*1.1:.1f}ms) out of {total_gaps} gaps ({spike_rate_percent:.1f}%), "
        f"metadata errors: {metadata_errors}"
    )

    # Success if we got enough frames
    success = context.frame_count >= (num_toggles * frames_per_state)
    return success, context.frame_count, metadata_errors, spike_count, spike_rate_percent

################################################################################################

test.start("Baseline streaming - measure frame gap without AE toggle")
# Establish baseline frame timing behavior with AE enabled but no toggling.
# This provides a reference for comparison with the AE toggle tests.

# Select 30fps depth profile
depth_profile = next(
    (p for p in depth_sensor.get_stream_profiles()
     if p.stream_type() == rs.stream.depth and p.fps() == 30),
    depth_sensor.get_stream_profiles()[0]  # Fallback to first profile
)
context = StreamingContext()

depth_sensor.open(depth_profile)
depth_sensor.start(lambda frame: context.frame_callback(frame))

# Enable AE and let it stabilize
depth_sensor.set_option(rs.option.enable_auto_exposure, True)
time.sleep(1.0)
log.d(f"Initial streaming started, received {context.frame_count} frames")

# Collect frames without any AE toggling
target_frames = 400
frames_before = context.frame_count

if not wait_for_frames(context, target_frames, timeout=20.0):
    log.e(f"Failed to collect {target_frames} baseline frames")
    try:
        depth_sensor.stop()
        depth_sensor.close()
    except RuntimeError:
        pass
    test.check(False, "Baseline streaming should collect frames without timeout")
else:
    # Stop streaming
    try:
        depth_sensor.stop()
        depth_sensor.close()
    except RuntimeError:
        pass
    
    # Count gap spikes in baseline
    fps = depth_profile.fps()
    frame_time_ms = 1000.0 / fps
    baseline_spike_count = count_gap_spikes(context, frame_time_ms, spike_threshold_percent=10.0)
    baseline_avg_frame_time_ms = calculate_average_frame_time(context, frame_time_ms=frame_time_ms, spike_threshold_percent=10.0)
    
    total_gaps = max(1, len(context.frame_timestamps) - 1)
    baseline_spike_rate = (baseline_spike_count / total_gaps) * 100.0
    
    log.i(f"Baseline: received {context.frame_count} frames, avg frame time: {baseline_avg_frame_time_ms:.2f}ms (expected: {frame_time_ms:.2f}ms), "
          f"{baseline_spike_count} gap spikes out of {total_gaps} gaps ({baseline_spike_rate:.1f}%)")
    
    # Baseline should have very few spikes (ideally 0)
    test.check(baseline_spike_rate < MAX_ACCEPTABLE_SPIKE_RATE_PERCENT, 
               f"Baseline spike rate {baseline_spike_rate:.1f}% should be < {MAX_ACCEPTABLE_SPIKE_RATE_PERCENT}%")

test.finish()

################################################################################################

test.start("Rapid AE toggle - verify camera stability and metadata correctness")
# Toggle AE on/off 10 times with 10 frames per state, allowing 30 frames between toggles.
# Verifies camera maintains stable frame timing during aggressive AE mode switching.
success, total_frames, metadata_errors, spike_count, spike_rate = toggle_ae_while_streaming(depth_sensor, num_toggles=10, frames_per_state=10, frames_between_toggles=30)

test.check(success, "Camera should handle rapid AE toggles without stalling")
test.check(total_frames >= 100, f"Expected at least 100 frames during rapid toggle, got {total_frames}")
# Expect no metadata mismatches: the helper waits after each toggle before checking.
test.check(metadata_errors == 0, f"Metadata errors should be minimal during rapid toggle, got {metadata_errors}")

# Check spike rate
test.check(spike_rate < MAX_ACCEPTABLE_SPIKE_RATE_PERCENT, 
           f"Spike rate should be < {MAX_ACCEPTABLE_SPIKE_RATE_PERCENT}%; got {spike_rate:.1f}%")

# Compare with baseline
if 'baseline_spike_count' in locals():
    log.i(f"Comparison: Baseline spikes: {baseline_spike_count} vs AE toggle spikes: {spike_count}")

test.finish()

################################################################################################

test.start("Switch from manual exposure (2 times frame time) to auto exposure; verify metadata and no stall")
# Verifies camera recovery when switching from long manual exposure (2x frame time) to AE.
# Tests robustness by repeating the sequence multiple times (configurable via NUM_MANUAL_TO_AE_ITERATIONS).
# Expected spike gaps are excluded from spike rate calculation since mode switches naturally cause timing disruptions.
# Average frame time calculation excludes manual exposure periods and transition frames (first 3 frames after ME->AE switch
# that are still captured at manual exposure rate before AE takes effect).

# Select 30fps depth profile
depth_profile = next(
    (p for p in depth_sensor.get_stream_profiles()
     if p.stream_type() == rs.stream.depth and p.fps() == 30),
    depth_sensor.get_stream_profiles()[0]  # Fallback to first profile
)
context = StreamingContext()

depth_sensor.open(depth_profile)
depth_sensor.start(lambda frame: context.frame_callback(frame))

try:
    # Get frame rate to calculate frame time
    fps = depth_profile.fps()
    frame_time_us = (1.0 / fps) * 1000000  # Frame time in microseconds
    frame_time_ms = 1000.0 / fps
    
    log.d(f"Stream FPS: {fps}, frame time: {frame_time_us:.1f} us")
    
    # Calculate exposure value once
    long_exposure = frame_time_us * 2.0
    exposure_range = depth_sensor.get_option_range(rs.option.exposure)
    # Clamp to valid range
    long_exposure = min(long_exposure, exposure_range.max)
    long_exposure = max(long_exposure, exposure_range.min)
    
    num_iterations = NUM_MANUAL_TO_AE_ITERATIONS
    total_manual_mode_errors = 0
    total_auto_mode_errors = 0
    total_spike_count = 0
    total_gaps = 0
    ae_frame_time_sum = 0.0  # Sum of frame times during AE periods (excluding transition frames)
    ae_frame_time_count = 0  # Count of gaps during AE periods (excluding transition frames)
    
    for iteration in range(num_iterations):
        log.d(f"\n=== Iteration {iteration + 1}/{num_iterations} ===")
        
        # Set manual exposure mode with exposure time longer than frame time
        depth_sensor.set_option(rs.option.enable_auto_exposure, 0)  # Disable AE
        depth_sensor.set_option(rs.option.exposure, long_exposure)
        log.d(f"Set manual exposure to {long_exposure:.1f} us (frame time: {frame_time_us:.1f} us)")
        
        # Wait and collect frames with long exposure
        time.sleep(1.5)
        frames_with_manual = context.frame_count
        log.d(f"Iteration {iteration + 1}: received {frames_with_manual} total frames")
        
        # Now switch to auto exposure
        frames_before_switch = context.frame_count
        depth_sensor.set_option(rs.option.enable_auto_exposure, 1)
        log.d(f"Iteration {iteration + 1}: Switched to auto exposure mode at frame {frames_before_switch}")
        
        # Wait for AE to settle and camera to recover
        time.sleep(1.5)
        
        frames_after_switch = context.frame_count - frames_before_switch
        log.d(f"Iteration {iteration + 1}: received {frames_after_switch} frames after switching to AE")
        
        # Count gap spikes for this iteration
        iteration_spike_count = count_gap_spikes(context, frame_time_ms, spike_threshold_percent=10.0, start_idx=frames_before_switch)
        iteration_gaps = max(1, context.frame_count - frames_before_switch - 1)
        total_spike_count += iteration_spike_count
        total_gaps += iteration_gaps
        
        # Calculate frame times for AE period in this iteration only
        # Skip first few frames after ME->AE switch as they are still captured at manual exposure rate
        # before AE takes effect (takes a couple frames to transition in this case)
        # Also exclude spike gaps from the average calculation
        frames_to_skip_after_switch = 2  # Skip transition frames still at ME rate
        spike_threshold_ms = frame_time_ms * 1.1  # 10% threshold for spikes
        ae_start_idx = frames_before_switch + frames_to_skip_after_switch
        for i in range(max(1, ae_start_idx + 1), context.frame_count):
            gap = context.frame_timestamps[i] - context.frame_timestamps[i-1]
            # Skip spike gaps when calculating average
            if gap <= spike_threshold_ms:
                ae_frame_time_sum += gap
                ae_frame_time_count += 1
        
        log.d(f"Iteration {iteration + 1}: {iteration_spike_count} spikes out of {iteration_gaps} gaps")
    
    # Final statistics across all iterations
    # Each iteration has 2 toggles (manual->AE during switch, AE->manual at next iteration start)
    # Since ME is 2x frame time, we expect a spike at each manual->AE switch
    # So expected spike gaps = 2 * num_iterations
    expected_spike_gaps = 2 * num_iterations
    unexpected_spikes = max(0, total_spike_count - expected_spike_gaps)
    adjusted_gaps = max(1, total_gaps - expected_spike_gaps)
    adjusted_spike_rate = (unexpected_spikes / adjusted_gaps) * 100.0 if adjusted_gaps > 0 else 0.0
    
    # Calculate average frame time for AE periods only (excluding manual exposure periods and transition frames)
    overall_avg_frame_time_ms = (ae_frame_time_sum / ae_frame_time_count) if ae_frame_time_count > 0 else 0.0
    
    log.i(
        f"Overall: {num_iterations} iterations, {context.frame_count} total frames, "
        f"avg frame time (AE only, excluding transition frames): {overall_avg_frame_time_ms:.2f}ms (expected: {frame_time_ms:.2f}ms), "
        f"{unexpected_spikes} gap spikes out of {total_gaps} gaps "
        f"(adjusted spike rate: {adjusted_spike_rate:.1f}%, excluding {expected_spike_gaps} expected toggle spikes)"
    )
    
    # Check that we got frames
    test.check(context.frame_count > 0, f"Should receive frames, got {context.frame_count}")
    
    # Check that adjusted spike rate is reasonable (excluding expected toggle spikes)
    test.check(
        adjusted_spike_rate < MAX_ACCEPTABLE_SPIKE_RATE_PERCENT,
        f"Adjusted spike rate should be < {MAX_ACCEPTABLE_SPIKE_RATE_PERCENT}%; got {adjusted_spike_rate:.1f}%"
    )
    
    # Compare with baseline
    if 'baseline_spike_count' in locals():
        log.i(f"Comparison: Baseline spikes: {baseline_spike_count} vs Manual-to-AE spikes: {unexpected_spikes}")
    
finally:
    try:
        depth_sensor.stop()
        depth_sensor.close()
    except RuntimeError:
        pass

test.finish()

################################################################################################
test.print_results_and_exit()
