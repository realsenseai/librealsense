# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
#
# test:device each(D400*)
#
# Validates emitter control functionality on D400 devices by verifying:
# 1. Emitter can be enabled and disabled via emitter_enabled option
# 2. Emitter_on_off option can be set (if supported)
# 3. IR frame contrast and metadata correctly reflect emitter state
# 4. Emitter state persists across stream start/stop cycles
# 5. Camera remains stable during rapid emitter toggling
# 6. Emitter_on_off alternates emitter state frame-by-frame (if supported)
#
# Test methodology:
# - Uses IR frame analysis (pixel contrast/standard deviation) to detect emitter state
# - Higher contrast = structured light pattern (emitter ON)
# - Lower contrast = ambient light only (emitter OFF)
# - Calibrates contrast threshold by measuring ON/OFF states
# - Validates frame metadata (frame_emitter_mode, frame_laser_power) when available
#
# Configuration flags:
# - USE_AUTO_EXPOSURE: Use auto exposure vs manual (1/6 frame time)
# - ENABLE_VISUAL_VERIFICATION: Display IR frames with cv2 for visual inspection
# - CV2_WAIT_KEY_MS: Duration to display visualization windows
# - CONTRAST_DIFF_THRESHOLD: Minimum contrast difference to distinguish emitter states
#
# Requirements:
# - Device must support emitter_enabled option
# - IR stream profile must be available

import pyrealsense2 as rs
from rspy import test, log
import time
import numpy as np
import cv2

# Flag to use auto exposure (default is manual exposure at 1/6 frame time)
USE_AUTO_EXPOSURE = False

# Flag to enable visual verification of IR frames
ENABLE_VISUAL_VERIFICATION = False

# Duration (in milliseconds) to display visualization windows (0 = wait for key press)
CV2_WAIT_KEY_MS = 3000

# Minimum contrast difference threshold to reliably distinguish emitter ON/OFF states
CONTRAST_DIFF_THRESHOLD = 25.0

device, _ = test.find_first_device_or_exit()
depth_sensor = device.first_depth_sensor()

# Check if emitter_enabled option is supported
if not depth_sensor.supports(rs.option.emitter_enabled):
    log.i("Device does not support emitter_enabled option, skipping test...")
    test.print_results_and_exit()

