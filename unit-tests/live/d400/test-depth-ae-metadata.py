# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
#
# test:donotrun # until fw issue resolved
# test:device each(D400*)
#
# Validates that the AUTO_EXPOSURE frame metadata correctly reflects the state of the
# enable_auto_exposure option on D400 devices.
#
# Test flow:
# 1. Verify AUTO_EXPOSURE metadata is non-zero when AE is enabled
#    - Test with default AE mode (typically REGULAR)
#    - If supported, test with ACCELERATED AE mode
# 2. Verify AUTO_EXPOSURE metadata is zero when AE is disabled
# 3. Rapid AE toggle test - verify metadata tracks AE state correctly during multiple toggles
#    - Test with default mode (NUM_TOGGLES toggles, configurable)
#    - If supported, repeat test with ACCELERATED mode
#
# Requirements:
# - Firmware version >= 5.15.0.0 (for DEPTH_AUTO_EXPOSURE_MODE support)
# - 30fps depth profile available (to ensure adequate frame rate for testing)

import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy import test, log
import time

# Initialize device and sensor
device, _ = test.find_first_device_or_exit()
depth_sensor = device.first_depth_sensor()
fw_version = rsutils.version(device.get_info(rs.camera_info.firmware_version))

# Check firmware version compatibility
if fw_version < rsutils.version(5, 15, 0, 0):
    log.i(f"FW version {fw_version} does not support DEPTH_AUTO_EXPOSURE_MODE option, skipping test...")
    test.print_results_and_exit()

# Constants for auto_exposure_mode option values
REGULAR = 0.0       # Standard auto-exposure algorithm
ACCELERATED = 1.0   # Faster converging auto-exposure algorithm

# Test configuration - Adjust these values to control rapid toggle test behavior
NUM_TOGGLES = 10            # Number of AE on/off cycles to perform in rapid toggle test
FRAMES_PER_STATE = 10       # Number of frames to collect and verify per AE state
FRAMES_BETWEEN_TOGGLES = 30 # Stabilization frames to allow hardware to settle between toggles

# StreamingContext: collects frame count and metadata during streaming for rapid toggle test
class StreamingContext:
    """Collects frame count and AUTO_EXPOSURE metadata during streaming.
    
    Used by the rapid toggle test to track frames and metadata in real-time
    while the test toggles AE state multiple times.
    """
    def __init__(self):
        self.frame_count = 0
        self.frame_ae_metadata = []  # AUTO_EXPOSURE metadata values for each frame
    
    def frame_callback(self, frame):
        """Callback invoked for each frame received during streaming."""
        self.frame_count += 1
        if frame.supports_frame_metadata(rs.frame_metadata_value.auto_exposure):
            ae_metadata = frame.get_frame_metadata(rs.frame_metadata_value.auto_exposure)
            self.frame_ae_metadata.append(ae_metadata)
        else:
            # Frame doesn't support AUTO_EXPOSURE metadata
            self.frame_ae_metadata.append(None)

