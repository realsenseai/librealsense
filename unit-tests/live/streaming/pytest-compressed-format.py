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

# A camera can publish H.264 and JPEG as profiles of one compressed colour stream, at the same geometries. Each
# test here streams the format it is named for and decodes it itself, so the codec under test is never left to
# converter registration order; the last test covers what an application gets from a plain RGB8 request.


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


def _start_pipeline(context, module_device_setup, profile, fmt=None):
    """Streams 'profile', or the same stream and geometry in 'fmt' - which is how a converted format is asked for."""
    pipeline = rs.pipeline(context)
    config = rs.config()
    if isinstance(module_device_setup, str):
        config.enable_device(module_device_setup)
    vp = profile.as_video_stream_profile()
    config.enable_stream(rs.stream.color, vp.stream_index(), vp.width(), vp.height(), fmt or profile.format(), vp.fps())
    pipeline.start(config)
    return pipeline


@pytest.fixture(scope="module")
def context():
    """One context for the module. A profile does not keep alive what it came from, so this has to outlive them all."""
    yield rs.context()


@pytest.fixture(scope="module")
def compressed_profiles(module_device_setup):
    """The device's H.264 and JPEG profiles, or None each where it publishes neither."""
    # Raw, so the device's own profiles are listed on their own, without the converted ones mixed in
    ctx = rs.context({"format-conversion": "raw"})
    sensor = _target_device(ctx, module_device_setup).first_color_sensor()

    def pick(fmt):
        # Any published profile will do - the geometry is not what these tests are about
        return next((p for p in sensor.profiles
                     if p.stream_type() == rs.stream.color and p.format() == fmt), None)

    profiles = {fmt: pick(fmt) for fmt in (rs.format.h264, rs.format.mjpeg)}
    log.debug(f"compressed profiles: {[str(p) for p in profiles.values()]}")
    # ctx and sensor stay referenced by this frame: a profile keeps alive neither, and degrades to 0x0 without them
    yield profiles


@pytest.fixture(scope="module")
def h264_profile(compressed_profiles):
    profile = compressed_profiles[rs.format.h264]
    if profile is None:
        # Some firmware versions do not advertise H.264 as a DDS profile, leaving the SDK nothing to select.
        # Skip until the camera offers it again.
        pytest.skip("Device does not publish H.264 profiles")
    return profile


@pytest.fixture(scope="module")
def jpeg_profile(compressed_profiles):
    profile = compressed_profiles[rs.format.mjpeg]
    if profile is None:
        pytest.fail("Device does not publish JPEG profiles")
    return profile


@pytest.fixture
def h264_decoder():
    """A decoder block, or a skip where the platform has no backend for one."""
    if not rs.h264_decoder.is_supported():
        # Linux without BUILD_WITH_VPL, or without an Intel GPU to back it. Asking the decoder is the only honest
        # test: JPEG shares this stream's index and geometry, so an RGB8 profile is offered either way.
        pytest.skip("This platform cannot decode H.264")
    return rs.h264_decoder()


def test_h264_format_support(h264_profile):
    """Device must publish H.264 profiles."""
    assert h264_profile.as_video_stream_profile().width() > 0


def test_h264_streaming_conversion(context, module_device_setup, h264_profile, h264_decoder):
    """Stream H.264 color and verify decoding to RGB8 succeeds for 10 frames."""
    vp = h264_profile.as_video_stream_profile()
    queue = rs.frame_queue(capacity=10, keep_frames=False)
    h264_decoder.start(queue)

    decoded = 0
    pipeline = _start_pipeline(context, module_device_setup, h264_profile)
    try:
        # The backend is built on its own thread, costing a couple of frames before the first picture on Linux
        # and up to nine on Windows, so the budget is well clear of the ten frames actually wanted
        for i in range(60):
            # H.264 has a color index of its own, so the default get_color_frame() would look at the wrong stream
            color = pipeline.wait_for_frames().get_color_frame(vp.stream_index())
            assert color.get_profile().format() == rs.format.h264
            # The decoder yields nothing until it starts, so feed it rather than process() it, which would throw
            h264_decoder.invoke(color)
            out = queue.poll_for_frame()
            if out:
                vf = out.as_video_frame()
                assert out.get_profile().format() == rs.format.rgb8
                assert vf.get_width() == vp.width() and vf.get_height() == vp.height()
                assert vf.get_data_size() == vp.width() * vp.height() * 3
                log.debug(out)
                decoded += 1
                if decoded == 10:
                    break
    finally:
        pipeline.stop()

    assert decoded == 10, f"Decoded {decoded} of 10 frames before the stream ran out"


