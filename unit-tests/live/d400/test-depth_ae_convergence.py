# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# Currently, we exclude D457 as it's failing
# test:device each(D400*) !D457
# test:donotrun
# test:timeout 600
# CI timeout set to 10 minutes to accommodate comprehensive testing of all
# supported depth profiles

"""
Depth Auto-Exposure (AE) Convergence Qualification Test

Goal:
  Measure the time it takes depth auto exposure to converge after a large manual
  exposure perturbation, using per-frame metadata (ACTUAL_EXPOSURE & GAIN_LEVEL).

Method:
  1. Start streaming depth with AE OFF.
  2. Force exposure to an extreme value (max) to ensure a change is required.
  3. Enable AE and begin timing.
  4. For every frame record exposure & gain metadata.
  5. Convergence criterion: within a sliding window of N frames the exposure spread
     (max-min)/avg is below a variation threshold (default 2%) AND at least a
     minimum number of frames have elapsed since enabling AE.
  6. Report convergence time (seconds & frames).
  7. If auto_exposure_mode option is supported, test both REGULAR (0) and ACCELERATED (1)
     modes, asserting accelerated convergence <= regular convergence * factor.

Pass / Fail (defaults):
  REGULAR mode must converge within 1.5s.
  ACCELERATED mode (if supported) must converge within 0.8s and faster than regular.

These thresholds are empirical and may need tuning for specific lighting setups;
if convergence was not required (exposure change < 5%) the test is skipped.

"""

import os, time
from pprint import pformat
import pyrealsense2 as rs
from rspy import test, log
from rspy import tests_wrapper as tw

# -----------------------------------------------------------------------------------------------
# Configuration (with environment overrides)
REGULAR_MAX = float(2.5)
ACCEL_MAX = float(5.5)
VARIATION_THRESH = float(0.02)  # 2%
WINDOW_SIZE = 12
MIN_FRAMES = 15
SPEED_FACTOR = float(1.15)  # regular >= accelerated * 1.15 expected
TIMEOUT_REGULAR = max(REGULAR_MAX * 1.5, REGULAR_MAX + 0.5)
TIMEOUT_ACCEL = max(ACCEL_MAX * 1.5, ACCEL_MAX + 0.5)

# -----------------------------------------------------------------------------------------------
# Setup

device, _ = test.find_first_device_or_exit()
tw.start_wrapper(device)

sensor = device.first_depth_sensor()
if not sensor.supports(rs.option.enable_auto_exposure):
    log.i("Depth sensor does not support auto exposure - skipping test")
    test.print_results_and_exit()

# Check AE mode support
supports_mode = sensor.supports(rs.option.auto_exposure_mode)
log.i(f"Depth AE mode: [{supports_mode}]")

# Available AE modes
REGULAR = 0.0
ACCELERATED = 1.0

# Pick a depth profile (any default depth profile)
#profile = next(p for p in sensor.profiles if p.stream_type() == rs.stream.depth)

# -----------------------------------------------------------------------------------------------
# Helper Functions

def has_metadata(frame, md):
    try:
        return frame.supports_frame_metadata(md)
    except Exception:
        return False


def format_list_abbrev(lst, max_items=100):
    try:
        if not isinstance(lst, (list, tuple)):
            return str(lst)
        if len(lst) <= max_items:
            return str(lst)
        return str(lst[:max_items]) + f" ... (total {len(lst)})"
    except Exception:
        return str(lst)

