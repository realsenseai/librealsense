# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Not frequently changing, no need to test for each commit

import sys
import time
import json
import pyrealsense2 as rs
import pytest
from pytest_check import check
from rspy import tests_wrapper as tw
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D585S"),
    pytest.mark.context("nightly"),
    pytest.mark.skipif(sys.platform != "linux", reason="Linux only"),
]

#############################################################################################
# Helpers
#############################################################################################

# Zone geometry (meters). x = forward distance from the camera; y = lateral half-width.
# The obstacle is the lab WALL (a bit beyond 0.8 m). The discriminator between the two
# presets is the danger zone's FORWARD extent, not lateral:
#   - preset 0 danger ends at DANGER_FAR_BEFORE_WALL (before the wall) -> wall NOT in zone,
#     danger_collision=0
#   - preset 1 danger ends at DANGER_FAR_PAST_WALL (past the wall)   -> wall IN zone,
#     danger_collision=1
# The warning zone and the lateral half-width are identical for both presets.
WARNING_X_NEAR         = 0.3   # warning zone starts here (near edge)
ZONE_X_MID             = 0.5   # warning ends / danger starts (shared boundary)
DANGER_FAR_BEFORE_WALL = 0.8   # preset 0: danger ends BEFORE the wall
DANGER_FAR_PAST_WALL   = 1.5   # preset 1: danger ends PAST the wall (wall inside danger)
ZONE_Y_HALF            = 0.1   # lateral half-width (same for both presets)


def zone_polygon(x_near, x_far):
    """Rectangle spanning x_near..x_far forward and +/-ZONE_Y_HALF laterally, clockwise
    (near+y -> far+y -> far-y -> near-y)."""
    return {
        "p0": {"x": x_near, "y": ZONE_Y_HALF},
        "p1": {"x": x_far, "y": ZONE_Y_HALF},
        "p2": {"x": x_far, "y": -ZONE_Y_HALF},
        "p3": {"x": x_near, "y": -ZONE_Y_HALF}
    }


def update_preset_zones(preset_json, danger_x_far):
    """Return the device's own preset with only the two zone polygons replaced.

    Everything else - masking zones, platform config, environment - is carried over from the
    device untouched, so a FW/flash table update that changes those fields cannot invalidate
    the test. warning is the nearer zone (WARNING_X_NEAR -> ZONE_X_MID); danger is farther
    (ZONE_X_MID -> danger_x_far), and danger_x_far is what makes preset 1 differ from preset 0.
    """
    preset = json.loads(preset_json)
    zones = preset["safety_preset"]["safety_zones"]
    zones["warning_zone"]["zone_polygon"] = zone_polygon(WARNING_X_NEAR, ZONE_X_MID)
    zones["danger_zone"]["zone_polygon"] = zone_polygon(ZONE_X_MID, danger_x_far)
    return json.dumps(preset)


def _hex(value):
    return "n/a" if value is None else f"0x{value:x}"


def read_safety_signal(pipe, prefix, settle_frames=10):
    """Drain a few frames so the active preset takes effect, then return the safety signal."""
    safety_frame = None
    for _ in range(settle_frames):
        frames = pipe.wait_for_frames()
        candidate = frames.first_or_default(rs.stream.safety)
        if candidate:
            safety_frame = candidate
    if safety_frame is None:
        pytest.fail(f"{prefix}: no safety frame received in {settle_frames} frames")

    def md(value):
        # Some diagnostic fields may not be present on every frame; guard to avoid throwing.
        if safety_frame.supports_frame_metadata(value):
            return int(safety_frame.get_frame_metadata(value))
        return None

    vision_verdict = md(rs.frame_metadata_value.safety_vision_verdict)
    sip_activate = md(rs.frame_metadata_value.safety_sip_generic_metrics_activate)
    sip_state = md(rs.frame_metadata_value.safety_sip_generic_metrics_state)

    def bit(value, n):
        return None if value is None else (value >> n) & 1

    signal = {
        "safety_vision_verdict": vision_verdict,
        # Occlusion detection: is the obstacle inside a zone? Take only the collision bits of
        # vision_verdict (bit1=danger, bit2=warning), ignoring bit0 (aggregate not-safe, which
        # the depth-fill fail-safe can trip even with no obstacle).
        "danger_collision": bit(vision_verdict, 1),
        "warning_collision": bit(vision_verdict, 2),
        # Holes = SIP generic metric bit 4 (AICV mapping): activate.4=enabled, state.4=signalled
        # (1 = holes danger, 0 = holes safe).
        "holes_enabled": bit(sip_activate, 4),
        "holes_signalled": bit(sip_state, 4),
        "safety_preset_id_used": md(rs.frame_metadata_value.safety_preset_id_used)
    }

    # This test checks ONLY occlusion detection: whether the obstacle is reported inside a
    # safety zone. We compare the COLLISION BITS of vision_verdict (bit1=danger, bit2=warning),
    # NOT the raw vision_verdict value (its bit0=not-safe can be tripped by a depth-fill
    # fail-safe with no obstacle).
    log.info("")  # blank line to separate consecutive presets
    log.info(f"{prefix} CHECKED: occlusion = vision_verdict collision bits "
             f"[danger(bit1)={signal['danger_collision']}, warning(bit2)={signal['warning_collision']}] "
             f"(raw vision_verdict={_hex(signal['safety_vision_verdict'])}; bit0=not-safe NOT compared)")
    log.info(f"{prefix} CHECKED: holes = SIP metric bit4 "
             f"[enabled(activate.4)={signal['holes_enabled']}, signalled(state.4)={signal['holes_signalled']}] "
             f"(activate={_hex(sip_activate)}, state={_hex(sip_state)}; signalled=0 means SAFE)")

    return signal


