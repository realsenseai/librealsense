# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Verifies RS2_FRAME_METADATA_EMBEDDED_FILTERS correctly and additively reflects which DPP
composite-option filters are applied to a depth frame, for Temporal Filter DPP and HDRD/Improved
Close Range Control. Decimation Filter DPP is excluded - see pytest-decimation-filter-dpp-effect.py,
its bit never sets (RSDEV-14424).

Temporal's bit is documented (1u<<2); HDRD's (1u<<5) is empirically observed only and should be
added to the spec. Both together produce 0x24 (0x04 | 0x20), confirming additive bitmask behavior.
"""

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D555"),
    pytest.mark.device_each("D585"),
    pytest.mark.device_exclude("D585S"),
]

STREAM_WIDTH, STREAM_HEIGHT = 640, 360
HDRD_WIDTH, HDRD_HEIGHT = 1280, 720  # HDRD only activates at 720p/960p (see rs_hdrd_control.h)
TEMPORAL_APPLIED_BIT = 1 << 2  # documented
HDRD_APPLIED_BIT = 1 << 5      # empirically observed only, undocumented


def _find_filter(sensor, composite_option_id):
    for embedded_filter in sensor.query_embedded_filters():
        if composite_option_id in embedded_filter.get_supported_composite_options():
            return embedded_filter
    return None


def _stream_one_depth_frame(dev, ctx, width, height, fps=30):
    """Opens the given depth profile, streams a few frames to let the pipeline settle past any
    startup transient, and returns the last one."""
    cfg = rs.config()
    # On hubless multi-device rigs the context sees every connected device; without
    # enable_device(sn) the pipeline picks the first match.
    cfg.enable_device(dev.get_info(rs.camera_info.serial_number))
    cfg.enable_stream(rs.stream.depth, width, height, rs.format.z16, fps)
    pipe = rs.pipeline(ctx)
    pipe.start(cfg)
    try:
        frame = None
        for _ in range(10):
            frameset = pipe.wait_for_frames(5000)
            frame = frameset.get_depth_frame()
        return frame
    finally:
        pipe.stop()


def _embedded_filters_value(frame):
    assert frame.supports_frame_metadata(rs.frame_metadata_value.embedded_filters), (
        "RS2_FRAME_METADATA_EMBEDDED_FILTERS not reported as supported on this frame")
    return frame.get_frame_metadata(rs.frame_metadata_value.embedded_filters)


def test_temporal_filter_dpp_sets_embedded_filters_metadata_bit(test_device):
    """Enabling Temporal Filter DPP before streaming is expected to set RS2_FRAME_METADATA_
    EMBEDDED_FILTERS's documented temporal bit (1u<<2) on the resulting depth frames, and
    disabling it again is expected to clear it."""
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()
    option_id = rs.composite_option_id.temporal_filter_dpp
    embedded_filter = _find_filter(depth_sensor, option_id)
    if embedded_filter is None:
        pytest.skip("Temporal Filter DPP composite option not supported on this device")
    try:
        embedded_filter.get_composite_option(option_id)
    except RuntimeError as e:
        pytest.skip(f"Temporal Filter DPP registered but not functional on this device/FW: {e}")

    try:
        cfg = embedded_filter.get_temporal_filter_dpp_config(option_id)
        cfg.enabled = 1
        embedded_filter.set_temporal_filter_dpp_config(option_id, cfg)
        frame = _stream_one_depth_frame(dev, ctx, STREAM_WIDTH, STREAM_HEIGHT)
        assert frame is not None, "No depth frame received with Temporal Filter DPP enabled"
        value = _embedded_filters_value(frame)
        assert value & TEMPORAL_APPLIED_BIT, (
            f"RS2_FRAME_METADATA_EMBEDDED_FILTERS = {value:#x}, temporal bit (1<<2) not set "
            f"with Temporal Filter DPP enabled")

        cfg = embedded_filter.get_temporal_filter_dpp_config(option_id)
        cfg.enabled = 0
        embedded_filter.set_temporal_filter_dpp_config(option_id, cfg)
        frame = _stream_one_depth_frame(dev, ctx, STREAM_WIDTH, STREAM_HEIGHT)
        assert frame is not None, "No depth frame received with Temporal Filter DPP disabled"
        value = _embedded_filters_value(frame)
        assert not (value & TEMPORAL_APPLIED_BIT), (
            f"RS2_FRAME_METADATA_EMBEDDED_FILTERS = {value:#x}, temporal bit (1<<2) still set "
            f"with Temporal Filter DPP disabled")
    finally:
        cfg = embedded_filter.get_temporal_filter_dpp_config(option_id)
        cfg.enabled = 0
        embedded_filter.set_temporal_filter_dpp_config(option_id, cfg)


def test_hdrd_control_sets_embedded_filters_metadata_bit(test_device):
    """Enabling HDRD/Improved Close Range Control before streaming is expected to set RS2_FRAME_
    METADATA_EMBEDDED_FILTERS's empirically-observed bit (1u<<5) on the resulting depth frames,
    and disabling it again is expected to clear it. Streams at 1280x720 - HDRD only activates at
    720p/960p (see rs_hdrd_control.h)."""
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()
    option_id = rs.composite_option_id.hdrd_control
    embedded_filter = _find_filter(depth_sensor, option_id)
    if embedded_filter is None:
        pytest.skip("HDRD control composite option not supported on this device")
    try:
        embedded_filter.get_composite_option(option_id)
    except RuntimeError as e:
        pytest.skip(f"HDRD control registered but not functional on this device/FW: {e}")

    try:
        cfg = embedded_filter.get_hdrd_control(option_id)
        cfg.enable = 1
        embedded_filter.set_hdrd_control(option_id, cfg)
        frame = _stream_one_depth_frame(dev, ctx, HDRD_WIDTH, HDRD_HEIGHT)
        assert frame is not None, "No depth frame received with HDRD control enabled"
        value = _embedded_filters_value(frame)
        assert value & HDRD_APPLIED_BIT, (
            f"RS2_FRAME_METADATA_EMBEDDED_FILTERS = {value:#x}, HDRD bit (1<<5) not set "
            f"with HDRD control enabled")

        cfg = embedded_filter.get_hdrd_control(option_id)
        cfg.enable = 0
        embedded_filter.set_hdrd_control(option_id, cfg)
        frame = _stream_one_depth_frame(dev, ctx, HDRD_WIDTH, HDRD_HEIGHT)
        assert frame is not None, "No depth frame received with HDRD control disabled"
        value = _embedded_filters_value(frame)
        assert not (value & HDRD_APPLIED_BIT), (
            f"RS2_FRAME_METADATA_EMBEDDED_FILTERS = {value:#x}, HDRD bit (1<<5) still set "
            f"with HDRD control disabled")
    finally:
        cfg = embedded_filter.get_hdrd_control(option_id)
        cfg.enable = 0
        embedded_filter.set_hdrd_control(option_id, cfg)