# Helper function to capture frames and verify metadata
def verify_ae_metadata(depth_sensor, expected_ae_enabled, num_frames=10):
    """
    Open a streaming session, capture frames, and verify AUTO_EXPOSURE metadata correctness.
    
    This is a simple helper for tests 1 and 2, which verify metadata in a single AE state.
    For more complex multi-toggle scenarios, use toggle_ae_while_streaming() instead.
    
    Args:
        depth_sensor: The depth sensor to stream from
        expected_ae_enabled: True if AE should be enabled (metadata != 0), False if disabled (metadata == 0)
        num_frames: Number of frames to capture and verify (default 10)
    
    Returns:
        Tuple of (metadata_matches, frames_checked) where:
        - metadata_matches: Number of frames with correct AUTO_EXPOSURE metadata
        - frames_checked: Total number of frames examined
    """
    # Select a 30fps depth profile to ensure adequate frame rate for the test.
    # Lower frame rates (e.g., 5fps) can cause timeout failures when waiting for frames.
    depth_profile = next((p for p in depth_sensor.profiles 
                         if p.stream_type() == rs.stream.depth 
                         and p.format() == rs.format.z16 
                         and p.fps() == 30), None)
    if depth_profile is None:
        log.e("No 30fps depth profile with z16 format found. Test requires 30fps.")
        return 0, 0
    
    frames_checked = 0
    metadata_matches = 0
    
    # Open sensor and start streaming
    depth_sensor.open(depth_profile)
    
    received_frames = []
    def frame_callback(frame):
        received_frames.append(frame)
    
    depth_sensor.start(frame_callback)
    
    # Wait for frames to accumulate (up to 2 second timeout)
    timeout = 2.0
    start_time = time.time()
    while len(received_frames) < num_frames and (time.time() - start_time) < timeout:
        time.sleep(0.01)
    
    # Clean up streaming
    depth_sensor.stop()
    depth_sensor.close()
    
    if len(received_frames) < num_frames:
        log.w(f"Only received {len(received_frames)} frames out of {num_frames} requested")
    
    # Verify AUTO_EXPOSURE metadata for each frame
    # Metadata convention: 0 = AE disabled, non-zero = AE enabled
    for frame in received_frames[:num_frames]:
        frames_checked += 1
        if frame.supports_frame_metadata(rs.frame_metadata_value.auto_exposure):
            ae_metadata = frame.get_frame_metadata(rs.frame_metadata_value.auto_exposure)
            # Metadata convention: 0 = AE off, non-zero = AE on
            ae_enabled_in_metadata = (ae_metadata != 0)
            if ae_enabled_in_metadata == expected_ae_enabled:
                metadata_matches += 1
            else:
                log.d(f"Frame {frames_checked}: AUTO_EXPOSURE metadata = {ae_metadata}, expected AE enabled = {expected_ae_enabled}")
        else:
            log.w(f"Frame {frames_checked} does not support AUTO_EXPOSURE metadata")
    
    return metadata_matches, frames_checked

################################################################################################
# Helper functions for rapid AE toggle test (Test 3)
#
# These functions support the more complex test case where AE is toggled multiple times
# during a single streaming session, requiring frame tracking and metadata validation
# across state transitions.
################################################################################################