# Helper to capture frames and analyze depth/IR characteristics
def capture_and_analyze_frames(depth_sensor, expected_emitter_state, num_frames=10):
    """
    Open streaming session, capture IR frames, and analyze emitter state.
    
    This function:
    1. Opens IR profile and starts streaming
    2. Configures exposure (auto or manual at 1/6 frame time)
    3. Sets emitter state after streaming starts (required for control to take effect)
    4. Captures frames after stabilization period
    5. Analyzes IR pixel contrast (std dev) and frame metadata
    
    Args:
        depth_sensor: Sensor to stream from
        expected_emitter_state: 1 for emitter ON, 0 for emitter OFF
        num_frames: Number of frames to capture and analyze (default 10)
    
    Returns:
        Tuple of (option_matches, ir_contrast, ir_mean_intensity, metadata_matches, metadata_fraction)
        - option_matches: True if get_option matches expected state
        - ir_contrast: Mean IR pixel standard deviation across frames
        - ir_mean_intensity: Mean IR pixel intensity across frames
        - metadata_matches: True if majority of frames report expected emitter state
        - metadata_fraction: Fraction of frames with metadata available (0.0-1.0)
    """
    depth_profile = next((p for p in depth_sensor.profiles 
                         if p.stream_type() == rs.stream.depth 
                         and p.format() == rs.format.z16), None)
    
    ir_profile = next((p for p in depth_sensor.profiles 
                      if p.stream_type() == rs.stream.infrared 
                      and p.format() in [rs.format.y8]), None)
    
    if not depth_profile:
        log.w("No depth profile found")
        return False, 0.0, 0.0
    
    if not ir_profile:
        log.w("No IR profile found - cannot verify emitter state with IR frames")
        return False, 0.0, 0.0
    
    received_framesets = []
    
    def frame_callback(frame):
        received_framesets.append(frame)
    
    # Open only IR profile for emitter verification
    depth_sensor.open(ir_profile)
        
    # Configure exposure mode
    if USE_AUTO_EXPOSURE:
        if depth_sensor.supports(rs.option.enable_auto_exposure):
            depth_sensor.set_option(rs.option.enable_auto_exposure, 1.0)
    else:
        # Set manual exposure to 1/6 of frame time
        if depth_sensor.supports(rs.option.exposure):
            fps = ir_profile.fps()
            frame_time_us = (1.0 / fps) * 1000000
            exposure_value = frame_time_us / 6.0
            depth_sensor.set_option(rs.option.exposure, exposure_value)

    # Start streaming
    depth_sensor.start(frame_callback)

    # Wait a second for streaming to stabilize before changing emitter
    time.sleep(1)
    
    # Set emitter state after streaming starts (required for emitter control to take effect)
    depth_sensor.set_option(rs.option.emitter_enabled, expected_emitter_state)
    
    # Discard initial frames to allow emitter state to stabilize (emitter takes 2-3 frames to change state)
    time.sleep(0.3)
    received_framesets.clear()
    
    # Wait for frames to accumulate with new emitter state
    timeout = 2.0
    start_time = time.time()
    while len(received_framesets) < num_frames and (time.time() - start_time) < timeout:
        time.sleep(0.01)
    
    depth_sensor.stop()
    depth_sensor.close()
    
    if len(received_framesets) < num_frames:
        log.w(f"Only received {len(received_framesets)} frames out of {num_frames} requested")
    
    # Verify option query
    current_emitter_state = depth_sensor.get_option(rs.option.emitter_enabled)
    option_matches = (current_emitter_state == expected_emitter_state)
    
    # Analyze IR pixel contrast (standard deviation) and mean intensity
    ir_contrasts = []
    ir_intensities = []
    ir_images_to_show = []
    # Collect per-frame emitter metadata when available
    md_vals = []
    
    for idx, frame in enumerate(received_framesets[:num_frames]):
        # Extract IR frame from frameset or single frame
        ir_frame = None
        
        if frame.is_frameset():
            fs = frame.as_frameset()
            ir_frame = fs.get_infrared_frame(1) or fs.get_infrared_frame()
        elif frame.is_video_frame():
            profile = frame.get_profile()
            if profile.stream_type() == rs.stream.infrared:
                ir_frame = frame
        
        if ir_frame:
            ir_data = np.asanyarray(ir_frame.get_data()).astype(np.float32)
            contrast = np.std(ir_data)  # Higher std dev = structured light pattern (emitter on)
            intensity = np.mean(ir_data)
            ir_contrasts.append(contrast)
            ir_intensities.append(intensity)

            # Try reading emitter-related metadata for this frame (if supported)
            md_val = None
            try:
                if ir_frame.supports_frame_metadata(rs.frame_metadata_value.frame_emitter_mode):
                    md_val = ir_frame.get_frame_metadata(rs.frame_metadata_value.frame_emitter_mode)
                elif ir_frame.supports_frame_metadata(rs.frame_metadata_value.frame_laser_power_mode):
                    md_val = ir_frame.get_frame_metadata(rs.frame_metadata_value.frame_laser_power_mode)
            except Exception:
                md_val = None

            md_vals.append(md_val)

            # Save first 3 and last frame for visualization
            if idx < 3 or idx == num_frames - 1:
                ir_normalized = cv2.normalize(ir_data, None, 0, 255, cv2.NORM_MINMAX, dtype=cv2.CV_8U)
                ir_images_to_show.append((ir_normalized, contrast, intensity, idx))
    
    if not ir_contrasts:
        log.w(f"No IR frames captured! Received {len(received_framesets)} framesets")
    
    # Display IR frames for visual verification (if enabled)
    if ENABLE_VISUAL_VERIFICATION and ir_images_to_show:
        emitter_state_str = 'ON' if expected_emitter_state == 1 else 'OFF'
        for img, contrast, intensity, frame_idx in ir_images_to_show:
            window_name = f"Frame {frame_idx} - Emitter {emitter_state_str} (std={contrast:.1f})"
            cv2.imshow(window_name, img)
        
        cv2.waitKey(CV2_WAIT_KEY_MS)
        cv2.destroyAllWindows()
    
    ir_contrast = np.mean(ir_contrasts) if ir_contrasts else 0.0
    ir_mean_intensity = np.mean(ir_intensities) if ir_intensities else 0.0

    # Evaluate metadata: consider metadata indicating 'on' when value != 0
    metadata_present = [m is not None for m in md_vals]
    metadata_count = sum(1 for p in metadata_present if p)
    metadata_fraction = (metadata_count / len(md_vals)) if md_vals else 0.0
    metadata_on = [1 if (m is not None and int(m) != 0) else 0 for m in md_vals]
    # Majority vote whether metadata reports emitter ON
    metadata_matches = False
    if md_vals:
        on_count = sum(metadata_on)
        metadata_matches = (on_count >= (len(metadata_on) / 2)) if len(metadata_on) > 0 else False

    return option_matches, ir_contrast, ir_mean_intensity, metadata_matches, metadata_fraction

