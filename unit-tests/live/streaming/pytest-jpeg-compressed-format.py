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
    # On hubless multi-device rigs (e.g. Jetson with D457 + D436) the context sees every
    # connected device; find the parametrized one by SN rather than picking index 0.
    target_sn = module_device_setup if isinstance(module_device_setup, str) else None
    if target_sn:
        target_dev = next(
            (d for d in ctx.query_devices()
             if d.supports(rs.camera_info.serial_number)
             and d.get_info(rs.camera_info.serial_number) == target_sn),
            None)
        if target_dev is None:
            pytest.fail(f"Target device {target_sn} not visible in context")
    else:
        target_dev = ctx.query_devices()[0]
    color_sensor = target_dev.first_color_sensor()
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
    # On hubless multi-device rigs (e.g. Jetson with D457 + D436) the context sees every
    # connected device; without enable_device(sn) the pipeline picks the first match.
    if isinstance(module_device_setup, str):
        config.enable_device(module_device_setup)
    vp = _jpeg_profile.as_video_stream_profile()
    config.enable_stream(rs.stream.color, vp.stream_index(), vp.width(), vp.height(), rs.format.rgb8, vp.fps()) # JPEG is converted to RGB8
    pipeline.start(config)
    try:
        for i in range(10):
            frames = pipeline.wait_for_frames()
            log.debug(frames)
    finally:
        pipeline.stop()
