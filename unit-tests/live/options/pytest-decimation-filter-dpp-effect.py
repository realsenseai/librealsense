# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Verifies the effect of Decimation Filter DPP on the actual depth stream, unlike
pytest-decimation-filter-dpp.py which only bounces the control's fields and never streams.
640x360 is also a natively-offered profile with no DPP involvement, so frame size alone can't
tell the two apart - RS2_FRAME_METADATA_EMBEDDED_FILTERS is the only way to confirm a 640x360
frame came from the DPP decimation block rather than the native path.

Known FW gap (RSDEV-14424): the metadata field itself is reliable - Temporal Filter DPP and
HDRD/Improved Close Range Control both set their bits correctly (see
pytest-embedded-filters-metadata.py), and it reads 0x00 with all three DPP filters disabled.
Only Decimation's bit (1u<<0) never sets, at both 1280x720 and 640x360. Not a metadata-plumbing
bug - the decimation DPP block itself isn't being applied by FW on this build.
test_..._with_metadata_set is left as a real (non-xfail) failing assertion - see its own
docstring for why.
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
DECIMATION_APPLIED_BIT = 1 << 0  # RS2_FRAME_METADATA_EMBEDDED_FILTERS bit layout


def _find_decimation_filter_dpp_filter(sensor):
    """Return the embedded filter exposing Decimation Filter DPP on this sensor, or None - the
    composite option lives on one of the sensor's embedded filters, its own independent options
    registry, not on the sensor itself (see rs2::embedded_filter)."""
    for embedded_filter in sensor.query_embedded_filters():
        if rs.composite_option_id.decimation_filter_dpp in embedded_filter.get_supported_composite_options():
            return embedded_filter
    return None


def _has_profile(depth_sensor, width, height):
    return any(p.stream_type() == rs.stream.depth
               and p.as_video_stream_profile().width() == width
               and p.as_video_stream_profile().height() == height
               for p in depth_sensor.get_stream_profiles())


def _set_enabled(embedded_filter, option_id, enabled, magnitude=None):
    """Decimation Filter DPP is read/write only while Depth/IR is idle - call this only before
    a stream is opened, never while one is active."""
    cfg = embedded_filter.get_decimation_filter_dpp_config(option_id)
    cfg.enabled = 1 if enabled else 0
    if magnitude is not None:
        cfg.magnitude = magnitude
    embedded_filter.set_decimation_filter_dpp_config(option_id, cfg)


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


def _decimation_filter_or_skip(depth_sensor):
    embedded_filter = _find_decimation_filter_dpp_filter(depth_sensor)
    if embedded_filter is None:
        pytest.skip("Decimation Filter DPP composite option not supported on this device")

    option_id = rs.composite_option_id.decimation_filter_dpp
    try:
        embedded_filter.get_composite_option(option_id)
    except RuntimeError as e:
        # Registered but not actually functional on this device/FW is a real, expected outcome -
        # get_supported_composite_options() only reflects static registration, never a live
        # capability check.
        pytest.skip(f"Decimation Filter DPP registered but not functional on this device/FW: {e}")

    if not _has_profile(depth_sensor, STREAM_WIDTH, STREAM_HEIGHT):
        pytest.skip(f"No {STREAM_WIDTH}x{STREAM_HEIGHT} depth profile on this device")

    return embedded_filter, option_id


def test_decimation_filter_dpp_produces_640x360_with_metadata_set(test_device):
    """Enable Decimation Filter DPP (enable=1, magnitude=2) before streaming starts, then stream
    the 640x360 depth profile: the frames must actually be 640x360 and must carry
    RS2_FRAME_METADATA_EMBEDDED_FILTERS with the decimation bit set.

    Expected to fail today on real HW - see the module docstring's "Known FW limitation" note.
    Left as a real (non-xfail) assertion, both because this repo's conftest.py crashes on xfail
    reports (ExceptionChainRepr isn't subscriptable in pytest_runtest_makereport) and because a
    plain failure is the most visible signal the moment FW starts populating this attribute."""
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()
    embedded_filter, option_id = _decimation_filter_or_skip(depth_sensor)

    try:
        _set_enabled(embedded_filter, option_id, enabled=True, magnitude=2)
        frame = _stream_one_depth_frame(dev, ctx, STREAM_WIDTH, STREAM_HEIGHT)
        assert frame is not None, "No depth frame received with decimation enabled"
        assert (frame.get_width(), frame.get_height()) == (STREAM_WIDTH, STREAM_HEIGHT), (
            f"Expected {STREAM_WIDTH}x{STREAM_HEIGHT}, got {frame.get_width()}x{frame.get_height()}")

        assert frame.supports_frame_metadata(rs.frame_metadata_value.embedded_filters), (
            "RS2_FRAME_METADATA_EMBEDDED_FILTERS not reported as supported on this frame "
            "with Decimation Filter DPP enabled")
        value = frame.get_frame_metadata(rs.frame_metadata_value.embedded_filters)
        assert value & DECIMATION_APPLIED_BIT, (
            f"RS2_FRAME_METADATA_EMBEDDED_FILTERS = {value:#x}, decimation bit (1<<0) not set")
    finally:
        _set_enabled(embedded_filter, option_id, enabled=False)


def test_decimation_filter_dpp_disabled_does_not_set_metadata_bit(test_device):
    """Baseline/negative check: with Decimation Filter DPP disabled, the same 640x360 profile
    still streams at 640x360 (produced by the native path), but the decimation bit of
    RS2_FRAME_METADATA_EMBEDDED_FILTERS must not be set."""
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()
    embedded_filter, option_id = _decimation_filter_or_skip(depth_sensor)

    try:
        _set_enabled(embedded_filter, option_id, enabled=False)
        frame = _stream_one_depth_frame(dev, ctx, STREAM_WIDTH, STREAM_HEIGHT)
        assert frame is not None, "No depth frame received with decimation disabled"
        assert (frame.get_width(), frame.get_height()) == (STREAM_WIDTH, STREAM_HEIGHT), (
            f"Expected {STREAM_WIDTH}x{STREAM_HEIGHT}, got {frame.get_width()}x{frame.get_height()}")

        if frame.supports_frame_metadata(rs.frame_metadata_value.embedded_filters):
            value = frame.get_frame_metadata(rs.frame_metadata_value.embedded_filters)
            assert not (value & DECIMATION_APPLIED_BIT), (
                f"RS2_FRAME_METADATA_EMBEDDED_FILTERS = {value:#x} has the decimation bit set "
                f"even though Decimation Filter DPP is disabled")
    finally:
        _set_enabled(embedded_filter, option_id, enabled=False)
