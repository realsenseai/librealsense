# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Test FPS accuracy for single depth and color streams at various frame rates.
Verify that actual fps is within 5% of requested, with manual exposure (half the frame time)
on every build and auto exposure added on nightly.
"""

import pytest
import pyrealsense2 as rs
import pyrsutils as rsutils
import fps_helper
import platform
import logging
from rspy.pytest.device_helpers import require_min_fw_version
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
    pytest.mark.device_exclude("D555"),
]

TESTED_FPS =          [5,  6,  15, 30, 60, 90]
TIME_TO_TEST_FPS =    [13, 12, 10, 7,  5,  4]

fps_helper.TIME_FOR_STEADY_STATE = 1.2  # t2ff KPI is 1 second + some extra

exposure_modes = pytest.mark.parametrize("exposure", ["manual", pytest.param("auto", marks=pytest.mark.context("nightly"))])


def set_exposure_half_frame_time(sensor, requested_fps):
    """Set sensor exposure to half the frame time for the given requested_fps.
    Returns the exposure value that was set (or None if not supported/failed).
    """
    if not sensor.supports(rs.option.exposure):
        return None
    try:
        r = sensor.get_option_range(rs.option.exposure)
        frame_time_s = 1.0 / float(requested_fps)
        desired_s = frame_time_s / 2.0

        # RGB sensors take 100 microsecond units; depth sensors (also D405/D401 color) take microseconds
        desired_units = desired_s * 1e6 / (100.0 if sensor.is_color_sensor() else 1.0)

        if desired_units < r.min:
            desired_units = r.min
        if desired_units > r.max:
            desired_units = r.max

        try:
            sensor.set_option(rs.option.exposure, desired_units)
        except Exception:
            sensor.set_option(rs.option.exposure, int(desired_units))
        return desired_s * 1e6  # return in microseconds
    except Exception:
        return None


def check_fps_ladder(sensor, exposure, label, matches, tolerance_at_5fps=0.05):
    """Stream each TESTED_FPS rate the sensor offers; return (rates tested, failures)."""
    # Set explicitly: a manual run before this one leaves auto exposure off
    if sensor.supports(rs.option.enable_auto_exposure):
        sensor.set_option(rs.option.enable_auto_exposure, int(exposure == "auto"))
    if sensor.supports(rs.option.auto_exposure_priority):
        sensor.set_option(rs.option.auto_exposure_priority, 0)

    tested, failures = 0, []
    for requested_fps, count_time in zip(TESTED_FPS, TIME_TO_TEST_FPS):
        profile = next((p for p in sensor.profiles if p.fps() == requested_fps and matches(p)), None)
        if profile is None:
            log.info(f"Requested fps: {requested_fps:.1f} [Hz], not supported")
            continue

        tested += 1
        exposure_us = set_exposure_half_frame_time(sensor, requested_fps) if exposure == "manual" else None
        fps_helper.TIME_TO_COUNT_FRAMES = count_time
        fps = fps_helper.measure_fps({sensor: [profile]}).get(profile.stream_name(), 0)
        log.info(f"Exposure: {f'{exposure_us / 1000:.1f} [msec]' if exposure_us else exposure}, "
                 f"requested fps: {requested_fps:.1f} [Hz], actual fps: {fps:.1f} [Hz]")
        delta_Hz = requested_fps * (tolerance_at_5fps if requested_fps == 5 else 0.05)
        if not (requested_fps - delta_Hz <= fps <= requested_fps + delta_Hz):
            failures.append(f"{label} {requested_fps}Hz: got {fps:.1f}Hz")
    return tested, failures


@exposure_modes
def test_depth_fps(test_device, exposure):
    dev, ctx = test_device
    product_line = dev.get_info(rs.camera_info.product_line)
    camera_name = dev.get_info(rs.camera_info.name)

    log.info(f"Testing depth fps ({exposure} exposure) {product_line} device - {platform.system()} OS")
    log.info(f"Device: {camera_name}, firmware: {dev.get_info(rs.camera_info.firmware_version)}")

    # On D585S the operational depth resolution is 1280x720; 1280x960 is service mode only
    # and 60 fps is only available in bypass mode
    _, failures = check_fps_ladder(dev.first_depth_sensor(), exposure, "Depth",
                                   lambda p: p.stream_type() == rs.stream.depth
                                   and p.format() == rs.format.z16
                                   and ((p.as_video_stream_profile().height() == 720 and p.fps() != 60) if "D585S" in camera_name else True))

    assert not failures, "Depth FPS out of tolerance:\n" + "\n".join(failures)


@exposure_modes
def test_color_fps(test_device, exposure):
    dev, ctx = test_device
    product_line = dev.get_info(rs.camera_info.product_line)
    product_name = dev.get_info(rs.camera_info.name)

    if product_line == "D400" and exposure == "manual":
        require_min_fw_version(dev, rsutils.version(5, 17, 4, 6), "color manual exposure FPS")

    log.info(f"Testing color fps ({exposure} exposure) {product_line} device - {platform.system()} OS")

    # D405 and D401 expose color through the depth sensor (no separate color sensor)
    try:
        cs = dev.first_color_sensor()
    except RuntimeError:
        if 'D405' in product_name or 'D401' in product_name:
            cs = dev.first_depth_sensor()
        elif 'D421' in product_name:
            pytest.skip(f"Device {product_name} has no color sensor")
        else:
            raise

    # Resolution does not affect FPS accuracy; the KPI allows 10% at 5 FPS
    tested, failures = check_fps_ladder(cs, exposure, "Color",
                                        lambda p: p.stream_type() == rs.stream.color and p.format() == rs.format.rgb8,
                                        tolerance_at_5fps=0.10)

    assert tested > 0, f"No color profiles found on {product_name} — test ran no FPS checks"
    assert not failures, "Color FPS out of tolerance:\n" + "\n".join(failures)


@pytest.mark.context("nightly")
def test_object_detection_fps(test_device):
    dev, ctx = test_device
    camera_name = dev.get_info(rs.camera_info.name)

    if "D585S" in camera_name or not any(model in camera_name for model in ["D555", "D585"]):
        pytest.skip(f"Object detection is not expected on {camera_name}")

    ps = dev.first_perception_sensor()

    op = next((p for p in ps.profiles if p.stream_type() == rs.stream.object_detection), None)
    assert op is not None, "Perception sensor has no object-detection profile"

    requested_fps = op.fps()
    fps_helper.TIME_TO_COUNT_FRAMES = 5
    fps_dict = fps_helper.measure_fps({ps: [op]})
    fps = fps_dict.get(op.stream_name(), 0)
    log.info(f"Requested fps: {requested_fps:.1f} [Hz], actual fps: {fps:.1f} [Hz]")

    delta_Hz = requested_fps * 0.05
    assert fps >= requested_fps - delta_Hz and fps <= requested_fps + delta_Hz, \
        f"Object Detection {requested_fps}Hz: got {fps:.1f}Hz"