################################################################################################

test.start("Verify emitter can be enabled and disabled")
# Test 1: Basic emitter control - verify option can be set and queried
# This tests the fundamental emitter_enabled option functionality without streaming

# Test emitter ON
depth_sensor.set_option(rs.option.emitter_enabled, 1)
test.check_equal(depth_sensor.get_option(rs.option.emitter_enabled), 1.0)

# Test emitter OFF
depth_sensor.set_option(rs.option.emitter_enabled, 0)
test.check_equal(depth_sensor.get_option(rs.option.emitter_enabled), 0.0)

# Test emitter ON again
depth_sensor.set_option(rs.option.emitter_enabled, 1)
test.check_equal(depth_sensor.get_option(rs.option.emitter_enabled), 1.0)
test.finish()

################################################################################################

test.start("Verify emitter_on_off option when supported")
# Test 2: Verify emitter_on_off option (if available)
# The emitter_on_off option enables frame-by-frame emitter alternation for some devices.
# This is a basic set/get test - detailed alternation behavior is tested in test 6.

if depth_sensor.supports(rs.option.emitter_on_off):
    # Try enable emitter_on_off (alternating emitter each frame)
    orig = depth_sensor.get_option(rs.option.emitter_on_off)
    try:
        try:
            depth_sensor.set_option(rs.option.emitter_on_off, 1)
            test.check(depth_sensor.get_option(rs.option.emitter_on_off) == 1)
            depth_sensor.set_option(rs.option.emitter_on_off, 0)
            test.check(depth_sensor.get_option(rs.option.emitter_on_off) == 0)
        except RuntimeError as e:
            # Hardware / firmware may reject this control; log and skip this quick-option check
            log.w(f"Could not set emitter_on_off option: {e}; skipping quick emitter_on_off check")
    finally:
        # Restore original
        try:
            depth_sensor.set_option(rs.option.emitter_on_off, orig)
        except Exception:
            # best-effort restore; ignore errors during restore
            log.w("Failed to restore original emitter_on_off option value")
else:
    log.i("Device does not support emitter_on_off option, skipping this check")

test.finish()

################################################################################################

test.start("Calibrate emitter ON/OFF contrast threshold")
# Test 3: Calibrate IR contrast threshold for emitter state detection
# 
# Methodology:
# - Measure IR pixel contrast (standard deviation) with emitter ON and OFF
# - Calculate threshold as midpoint between the two measurements
# - Higher contrast indicates structured light pattern (emitter ON)
# - Lower contrast indicates ambient light only (emitter OFF)
# - Validates that the difference is large enough (>CONTRAST_DIFF_THRESHOLD) for reliable detection
# - If available, cross-validates with frame metadata (frame_emitter_mode, frame_laser_power_mode)

log.d("Measuring IR contrast with emitter ON...")
option_ok_on, contrast_on, ir_on, md_matches_on, md_frac_on = capture_and_analyze_frames(depth_sensor, expected_emitter_state=1, num_frames=10)

log.d("Measuring IR contrast with emitter OFF...")
option_ok_off, contrast_off, ir_off, md_matches_off, md_frac_off = capture_and_analyze_frames(depth_sensor, expected_emitter_state=0, num_frames=10)

log.d(f"Emitter ON: IR contrast (std)={contrast_on:.2f}, mean={ir_on:.1f}, metadata_on={md_matches_on} (frac={md_frac_on:.2f})")
log.d(f"Emitter OFF: IR contrast (std)={contrast_off:.2f}, mean={ir_off:.1f}, metadata_on={md_matches_off} (frac={md_frac_off:.2f})")

test.check(option_ok_on, "Option query should return emitter=ON")
test.check(option_ok_off, "Option query should return emitter=OFF")