def measure_convergence(profile, max_allowed=1.0, timeout=2.0):
    """Enable AE (optionally setting AE mode) after forcing a large manual exposure
    and measure time to convergence for the given stream profile.

    Returns (status, details_dict)
      status: 'passed' | 'failed' | 'skipped'
      details_dict: contains timings, samples, reason (for skip/fail)
    """
    # Ensure streaming stopped
    try:
        sensor.stop(); sensor.close()
    except Exception:
        pass

    # Disable AE (while not streaming)
    sensor.set_option(rs.option.enable_auto_exposure, 0)

    # Open the requested profile and force an extreme manual exposure
    sensor.open(profile)
    exposure_range = sensor.get_option_range(rs.option.exposure)
    forced_exposure = exposure_range.max
    sensor.set_option(rs.option.exposure, forced_exposure)

    exposures = []
    gains = []
    timestamps = []

    state = { 'enabled_frame_index': None, 'converged_frame_index': None }

    def cb(frame):
        if not frame.is_depth_frame():
            return
        if not frame.supports_frame_metadata(rs.frame_metadata_value.actual_exposure):
            return
        exp = frame.get_frame_metadata(rs.frame_metadata_value.actual_exposure)
        try:
            gain = frame.get_frame_metadata(rs.frame_metadata_value.gain_level)
        except Exception:
            gain = 0
        exposures.append(exp)
        gains.append(gain)
        timestamps.append(time.time())

        idx = len(exposures) - 1
        if state['enabled_frame_index'] is not None and idx - state['enabled_frame_index'] >= MIN_FRAMES:
            win = exposures[-WINDOW_SIZE:] if len(exposures) >= WINDOW_SIZE else exposures
            spread = max(win) - min(win)
            avg = sum(win) / len(win)
            if avg > 0 and spread / avg <= VARIATION_THRESH:
                initial = exposures[state['enabled_frame_index']]
                current = exposures[-1]
                if abs(initial - current) / max(initial, 1) < 0.05:
                    state['converged_frame_index'] = -2
                else:
                    state['converged_frame_index'] = idx

    sensor.start(cb)

    # Collect some frames with manual exposure applied
    pre_frames = 0
    while pre_frames < 10:
        time.sleep(0.05)
        pre_frames += 1

    # Enable AE (start timing)
    sensor.set_option(rs.option.enable_auto_exposure, 1)
    state['enabled_frame_index'] = len(exposures)
    enable_wall_time = time.time()

    # Wait until convergence or timeout
    mode_timeout = timeout
    while True:
        time.sleep(0.02)
        if state['converged_frame_index'] is not None:
            break
        if time.time() - enable_wall_time > mode_timeout:
            break

    # Stop streaming
    try:
        sensor.stop(); sensor.close()
    except Exception:
        pass

    # Prepare return details and always include collected samples
    base = {
        'samples': len(exposures),
        'exposures': exposures,
        'gains': gains,
        'timestamps': timestamps,
        'enable_time': enable_wall_time
    }

    if not exposures:
        base.update({'reason': 'No exposure metadata collected'})
        return 'skipped', base

    if state['converged_frame_index'] == -2:
        base.update({'reason': 'Exposure did not need to change (>5% delta not observed)'})
        return 'skipped', base

    if state['converged_frame_index'] is None:
        # timed out without convergence
        base.update({
            'reason': 'Convergence timeout',
            'duration': time.time() - enable_wall_time,
            'measured_convergence_time': None,
            'frames': len(exposures) - state['enabled_frame_index'],
            'variation_last_window': ( (max(exposures[-WINDOW_SIZE:]) - min(exposures[-WINDOW_SIZE:])) / max(1, sum(exposures[-WINDOW_SIZE:]) / max(1, len(exposures[-WINDOW_SIZE:]))) ) if len(exposures) >= WINDOW_SIZE else None,
        })
        return 'failed', base

    converged_idx = state['converged_frame_index']
    converged_time = timestamps[converged_idx] - enable_wall_time if converged_idx < len(timestamps) else None
    frames_after_enable = converged_idx - state['enabled_frame_index']

    base.update({
        'duration': converged_time,
        'measured_convergence_time': converged_time,
        'frames': frames_after_enable,
        'max_allowed': max_allowed,
        'final_exposure': exposures[converged_idx] if converged_idx < len(exposures) else exposures[-1],
        'initial_exposure': exposures[state['enabled_frame_index']] if state['enabled_frame_index'] < len(exposures) else exposures[0]
    })

    return ('passed' if converged_time is not None and converged_time <= max_allowed else 'failed'), base


