# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Color FPS must hold with Auto-Exposure Priority on, in a bright scene (RealTek ISP SKUs).

Escapee RSDSO-21982: with auto_exposure_priority on, D436 color dropped to 15 fps even in daylight.
Root cause was RealTek ISP FW 3613 (DS5 5.17.3.1 - 5.17.3.10); ISP FW 3614 fixes it.

Method:
  1. Calibrate the scene on the rig: stream with AE on and AE priority off (fps stays fixed), take the
     median luma once AE converged. Then stream with manual exposure of half the frame period at minimum
     gain. If that probe is darker than the AE result, AE would need more than half a period: skip.
     Minimum gain makes the probe a lower bound for any gain AE may pick, so the skip is conservative.
     Both values are in the same (gamma-encoded) pixel space, so no gamma model or fixed threshold is needed.
  2. Otherwise enable AE and AE priority and measure the color fps (fps_helper.measure_fps).
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
import fps_helper

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D436", "D455", "D457"),
    pytest.mark.context("nightly"),
]

fps_helper.TIME_FOR_STEADY_STATE = 3.0   # let AE converge after start
fps_helper.TIME_TO_COUNT_FRAMES = 6.0
FPS_MIN_RATIO, FPS_MAX_RATIO = 0.90, 1.05
EXPOSURE_UNIT_USEC = 100      # color exposure option units
AE_PROBE_STEADY, MANUAL_PROBE_STEADY, PROBE_COUNT = 3.0, 1.5, 2.0
LUMA_SAMPLE_STRIDE = 97       # sample every Nth byte, keeps the frame callback cheap


def _stream_luma(sensor, profile, steady_time, count_time):
    """Stream the profile and return the median mean-pixel value (0-255) of the frames after steady_time."""
    state = {'counting': False, 'lumas': []}

    def on_frame(frame):
        if state['counting']:
            data = np.asanyarray(frame.get_data()).reshape(-1)
            state['lumas'].append(float(data[::LUMA_SAMPLE_STRIDE].mean()))

    sensor.open(profile)
    try:
        sensor.start(on_frame)
        try:
            time.sleep(steady_time)
            state['counting'] = True
            time.sleep(count_time)
            state['counting'] = False
        finally:
            sensor.stop()
    finally:
        sensor.close()
    if not state['lumas']:
        pytest.skip("Brightness probe received no color frames, cannot tell if the scene is bright")
    return statistics.median(state['lumas'])


def _calibrate_scene(sensor, profile, fps):
    """Return (AE-converged luma with fixed fps, luma at half-period manual exposure and minimum gain)."""
    sensor.set_option(rs.option.enable_auto_exposure, 1)
    sensor.set_option(rs.option.auto_exposure_priority, 0)   # fps stays fixed, AE may use up to a full period
    ae_luma = _stream_luma(sensor, profile, AE_PROBE_STEADY, PROBE_COUNT)

    exp_range = sensor.get_option_range(rs.option.exposure)
    half_period_units = 0.5e6 / fps / EXPOSURE_UNIT_USEC
    exposure = min(max(half_period_units, exp_range.min), exp_range.max)
    sensor.set_option(rs.option.enable_auto_exposure, 0)
    sensor.set_option(rs.option.exposure, exposure)
    if sensor.supports(rs.option.gain):
        sensor.set_option(rs.option.gain, sensor.get_option_range(rs.option.gain).min)
    half_luma = _stream_luma(sensor, profile, MANUAL_PROBE_STEADY, PROBE_COUNT)
    log.info(f"Brightness calibration: AE mean pixel {ae_luma:.1f}, "
             f"manual {exposure * EXPOSURE_UNIT_USEC / 1000:.2f} msec mean pixel {half_luma:.1f}")
    return ae_luma, half_luma


@pytest.mark.parametrize("fps", [30, 60])
def test_color_fps_with_ae_priority(test_device, fps):
    dev, ctx = test_device
    product_name = dev.get_info(rs.camera_info.name)
    log.info(f"Testing color fps {fps} with AE priority on, {product_name}, "
             f"firmware: {dev.get_info(rs.camera_info.firmware_version)}")

    cs = dev.first_color_sensor()
    if not cs.supports(rs.option.auto_exposure_priority):
        pytest.skip(f"{product_name} color sensor does not support auto_exposure_priority")
    profile = next((p for p in cs.profiles if p.stream_type() == rs.stream.color and p.fps() == fps
                    and p.format() == rs.format.rgb8), None)
    if profile is None:
        pytest.skip(f"{product_name} has no rgb8 color profile at {fps} fps")

    orig_ae = cs.get_option(rs.option.enable_auto_exposure)
    orig_priority = cs.get_option(rs.option.auto_exposure_priority)
    orig_exposure = cs.get_option(rs.option.exposure)
    orig_gain = cs.get_option(rs.option.gain) if cs.supports(rs.option.gain) else None
    try:
        ae_luma, half_luma = _calibrate_scene(cs, profile, fps)
        if half_luma < ae_luma:
            pytest.skip(f"Scene too dark: half-frame-period exposure gives mean pixel {half_luma:.1f}, below the "
                        f"AE result {ae_luma:.1f}; an AE-priority fps drop at {fps} fps would be legitimate")

        cs.set_option(rs.option.enable_auto_exposure, 1)
        cs.set_option(rs.option.auto_exposure_priority, 1)
        measured_fps = fps_helper.measure_fps({cs: [profile]})[profile.stream_name()]
    finally:
        cs.set_option(rs.option.enable_auto_exposure, 0)
        cs.set_option(rs.option.exposure, orig_exposure)
        if orig_gain is not None:
            cs.set_option(rs.option.gain, orig_gain)
        cs.set_option(rs.option.auto_exposure_priority, orig_priority)
        cs.set_option(rs.option.enable_auto_exposure, orig_ae)

    log.info(f"Requested {fps} fps, measured {measured_fps:.1f} fps, scene mean pixel {ae_luma:.1f}")
    low, high = fps * FPS_MIN_RATIO, fps * FPS_MAX_RATIO
    assert low <= measured_fps <= high, (
        f"Color {fps} fps with AE priority on in a bright scene (mean pixel {ae_luma:.1f}): "
        f"got {measured_fps:.1f} fps, expected {low:.1f}-{high:.1f}")