# Calculate threshold as midpoint between ON and OFF states
if contrast_on > 0 and contrast_off > 0:
    contrast_threshold = (contrast_on + contrast_off) / 2.0
    contrast_diff = abs(contrast_on - contrast_off)
    log.d(f"Contrast difference: {contrast_diff:.2f}")
    log.i(f"Using threshold: {contrast_threshold:.2f} (midpoint between ON and OFF)")
    
    # Verify there's a measurable difference
    test.check(contrast_diff > CONTRAST_DIFF_THRESHOLD, 
              f"Contrast difference should be >{CONTRAST_DIFF_THRESHOLD} to reliably distinguish states, got {contrast_diff:.2f}")
    
    # Verify ON state is above threshold and OFF is below
    test.check(contrast_on > contrast_threshold,
              f"Emitter ON contrast ({contrast_on:.2f}) should be above threshold ({contrast_threshold:.2f})")
    test.check(contrast_off < contrast_threshold,
              f"Emitter OFF contrast ({contrast_off:.2f}) should be below threshold ({contrast_threshold:.2f})")

    # If metadata is available for a majority of frames, ensure it matches expected states
    if md_frac_on > 0.5 or md_frac_off > 0.5:
        test.check(md_matches_on, "Frame metadata majority should indicate emitter ON for ON measurement")
        test.check(not md_matches_off, "Frame metadata majority should indicate emitter OFF for OFF measurement")
    else:
        test.check(False, "Insufficient metadata available to verify emitter state")
else:
    log.e("Failed to measure contrast for both states")
    contrast_threshold = 50.0  # Fallback threshold
    test.check(False, "Could not calibrate contrast threshold")

test.finish()

################################################################################################

test.start("Verify emitter state using calibrated threshold")
# Test 4: Apply calibrated threshold to verify emitter state detection
# Uses the threshold from test 3 to validate emitter ON and OFF states
# can be reliably distinguished via IR frame analysis.

# Test ON state
_, contrast_test_on, _, md_matches_test_on, md_frac_test_on = capture_and_analyze_frames(depth_sensor, expected_emitter_state=1, num_frames=10)
detected_on = contrast_test_on > contrast_threshold
test.check(detected_on, f"Emitter ON should be detected (contrast {contrast_test_on:.2f} > threshold {contrast_threshold:.2f})")
if md_frac_test_on > 0.5:
    test.check(md_matches_test_on, "Frame metadata should indicate emitter ON for ON verification")

# Test OFF state
_, contrast_test_off, _, md_matches_test_off, md_frac_test_off = capture_and_analyze_frames(depth_sensor, expected_emitter_state=0, num_frames=10)
detected_off = contrast_test_off < contrast_threshold
test.check(detected_off, f"Emitter OFF should be detected (contrast {contrast_test_off:.2f} < threshold {contrast_threshold:.2f})")
if md_frac_test_off > 0.5:
    test.check(not md_matches_test_off, "Frame metadata should indicate emitter OFF for OFF verification")

test.finish()

################################################################################################

test.start("Toggle emitter while streaming - verify camera stability")
# Test 5: Rapid emitter toggling during streaming - verify no frame drops or stalls
#
# This test validates that:
# - Camera continues streaming smoothly during emitter state changes
# - Frame gaps remain within acceptable bounds (< 4x frame time at 30fps)
# - Emitter state can be detected in captured frames (informational only)
#
# Uses 30fps IR profile for consistent frame timing and reliable gap measurement.

# Select 30fps IR profile for consistent timing
ir_profile = next((p for p in depth_sensor.profiles 
                   if p.stream_type() == rs.stream.infrared 
                   and p.format() in [rs.format.y8]
                   and p.fps() == 30), None)

if not ir_profile:
    log.e("No 30fps IR profile found. Test requires 30fps for consistent frame timing.")
    test.check(False, "30fps IR profile required")
    test.finish()