def verify_ae_metadata_from_context(context, start_frame, end_frame, expected_ae_state):
    """
    Verify AUTO_EXPOSURE metadata correctness for a range of frames within a StreamingContext.
    
    Used during rapid toggle test to validate that metadata matches the expected AE state
    for frames captured in a specific time window.
    
    Args:
        context: StreamingContext with collected frame metadata
        start_frame: First frame index to check (inclusive)
        end_frame: Last frame index to check (exclusive)
        expected_ae_state: True if AE should be enabled, False if disabled
    
    Returns:
        Number of frames with mismatched metadata (0 = all correct)
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


def wait_for_frames(context, target_frames, timeout=10.0):
    """
    Wait until the specified number of new frames have been received.
    
    Polls the StreamingContext until the frame count increases by target_frames,
    or the timeout is reached.
    
    Args:
        context: StreamingContext to monitor
        target_frames: Number of new frames to wait for
        timeout: Maximum time to wait in seconds (default 10.0)
    
    Returns:
        True if target_frames received, False if timeout occurred
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
    Perform rapid AE toggles during a single streaming session and verify metadata correctness.
    
    Opens a streaming session, then toggles enable_auto_exposure multiple times while collecting
    frames. After each toggle, waits for stabilization frames before collecting frames to verify.
    This tests that AUTO_EXPOSURE metadata correctly tracks AE state changes in real-time.
    
    Args:
        depth_sensor: Depth sensor to test
        num_toggles: Number of AE state transitions to perform
        frames_per_state: Number of frames to collect for metadata verification in each state
        frames_between_toggles: Number of frames to wait between toggles for hardware stabilization
    
    Returns:
        Tuple of (success, total_frames, metadata_errors) where:
        - success: True if enough frames were collected without stream stalling
        - total_frames: Total number of frames received during the test
        - metadata_errors: Number of frames with incorrect metadata
    """
    # Select 30fps depth profile
    depth_profile = next(
        (p for p in depth_sensor.get_stream_profiles()
         if p.stream_type() == rs.stream.depth and p.fps() == 30),
        None
    )
    if depth_profile is None:
        log.e("No 30fps depth profile found for rapid AE toggle test")
        return False, 0, 0
    
    context = StreamingContext()
    
    depth_sensor.open(depth_profile)
    depth_sensor.start(lambda frame: context.frame_callback(frame))
    
    # Initialize test with AE enabled (will toggle to disabled in first iteration)
    current_ae_state = True
    depth_sensor.set_option(rs.option.enable_auto_exposure, current_ae_state)
    
    # Allow initial streaming to stabilize before beginning toggles
    time.sleep(1.0)
    log.d(f"Initial streaming started, received {context.frame_count} frames")
    
    metadata_errors = 0
    toggle_count = 0
    
    try:
        for toggle_idx in range(num_toggles):
            # Toggle AE state (skip on first iteration since we start with AE=True)
            if toggle_idx > 0:
                current_ae_state = not current_ae_state
                depth_sensor.set_option(rs.option.enable_auto_exposure, current_ae_state)
                toggle_count += 1
                log.d(f"Toggled AE to {current_ae_state} (toggle {toggle_count}/{num_toggles})")
                
                # Wait for stabilization frames to allow camera hardware and firmware to
                # fully transition to the new AE state before collecting frames to verify
                frames_before_stabilize = context.frame_count
                log.d(f"Waiting {frames_between_toggles} frames for camera to stabilize after toggle...")
                if not wait_for_frames(context, frames_between_toggles):
                    log.e(f"Failed to collect {frames_between_toggles} stabilization frames after toggling to AE={current_ae_state}")
                    try:
                        depth_sensor.stop()
                        depth_sensor.close()
                    except RuntimeError:
                        pass
                    return False, context.frame_count, metadata_errors
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
                return False, context.frame_count, metadata_errors
            
            log.d(f"Toggle {toggle_idx}: collected {context.frame_count - frames_before} frames, total now {context.frame_count}")
            
            # Check metadata for frames received in this state
            metadata_errors += verify_ae_metadata_from_context(context, frames_before, context.frame_count, current_ae_state)
    
    finally:
        try:
            depth_sensor.stop()
            depth_sensor.close()
        except RuntimeError:
            pass  # Sensor may have already stopped

    log.i(f"Received {context.frame_count} total frames, metadata errors: {metadata_errors}")

    # Success if we got enough frames
    success = context.frame_count >= (num_toggles * frames_per_state)
    return success, context.frame_count, metadata_errors

################################################################################################

# Check if auto_exposure_mode option is supported (device-specific capability)
ae_mode_supported = depth_sensor.supports(rs.option.auto_exposure_mode)

test.start("Verify AUTO_EXPOSURE metadata with AE enabled")
# Test 1: Verify that AUTO_EXPOSURE metadata is non-zero when enable_auto_exposure is True.
# Tests with default AE mode and optionally with ACCELERATED mode (if supported).
# This validates that metadata correctly reports AE as active.

# Enable auto-exposure with default mode and verify the option is set
depth_sensor.set_option(rs.option.enable_auto_exposure, True)
test.check_equal(bool(depth_sensor.get_option(rs.option.enable_auto_exposure)), True)

# Capture frames and verify AUTO_EXPOSURE metadata is non-zero (indicating AE is active)
matches, total = verify_ae_metadata(depth_sensor, expected_ae_enabled=True, num_frames=10)
log.d(f"AE enabled (default mode): {matches}/{total} frames had correct AUTO_EXPOSURE metadata")
test.check(matches == total, f"Expected all {total} frames to have AUTO_EXPOSURE metadata != 0, got {matches}")

# If accelerated mode is supported, test with ACCELERATED mode as well
if ae_mode_supported:
    depth_sensor.set_option(rs.option.auto_exposure_mode, ACCELERATED)
    test.check_equal(depth_sensor.get_option(rs.option.auto_exposure_mode), ACCELERATED)
    
    matches_accel, total_accel = verify_ae_metadata(depth_sensor, expected_ae_enabled=True, num_frames=10)
    log.d(f"AE enabled (ACCELERATED mode): {matches_accel}/{total_accel} frames had correct AUTO_EXPOSURE metadata")
    test.check(matches_accel == total_accel, f"Expected all {total_accel} frames to have AUTO_EXPOSURE metadata != 0 with ACCELERATED mode, got {matches_accel}")

test.finish()

################################################################################################

test.start("Verify AUTO_EXPOSURE metadata with AE disabled")
# Test 2: Verify that AUTO_EXPOSURE metadata is zero when enable_auto_exposure is False.
# This validates that metadata correctly reports AE as inactive.

# Disable auto-exposure and verify the option is set
depth_sensor.set_option(rs.option.enable_auto_exposure, False)
test.check_equal(bool(depth_sensor.get_option(rs.option.enable_auto_exposure)), False)

# Capture frames and verify AUTO_EXPOSURE metadata is zero (indicating AE is inactive)
matches, total = verify_ae_metadata(depth_sensor, expected_ae_enabled=False, num_frames=10)
log.d(f"AE disabled: {matches}/{total} frames had correct AUTO_EXPOSURE metadata")
test.check(matches == total, f"Expected all {total} frames to have AUTO_EXPOSURE metadata == 0, got {matches}")

test.finish()

################################################################################################

test.start("Rapid AE toggle - verify metadata correctness")
# Test 3: Verify that AUTO_EXPOSURE metadata correctly tracks enable_auto_exposure state changes
# during continuous streaming with multiple rapid toggles.
#
# This test performs NUM_TOGGLES rapid AE state changes during a single streaming session,
# collecting FRAMES_PER_STATE frames after each toggle (with FRAMES_BETWEEN_TOGGLES stabilization
# frames between toggles). Tests both default and ACCELERATED modes (if supported) to ensure
# metadata accuracy across different AE algorithm implementations.

# Test with default AE mode
success, total_frames, metadata_errors = toggle_ae_while_streaming(depth_sensor, num_toggles=NUM_TOGGLES, frames_per_state=FRAMES_PER_STATE, frames_between_toggles=FRAMES_BETWEEN_TOGGLES)

test.check(success, "Camera should handle rapid AE toggles without stalling (default mode)")
test.check(total_frames >= NUM_TOGGLES * FRAMES_PER_STATE, f"Expected at least {NUM_TOGGLES * FRAMES_PER_STATE} frames during rapid toggle, got {total_frames}")
test.check(metadata_errors == 0, f"AUTO_EXPOSURE metadata should match enable_auto_exposure state, got {metadata_errors} mismatches")

# If ACCELERATED mode is supported, repeat the test with ACCELERATED mode
if ae_mode_supported:
    depth_sensor.set_option(rs.option.auto_exposure_mode, ACCELERATED)
    test.check_equal(depth_sensor.get_option(rs.option.auto_exposure_mode), ACCELERATED)
    
    success_accel, total_frames_accel, metadata_errors_accel = toggle_ae_while_streaming(depth_sensor, num_toggles=NUM_TOGGLES, frames_per_state=FRAMES_PER_STATE, frames_between_toggles=FRAMES_BETWEEN_TOGGLES)
    
    test.check(success_accel, "Camera should handle rapid AE toggles without stalling (ACCELERATED mode)")
    test.check(total_frames_accel >= NUM_TOGGLES * FRAMES_PER_STATE, f"Expected at least {NUM_TOGGLES * FRAMES_PER_STATE} frames during rapid toggle with ACCELERATED mode, got {total_frames_accel}")
    test.check(metadata_errors_accel == 0, f"AUTO_EXPOSURE metadata should match enable_auto_exposure state with ACCELERATED mode, got {metadata_errors_accel} mismatches")

test.finish()

################################################################################################
# Cleanup and exit
test.print_results_and_exit()
