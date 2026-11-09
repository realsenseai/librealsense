# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Improved Close Range Depth (MinZ) supports only some Depth profiles. Checks that the SDK rejects every
unsupported Depth profile; the nightly tests, which stream, also check that every supported one is accepted
and that enabling the filter while streaming follows the same rules."""

import time
from contextlib import contextmanager
import pytest
import pyrealsense2 as rs
from rspy.timer import Timer
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D555", "D585"),
    pytest.mark.device_exclude("D585S"),
]

ICR = rs.composite_option_id.icr_control
DECIMATION = rs.composite_option_id.decimation_filter_dpp
MAX_FPS = 30
MIN_WIDTH = 128  # the Downscale Secondary must be at least this wide

# mode -> (ICR mode, downscale_ratio, ratio both Depth dimensions must be divisible by)
MODES = {
    "downscale_x2": (1, 1, 2),
    "downscale_x4": (1, 2, 4),
    "high_res": (2, 1, 1),
}
EXCLUDED = {(1600, 1300), (1600, 966), (256, 144)}
# A stream opened with ICR Off prebuilds the Downscale x2 topology only; enabling any other mode live is rejected as busy
LIVE_ENABLE = {"downscale_x2"}


def _supported(mode, w, h, fps):
    _, _, divisor = MODES[mode]
    return (w, h) not in EXCLUDED and w % divisor == 0 and h % divisor == 0 and w // divisor >= MIN_WIDTH and fps <= MAX_FPS


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


def _set_icr(icr, mode, enable):
    icr_mode, ratio, _ = MODES[mode]
    cfg = icr.get_icr_control(ICR)
    cfg.mode = icr_mode if enable else 0
    cfg.downscale_ratio = ratio
    icr.set_icr_control(ICR, cfg)


def _set_decimation(decimation, enabled):
    if DECIMATION not in decimation.get_supported_composite_options():  # DDS exposes Decimation as scalar options
        decimation.set_option(rs.option.embedded_filter_enabled, enabled)
        return
    cfg = decimation.get_decimation_filter_dpp_config(DECIMATION)
    cfg.enabled = enabled
    decimation.set_decimation_filter_dpp_config(DECIMATION, cfg)


def _decimation_enabled(decimation):
    if DECIMATION not in decimation.get_supported_composite_options():
        return int(decimation.get_option(rs.option.embedded_filter_enabled))
    return decimation.get_decimation_filter_dpp_config(DECIMATION).enabled


@pytest.fixture
def filters(test_device):
    """Yields (depth sensor, icr filter, decimation filter) with both filters off, and restores their original
    values afterwards."""
    dev, ctx = test_device
    sensor = dev.first_depth_sensor()
    icr = _find_filter(sensor, ICR)
    decimation = next((f for f in sensor.query_embedded_filters() if f.get_type() == rs.embedded_filter_type.decimation), None)
    if icr is None or decimation is None:
        pytest.skip("Improved Close Range Depth / Decimation filters not supported on this device")
    original_icr = icr.get_composite_option(ICR)
    original_decimation = _decimation_enabled(decimation)
    _set_decimation(decimation, 0)
    _set_icr(icr, "downscale_x2", 0)
    yield sensor, icr, decimation
    if sensor.get_active_streams():  # an unsupported profile that opened by mistake; never started
        sensor.close()
    icr.set_composite_option(ICR, original_icr)
    _set_decimation(decimation, original_decimation)


def _profiles_by_support(sensor, mode):
    supported, unsupported = [], []
    for p in _depth_profiles(sensor):
        v = p.as_video_stream_profile()
        (supported if _supported(mode, v.width(), v.height(), p.fps()) else unsupported).append(p)
    return supported, unsupported


@pytest.mark.parametrize("mode", MODES)
def test_unsupported_profiles_rejected(filters, mode):
    sensor, icr, _ = filters
    _set_icr(icr, mode, 1)
    _, unsupported = _profiles_by_support(sensor, mode)
    for profile in unsupported:
        log.info(f"{mode} {_describe(profile)}: expecting rejection")
        with pytest.raises(RuntimeError, match="Improved Close Range Depth .*requires Depth"):
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
    sensor, icr, _ = filters
    _set_icr(icr, mode, 1)
    with pytest.raises(RuntimeError, match="Improved Close Range Depth .*requires Depth"):
        sensor.open(_unsupported_depth_ir(sensor, mode))


@pytest.mark.context("nightly")
@pytest.mark.parametrize("mode", MODES)
def test_depth_streams_after_rejection(filters, mode):
    """Both tickets expect a rejected profile not to affect normal Depth output"""
    sensor, icr, _ = filters
    _set_icr(icr, mode, 1)
    with pytest.raises(RuntimeError, match="Improved Close Range Depth .*requires Depth"):
        sensor.open(_unsupported_depth_ir(sensor, mode))
    supported, _ = _profiles_by_support(sensor, mode)
    assert _stream_until_frame(sensor, supported[0]), f"{mode} {_describe(supported[0])}: no frames after rejection"


@pytest.mark.context("nightly")
@pytest.mark.parametrize("mode", MODES)
def test_supported_profiles_accepted(filters, mode):
    sensor, icr, _ = filters
    _set_icr(icr, mode, 1)
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
    sensor, icr, _ = filters
    _set_icr(icr, mode, 0)
    supported, unsupported = _profiles_by_support(sensor, mode)
    assert supported and unsupported, f"{mode}: need both a supported and an unsupported Depth profile"

    with _streaming(sensor, unsupported[0]):
        with pytest.raises(RuntimeError, match="Improved Close Range Depth .*requires Depth"):
            _set_icr(icr, mode, 1)
    assert icr.get_icr_control(ICR).mode == 0, f"{mode} {_describe(unsupported[0])}: filter got enabled"

    frames = []
    with _streaming(sensor, supported[0], lambda f: frames.append(f.get_frame_number())):
        timer = Timer(5)  # FW builds its pipe by the first frame; a SET sent before it is not yet locked
        timer.start()
        while not frames and not timer.has_expired():
            time.sleep(0.05)
        assert frames, f"{mode} {_describe(supported[0])}: no frames"
        if mode in LIVE_ENABLE:
            _set_icr(icr, mode, 1)
        else:
            with pytest.raises(RuntimeError):
                _set_icr(icr, mode, 1)
    enabled = icr.get_icr_control(ICR).mode != 0
    assert enabled == (mode in LIVE_ENABLE), f"{mode} {_describe(supported[0])}: filter {'not ' if not enabled else ''}enabled"


def test_downscale_x4_with_decimation_rejected(filters):
    sensor, icr, decimation = filters
    profile = next((p for p in _depth_profiles(sensor) if _describe(p) == "640x360@30"), None)
    if profile is None:
        pytest.skip("Depth 640x360@30 not available")

    _set_decimation(decimation, 1)
    _set_icr(icr, "downscale_x4", 1)
    with pytest.raises(RuntimeError, match="cannot be combined with the embedded Decimation filter"):
        sensor.open(profile)