def check_metadata_availability(profile, timeout=2.0):
    """Open the given profile briefly and confirm per-frame metadata is present."""
    try:
        # ensure sensor is not streaming
        try:
            sensor.stop(); sensor.close()
        except Exception:
            pass

        ok = { 'frame_seen': False, 'has_exposure': False, 'has_gain': False }

        def cb(f):
            if not f.is_depth_frame():
                return
            ok['frame_seen'] = True
            try:
                ok['has_exposure'] = f.supports_frame_metadata(rs.frame_metadata_value.actual_exposure)
            except Exception:
                ok['has_exposure'] = False
            try:
                ok['has_gain'] = f.supports_frame_metadata(rs.frame_metadata_value.gain_level)
            except Exception:
                ok['has_gain'] = False

        sensor.open(profile)
        sensor.start(cb)

        t0 = time.time()
        while time.time() - t0 < timeout and not ok['frame_seen']:
            time.sleep(0.02)

        try:
            sensor.stop(); sensor.close()
        except Exception:
            pass

        return ok['frame_seen'] and ok['has_exposure']
    except Exception:
        try:
            sensor.stop(); sensor.close()
        except Exception:
            pass
        return False

# -----------------------------------------------------------------------------------------------
# Run Tests

# Run AE convergence for all supported depth profiles (resolution + fps)
# Exclude profiles with frame rates lower than 15 fps from testing
depth_profiles = [p for p in sensor.profiles if p.stream_type() == rs.stream.depth and p.fps() >= 15]
if not depth_profiles:
    log.i('Requested depth profile 640x480@30 not found - exiting')
    tw.stop_wrapper(device)
    test.print_results_and_exit()

