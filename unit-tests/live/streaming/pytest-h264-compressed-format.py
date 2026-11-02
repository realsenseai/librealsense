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


@pytest.fixture(scope="module")
def context():
    """One context for the module. A profile does not keep alive what it came from, so this has to outlive them all."""
    yield rs.context()


@pytest.fixture(scope="module")
def color_sensor(context, module_device_setup):
    yield _target_device(context, module_device_setup).first_color_sensor()


@pytest.fixture(scope="module")
def h264_profile(module_device_setup):
    """The device's H.264 profile. A fixture, so every test stands on its own whatever the run order."""
    # Raw, because only there do the profiles still carry the device's own default flag; conversion drops it
    ctx = rs.context({"format-conversion": "raw"})
    color_sensor = _target_device(ctx, module_device_setup).first_color_sensor()
    candidates = [p for p in color_sensor.profiles
                  if p.stream_type() == rs.stream.color and p.format() == rs.format.h264]
    if not candidates:
        pytest.fail("Device does not publish H.264 profiles")
    # The device marks the one it is built to stream; the others are published but not all of them deliver frames
    profile = next((p for p in candidates if p.is_default()), candidates[0])
    log.debug(f"Device publishes H.264 with profile: {profile}")
    yield profile


@pytest.fixture(scope="module")
def decoded_profile(color_sensor, h264_profile):
    """The RGB8 profile the H.264 stream decodes to, or a skip where the platform has no decoder."""
    vp = h264_profile.as_video_stream_profile()
    decoded = next(
        (p for p in color_sensor.profiles
         if p.format() == rs.format.rgb8 and p.stream_index() == vp.stream_index()
         and p.as_video_stream_profile().width() == vp.width()
         and p.as_video_stream_profile().height() == vp.height()),
        None
    )
    if decoded is None:
        # No decoder on this machine: Linux without BUILD_WITH_VPL, or without an Intel GPU to back it
        pytest.skip(f"No RGB8 profile at {vp.width()}x{vp.height()}; this platform cannot decode H.264")
    return decoded


def _start_pipeline(context, module_device_setup, profile):
    pipeline = rs.pipeline(context)
    config = rs.config()
    if isinstance(module_device_setup, str):
        config.enable_device(module_device_setup)
    vp = profile.as_video_stream_profile()
    config.enable_stream(rs.stream.color, vp.stream_index(), vp.width(), vp.height(), profile.format(), vp.fps())
    pipeline.start(config)
    return pipeline


def test_h264_format_support(h264_profile):
    """Device must publish H.264 profiles."""
    assert h264_profile.as_video_stream_profile().width() > 0


def test_h264_streaming_conversion(context, module_device_setup, decoded_profile):
    """Stream H.264 color and verify decoding to RGB8 succeeds for 10 frames."""
    vp = decoded_profile.as_video_stream_profile()
    pipeline = _start_pipeline(context, module_device_setup, decoded_profile)
    try:
        for i in range(10):
            frames = pipeline.wait_for_frames()
            # H.264 has a color index of its own, so the default get_color_frame() would look at the wrong stream
            color = frames.get_color_frame(vp.stream_index())
            assert color.get_width() == vp.width() and color.get_height() == vp.height()
            assert color.get_data_size() == vp.width() * vp.height() * 3
            log.debug(frames)
    finally:
        pipeline.stop()


def test_h264_passthrough(context, module_device_setup, h264_profile, decoded_profile):
    """Take the undecoded access units and decode them through the public block, as an application would."""
    vp = h264_profile.as_video_stream_profile()
    decoder = rs.h264_decoder()
    queue = rs.frame_queue(capacity=10, keep_frames=False)
    decoder.start(queue)

    sizes, decoded = set(), 0
    pipeline = _start_pipeline(context, module_device_setup, h264_profile)
    try:
        for i in range(30):
            color = pipeline.wait_for_frames().get_color_frame(vp.stream_index())
            sizes.add(color.get_data_size())
            # The decoder yields nothing until it starts, so feed it rather than process() it, which would throw
            decoder.invoke(color)
            out = queue.poll_for_frame()
            if out:
                assert out.get_profile().format() == rs.format.rgb8
                assert out.as_video_frame().get_data_size() == vp.width() * vp.height() * 3
                decoded += 1
    finally:
        pipeline.stop()

    assert len(sizes) > 1, f"Access units should vary in size, all were {sizes}"
    assert decoded > 0, "Decoder produced no pictures from the passthrough stream"