else:
    # Initialize frame tracking for this test
    # Using lists to avoid global variable issues in callback
    received_frames = []            # All frames received during test
    frame_gaps = []                 # Inter-frame time gaps for stability check
    last_frame_time = [time.time()] # Most recent frame timestamp

    def frame_callback(frame):
        current_time = time.time()
        gap = current_time - last_frame_time[0]
        frame_gaps.append(gap)
        last_frame_time[0] = current_time
        received_frames.append(frame)

    depth_sensor.open(ir_profile)
    depth_sensor.start(frame_callback)

    # Configure exposure mode
    if USE_AUTO_EXPOSURE:
        if depth_sensor.supports(rs.option.enable_auto_exposure):
            depth_sensor.set_option(rs.option.enable_auto_exposure, 1.0)
    else:
        # Set manual exposure to 1/6 of frame time
        if depth_sensor.supports(rs.option.exposure):
            fps = ir_profile.fps()
            frame_time_us = (1.0 / fps) * 1000000
            exposure_value = frame_time_us / 6.0
            depth_sensor.set_option(rs.option.exposure, exposure_value)

    # Set laser power to max for clear emitter detection
    if depth_sensor.supports(rs.option.laser_power):
        laser_range = depth_sensor.get_option_range(rs.option.laser_power)
        depth_sensor.set_option(rs.option.laser_power, laser_range.max)

    # Toggle emitter multiple times and verify each state
    test_duration = 3.0
    toggle_interval = 0.5  # Longer interval to allow state to stabilize
    start_time = time.time()
    toggle_count = 0
    current_emitter = 1
    state_verifications = []

    try:
        depth_sensor.set_option(rs.option.emitter_enabled, current_emitter)

        # Wait 2 seconds and reset frame tracking variables for this test
        time.sleep(2)
        received_frames = []
        frame_gaps = []  # Track all frame gaps
        last_frame_time = [time.time()]  # Use list to avoid global declaration issues
       
        while (time.time() - start_time) < test_duration:
            if (time.time() - start_time) > (toggle_count * toggle_interval):
                # Toggle state
                current_emitter = 1 - current_emitter
                depth_sensor.set_option(rs.option.emitter_enabled, current_emitter)
                
                # Wait longer for state to stabilize (emitter takes 2-3 frames)
                time.sleep(0.3)
                
                # Analyze recent frames without consuming them
                frames_to_analyze = received_frames[-5:] if len(received_frames) >= 5 else received_frames[:]
                
                # Analyze frames to verify emitter state
                contrasts = []
                for frame in frames_to_analyze:
                    if frame.is_video_frame():
                        ir_data = np.asanyarray(frame.get_data()).astype(np.float32)
                        contrast = np.std(ir_data)
                        contrasts.append(contrast)
                
                if contrasts:
                    avg_contrast = np.mean(contrasts)
                    detected_state = 1 if avg_contrast > contrast_threshold else 0
                    matches = (detected_state == current_emitter)
                    state_verifications.append((current_emitter, detected_state, avg_contrast, matches))
                
                toggle_count += 1
            time.sleep(0.01)
    finally:
        depth_sensor.stop()
        depth_sensor.close()

    # Check camera stability and stalleness during toggling
    fps = ir_profile.fps()
    expected_frame_time = 1.0 / fps
    max_acceptable_gap = expected_frame_time * 3  # Allow up to 3 frame times for gap tolerance
    max_frame_gap = max(frame_gaps) if frame_gaps else 0.0

    test.check(len(received_frames) > 10, f"Should receive substantial frames during toggle test, got {len(received_frames)}")
    test.check(max_frame_gap < max_acceptable_gap, 
              f"Max frame gap should be < {max_acceptable_gap:.3f}s (3x frame time at {fps}fps), got {max_frame_gap:.3f}s")
    # Log emitter state detection results (informational only)
    if state_verifications:
        correct_detections = sum(1 for _, _, _, matches in state_verifications if matches)
        total_checks = len(state_verifications)
        detection_rate = (correct_detections / total_checks * 100) if total_checks > 0 else 0
        log.d(f"Emitter state detection: {correct_detections}/{total_checks} ({detection_rate:.0f}%)")
        
        if detection_rate < 50:
            log.w(f"Low detection rate - emitter may not be controllable while streaming IR on this device")

    test.finish()

################################################################################################

test.start("Verify emitter state persists across stream start/stop")
# Set emitter OFF
depth_sensor.set_option(rs.option.emitter_enabled, 0)
test.check_equal(depth_sensor.get_option(rs.option.emitter_enabled), 0.0)

# Start and stop streaming
depth_profile = next(p for p in depth_sensor.profiles 
                    if p.stream_type() == rs.stream.depth 
                    and p.format() == rs.format.z16)
depth_sensor.open(depth_profile)
depth_sensor.start(lambda f: None)

# Configure exposure mode
if USE_AUTO_EXPOSURE:
    if depth_sensor.supports(rs.option.enable_auto_exposure):
        depth_sensor.set_option(rs.option.enable_auto_exposure, 1.0)
