# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device("D555"),
    pytest.mark.context("dds"),
    pytest.mark.context("nightly"),
]

_jpeg_profile = None  # set by test_jpeg_format_support, reused by test_jpeg_streaming_conversion
_module_state = {}


def test_jpeg_format_support(module_device_setup):
    """Prerequisite: device must support JPEG streaming."""
    global _jpeg_profile
    ctx = rs.context({"format-conversion": "raw"})
    color_sensor = ctx.query_devices()[0].first_color_sensor()
    _jpeg_profile = next(
        (p for p in color_sensor.profiles
         if p.stream_type() == rs.stream.color and p.format() == rs.format.mjpeg),
        None
    )
    if _jpeg_profile is None:
        pytest.fail("Device does not support JPEG streaming")
    log.debug(f"Device supports JPEG streaming with profile: {_jpeg_profile}")
    _module_state['jpeg_ok'] = True


def test_jpeg_streaming_conversion(module_device_setup):
    if not _module_state.get('jpeg_ok'):
        pytest.skip("prerequisite test_jpeg_format_support failed")
    """Stream JPEG color and verify conversion to RGB8 succeeds for 10 frames."""
    pipeline = rs.pipeline()
    config = rs.config()
    vp = _jpeg_profile.as_video_stream_profile()
    config.enable_stream(rs.stream.color, vp.stream_index(), vp.width(), vp.height(), rs.format.rgb8, vp.fps()) # JPEG is converted to RGB8
    pipeline.start(config)
    try:
        for i in range(10):
            frames = pipeline.wait_for_frames()
            log.debug(frames)
    finally:
        pipeline.stop()
