# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Color FPS must hold with Auto-Exposure Priority on, in a bright scene (RealTek ISP SKUs).

Escapee RSDSO-21982: with auto_exposure_priority on, D436 color dropped to 15 fps even in daylight.
Root cause was RealTek ISP FW 3613 (DS5 5.17.3.1 - 5.17.3.10); ISP FW 3614 fixes it.

Method:
  1. Calibrate the unit of the actual_exposure frame metadata (manual exposure, known value).
  2. Enable auto exposure and AE priority, stream color at 30 / 60 fps.
  3. Read the exposure from per-frame metadata. If it is not below the frame period the scene is
     too dark to tell a legitimate AE-priority fps drop from the bug: skip (never pass).
  4. Otherwise delivered fps must be 90-105% of the configured rate.

Needs a bright scene; a dark rig always skips.
"""

import logging
import statistics
import time

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
CALIBRATION_EXPOSURE = 50     # color exposure option units, 100 usec each (5 msec)
EXPOSURE_PERCENTILE = 0.95    # "exposure below frame period" is judged on this percentile

# Exposure scale is learned from the camera, as it must match what the metadata really reports.
_us_per_md_unit_cache = {}


def _get_color_profile(sensor, fps):
    profiles = [p for p in sensor.profiles
                if p.stream_type() == rs.stream.color and p.fps() == fps]
    # Prefer rgb8 like the neighbouring fps tests; resolution does not affect the fps cap
    return next((p for p in profiles if p.format() == rs.format.rgb8), profiles[0] if profiles else None)


def _stream(sensor, profile, steady_time, count_time):
    """Stream the profile; return (measured fps, [actual_exposure metadata of the counted frames],
    number of counted frames without the metadata)."""
    state = {'counting': False, 'frames': 0, 'no_md': 0, 'exposures': []}

    def on_frame(frame):
        if not state['counting']:
            return
        state['frames'] += 1
        if frame.supports_frame_metadata(rs.frame_metadata_value.actual_exposure):
            state['exposures'].append(frame.get_frame_metadata(rs.frame_metadata_value.actual_exposure))
        else:
            state['no_md'] += 1

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
    return state['frames'] / elapsed, state['exposures'], state['no_md']


def _calibrate_exposure_unit(dev, sensor, profile):
    """Return usec per actual_exposure metadata unit, learned with a manual exposure of known value."""
    sn = dev.get_info(rs.camera_info.serial_number)
    if sn in _us_per_md_unit_cache:
        return _us_per_md_unit_cache[sn]

    sensor.set_option(rs.option.enable_auto_exposure, 0)
    exp_range = sensor.get_option_range(rs.option.exposure)
    set_value = min(max(CALIBRATION_EXPOSURE, exp_range.min), exp_range.max)
    sensor.set_option(rs.option.exposure, set_value)
    _, exposures, no_md = _stream(sensor, profile, 1.0, 2.0)
    if not exposures:
        pytest.skip("Color frames carry no actual_exposure metadata, cannot tell if the scene is bright")
    md_value = statistics.median(exposures)
    ratio = md_value / set_value
    log.debug(f"Calibration: exposure option {set_value}, metadata median {md_value}, ratio {ratio:.2f}")
    if 0.8 <= ratio <= 1.2:
        us_per_unit = 100.0   # same unit as the option (100 usec)
    elif 80 <= ratio <= 120:
        us_per_unit = 1.0     # metadata reports usec
    else:
        pytest.skip(f"actual_exposure metadata ({md_value}) does not follow the exposure option ({set_value}), "
                    f"cannot interpret its unit")
    _us_per_md_unit_cache[sn] = us_per_unit
    return us_per_unit


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
    try:
        us_per_unit = _calibrate_exposure_unit(dev, cs, profile)

        cs.set_option(rs.option.enable_auto_exposure, 1)
        cs.set_option(rs.option.auto_exposure_priority, 1)
        measured_fps, exposures, no_md = _stream(cs, profile, TIME_FOR_STEADY_STATE, TIME_TO_COUNT_FRAMES)
    finally:
        cs.set_option(rs.option.auto_exposure_priority, orig_priority)
        cs.set_option(rs.option.enable_auto_exposure, orig_ae)

    if not exposures:
        pytest.skip("Color frames carry no actual_exposure metadata, cannot tell if the scene is bright")
    exposures_us = sorted(e * us_per_unit for e in exposures)
    exposure_us = exposures_us[min(int(len(exposures_us) * EXPOSURE_PERCENTILE), len(exposures_us) - 1)]
    frame_period_us = 1e6 / fps
    log.info(f"Requested {fps} fps, measured {measured_fps:.1f} fps, "
             f"exposure p{int(EXPOSURE_PERCENTILE * 100)} {exposure_us / 1000:.2f} msec "
             f"(frame period {frame_period_us / 1000:.2f} msec), {no_md} frames without metadata")

    if exposure_us >= frame_period_us:
        pytest.skip(f"Scene too dark: exposure {exposure_us / 1000:.2f} msec is not below the "
                    f"{frame_period_us / 1000:.2f} msec frame period, an fps drop would be legitimate")

    low, high = fps * FPS_MIN_RATIO, fps * FPS_MAX_RATIO
    assert low <= measured_fps <= high, (
        f"Color {fps} fps with AE priority on, exposure {exposure_us / 1000:.2f} msec below frame period: "
        f"got {measured_fps:.1f} fps, expected {low:.1f}-{high:.1f}")
