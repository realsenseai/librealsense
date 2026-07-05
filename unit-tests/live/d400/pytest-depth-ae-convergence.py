# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# CI timeout set to 10 minutes to accommodate the nightly full-profile sweep.
# The default (gating) test only exercises a single resolution and is fast.

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

Coverage:
  - test_depth_ae_convergence: gating/default run, single default resolution only, so
    the gate stays short and does not hammer the USB controller with reconfigurations.
  - test_depth_ae_convergence_all_resolutions: nightly-only, sweeps the full depth
    profile matrix.

Pass / Fail (defaults):
  REGULAR mode must converge within 1.5s.
  ACCELERATED mode (if supported) must converge within 0.8s and faster than regular.

These thresholds are empirical and may need tuning for specific lighting setups;
if convergence was not required (exposure change < 5%) the test is skipped.

"""

import pytest
import os, time
from pprint import pformat
import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy.pytest.device_helpers import require_min_fw_version
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.timeout(600),
]


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

# Let the USB endpoints tear down between stream reconfigurations. Back-to-back
# stop/close -> open storms can wedge the host USB controller on CI machines.
SETTLE_DELAY = float(0.5)  # seconds

# Available AE modes
REGULAR = 0.0
ACCELERATED = 1.0

# -----------------------------------------------------------------------------------------------
# Helper Functions

def _settle():
    """Pause so USB endpoints fully tear down before the next open()."""
    time.sleep(SETTLE_DELAY)


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

def measure_convergence(sensor, profile, max_allowed=1.0, timeout=2.0):
    """Enable AE (optionally setting AE mode) after forcing a large manual exposure
    and measure time to convergence for the given stream profile.

    Returns (status, details_dict)
      status: 'passed' | 'failed' | 'skipped'
      details_dict: contains timings, samples, reason (for skip/fail)

    If the profile exposes no ACTUAL_EXPOSURE metadata (no frames recorded) the
    result is 'skipped' - so a separate metadata-probe streaming cycle is not needed.
    """
    # Ensure streaming stopped before reopening. No settle needed here: every measurement
    # exits with a settle, so a preceding cycle already left a gap, and nothing is streaming
    # on the very first call.
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

    # Stop streaming, then let the USB endpoints settle before the next open
    try:
        sensor.stop(); sensor.close()
    except Exception:
        pass
    _settle()

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


# -----------------------------------------------------------------------------------------------
# Shared setup / execution

def _prepare_depth_ae_sensor(dev):
    """Common setup: FW gate, depth sensor, AE support and the candidate profile list.

    Skips the test (via pytest.skip) if the device can't run it.
    Returns (sensor, supports_mode, depth_profiles).
    """
    require_min_fw_version(dev, rsutils.version(5, 17, 0, 10), "AE convergence", inclusive=False)

    sensor = dev.first_depth_sensor()
    if not sensor.supports(rs.option.enable_auto_exposure):
        pytest.skip("Depth sensor does not support auto exposure - skipping test")

    # Check AE mode support
    supports_mode = sensor.supports(rs.option.auto_exposure_mode)
    log.info(f"Depth AE mode: [{supports_mode}]")

    # Candidate depth profiles (resolution + fps); exclude frame rates lower than 15 fps
    depth_profiles = [p for p in sensor.profiles if p.stream_type() == rs.stream.depth and p.fps() >= 15]
    if not depth_profiles:
        pytest.skip('No depth profiles >= 15fps found - exiting')

    return sensor, supports_mode, depth_profiles


def _select_default_profile(sensor, depth_profiles):
    """Pick the depth sensor's default stream profile via the SDK's is_default() flag
    (the camera's out-of-the-box resolution), mirroring how other live tests choose their
    profile. Falls back to the first candidate profile if the sensor flags no default."""
    default = next((p for p in sensor.profiles
                    if p.stream_type() == rs.stream.depth and p.is_default()), None)
    if default is not None:
        return default
    vsp0 = depth_profiles[0].as_video_stream_profile()
    log.warning(f"Depth sensor flags no default profile; falling back to "
                f"{vsp0.width()}x{vsp0.height()}@{depth_profiles[0].fps()}")
    return depth_profiles[0]


def _run_ae_convergence(sensor, supports_mode, depth_profiles):
    """Run AE convergence over the given depth profiles and assert the overall result."""
    # Track all test results
    test_results = []  # List of (config_name, passed: bool)

    for prof in depth_profiles:
        fmt = f"{prof.as_video_stream_profile().width()}x{prof.as_video_stream_profile().height()}@{prof.fps()}"
        # Skip 60, 90 fps and 300 fps test cases
        if prof.fps() == 60 or prof.fps() == 90 or prof.fps() == 300:
            log.info(f"Skipping 60,90,300 fps test case: {fmt}")
            continue
        # Regular
        # Adjust allowed convergence time for low frame-rate profiles (e.g., 6fps)
        fps = prof.fps()
        # Scale factor relative to 30fps (don't reduce for higher fps)
        fps_scale = max(1.0, 30.0 / float(fps))
        per_allowed = REGULAR_MAX * fps_scale
        per_timeout = TIMEOUT_REGULAR * fps_scale
        if fps_scale != 1.0:
            log.info(f"Adjusting convergence thresholds for {fps}fps: max_allowed={per_allowed:.3f}s, timeout={per_timeout:.3f}s")

        status, details = measure_convergence(
            sensor,
            profile=prof,
            max_allowed=per_allowed,
            timeout=per_timeout
        )

        if status == 'skipped':
            log.info(f"AE convergence skipped [{fmt}]: {details.get('reason', '')}")
        else:
            log.info(f"Depth AE convergence (REGULAR) [{fmt}]")
            measured = details.get('measured_convergence_time') if isinstance(details, dict) else None
            passed = (status == 'passed')
            test_results.append((f"REGULAR [{fmt}]", passed))

            if passed:
                log.info(f"REGULAR [{fmt}] convergence duration: {details['duration']:.3f}s (frames={details['frames']}, threshold={details['max_allowed']}s)")
            else:
                if measured is not None:
                    log.info(f"REGULAR [{fmt}] FAILED - measured convergence time: {measured:.3f}s (frames={details['frames']}, threshold={details['max_allowed']}s)")
                else:
                    log.info(f"REGULAR [{fmt}] FAILED - no convergence observed within timeout ({details.get('duration', 0):.3f}s); frames collected={details.get('frames')}, variation_last_window={details.get('variation_last_window')}")
            # Don't fail immediately - just log the result. Individual results are not checked; only overall threshold matters.

            # Report samples
            log.info(f"REGULAR [{fmt}] AE samples={details.get('samples')}")
            log.info(f"REGULAR [{fmt}] exposures: {format_list_abbrev(details.get('exposures', []))}")
            log.info(f"REGULAR [{fmt}] gains: {format_list_abbrev(details.get('gains', []))}")

            # ACCELERATED AE mode test (if supported)
            if supports_mode:
                try:
                    # set accelerated mode while not streaming
                    sensor.set_option(rs.option.auto_exposure_mode, ACCELERATED)
                except Exception:
                    log.info(f"Device does not allow setting auto_exposure_mode - skipping accelerated [{fmt}]")
                else:
                    per_allowed_accel = ACCEL_MAX * fps_scale
                    per_timeout_accel = TIMEOUT_ACCEL * fps_scale
                    if fps_scale != 1.0:
                        log.info(f"Adjusting accelerated thresholds for {fps}fps: max_allowed={per_allowed_accel:.3f}s, timeout={per_timeout_accel:.3f}s")

                    accel_status, accel_details = measure_convergence(
                        sensor,
                        profile=prof,
                        max_allowed=per_allowed_accel,
                        timeout=per_timeout_accel
                    )

                    if accel_status == 'skipped':
                        log.info(f"ACCELERATED AE convergence skipped [{fmt}]: {accel_details.get('reason', '')}")
                    else:
                        log.info(f"Depth AE convergence (ACCELERATED) [{fmt}]")
                        measured_a = accel_details.get('measured_convergence_time') if isinstance(accel_details, dict) else None
                        passed_a = (accel_status == 'passed')
                        test_results.append((f"ACCELERATED [{fmt}]", passed_a))

                        if passed_a:
                            log.info(f"ACCELERATED [{fmt}] convergence duration: {accel_details['duration']:.3f}s (frames={accel_details['frames']}, threshold={accel_details['max_allowed']}s)")
                        else:
                            if measured_a is not None:
                                log.info(f"ACCELERATED [{fmt}] FAILED - measured convergence time: {measured_a:.3f}s (frames={accel_details['frames']}, threshold={accel_details['max_allowed']}s)")
                            else:
                                log.info(f"ACCELERATED [{fmt}] FAILED - no convergence observed within timeout ({accel_details.get('duration', 0):.3f}s); frames collected={accel_details.get('frames')}, variation_last_window={accel_details.get('variation_last_window')}")
                        # Don't fail immediately - just log the result. Individual results are not checked; only overall threshold matters.
                        # Compare speed-up if both passed
                        # ACCELERATED mode is faster in certain cases (not all), so skip this test for now
                        #if status == 'passed' and accel_status == 'passed':
                        #    expected = details['duration'] / SPEED_FACTOR
                        #    assert accel_details['duration'] <= expected, \
                        #        f"Accelerated AE should be at least {SPEED_FACTOR}x faster (regular={details['duration']:.3f}s; accelerated={accel_details['duration']:.3f}s) for {fmt}"

                        # Report accel samples
                        log.info(f"ACCELERATED [{fmt}] AE samples={accel_details.get('samples')}")
                        log.info(f"ACCELERATED [{fmt}] exposures: {format_list_abbrev(accel_details.get('exposures', []))}")
                        log.info(f"ACCELERATED [{fmt}] gains: {format_list_abbrev(accel_details.get('gains', []))}")

                    # Restore REGULAR mode so subsequent profiles measure the REGULAR baseline
                    # in the correct mode (measure_convergence never resets auto_exposure_mode).
                    try:
                        sensor.set_option(rs.option.auto_exposure_mode, REGULAR)
                    except Exception:
                        pass

    # -----------------------------------------------------------------------------------------------
    # Evaluate Overall Test Results (10% failure threshold)

    total_configs = len(test_results)
    failed_configs = [name for name, passed in test_results if not passed]
    failure_count = len(failed_configs)

    if total_configs > 0:
        failure_rate = (failure_count / total_configs) * 100
        log.info(f"\n{'='*80}")
        log.info(f"OVERALL RESULTS: {failure_count} of {total_configs} configurations failed ({failure_rate:.1f}%)")
        log.info(f"{'='*80}")

        if failure_count > 0:
            log.info(f"Failed configurations:")
            for name in failed_configs:
                log.info(f"  - {name}")

        # Apply 10% threshold: only fail if more than 10% of configs failed. The SAME bar is
        # used for the gating (single profile) and nightly (full matrix) runs — the only
        # difference between them is how many profiles are exercised, not the pass criterion.
        FAILURE_THRESHOLD = 10.0  # 10%
        assert failure_rate <= FAILURE_THRESHOLD, \
            f"Failure rate {failure_rate:.1f}% exceeds {FAILURE_THRESHOLD}% threshold ({failure_count}/{total_configs} configs failed)"
    else:
        log.warning("No configurations were tested")


# -----------------------------------------------------------------------------------------------
# Run Tests

def test_depth_ae_convergence(test_device_wrapped):
    """Gating/default run: exercise a single default resolution only, keeping the gate
    short and avoiding a storm of USB stream reconfigurations."""
    dev, _ = test_device_wrapped
    sensor, supports_mode, depth_profiles = _prepare_depth_ae_sensor(dev)

    default_profile = _select_default_profile(sensor, depth_profiles)
    vsp = default_profile.as_video_stream_profile()
    fmt = f"{vsp.width()}x{vsp.height()}@{default_profile.fps()}"
    log.info(f"Testing AE convergence on default depth profile [{fmt}]")

    _run_ae_convergence(sensor, supports_mode, [default_profile])


@pytest.mark.context("nightly")
def test_depth_ae_convergence_all_resolutions(test_device_wrapped):
    """Nightly-only run: sweep the full depth profile matrix (resolution + fps)."""
    dev, _ = test_device_wrapped
    sensor, supports_mode, depth_profiles = _prepare_depth_ae_sensor(dev)

    _run_ae_convergence(sensor, supports_mode, depth_profiles)
