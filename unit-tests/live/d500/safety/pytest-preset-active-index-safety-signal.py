# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Not frequently changing, no need to test for each commit

import time
import json
import pyrealsense2 as rs
import pytest
from pytest_check import check
from rspy import tests_wrapper as tw
import logging
log = logging.getLogger(__name__)

# Add retries as occasionally HKR FW fails during this initialization
pytestmark = [
    pytest.mark.device_each("D585S"),
    pytest.mark.priority(10),
    pytest.mark.context("nightly"),
    pytest.mark.flaky(retries=3),
]

#############################################################################################
# Helpers
#############################################################################################

def create_safety_preset(shift_y):
    """Build a safety preset JSON with its danger/warning zones shifted by shift_y (meters).

    The schema/values mirror the known-good preset round-tripped in pytest-preset-get-set.py;
    only the zone y-coordinates are offset by shift_y so the two presets differ.
    """
    return json.dumps({
        "safety_preset": {
            "platform_config": {
                "transformation_link": {
                    "rotation": [
                        [0.0, 0.0, 1.0],
                        [-1.0, 0.0, 0.0],
                        [0.0, -1.0, 0.0]
                    ],
                    "translation": [0.0, 0.0, 0.27]
                },
                "robot_height": 1.0,
                "reserved": [0] * 20
            },
            "safety_zones": {
                # Two adjacent rectangular zones along x (forward distance), sharing the
                # boundary at x=0.8. warning is the nearer zone starting at min-z (0.3 m);
                # danger is farther (0.8-1.2 m). Points are ordered clockwise (near+y ->
                # far+y -> far-y -> near-y). shift_y translates the whole zone laterally so
                # preset 1 differs from preset 0.
                "warning_zone": {
                    "zone_polygon": {
                        "p0": {"x": 0.3, "y": 0.1 + shift_y},
                        "p1": {"x": 0.8, "y": 0.1 + shift_y},
                        "p2": {"x": 0.8, "y": -0.1 - shift_y},
                        "p3": {"x": 0.3, "y": -0.1 - shift_y}
                    },
                    "safety_trigger_confidence": 3,
                    "reserved": [0] * 7
                },
                "danger_zone": {
                    "zone_polygon": {
                        "p0": {"x": 0.8, "y": 0.1 + shift_y},
                        "p1": {"x": 1.2, "y": 0.1 + shift_y},
                        "p2": {"x": 1.2, "y": -0.1 - shift_y},
                        "p3": {"x": 0.8, "y": -0.1 - shift_y}
                    },
                    "safety_trigger_confidence": 3,
                    "reserved": [0] * 7
                }
            },
            "masking_zones": {
                # Zone 0 is an active mask (attributes=1) over a specific ROI;
                # zones 1-7 are inactive placeholders sharing the same default ROI.
                "0": {
                    "attributes": 1,
                    "minimal_range": 0.5,
                    "region_of_interests": {
                        "vertex_0": [23, 54],
                        "vertex_1": [23, 639],
                        "vertex_2": [325, 639],
                        "vertex_3": [325, 54]
                    }
                },
                **{
                    str(i): {
                        "attributes": 0,
                        "minimal_range": 0.5,
                        "region_of_interests": {
                            "vertex_0": [0, 0],
                            "vertex_1": [0, 320],
                            "vertex_2": [200, 320],
                            "vertex_3": [200, 0]
                        }
                    } for i in range(1, 8)
                }
            },
            "reserved": [0] * 16,
            "environment": {
                "safety_trigger_duration": 1.0,
                "zero_safety_monitoring": 0,
                "hara_history_continuation": 0,
                "reserved1": [0, 0],
                "angular_velocity": 0.0,
                "payload_weight": 0.0,
                "surface_inclination": 15.0,
                # Note: surface_height moved to the safety interface config in v0.95;
                # it is no longer part of the preset environment.
                "diagnostic_zone_fill_rate_threshold": 90,
                "floor_fill_threshold": 0,
                "depth_fill_threshold": 20,
                "diagnostic_zone_height_median_threshold": 255,
                "vision_hara_persistency": 2,
                "crypto_signature": [0] * 32,
                "reserved2": [0, 0, 0]
            }
        }
    })


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
    assert safety_frame is not None

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
        "safety_level1": md(rs.frame_metadata_value.safety_level1),
        "safety_level2": md(rs.frame_metadata_value.safety_level2),
        "safety_level1_verdict": md(rs.frame_metadata_value.safety_level1_verdict),
        "safety_level2_verdict": md(rs.frame_metadata_value.safety_level2_verdict),
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

    # Diagnostics: explain *why* a zone is High (e.g. low depth/floor fill rate, posture
    # deviation, preset error) rather than an actual obstacle. Bitmasks shown in hex.
    diagnostics = {
        "safety_vision_verdict": signal["safety_vision_verdict"],
        "safety_hara_events": md(rs.frame_metadata_value.safety_hara_events),
        "safety_preset_integrity": md(rs.frame_metadata_value.safety_preset_integrity),
        "depth_fill_rate": md(rs.frame_metadata_value.depth_fill_rate),
        # Safety subsystem state: is the SC actually in RUN, and is the safety MCU healthy?
        "safety_operational_mode": md(rs.frame_metadata_value.safety_operational_mode),
        "safety_mb_status": md(rs.frame_metadata_value.safety_mb_status),
        "safety_mb_fusa_event": md(rs.frame_metadata_value.safety_mb_fusa_event),
        "safety_mb_fusa_action": md(rs.frame_metadata_value.safety_mb_fusa_action),
        # SIP generic metrics: up to 8 AICV-defined Vision-HaRa metrics. 'activate'=enable mask,
        # 'state'=signalled on/off mask; value/threshold are the "X-out-of-Y" count vs trigger.
        # Which bit is "holes" is AICV-defined, not in the SDK - induce holes and watch 'state'.
        "sip_metrics_activate": sip_activate,
        "sip_metrics_state": sip_state,
        "sip_metrics_value1": md(rs.frame_metadata_value.safety_sip_generic_metrics_value1),
        "sip_metrics_value2": md(rs.frame_metadata_value.safety_sip_generic_metrics_value2),
        "sip_metrics_threshold1": md(rs.frame_metadata_value.safety_sip_generic_metrics_threshold1),
        "sip_metrics_threshold2": md(rs.frame_metadata_value.safety_sip_generic_metrics_threshold2),
    }

    # This test checks ONLY occlusion detection: whether the obstacle is reported inside a
    # safety zone. We compare the COLLISION BITS of vision_verdict (bit1=danger, bit2=warning),
    # NOT the raw vision_verdict value (its bit0=not-safe can be tripped by a depth-fill
    # fail-safe with no obstacle). Everything on the "context" line is debug only - not checked.
    log.info("")  # blank line to separate consecutive presets
    log.info(f"{prefix} CHECKED: occlusion = vision_verdict collision bits "
             f"[danger(bit1)={signal['danger_collision']}, warning(bit2)={signal['warning_collision']}] "
             f"(raw vision_verdict={_hex(signal['safety_vision_verdict'])}; bit0=not-safe NOT compared)")
    log.info(f"{prefix} CHECKED: holes = SIP metric bit4 "
             f"[enabled(activate.4)={signal['holes_enabled']}, signalled(state.4)={signal['holes_signalled']}] "
             f"(activate={_hex(sip_activate)}, state={_hex(sip_state)}; signalled=0 means SAFE)")
    log.info(f"{prefix} context  (NOT checked): "
             f"level2_verdict={signal['safety_level2_verdict']} "
             f"hara_events={_hex(diagnostics['safety_hara_events'])} "
             f"preset_integrity={_hex(diagnostics['safety_preset_integrity'])} "
             f"depth_fill_rate={diagnostics['depth_fill_rate']} "
             f"operational_mode={diagnostics['safety_operational_mode']} "
             f"mb_status={_hex(diagnostics['safety_mb_status'])}")
    log.info(f"{prefix} context  (SIP metrics, AICV-defined - holes enable/status candidate): "
             f"activate={_hex(diagnostics['sip_metrics_activate'])} "
             f"state={_hex(diagnostics['sip_metrics_state'])} "
             f"value1={diagnostics['sip_metrics_value1']}/thr1={diagnostics['sip_metrics_threshold1']} "
             f"value2={diagnostics['sip_metrics_value2']}/thr2={diagnostics['sip_metrics_threshold2']}")

    return signal


