# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#
# test:device each(D400*)
#
# Validates emitter off functionality via laser power control on D400 devices by verifying:
# 1. Emitter ON/OFF contrast threshold can be calibrated for detection
# 2. Emitter is ON when streaming with default laser power level
# 3. Emitter is effectively OFF when laser power is set to 0
#
# Test methodology:
# - Uses IR frame analysis (pixel contrast/standard deviation) to detect emitter state
# - Higher contrast = structured light pattern (emitter ON)
# - Lower contrast = ambient light only (emitter OFF)
# - Calibrates contrast threshold by measuring ON/OFF states
# - Tests 1-2 validate frame metadata when available
# - Test 3 uses contrast-only detection (metadata may still show ON despite 0 power)
#
# Configuration flags:
# - USE_AUTO_EXPOSURE: Use auto exposure vs manual (1/6 frame time); overridden to True for D415
# - ENABLE_VISUAL_VERIFICATION: Display IR frames with cv2 for visual inspection
# - CV2_WAIT_KEY_MS: Duration to display visualization windows
# - CONTRAST_DIFF_THRESHOLD: Minimum contrast difference to distinguish emitter states; overridden to 5.0 for D415
#
# Requirements:
# - Device must support emitter_enabled option
# - Device must support laser_power option
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

# Override USE_AUTO_EXPOSURE and CONTRAST_DIFF_THRESHOLD for D415 devices
device_name = device.get_info(rs.camera_info.name)
if 'D415' in device_name:
    USE_AUTO_EXPOSURE = True
    CONTRAST_DIFF_THRESHOLD = 5.0
    log.i(f"D415 detected - overriding USE_AUTO_EXPOSURE to True and CONTRAST_DIFF_THRESHOLD to 5.0")

# Check if emitter_enabled option is supported
if not depth_sensor.supports(rs.option.emitter_enabled):
    log.i("Device does not support emitter_enabled option, skipping test...")
    test.print_results_and_exit()

# Check if laser_power option is supported
if not depth_sensor.supports(rs.option.laser_power):
    log.i("Device does not support laser_power option, skipping test...")
    test.print_results_and_exit()

# Helper functions for frame capture and analysis

def configure_sensor_exposure(depth_sensor, ir_profile):
    """Configure sensor exposure mode (auto or manual)."""
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

def extract_ir_frame(frame):
    """Extract IR frame from frameset or single frame."""
    if frame.is_frameset():
        fs = frame.as_frameset()
        return fs.get_infrared_frame(1) or fs.get_infrared_frame()
    elif frame.is_video_frame():
        profile = frame.get_profile()
        if profile.stream_type() == rs.stream.infrared:
            return frame
    return None

def read_frame_metadata(ir_frame):
    """Read emitter-related metadata from frame if available."""
    try:
        if ir_frame.supports_frame_metadata(rs.frame_metadata_value.frame_emitter_mode):
            return ir_frame.get_frame_metadata(rs.frame_metadata_value.frame_emitter_mode)
        elif ir_frame.supports_frame_metadata(rs.frame_metadata_value.frame_laser_power_mode):
            return ir_frame.get_frame_metadata(rs.frame_metadata_value.frame_laser_power_mode)
        elif ir_frame.supports_frame_metadata(rs.frame_metadata_value.frame_laser_power):
            return ir_frame.get_frame_metadata(rs.frame_metadata_value.frame_laser_power)
    except Exception:
        pass
    return None

def analyze_ir_frames(received_framesets, num_frames, expected_emitter_state):
    """
    Analyze IR frames for contrast, intensity, and metadata.
    
    Extracts IR data from each frame, computes pixel standard deviation (contrast)
    and mean intensity, reads metadata, and prepares visualization samples.
    
    Args:
        received_framesets: List of captured frames
        num_frames: Maximum number of frames to analyze
        expected_emitter_state: Expected emitter state (for visualization labeling)
    
    Returns:
        Tuple of (ir_contrasts, ir_intensities, md_vals, ir_images_to_show)
    """
    ir_contrasts = []
    ir_intensities = []
    md_vals = []
    ir_images_to_show = []
    
    for idx, frame in enumerate(received_framesets[:num_frames]):
        ir_frame = extract_ir_frame(frame)
        
        if ir_frame:
            ir_data = np.asanyarray(ir_frame.get_data()).astype(np.float32)
            contrast = np.std(ir_data)
            intensity = np.mean(ir_data)
            ir_contrasts.append(contrast)
            ir_intensities.append(intensity)
            
            md_vals.append(read_frame_metadata(ir_frame))
            
            # Save first 3 and last frame for visualization
            if idx < 3 or idx == num_frames - 1:
                ir_normalized = cv2.normalize(ir_data, None, 0, 255, cv2.NORM_MINMAX, dtype=cv2.CV_8U)
                ir_images_to_show.append((ir_normalized, contrast, intensity, idx))
    
    return ir_contrasts, ir_intensities, md_vals, ir_images_to_show

