# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device("D555"),
    pytest.mark.context("dds"),
    pytest.mark.context("nightly"),
]

_h264_profile = None  # set by test_h264_format_support, reused by test_h264_streaming_conversion
_module_state = {}


def _target_device(ctx, module_device_setup):
    # On hubless multi-device rigs the context sees every connected device; find the parametrized
    # one by SN rather than picking index 0.
    target_sn = module_device_setup if isinstance(module_device_setup, str) else None
    if not target_sn:
        return ctx.query_devices()[0]
    dev = next(
        (d for d in ctx.query_devices()
         if d.supports(rs.camera_info.serial_number)
         and d.get_info(rs.camera_info.serial_number) == target_sn),
        None)
    if dev is None:
        pytest.fail(f"Target device {target_sn} not visible in context")
    return dev


def test_h264_format_support(module_device_setup):
    """Prerequisite: device must publish H.264 profiles."""
    global _h264_profile
    ctx = rs.context({"format-conversion": "raw"})
    color_sensor = _target_device(ctx, module_device_setup).first_color_sensor()
    _h264_profile = next(
        (p for p in color_sensor.profiles
         if p.stream_type() == rs.stream.color and p.format() == rs.format.h264),
        None
    )
    if _h264_profile is None:
        pytest.fail("Device does not publish H.264 profiles")
    log.debug(f"Device publishes H.264 with profile: {_h264_profile}")
    _module_state['h264_ok'] = True


def test_h264_streaming_conversion(module_device_setup):
    if not _module_state.get('h264_ok'):
        pytest.skip("prerequisite test_h264_format_support failed")
    """Stream H.264 color and verify decoding to RGB8 succeeds for 10 frames."""
    pipeline = rs.pipeline()
    config = rs.config()
    if isinstance(module_device_setup, str):
        config.enable_device(module_device_setup)
    vp = _h264_profile.as_video_stream_profile()
    # H.264 is decoded to RGB8; without a platform decoder no such profile is exposed at all
    config.enable_stream(rs.stream.color, vp.stream_index(), vp.width(), vp.height(), rs.format.rgb8, vp.fps())
    pipeline.start(config)
    try:
        for i in range(10):
            frames = pipeline.wait_for_frames()
            color = frames.get_color_frame()
            assert color.get_width() == vp.width() and color.get_height() == vp.height()
            assert color.get_data_size() == vp.width() * vp.height() * 3
            log.debug(frames)
    finally:
        pipeline.stop()