def check_against_expected(prefix, result, expected):
    """Assert each filled-in expected field, logging got-vs-expected so the check is explicit."""
    for key, expected_value in expected.items():
        if expected_value is None:
            continue
        actual = result[key]
        status = "OK" if actual == expected_value else "MISMATCH"
        log.info(f"{prefix}   verify {key}: got={actual} expected={expected_value} -> {status}")
        check.equal(actual, expected_value)


# safety_trigger_duration in the preset is 1.0s, so a danger/warning signal is held for
# ~1s after a trigger. Wait longer than that after switching the active preset, so the
# previous preset's held verdict expires before we sample the new one.
PRESET_SETTLE_SEC = 2.0


def stream_and_verify(safety_sensor, active_index, prefix, expected):
    """Activate a preset, let it settle, stream safety+depth, read the signal and check it."""
    safety_sensor.set_option(rs.option.safety_preset_active_index, active_index)

    cfg = rs.config()
    cfg.enable_stream(rs.stream.safety, rs.format.y8, 30)
    # Co-enable depth so the vision-safety algo receives depth frames.
    cfg.enable_stream(rs.stream.depth)
    pipe = rs.pipeline()
    pipe.start(cfg)
    try:
        # Settle while depth streams, so the previous preset's trigger hold expires and
        # the algo re-evaluates with the newly-activated preset before we sample.
        time.sleep(PRESET_SETTLE_SEC)
        result = read_safety_signal(pipe, prefix)
        check_against_expected(prefix, result, expected)
    finally:
        pipe.stop()
    return result


