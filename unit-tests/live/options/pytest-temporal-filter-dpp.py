# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D555"),
    pytest.mark.device_each("D585"),
    pytest.mark.device_exclude("D585S"),
]


def _find_temporal_filter_dpp_filter(sensor):
    """Return the embedded filter exposing Temporal Filter DPP on this sensor, or None - the
    composite option lives on one of the sensor's embedded filters, its own independent options
    registry, not on the sensor itself (see rs2::embedded_filter)."""
    for embedded_filter in sensor.query_embedded_filters():
        if rs.composite_option_id.temporal_filter_dpp in embedded_filter.get_supported_composite_options():
            return embedded_filter
    return None


def _bounce_field(embedded_filter, option_id, original, range, field):
    """Every field here is a plain range-bounded number - no conditional-relevance branching,
    unlike HDRD. Sets `field` to a new in-range value, verifies the readback, then restores it.
    Returns whether there was room to change `field` at all."""
    lo, hi = getattr(range.min, field), getattr(range.max, field)
    if lo >= hi:
        log.info(f"{field}: no room to change, range is [{lo}, {hi}]")
        return False
    current = getattr(original, field)
    new_value = lo if current != lo else hi

    # Each set starts from a freshly-read struct - the header/other fields must be carried over
    # exactly as the device just reported them, not zero-initialized.
    cfg = embedded_filter.get_temporal_filter_dpp_config(option_id)
    setattr(cfg, field, new_value)
    embedded_filter.set_temporal_filter_dpp_config(option_id, cfg)

    readback = embedded_filter.get_temporal_filter_dpp_config(option_id)
    assert getattr(readback, field) == new_value, f"{field}: expected {new_value}, got {getattr(readback, field)}"

    cfg = embedded_filter.get_temporal_filter_dpp_config(option_id)
    setattr(cfg, field, current)
    embedded_filter.set_temporal_filter_dpp_config(option_id, cfg)
    return True


def _restore_original_raw(embedded_filter, option_id, original_raw):
    """Always restore the very first raw payload read from the device, regardless of what
    happened above. `reserved` MUST be zero on SET, but a real device can hand back non-zero on
    GET - zero it in both the sent buffer and the readback comparison before restoring."""
    expected_raw = bytearray(original_raw)
    expected_raw[-16:] = b"\x00" * 16
    expected_raw = bytes(expected_raw)
    embedded_filter.set_composite_option(option_id, expected_raw)
    readback_raw = bytearray(embedded_filter.get_composite_option(option_id))
    readback_raw[-16:] = b"\x00" * 16
    assert bytes(readback_raw) == expected_raw, "failed to restore original Temporal Filter DPP value"


def test_temporal_filter_dpp_basic_parameter_changes(test_device):
    """Check whether Temporal Filter DPP is supported; if so, bounce its writable fields to new
    in-range values (verifying each readback) and restore the original value at the end."""
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()
    embedded_filter = _find_temporal_filter_dpp_filter(depth_sensor)
    if embedded_filter is None:
        pytest.skip("Temporal Filter DPP composite option not supported on this device")

    option_id = rs.composite_option_id.temporal_filter_dpp
    try:
        original_raw = embedded_filter.get_composite_option(option_id)
    except RuntimeError as e:
        # Registered but not actually functional on this device/FW is a real, expected outcome -
        # get_supported_composite_options() only reflects static registration, never a live
        # capability check.
        pytest.skip(f"Temporal Filter DPP registered but not functional on this device/FW: {e}")

    # Typed get/set (see pyrs_options.cpp) - the SDK's own bound struct, no hand-rolled
    # struct.pack/unpack format string to keep in sync. Only the final restore-and-verify below
    # uses the raw bytes, for the strongest possible guarantee.
    original = embedded_filter.get_temporal_filter_dpp_config(option_id)
    range = embedded_filter.get_temporal_filter_dpp_range(option_id)

    try:
        changed_any = False
        for field in ('smooth_alpha', 'smooth_delta', 'persistency_index'):
            changed_any |= _bounce_field(embedded_filter, option_id, original, range, field)

        if not changed_any:
            pytest.skip("No writable Temporal Filter DPP field had room to change on this device/FW")
    finally:
        _restore_original_raw(embedded_filter, option_id, original_raw)
