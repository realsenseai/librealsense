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


def _find_hdrd_filter(sensor):
    """Return the embedded filter exposing Improved Close Range Control on this sensor, or None - the
    composite option lives on one of the sensor's embedded filters, its own independent options
    registry, not on the sensor itself (see rs2::embedded_filter)."""
    for embedded_filter in sensor.query_embedded_filters():
        if rs.composite_option_id.hdrd_control in embedded_filter.get_supported_composite_options():
            return embedded_filter
    return None


def _bounce_field_pair(embedded_filter, option_id, original, range, filter_type, field):
    """filter_type selects which of two field-pairs is meaningful: downscale_ratio under Downscale
    (0), shift_pixels (Manual shift_mode) under Lookup Shift (1) - mutually exclusive branches.
    Sets `field` to a new in-range value, verifies, then restores everything. Returns whether
    there was room to change `field` at all."""
    lo, hi = getattr(range.min, field), getattr(range.max, field)
    if lo >= hi:
        log.info(f"{field}: no room to change, range is [{lo}, {hi}]")
        return False
    current = getattr(original, field)
    new_value = lo if current != lo else hi

    # Each set starts from a freshly-read struct - the header/other fields must be carried over
    # exactly as the device just reported them, not zero-initialized.
    cfg = embedded_filter.get_hdrd_control(option_id)
    cfg.filter_type = filter_type
    if field == 'shift_pixels':
        cfg.shift_mode = 2  # Manual - the only mode that makes shift_pixels meaningful
    setattr(cfg, field, new_value)
    embedded_filter.set_hdrd_control(option_id, cfg)

    readback = embedded_filter.get_hdrd_control(option_id)
    assert readback.filter_type == filter_type, f"filter_type: expected {filter_type}, got {readback.filter_type}"
    assert getattr(readback, field) == new_value, f"{field}: expected {new_value}, got {getattr(readback, field)}"

    cfg = embedded_filter.get_hdrd_control(option_id)
    cfg.filter_type = original.filter_type
    if field == 'shift_pixels':
        cfg.shift_mode = original.shift_mode
    setattr(cfg, field, current)
    embedded_filter.set_hdrd_control(option_id, cfg)
    return True


def _bounce_threshold(embedded_filter, option_id, original, range):
    """threshold_mode cycles independently of filter_type - threshold_mm only matters when
    threshold_mode is Manual (2). Same bounce-verify-restore shape as _bounce_field_pair() above.
    Returns whether there was room to change threshold_mm at all."""
    lo, hi = range.min.threshold_mm, range.max.threshold_mm
    if lo >= hi:
        log.info(f"threshold_mm: no room to change, range is [{lo}, {hi}]")
        return False

    cfg = embedded_filter.get_hdrd_control(option_id)
    cfg.threshold_mode = 2
    cfg.threshold_mm = lo if original.threshold_mm != lo else hi
    embedded_filter.set_hdrd_control(option_id, cfg)

    readback = embedded_filter.get_hdrd_control(option_id)
    assert readback.threshold_mode == 2, f"threshold_mode: expected 2, got {readback.threshold_mode}"
    assert readback.threshold_mm == cfg.threshold_mm, f"threshold_mm: expected {cfg.threshold_mm}, got {readback.threshold_mm}"

    cfg = embedded_filter.get_hdrd_control(option_id)
    cfg.threshold_mode = original.threshold_mode
    cfg.threshold_mm = original.threshold_mm
    embedded_filter.set_hdrd_control(option_id, cfg)
    return True


def _restore_original_raw(embedded_filter, option_id, original_raw):
    """Always restore the very first raw payload read from the device, regardless of what
    happened above. `reserved` MUST be zero on SET, but a real device can hand back non-zero on
    GET - zero it in both the sent buffer and the readback comparison before restoring."""
    expected_raw = bytearray(original_raw)
    expected_raw[-4:] = b"\x00\x00\x00\x00"
    expected_raw = bytes(expected_raw)
    embedded_filter.set_composite_option(option_id, expected_raw)
    readback_raw = bytearray(embedded_filter.get_composite_option(option_id))
    readback_raw[-4:] = b"\x00\x00\x00\x00"
    assert bytes(readback_raw) == expected_raw, "failed to restore original Improved Close Range Control value"


def test_hdrd_control_basic_parameter_changes(test_device):
    """Check whether Improved Close Range Control is supported; if so, bounce its writable fields to new
    in-range values (verifying each readback) and restore the original value at the end."""
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()
    embedded_filter = _find_hdrd_filter(depth_sensor)
    if embedded_filter is None:
        pytest.skip("Improved Close Range Control composite option not supported on this device")

    option_id = rs.composite_option_id.hdrd_control
    try:
        original_raw = embedded_filter.get_composite_option(option_id)
    except RuntimeError as e:
        # Registered but not actually functional on this device/FW is a real, expected outcome -
        # get_supported_composite_options() only reflects static registration, never a live
        # capability check.
        pytest.skip(f"Improved Close Range Control registered but not functional on this device/FW: {e}")

    # Typed get/set (see pyrs_options.cpp) - the SDK's own bound struct, no hand-rolled
    # struct.pack/unpack format string to keep in sync. Only the final restore-and-verify below
    # uses the raw bytes, for the strongest possible guarantee.
    original = embedded_filter.get_hdrd_control(option_id)
    range = embedded_filter.get_hdrd_control_range(option_id)

    try:
        changed_any = False
        for filter_type, field in ((0, 'downscale_ratio'), (1, 'shift_pixels')):
            changed_any |= _bounce_field_pair(embedded_filter, option_id, original, range, filter_type, field)
        changed_any |= _bounce_threshold(embedded_filter, option_id, original, range)

        if not changed_any:
            pytest.skip("No writable Improved Close Range field had room to change on this device/FW")
    finally:
        _restore_original_raw(embedded_filter, option_id, original_raw)
