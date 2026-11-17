# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Color FPS must hold with Auto-Exposure Priority on, in a bright scene (RealTek ISP SKUs).

Escapee RSDSO-21982: with auto_exposure_priority on, D436 color dropped to 15 fps even in daylight.
Root cause was RealTek ISP FW 3613 (DS5 5.17.3.1 - 5.17.3.10); ISP FW 3614 fixes it.

Method:
  1. Brightness probe: manual exposure of half the frame period at default gain. If the image is
     darker than BRIGHT_LUMA_MIN, AE would legitimately need more than a frame period: skip (never pass).
  2. Otherwise enable auto exposure and AE priority, stream color at 30 / 60 fps.
  3. Delivered fps must be 90-105% of the configured rate.

The brightness is read from image pixels: the actual_exposure frame metadata only echoes the last
exposure the host set (FW does not update it when AE changes exposure), so it cannot gate the scene.
Needs a bright scene; a dark rig always skips.
"""

import logging
import statistics
import time

import numpy as np
import pytest
import pyrealsense2 as rs

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D436", "D455", "D457"),
    pytest.mark.context("nightly"),
]

TIME_FOR_STEADY_STATE = 3.0   # let AE converge after start
TIME_TO_COUNT_FRAMES = 6.0
FPS_MIN_RATIO, FPS_MAX_RATIO = 0.90, 1.05
EXPOSURE_UNIT_USEC = 100      # color exposure option units
BRIGHT_LUMA_MIN = 60          # mean pixel (0-255) at half-period exposure; AE target is ~mid-grey, so
                              # reaching it then needs about one frame period at most
PROBE_STEADY, PROBE_COUNT = 1.5, 2.0
LUMA_SAMPLE_STRIDE = 97       # sample every Nth byte, keeps the frame callback cheap


def _get_color_profile(sensor, fps):
    profiles = [p for p in sensor.profiles
                if p.stream_type() == rs.stream.color and p.fps() == fps]
    # Prefer rgb8 like the neighbouring fps tests; resolution does not affect the fps cap
    return next((p for p in profiles if p.format() == rs.format.rgb8), profiles[0] if profiles else None)


def _stream(sensor, profile, steady_time, count_time, sample_luma=False):
    """Stream the profile; return (measured fps, sampled mean pixel value of the counted frames)."""
    state = {'counting': False, 'frames': 0, 'lumas': []}

    def on_frame(frame):
        if not state['counting']:
            return
        state['frames'] += 1
        if sample_luma:
            data = np.asanyarray(frame.get_data()).reshape(-1)
            state['lumas'].append(float(data[::LUMA_SAMPLE_STRIDE].mean()))

    sensor.open(profile)
    try:
        sensor.start(on_frame)
        try:
            time.sleep(steady_time)
            state['counting'] = True
            t0 = time.perf_counter()
            time.sleep(count_time)
            state['counting'] = False
            elapsed = time.perf_counter() - t0
        finally:
            sensor.stop()
    finally:
        sensor.close()
    return state['frames'] / elapsed, state['lumas']


def _probe_scene_luma(sensor, profile, fps):
    """Median mean-pixel value with manual exposure of half the frame period at default gain."""
    exp_range = sensor.get_option_range(rs.option.exposure)
    half_period_units = 0.5e6 / fps / EXPOSURE_UNIT_USEC
    exposure = min(max(half_period_units, exp_range.min), exp_range.max)
    sensor.set_option(rs.option.enable_auto_exposure, 0)
    sensor.set_option(rs.option.exposure, exposure)
    if sensor.supports(rs.option.gain):
        sensor.set_option(rs.option.gain, sensor.get_option_range(rs.option.gain).default)
    _, lumas = _stream(sensor, profile, PROBE_STEADY, PROBE_COUNT, sample_luma=True)
    if not lumas:
        pytest.skip("Brightness probe received no color frames, cannot tell if the scene is bright")
    luma = statistics.median(lumas)
    log.info(f"Brightness probe: exposure {exposure * EXPOSURE_UNIT_USEC / 1000:.2f} msec, mean pixel {luma:.1f}")
    return luma


@pytest.mark.parametrize("fps", [30, 60])
def test_color_fps_with_ae_priority(test_device, fps):
    dev, ctx = test_device
    product_name = dev.get_info(rs.camera_info.name)
    log.info(f"Testing color fps {fps} with AE priority on, {product_name}, "
             f"firmware: {dev.get_info(rs.camera_info.firmware_version)}")

    cs = dev.first_color_sensor()
    if not cs.supports(rs.option.auto_exposure_priority):
        pytest.skip(f"{product_name} color sensor does not support auto_exposure_priority")
    profile = _get_color_profile(cs, fps)
    if profile is None:
        pytest.skip(f"{product_name} has no color profile at {fps} fps")

    orig_ae = cs.get_option(rs.option.enable_auto_exposure)
    orig_priority = cs.get_option(rs.option.auto_exposure_priority)
    orig_exposure = cs.get_option(rs.option.exposure)
    orig_gain = cs.get_option(rs.option.gain) if cs.supports(rs.option.gain) else None
    try:
        luma = _probe_scene_luma(cs, profile, fps)
        if luma < BRIGHT_LUMA_MIN:
            pytest.skip(f"Scene too dark: mean pixel {luma:.1f} < {BRIGHT_LUMA_MIN} at half-frame-period exposure, "
                        f"an AE-priority fps drop at {fps} fps would be legitimate")

        cs.set_option(rs.option.enable_auto_exposure, 1)
        cs.set_option(rs.option.auto_exposure_priority, 1)
        measured_fps, _ = _stream(cs, profile, TIME_FOR_STEADY_STATE, TIME_TO_COUNT_FRAMES)
    finally:
        cs.set_option(rs.option.enable_auto_exposure, 0)
        cs.set_option(rs.option.exposure, orig_exposure)
        if orig_gain is not None:
            cs.set_option(rs.option.gain, orig_gain)
        cs.set_option(rs.option.auto_exposure_priority, orig_priority)
        cs.set_option(rs.option.enable_auto_exposure, orig_ae)

    log.info(f"Requested {fps} fps, measured {measured_fps:.1f} fps, scene mean pixel {luma:.1f}")
    low, high = fps * FPS_MIN_RATIO, fps * FPS_MAX_RATIO
    assert low <= measured_fps <= high, (
        f"Color {fps} fps with AE priority on in a bright scene (mean pixel {luma:.1f}): "
        f"got {measured_fps:.1f} fps, expected {low:.1f}-{high:.1f}")
