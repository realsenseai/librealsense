# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Improved Close Range Depth (MinZ) supports only some Depth profiles. Checks that the SDK rejects every
unsupported Depth profile; the nightly tests, which stream, also check that every supported one is accepted
and that enabling the filter while streaming follows the same rules."""

import time
from contextlib import contextmanager
import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D585"),
    pytest.mark.device_exclude("D585S"),
    pytest.mark.device_type_exclude("DDS"),  # DDS devices expose a different close range filter
]

HDRD = rs.composite_option_id.hdrd_control
DECIMATION = rs.composite_option_id.decimation_filter_dpp
MAX_FPS = 30

# mode -> (filter_type, downscale_ratio, min width, min height, width/height alignment)
MODES = {
    "downscale_x2": (0, 1, 640, 360, 2),
    "downscale_x4": (0, 2, 1280, 720, 4),
    "lookup_shift": (1, 1, 424, 240, 1),
}


def _supported(mode, w, h, fps):
    _, _, min_w, min_h, align = MODES[mode]
    return w >= min_w and h >= min_h and w % align == 0 and h % align == 0 and fps <= MAX_FPS


def _find_filter(sensor, option_id):
    for embedded_filter in sensor.query_embedded_filters():
        if option_id in embedded_filter.get_supported_composite_options():
            return embedded_filter
    return None


def _depth_profiles(sensor):
    return [p for p in sensor.get_stream_profiles()
            if p.stream_type() == rs.stream.depth and p.format() == rs.format.z16]


def _describe(p):
    v = p.as_video_stream_profile()
    return f"{v.width()}x{v.height()}@{p.fps()}"


@contextmanager
def _streaming(sensor, profile, callback=lambda f: None):
    """Stops and closes even when the body fails, so the next test finds the sensor idle"""
    sensor.open(profile)
    try:
        sensor.start(callback)
        try:
            yield
        finally:
            sensor.stop()
    finally:
        sensor.close()


def _stream_until_frame(sensor, profile, timeout=5):
    """Always start after open: open+close without streaming can leave a D585 USB pin dead until reset"""
    frames = []
    with _streaming(sensor, profile, lambda f: frames.append(f.get_frame_number())):
        deadline = time.time() + timeout
        while not frames and time.time() < deadline:
            time.sleep(0.05)
    return len(frames)


def _set_hdrd(hdrd, mode, enable):
    filter_type, ratio, _, _, _ = MODES[mode]
    cfg = hdrd.get_hdrd_control(HDRD)
    cfg.enable = enable
    cfg.filter_type = filter_type
    cfg.downscale_ratio = ratio
    hdrd.set_hdrd_control(HDRD, cfg)


def _set_decimation(decimation, enabled):
    cfg = decimation.get_decimation_filter_dpp_config(DECIMATION)
    cfg.enabled = enabled
    decimation.set_decimation_filter_dpp_config(DECIMATION, cfg)


@pytest.fixture
def filters(test_device):
    """Yields (depth sensor, hdrd filter, decimation filter) with both filters off, and restores their original
    values afterwards."""
    dev, ctx = test_device
    sensor = dev.first_depth_sensor()
    hdrd = _find_filter(sensor, HDRD)
    decimation = _find_filter(sensor, DECIMATION)
    if hdrd is None or decimation is None:
        pytest.skip("Improved Close Range Depth / Decimation composite options not supported on this device")
    original_hdrd = bytearray(hdrd.get_composite_option(HDRD))
    original_hdrd[-4:] = bytes(4)  # reserved MUST be zero on SET
    original_decimation = decimation.get_composite_option(DECIMATION)
    _set_decimation(decimation, 0)
    _set_hdrd(hdrd, "downscale_x2", 0)
    yield sensor, hdrd, decimation
    if sensor.get_active_streams():  # an unsupported profile that opened by mistake; never started
        sensor.close()
    hdrd.set_composite_option(HDRD, bytes(original_hdrd))
    decimation.set_composite_option(DECIMATION, original_decimation)


def _profiles_by_support(sensor, mode):
    supported, unsupported = [], []
    for p in _depth_profiles(sensor):
        v = p.as_video_stream_profile()
        (supported if _supported(mode, v.width(), v.height(), p.fps()) else unsupported).append(p)
    return supported, unsupported


@pytest.mark.parametrize("mode", MODES)
def test_unsupported_profiles_rejected(filters, mode):
    sensor, hdrd, _ = filters
    _set_hdrd(hdrd, mode, 1)
    _, unsupported = _profiles_by_support(sensor, mode)
    for profile in unsupported:
        log.info(f"{mode} {_describe(profile)}: expecting rejection")
        with pytest.raises(RuntimeError, match="Improved Close Range Depth .* requires Depth"):
            sensor.open(profile)


def _unsupported_depth_ir(sensor, mode):
    """An unsupported Depth profile with the IR 1 profile of the same size and FPS - RSDEV-15484 is Depth+IR"""
    _, unsupported = _profiles_by_support(sensor, mode)
    for depth in unsupported:
        d = depth.as_video_stream_profile()
        ir = next((p for p in sensor.get_stream_profiles()
                   if p.stream_type() == rs.stream.infrared and p.stream_index() == 1 and p.fps() == depth.fps()
                   and p.as_video_stream_profile().width() == d.width()
                   and p.as_video_stream_profile().height() == d.height()), None)
        if ir:
            return [depth, ir]
    pytest.skip(f"{mode}: no unsupported Depth profile with matching IR")


@pytest.mark.parametrize("mode", MODES)
def test_unsupported_depth_ir_rejected(filters, mode):
    sensor, hdrd, _ = filters
    _set_hdrd(hdrd, mode, 1)
    with pytest.raises(RuntimeError, match="Improved Close Range Depth .* requires Depth"):
        sensor.open(_unsupported_depth_ir(sensor, mode))


@pytest.mark.context("nightly")
@pytest.mark.parametrize("mode", MODES)
def test_depth_streams_after_rejection(filters, mode):
    """Both tickets expect a rejected profile not to affect normal Depth output"""
    sensor, hdrd, _ = filters
    _set_hdrd(hdrd, mode, 1)
    with pytest.raises(RuntimeError, match="Improved Close Range Depth .* requires Depth"):
        sensor.open(_unsupported_depth_ir(sensor, mode))
    supported, _ = _profiles_by_support(sensor, mode)
    assert _stream_until_frame(sensor, supported[0]), f"{mode} {_describe(supported[0])}: no frames after rejection"


@pytest.mark.context("nightly")
@pytest.mark.parametrize("mode", MODES)
def test_supported_profiles_accepted(filters, mode):
    sensor, hdrd, _ = filters
    _set_hdrd(hdrd, mode, 1)
    supported, _ = _profiles_by_support(sensor, mode)
    by_resolution = {}  # one profile per resolution, at the highest supported FPS, keeps the run short
    for p in supported:
        v = p.as_video_stream_profile()
        key = (v.width(), v.height())
        if key not in by_resolution or p.fps() > by_resolution[key].fps():
            by_resolution[key] = p
    for profile in by_resolution.values():
        log.info(f"{mode} {_describe(profile)}: expecting acceptance")
        assert _stream_until_frame(sensor, profile), f"{mode} {_describe(profile)}: no frames"


@pytest.mark.context("nightly")
@pytest.mark.parametrize("mode", MODES)
def test_enable_while_streaming(filters, mode):
    sensor, hdrd, _ = filters
    _set_hdrd(hdrd, mode, 0)  # filter_type/downscale_ratio cannot change while streaming
    supported, unsupported = _profiles_by_support(sensor, mode)
    assert supported and unsupported, f"{mode}: need both a supported and an unsupported Depth profile"

    with _streaming(sensor, unsupported[0]):
        with pytest.raises(RuntimeError, match="Improved Close Range Depth .* requires Depth"):
            _set_hdrd(hdrd, mode, 1)
    assert hdrd.get_hdrd_control(HDRD).enable == 0, f"{mode} {_describe(unsupported[0])}: filter got enabled"

    with _streaming(sensor, supported[0]):
        _set_hdrd(hdrd, mode, 1)
    assert hdrd.get_hdrd_control(HDRD).enable == 1, f"{mode} {_describe(supported[0])}: filter not enabled"


@pytest.mark.context("nightly")
def test_geometry_change_while_streaming_rejected(filters):
    sensor, hdrd, _ = filters
    profile = next((p for p in _depth_profiles(sensor) if _describe(p) == "640x480@30"), None)
    if profile is None:
        pytest.skip("Depth 640x480@30 not available")

    _set_hdrd(hdrd, "downscale_x2", 1)
    with _streaming(sensor, profile):
        with pytest.raises(RuntimeError, match="cannot be changed while Depth or Infrared is streaming"):
            _set_hdrd(hdrd, "lookup_shift", 1)
        _set_hdrd(hdrd, "downscale_x2", 0)  # same Filter Type and Downscale Ratio - allowed
    assert hdrd.get_hdrd_control(HDRD).filter_type == 0, "Filter Type changed while streaming"


def test_downscale_x4_with_decimation_rejected(filters):
    sensor, hdrd, decimation = filters
    profile = next((p for p in _depth_profiles(sensor) if _describe(p) == "640x360@30"), None)
    if profile is None:
        pytest.skip("Depth 640x360@30 not available")

    _set_decimation(decimation, 1)
    _set_hdrd(hdrd, "downscale_x4", 1)
    with pytest.raises(RuntimeError, match="cannot be combined with the embedded Decimation filter"):
        sensor.open(profile)