def test_h264_passthrough(context, module_device_setup, h264_profile):
    """Take the undecoded access units, as an application that decodes them itself would. Needs no decoder."""
    vp = h264_profile.as_video_stream_profile()

    sizes = set()
    pipeline = _start_pipeline(context, module_device_setup, h264_profile)
    try:
        for i in range(30):
            color = pipeline.wait_for_frames().get_color_frame(vp.stream_index())
            assert color.get_profile().format() == rs.format.h264
            assert color.get_width() == vp.width() and color.get_height() == vp.height()
            assert color.get_data_size() > 0
            sizes.add(color.get_data_size())
    finally:
        pipeline.stop()

    assert len(sizes) > 1, f"Access units should vary in size, all were {sizes}"


def test_jpeg_format_support(jpeg_profile):
    """Device must publish JPEG profiles."""
    assert jpeg_profile.as_video_stream_profile().width() > 0


def test_jpeg_streaming_conversion(context, module_device_setup, jpeg_profile):
    """Stream JPEG color and verify decoding to RGB8 succeeds for 10 frames."""
    vp = jpeg_profile.as_video_stream_profile()
    decoder = rs.mjpeg_decoder()

    pipeline = _start_pipeline(context, module_device_setup, jpeg_profile)
    try:
        for i in range(10):
            # JPEG has a color index of its own, so the default get_color_frame() would look at the wrong stream
            color = pipeline.wait_for_frames().get_color_frame(vp.stream_index())
            assert color.get_profile().format() == rs.format.mjpeg
            rgb = decoder.process(color).as_video_frame()
            assert rgb.get_profile().format() == rs.format.rgb8
            assert rgb.get_width() == vp.width() and rgb.get_height() == vp.height()
            assert rgb.get_data_size() == vp.width() * vp.height() * 3
            log.debug(rgb)
    finally:
        pipeline.stop()


def test_jpeg_passthrough(context, module_device_setup, jpeg_profile):
    """Take the undecoded JPEG frames, as an application that decodes them itself would."""
    vp = jpeg_profile.as_video_stream_profile()
    padded_size = vp.width() * vp.height()

    sizes = set()
    pipeline = _start_pipeline(context, module_device_setup, jpeg_profile)
    try:
        for i in range(30):
            color = pipeline.wait_for_frames().get_color_frame(vp.stream_index())
            assert color.get_profile().format() == rs.format.mjpeg
            assert color.get_width() == vp.width() and color.get_height() == vp.height()
            # Firmware before 7.61 pads JPEG into a fixed width*height buffer, later firmware sends the
            # compressed bytes alone. Both are supported, so bound the size instead of requiring it to vary.
            assert 0 < color.get_data_size() <= padded_size
            sizes.add(color.get_data_size())
    finally:
        pipeline.stop()

    log.debug(f"JPEG frames were {'padded to' if sizes == {padded_size} else 'compressed under'} {padded_size} bytes")


def test_compressed_rgb8_conversion(context, module_device_setup, jpeg_profile):
    """What an application actually asks for: RGB8 on the compressed stream, whichever codec the SDK resolves to."""
    # Driven from the JPEG profile because this stream always carries JPEG, so the converted path stays covered
    # on firmware that publishes no H.264 at all
    vp = jpeg_profile.as_video_stream_profile()

    pipeline = _start_pipeline(context, module_device_setup, jpeg_profile, rs.format.rgb8)
    try:
        for i in range(10):
            rgb = pipeline.wait_for_frames().get_color_frame(vp.stream_index()).as_video_frame()
            assert rgb.get_profile().format() == rs.format.rgb8
            assert rgb.get_width() == vp.width() and rgb.get_height() == vp.height()
            assert rgb.get_data_size() == vp.width() * vp.height() * 3
            log.debug(rgb)
    finally:
        pipeline.stop()