# Two presets with differently-placed safety zones, written to indexes 0 and 1.
preset_json_0 = create_safety_preset(0.0)
preset_json_1 = create_safety_preset(0.2)

# Compare occlusion detection only: is the obstacle inside a safety zone? Controlled scene:
# a fixed obstacle at ~y=0.2 m, x=0.8-1.2 m, inside preset 1's wider danger zone but outside
# preset 0's narrow one. Asserting the danger/warning collision bits (vision_verdict bits 1/2),
# NOT bit0 (not-safe), so the result is independent of the depth-fill fail-safe.
expected_signal_0 = {
    "danger_collision": 0,    # obstacle outside preset 0's zones
    "warning_collision": 0,
    "holes_enabled": 1,       # holes metric active
    "holes_signalled": 0,     # holes safe
    "safety_preset_id_used": 0
}

expected_signal_1 = {
    "danger_collision": 1,    # obstacle inside preset 1's danger zone
    "warning_collision": 0,
    "holes_enabled": 1,       # holes metric active
    "holes_signalled": 0,     # holes safe
    "safety_preset_id_used": 1
}

#############################################################################################
# Tests
#############################################################################################

@pytest.fixture
def safety_sensor(test_device):
    dev, _ = test_device
    return dev, dev.first_safety_sensor()


def test_active_index_changes_safety_verdict(safety_sensor):
    dev, sensor = safety_sensor

    assert sensor.supports(rs.option.safety_preset_active_index)

    # Write safety presets to indexes 0 and 1.
    # Writing safety presets is only allowed in safety service mode; switch back to
    # run mode afterwards so the safety algorithm computes the signal while streaming.
    tw.start_wrapper(dev)
    # Save the existing presets at indexes 0 and 1 so they can be restored at the end.
    original_preset_0 = sensor.get_safety_preset(0)
    original_preset_1 = sensor.get_safety_preset(1)
    sensor.set_safety_preset(0, preset_json_0)
    sensor.set_safety_preset(1, preset_json_1)
    tw.stop_wrapper(dev)

    # Verify active preset index changes the safety verdict.
    stream_and_verify(sensor, 0, "Preset 0", expected_signal_0)
    stream_and_verify(sensor, 1, "Preset 1", expected_signal_1)

    # Restore original safety presets: put indexes 0 and 1 back to whatever they held
    # before the test ran.
    tw.start_wrapper(dev)
    #sensor.set_safety_preset(0, original_preset_0)
    #sensor.set_safety_preset(1, original_preset_1)
    tw.stop_wrapper(dev)