def check_against_expected(prefix, result, expected):
    """Assert each filled-in expected field, logging got-vs-expected so the check is explicit."""
    for key, expected_value in expected.items():
        if expected_value is None:
            continue
        actual = result[key]
        ok = actual == expected_value
        status = "OK" if ok else "MISMATCH"
        log_fn = log.info if ok else log.error
        log_fn(f"{prefix}   verify {key}: got={actual} expected={expected_value} -> {status}")
        check.equal(actual, expected_value, f"{prefix} {key}: got={actual} expected={expected_value}")


# safety_trigger_duration in the preset is 1.0s, so a danger/warning signal is held for
# ~1s after a trigger. Wait longer than that after switching the active preset, so the
# previous preset's held verdict expires before we sample the new one.
PRESET_SETTLE_SEC = 2.0


def stream_and_verify(dev, safety_sensor, ctx, active_index, prefix, expected):
    """Activate a preset, let it settle, stream safety+depth, read the signal and check it."""
    safety_sensor.set_option(rs.option.safety_preset_active_index, active_index)

    cfg = rs.config()
    cfg.enable_device(dev.get_info(rs.camera_info.serial_number))
    cfg.enable_stream(rs.stream.safety, rs.format.y8, 30)
    # Co-enable depth so the vision-safety algo receives depth frames.
    cfg.enable_stream(rs.stream.depth)
    pipe = rs.pipeline(ctx)
    pipe.start(cfg)
    try:
        # Settle while depth streams, so the previous preset's trigger hold expires and
        # the algo re-evaluates with the newly-activated preset before we sample.
        time.sleep(PRESET_SETTLE_SEC)
        result = read_safety_signal(pipe, prefix)
        check_against_expected(prefix, result, expected)
    finally:
        pipe.stop()
        time.sleep(1)  # allow the device to fully release before the next pipeline start


# Compare occlusion detection only: is the obstacle inside a safety zone? Controlled scene:
# the lab WALL a bit beyond 0.8 m is the obstacle. preset 0's danger zone ends before the
# wall (DANGER_FAR_BEFORE_WALL) so the wall is outside it; preset 1's danger zone reaches past
# the wall (DANGER_FAR_PAST_WALL) so the wall falls inside it. Asserting the danger/warning
# collision bits (vision_verdict bits 1/2), NOT bit0 (not-safe), so the result is independent
# of the depth-fill fail-safe.
expected_signal_0 = {
    "danger_collision": 0,    # wall beyond preset 0's danger zone (ends before the wall)
    "warning_collision": 0,
    "holes_enabled": 1,       # holes metric active
    "holes_signalled": 0,     # holes safe
    "safety_preset_id_used": 0
}

expected_signal_1 = {
    "danger_collision": 1,    # wall inside preset 1's danger zone (reaches past the wall)
    "warning_collision": 0,
    "holes_enabled": 1,       # holes metric active
    "holes_signalled": 0,     # holes safe
    "safety_preset_id_used": 1
}

#############################################################################################
# Tests
#############################################################################################

@pytest.fixture
def safety_presets(test_device, request):
    """Provide (dev, sensor, ctx) with the two test presets written to indexes 0 and 1.

    For each index the original is saved and its restore is registered with
    request.addfinalizer *before* that index is overwritten. pytest runs every registered
    finalizer at teardown - even if setup later raises before reaching yield, and independently
    of one another - so a write that fails mid-sequence (or a restore that itself fails) never
    leaves an index holding the test preset. Preset writes are only allowed in safety service
    mode, so each restore (and the setup writes) toggles the service-mode wrapper in its own
    try/finally and is never left stuck in service mode.
    """
    dev, ctx = test_device
    sensor = dev.first_safety_sensor()
    assert sensor.supports(rs.option.safety_preset_active_index)

    def restore(index, preset):
        tw.start_wrapper(dev)
        try:
            sensor.set_safety_preset(index, preset)
        finally:
            tw.stop_wrapper(dev)

    def write_test_preset(index, danger_x_far):
        # Save the original and schedule its restore before overwriting the index.
        original = sensor.get_safety_preset(index)
        request.addfinalizer(lambda: restore(index, original))
        sensor.set_safety_preset(index, update_preset_zones(original, danger_x_far))

    # Return to run mode afterwards so the safety algorithm computes the signal while streaming.
    tw.start_wrapper(dev)
    try:
        write_test_preset(0, DANGER_FAR_BEFORE_WALL)
        write_test_preset(1, DANGER_FAR_PAST_WALL)
    finally:
        tw.stop_wrapper(dev)

    yield dev, sensor, ctx


def test_active_index_changes_safety_verdict(safety_presets):
    dev, sensor, ctx = safety_presets

    # Verify active preset index changes the safety verdict.
    stream_and_verify(dev, sensor, ctx, 0, "Preset 0", expected_signal_0)
    stream_and_verify(dev, sensor, ctx, 1, "Preset 1", expected_signal_1)