def display_ir_frames_for_verification(ir_images_to_show, expected_emitter_state):
    """Display IR frames for visual verification if enabled."""
    if ENABLE_VISUAL_VERIFICATION and ir_images_to_show:
        emitter_state_str = 'ON' if expected_emitter_state == 1 else 'OFF'
        for img, contrast, intensity, frame_idx in ir_images_to_show:
            window_name = f"Frame {frame_idx} - Emitter {emitter_state_str} (std={contrast:.1f})"
            cv2.imshow(window_name, img)
        cv2.waitKey(CV2_WAIT_KEY_MS)
        cv2.destroyAllWindows()

def evaluate_metadata(md_vals):
    """
    Evaluate metadata to determine emitter state via majority voting.
    
    Metadata value != 0 indicates emitter ON, == 0 indicates OFF.
    Uses majority voting across all frames with metadata available.
    
    Args:
        md_vals: List of metadata values (may contain None for unavailable metadata)
    
    Returns:
        Tuple of (metadata_matches, metadata_fraction)
        - metadata_matches: True if majority indicates emitter ON
        - metadata_fraction: Fraction of frames with metadata (0.0-1.0)
    """
    metadata_present = [m is not None for m in md_vals]
    metadata_count = sum(1 for p in metadata_present if p)
    metadata_fraction = (metadata_count / len(md_vals)) if md_vals else 0.0
    
    metadata_on = [1 if (m is not None and int(m) != 0) else 0 for m in md_vals]
    metadata_matches = False
    if md_vals:
        on_count = sum(metadata_on)
        metadata_matches = (on_count >= (len(metadata_on) / 2)) if len(metadata_on) > 0 else False
    
    return metadata_matches, metadata_fraction

def capture_and_analyze_frames(depth_sensor, expected_emitter_state, num_frames=10, laser_power_value=None):
    """
    Capture and analyze IR frames to detect emitter state.
    
    Orchestrates the complete workflow:
    1. Opens IR streaming profile
    2. Configures exposure via configure_sensor_exposure()
    3. Sets emitter state and optional laser power
    4. Captures frames after stabilization period
    5. Analyzes frames via analyze_ir_frames() for contrast and metadata
    6. Evaluates results via evaluate_metadata()
    
    Args:
        depth_sensor: Sensor to stream from
        expected_emitter_state: 1 for emitter ON, 0 for emitter OFF
        num_frames: Number of frames to capture and analyze (default 10)
        laser_power_value: Laser power value to set (None to use current setting)
    
    Returns:
        Tuple of (option_matches, ir_contrast, ir_mean_intensity, metadata_matches, 
                  metadata_fraction, laser_power_readback)
    """
    depth_profile = next((p for p in depth_sensor.profiles 
                         if p.stream_type() == rs.stream.depth 
                         and p.format() == rs.format.z16), None)
    
    ir_profile = next((p for p in depth_sensor.profiles 
                      if p.stream_type() == rs.stream.infrared 
                      and p.format() in [rs.format.y8]), None)
    
    if not depth_profile:
        log.w("No depth profile found")
        return False, 0.0, 0.0, False, 0.0, None
    
    if not ir_profile:
        log.w("No IR profile found - cannot verify emitter state with IR frames")
        return False, 0.0, 0.0, False, 0.0, None
    
    received_framesets = []
    
    def frame_callback(frame):
        received_framesets.append(frame)
    
    # Open only IR profile for emitter verification
    depth_sensor.open(ir_profile)
    configure_sensor_exposure(depth_sensor, ir_profile)
    
    # Start streaming
    depth_sensor.start(frame_callback)
    
    # Wait for streaming to stabilize before changing emitter
    time.sleep(1)
    
    # Set emitter state after streaming starts (required for emitter control to take effect)
    depth_sensor.set_option(rs.option.emitter_enabled, expected_emitter_state)
    
    # Set laser power if provided
    if laser_power_value is not None:
        depth_sensor.set_option(rs.option.laser_power, laser_power_value)
        log.d(f"Set laser power to {laser_power_value}")
    
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
    
    # Read back laser power
    laser_power_readback = None
    if depth_sensor.supports(rs.option.laser_power):
        laser_power_readback = depth_sensor.get_option(rs.option.laser_power)
    
    # Analyze IR frames
    ir_contrasts, ir_intensities, md_vals, ir_images_to_show = analyze_ir_frames(
        received_framesets, num_frames, expected_emitter_state
    )
    
    if not ir_contrasts:
        log.w(f"No IR frames captured! Received {len(received_framesets)} framesets")
    
    # Display IR frames for visual verification (if enabled)
    display_ir_frames_for_verification(ir_images_to_show, expected_emitter_state)
    
    # Calculate average contrast and intensity
    ir_contrast = np.mean(ir_contrasts) if ir_contrasts else 0.0
    ir_mean_intensity = np.mean(ir_intensities) if ir_intensities else 0.0
    
    # Evaluate metadata
    metadata_matches, metadata_fraction = evaluate_metadata(md_vals)

    return option_matches, ir_contrast, ir_mean_intensity, metadata_matches, metadata_fraction, laser_power_readback

################################################################################################

