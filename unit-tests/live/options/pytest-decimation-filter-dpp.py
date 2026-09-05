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


def _find_decimation_filter_dpp_filter(sensor):
    """Return the embedded filter exposing Decimation Filter DPP on this sensor, or None - the
    composite option lives on one of the sensor's embedded filters, its own independent options
    registry, not on the sensor itself (see rs2::embedded_filter). This is the USB/composite-option
    path only, independent of the DDS-connected device's own scalar-option decimation filter."""
    for embedded_filter in sensor.query_embedded_filters():
        if rs.composite_option_id.decimation_filter_dpp in embedded_filter.get_supported_composite_options():
            return embedded_filter
    return None


def _bounce_magnitude(embedded_filter, option_id, original, range):
    """magnitude was documented as a single fixed value, but the device may report a wider
    range (see rs_decimation_filter_dpp.h) - bounce it like any other field if there's room,
    skip cleanly if there isn't. Sets magnitude to a new in-range value, verifies the readback,
    then restores it. Returns whether there was room to change it at all."""
    lo, hi = range.min.magnitude, range.max.magnitude
    if lo >= hi:
        log.info(f"magnitude: no room to change, range is [{lo}, {hi}]")
        return False
    current = original.magnitude
    new_value = lo if current != lo else hi

    cfg = embedded_filter.get_decimation_filter_dpp_config(option_id)
    cfg.magnitude = new_value
    embedded_filter.set_decimation_filter_dpp_config(option_id, cfg)

    readback = embedded_filter.get_decimation_filter_dpp_config(option_id)
    assert readback.magnitude == new_value, f"magnitude: expected {new_value}, got {readback.magnitude}"

    cfg = embedded_filter.get_decimation_filter_dpp_config(option_id)
    cfg.magnitude = current
    embedded_filter.set_decimation_filter_dpp_config(option_id, cfg)
    return True


def _restore_original_raw(embedded_filter, option_id, original_raw):
    """Always restore the very first raw payload read from the device, regardless of what
    happened above. `reserved` MUST be zero on SET, but a real device can hand back non-zero on
    GET - zero it in both the sent buffer and the readback comparison before restoring."""
    expected_raw = bytearray(original_raw)
    expected_raw[-24:] = b"\x00" * 24
    expected_raw = bytes(expected_raw)
    embedded_filter.set_composite_option(option_id, expected_raw)
    readback_raw = bytearray(embedded_filter.get_composite_option(option_id))
    readback_raw[-24:] = b"\x00" * 24
    assert bytes(readback_raw) == expected_raw, "failed to restore original Decimation Filter DPP value"


def test_decimation_filter_dpp_basic_parameter_changes(test_device):
    """Check whether Decimation Filter DPP is supported; if so, bounce its writable fields to new
    in-range values (verifying each readback) and restore the original value at the end. Unlike
    Temporal Filter DPP/HDRD, the device itself also rejects a SET here once Depth/IR starts
    streaming - not exercised here since this test never opens a stream."""
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()
    embedded_filter = _find_decimation_filter_dpp_filter(depth_sensor)
    if embedded_filter is None:
        pytest.skip("Decimation Filter DPP composite option not supported on this device")

    option_id = rs.composite_option_id.decimation_filter_dpp
    try:
        original_raw = embedded_filter.get_composite_option(option_id)
    except RuntimeError as e:
        # Registered but not actually functional on this device/FW is a real, expected outcome -
        # get_supported_composite_options() only reflects static registration, never a live
        # capability check.
        pytest.skip(f"Decimation Filter DPP registered but not functional on this device/FW: {e}")

    # Typed get/set (see pyrs_options.cpp) - the SDK's own bound struct, no hand-rolled
    # struct.pack/unpack format string to keep in sync. Only the final restore-and-verify below
    # uses the raw bytes, for the strongest possible guarantee.
    original = embedded_filter.get_decimation_filter_dpp_config(option_id)
    range = embedded_filter.get_decimation_filter_dpp_range(option_id)

    try:
        changed_any = _bounce_magnitude(embedded_filter, option_id, original, range)

        if not changed_any:
            pytest.skip("No writable Decimation Filter DPP field had room to change on this device/FW")
    finally:
        _restore_original_raw(embedded_filter, option_id, original_raw)