else:
    # Set manual exposure to 1/6 of frame time
    if depth_sensor.supports(rs.option.exposure):
        fps = depth_profile.fps()
        frame_time_us = (1.0 / fps) * 1000000
        exposure_value = frame_time_us / 6.0
        depth_sensor.set_option(rs.option.exposure, exposure_value)

time.sleep(0.5)
depth_sensor.stop()
depth_sensor.close()

# Verify emitter is still OFF
test.check_equal(depth_sensor.get_option(rs.option.emitter_enabled), 0.0, 
                "Emitter state should persist after streaming")

# Set emitter ON and test again
depth_sensor.set_option(rs.option.emitter_enabled, 1)
depth_sensor.open(depth_profile)
depth_sensor.start(lambda f: None)

# Configure exposure mode
if USE_AUTO_EXPOSURE:
    if depth_sensor.supports(rs.option.enable_auto_exposure):
        depth_sensor.set_option(rs.option.enable_auto_exposure, 1.0)
else:
    # Set manual exposure to 1/6 of frame time
    if depth_sensor.supports(rs.option.exposure):
        fps = depth_profile.fps()
        frame_time_us = (1.0 / fps) * 1000000
        exposure_value = frame_time_us / 6.0
        depth_sensor.set_option(rs.option.exposure, exposure_value)

time.sleep(0.5)
depth_sensor.stop()
depth_sensor.close()

test.check_equal(depth_sensor.get_option(rs.option.emitter_enabled), 1.0,
                "Emitter state should persist after streaming")
test.finish()

################################################################################################