test.start("Calibrate emitter ON/OFF contrast threshold")
# Test 1: Calibrate IR contrast threshold for emitter state detection
# 
# Methodology:
# - Measure IR pixel contrast (standard deviation) with emitter ON and OFF
# - Calculate threshold as midpoint between the two measurements
# - Higher contrast indicates structured light pattern (emitter ON)
# - Lower contrast indicates ambient light only (emitter OFF)
# - Validates that the difference is large enough (>CONTRAST_DIFF_THRESHOLD) for reliable detection
# - If available, cross-validates with frame metadata (frame_emitter_mode, frame_laser_power_mode)

log.d("Measuring IR contrast with emitter ON...")
option_ok_on, contrast_on, ir_on, md_matches_on, md_frac_on, _ = capture_and_analyze_frames(depth_sensor, expected_emitter_state=1, num_frames=10)

log.d("Measuring IR contrast with emitter OFF...")
option_ok_off, contrast_off, ir_off, md_matches_off, md_frac_off, _ = capture_and_analyze_frames(depth_sensor, expected_emitter_state=0, num_frames=10)

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
        log.w("Limited metadata available - relying on contrast-based emitter detection")
else:
    log.e("Failed to measure contrast for both states")
    contrast_threshold = 50.0  # Fallback threshold
    test.check(False, "Could not calibrate contrast threshold")

test.finish()

################################################################################################

test.start("Stream with laser ON and verify emitter is ON")
# Test 2: Stream with laser ON and default laser power level, then verify emitter is ON
#
# This test validates that:
# - Device streams successfully with emitter enabled
# - Default laser power produces detectable emitter state
# - IR contrast and metadata confirm emitter ON state

# Get laser power range and use default (max) value
laser_range = depth_sensor.get_option_range(rs.option.laser_power)
default_laser_power = laser_range.default

log.d(f"Testing with default laser power: {default_laser_power} (range: {laser_range.min}-{laser_range.max})")

# Capture frames with emitter ON and default laser power
option_ok, contrast_test, ir_test, md_matches, md_frac, laser_readback = capture_and_analyze_frames(
    depth_sensor, 
    expected_emitter_state=1, 
    num_frames=10,
    laser_power_value=default_laser_power
)

log.d(f"Results: IR contrast={contrast_test:.2f}, mean={ir_test:.1f}, metadata_on={md_matches} (frac={md_frac:.2f})")
log.d(f"Laser power readback: {laser_readback}")

# Verify emitter enabled option is set correctly
test.check(option_ok, "Emitter enabled option should be ON")

# Verify laser power was set correctly
test.check_equal(laser_readback, default_laser_power, "Laser power should match default value")

# Verify emitter is ON using calibrated contrast threshold
detected_on = contrast_test > contrast_threshold
test.check(detected_on, f"Emitter should be detected as ON (contrast {contrast_test:.2f} > threshold {contrast_threshold:.2f})")

# If metadata is available, verify it confirms emitter ON
if md_frac > 0.5:
    test.check(md_matches, "Frame metadata should indicate emitter ON")
else:
    log.w("Limited metadata available for verification")

test.finish()

################################################################################################

test.start("Set laser power to 0 and verify emitter is OFF")
# Test 3: Continue from test 2, set laser power to 0, then confirm emitter is OFF
#
# This test validates that:
# - Setting laser power to 0 effectively turns off the emitter
# - IR contrast confirms emitter OFF state (no structured light pattern)
# - Emitter can be controlled via laser power setting
#
# Note: Metadata verification is intentionally omitted because the laser is technically
# still "enabled" but at 0 power level, so metadata may report ON despite no light output

log.d("Testing with laser power set to 0...")

# Capture frames with emitter ON (so emitter_enabled=1) but laser power=0
option_ok, contrast_test, ir_test, md_matches, md_frac, laser_readback = capture_and_analyze_frames(
    depth_sensor, 
    expected_emitter_state=1,  # Keep emitter_enabled ON
    num_frames=10,
    laser_power_value=0.0  # But set laser power to 0
)

log.d(f"Results: IR contrast={contrast_test:.2f}, mean={ir_test:.1f}, metadata_on={md_matches} (frac={md_frac:.2f})")
log.d(f"Laser power readback: {laser_readback}")

# Verify emitter enabled option is still ON (we're testing laser power control)
test.check(option_ok, "Emitter enabled option should still be ON")

# Verify laser power was set to 0
test.check_equal(laser_readback, 0.0, "Laser power should be 0")

# Verify emitter is OFF using calibrated contrast threshold
# Even though emitter_enabled=1, laser_power=0 should effectively turn off the emitter
detected_off = contrast_test < contrast_threshold
test.check(detected_off, f"Emitter should be detected as OFF when laser power=0 (contrast {contrast_test:.2f} < threshold {contrast_threshold:.2f})")

# Note: Metadata verification is skipped because technically the laser is still "on" 
# but with 0-level power, so metadata may still report emitter as ON even though
# the emitter is effectively off (no structured light pattern emitted)

test.finish()

################################################################################################
test.print_results_and_exit()