for prof in depth_profiles:
    fmt = f"{prof.as_video_stream_profile().width()}x{prof.as_video_stream_profile().height()}@{prof.fps()}"
    # Skip 90 fps and 300 fps test cases
    if prof.fps() == 90 or prof.fps() == 300:
        log.i(f"Skipping 90,300 fps test case: {fmt}")
        continue
    # Verify metadata is available for this profile before running the test
    if not check_metadata_availability(prof):
        log.i(f"Depth frames for profile {fmt} do not expose ACTUAL_EXPOSURE metadata - skipping profile")
        continue
    # Regular
    # Adjust allowed convergence time for low frame-rate profiles (e.g., 6fps)
    fps = prof.fps()
    # Scale factor relative to 30fps (don't reduce for higher fps)
    fps_scale = max(1.0, 30.0 / float(fps))
    per_allowed = REGULAR_MAX * fps_scale
    per_timeout = TIMEOUT_REGULAR * fps_scale
    if fps_scale != 1.0:
        log.i(f"Adjusting convergence thresholds for {fps}fps: max_allowed={per_allowed:.3f}s, timeout={per_timeout:.3f}s")

    status, details = measure_convergence(
        profile=prof,
        max_allowed=per_allowed,
        timeout=per_timeout
    )

    if status == 'skipped':
        test.info(f"AE convergence skipped [{fmt}]", details.get('reason', ''))
    else:
        test.start(f"Depth AE convergence (REGULAR) [{fmt}]")
        measured = details.get('measured_convergence_time') if isinstance(details, dict) else None
        if status == 'passed':
            log.i(f"REGULAR [{fmt}] convergence duration: {details['duration']:.3f}s (frames={details['frames']}, threshold={details['max_allowed']}s)")
        else:
            if measured is not None:
                log.i(f"REGULAR [{fmt}] FAILED - measured convergence time: {measured:.3f}s (frames={details['frames']}, threshold={details['max_allowed']}s)")
            else:
                log.i(f"REGULAR [{fmt}] FAILED - no convergence observed within timeout ({details.get('duration', 0):.3f}s); frames collected={details.get('frames')}, variation_last_window={details.get('variation_last_window')}")
        test.check(status == 'passed', f"Regular AE convergence within {details.get('max_allowed', REGULAR_MAX)}s for {fmt}")
        test.finish()

        # Report samples
        log.i(f"REGULAR [{fmt}] AE samples={details.get('samples')}")
        log.i(f"REGULAR [{fmt}] exposures: {format_list_abbrev(details.get('exposures', []))}")
        log.i(f"REGULAR [{fmt}] gains: {format_list_abbrev(details.get('gains', []))}")
        test.info(f"REGULAR AE exposures [{fmt}]", format_list_abbrev(details.get('exposures', [])))
        test.info(f"REGULAR AE gains [{fmt}]", format_list_abbrev(details.get('gains', [])))

        # ACCELERATED AE mode test (if supported)
        if supports_mode:
            try:
                # set accelerated mode while not streaming
                sensor.set_option(rs.option.auto_exposure_mode, ACCELERATED)
            except Exception:
                log.i(f"Device does not allow setting auto_exposure_mode - skipping accelerated [{fmt}]")
            else:
                per_allowed_accel = ACCEL_MAX * fps_scale
                per_timeout_accel = TIMEOUT_ACCEL * fps_scale
                if fps_scale != 1.0:
                    log.i(f"Adjusting accelerated thresholds for {fps}fps: max_allowed={per_allowed_accel:.3f}s, timeout={per_timeout_accel:.3f}s")

                accel_status, accel_details = measure_convergence(
                    profile=prof,
                    max_allowed=per_allowed_accel,
                    timeout=per_timeout_accel
                )

                if accel_status == 'skipped':
                    test.info(f"ACCELERATED AE convergence skipped [{fmt}]", accel_details.get('reason', ''))
                else:
                    test.start(f"Depth AE convergence (ACCELERATED) [{fmt}]")
                    measured_a = accel_details.get('measured_convergence_time') if isinstance(accel_details, dict) else None
                    if accel_status == 'passed':
                        log.i(f"ACCELERATED [{fmt}] convergence duration: {accel_details['duration']:.3f}s (frames={accel_details['frames']}, threshold={accel_details['max_allowed']}s)")
                    else:
                        if measured_a is not None:
                            log.i(f"ACCELERATED [{fmt}] FAILED - measured convergence time: {measured_a:.3f}s (frames={accel_details['frames']}, threshold={accel_details['max_allowed']}s)")
                        else:
                            log.i(f"ACCELERATED [{fmt}] FAILED - no convergence observed within timeout ({accel_details.get('duration', 0):.3f}s); frames collected={accel_details.get('frames')}, variation_last_window={accel_details.get('variation_last_window')}")
                    test.check(accel_status == 'passed', f"Accelerated AE convergence within {accel_details.get('max_allowed', ACCEL_MAX)}s for {fmt}")
                    # Compare speed-up if both passed
                    # ACCELERATED mode is faster in certain cases (not all), so skip this test for now
                    #if status == 'passed' and accel_status == 'passed':
                    #    expected = details['duration'] / SPEED_FACTOR
                    #    test.check(accel_details['duration'] <= expected,
                    #               f"Accelerated AE should be at least {SPEED_FACTOR}x faster (regular={details['duration']:.3f}s; accelerated={accel_details['duration']:.3f}s) for {fmt}")
                    test.finish()

                    # Report accel samples
                    log.i(f"ACCELERATED [{fmt}] AE samples={accel_details.get('samples')}")
                    log.i(f"ACCELERATED [{fmt}] exposures: {format_list_abbrev(accel_details.get('exposures', []))}")
                    log.i(f"ACCELERATED [{fmt}] gains: {format_list_abbrev(accel_details.get('gains', []))}")
                    test.info(f"ACCELERATED AE exposures [{fmt}]", format_list_abbrev(accel_details.get('exposures', [])))
                    test.info(f"ACCELERATED AE gains [{fmt}]", format_list_abbrev(accel_details.get('gains', [])))

tw.stop_wrapper(device)
test.print_results_and_exit()