test.start("Verify emitter_on_off alternates emitter state while streaming")
if depth_sensor.supports(rs.option.emitter_on_off):
    # Open IR profile and stream
    ir_profile = next((p for p in depth_sensor.profiles 
                       if p.stream_type() == rs.stream.infrared 
                       and p.format() in [rs.format.y8]), None)
    if not ir_profile:
        log.w("No IR profile available for emitter_on_off test, skipping")
    else:
        contrasts = []
        frames = []
        laser_powers = []

        def cb(f):
            if f.is_video_frame():
                frames.append(f)

        depth_sensor.open(ir_profile)
        depth_sensor.start(cb)

        # Configure exposure mode
        if USE_AUTO_EXPOSURE:
            if depth_sensor.supports(rs.option.enable_auto_exposure):
                depth_sensor.set_option(rs.option.enable_auto_exposure, 1.0)
        else:
            # Set manual exposure to 1/6 of frame time
            if depth_sensor.supports(rs.option.exposure):
                fps = ir_profile.fps()
                frame_time_us = (1.0 / fps) * 1000000
                exposure_value = frame_time_us / 6.0
                depth_sensor.set_option(rs.option.exposure, exposure_value)

        # Set laser power to 1/4 of maximum for emitter_on_off test
        if depth_sensor.supports(rs.option.laser_power):
            laser_range = depth_sensor.get_option_range(rs.option.laser_power)
            laser_power_value = laser_range.max / 4.0
            depth_sensor.set_option(rs.option.laser_power, laser_power_value)
            log.d(f"Set laser power to {laser_power_value:.1f} (1/4 of max {laser_range.max}) for emitter_on_off test")

        # Enable emitter_on_off (alternate emitter each frame)
        orig = depth_sensor.get_option(rs.option.emitter_on_off)
        skip_alternation = False
        try:
            try:
                depth_sensor.set_option(rs.option.emitter_on_off, 1)
            except RuntimeError as e:
                log.w(f"Device refused emitter_on_off command while streaming: {e}; skipping alternation test")
                skip_alternation = True

            if not skip_alternation:
                # Give time to stabilize and collect frames
                time.sleep(0.5)
                frames.clear()

                # collect up to 60 frames
                timeout = time.time() + 2.0
                while len(frames) < 60 and time.time() < timeout:
                    time.sleep(0.01)
            else:
                log.i("Skipping alternation frame collection because device refused emitter_on_off")

            # Compute contrast and read laser power metadata per frame
            for fr in frames[:60]:
                data = np.asanyarray(fr.get_data()).astype(np.float32)
                contrasts.append(np.std(data))
                
                # Try reading laser power metadata
                lp_val = None
                try:
                    if fr.supports_frame_metadata(rs.frame_metadata_value.frame_laser_power):
                        lp_val = fr.get_frame_metadata(rs.frame_metadata_value.frame_laser_power)
                except Exception:
                    lp_val = None
                laser_powers.append(lp_val)

            # If visual verification enabled, show first few frames with their contrast values
            if ENABLE_VISUAL_VERIFICATION and frames:
                for i, fr in enumerate(frames[:6]):
                    img = cv2.normalize(np.asanyarray(fr.get_data()).astype(np.float32), None, 0, 255, cv2.NORM_MINMAX, dtype=cv2.CV_8U)
                    state = "HIGH" if contrasts[i] > contrast_threshold else "LOW"
                    cv2.imshow(f"ON_OFF Frame {i} - {state} (std={contrasts[i]:.1f})", img)
                cv2.waitKey(CV2_WAIT_KEY_MS)
                cv2.destroyAllWindows()

            # Verify laser power metadata and contrast threshold agree with each other
            if contrasts:
                # Check if we have sufficient laser power metadata
                laser_numeric = [lp for lp in laser_powers if lp is not None]
                has_metadata = len(laser_numeric) >= len(contrasts) * 0.8  # 80% metadata available
                
                # Classify frames using threshold method (same as reference implementation)
                # Frame is ON if contrast >= threshold, OFF if contrast < threshold
                contrast_states = [1 if c >= contrast_threshold else 0 for c in contrasts]
                
                laser_states = [1 if (lp is not None and lp > 0) else 0 for lp in laser_powers[:len(contrasts)]]
                
                # Log classification results
                log.d(f"Calibrated contrast: ON={contrast_on:.2f}, OFF={contrast_off:.2f}, diff={contrast_diff:.2f}")
                log.d(f"Frame contrasts: {[f'{c:.1f}' for c in contrasts[:10]]}")
                log.d(f"Contrast-based states (1=ON/0=OFF): {contrast_states[:10]}")
                
                if has_metadata:
                    log.d(f"Laser power values: {[int(lp) if lp is not None else 'N/A' for lp in laser_powers[:10]]}")
                    log.d(f"Laser-based states (1=ON/0=OFF): {laser_states[:10]}")
                    
                    # Verify agreement between laser power metadata and contrast threshold
                    # Only compare frames where metadata is available
                    agreements = []
                    for i in range(len(contrasts)):
                        if laser_powers[i] is not None:
                            agrees = (contrast_states[i] == laser_states[i])
                            agreements.append(agrees)
                    
                    if agreements:
                        agreement_rate = sum(agreements) / len(agreements)
                        log.d(f"Laser power metadata and contrast threshold agreement: {agreement_rate:.2f} ({sum(agreements)}/{len(agreements)} frames)")
                        test.check(agreement_rate > 0.7, 
                                  f"Laser power metadata should agree with contrast threshold (>0.7), got {agreement_rate:.2f}")
                    
                    # Use laser power metadata as primary for alternation detection (more reliable)
                    emitter_states = laser_states
                    log.d("Using laser power metadata for alternation verification")
                else:
                    # Fall back to contrast-based classification
                    emitter_states = contrast_states
                    log.d("Using contrast threshold for alternation verification (limited metadata)")
                
                # Count alternations (transitions between ON and OFF)
                alternations = sum(1 for i in range(1, len(emitter_states)) if emitter_states[i] != emitter_states[i-1])
                alternation_rate = alternations / max(1, (len(emitter_states)-1))
                
                log.i(f"Emitter_on_off alternation rate: {alternation_rate:.2f} ({alternations}/{max(1, len(emitter_states)-1)} transitions)")
                
                # Verify alternation rate is high (expect most frames to alternate)
                test.check(alternation_rate > 0.6, 
                          f"Emitter_on_off should alternate frames (rate>0.6), got {alternation_rate:.2f}")
                
                # Additional check: verify we have both high and low states
                high_count = sum(emitter_states)
                low_count = len(emitter_states) - high_count
                test.check(high_count > 0 and low_count > 0, 
                          f"Should have both ON ({high_count}) and OFF ({low_count}) states")
            else:
                test.check(False, "No frames collected for emitter_on_off test")
        finally:
            try:
                depth_sensor.set_option(rs.option.emitter_on_off, orig)
            except Exception:
                log.w("Failed to restore emitter_on_off option after alternation test")
            depth_sensor.stop()
            depth_sensor.close()
else:
    log.i("Device does not support emitter_on_off, skipping streaming alternation test")

test.finish()

################################################################################################
test.print_results_and_exit()

