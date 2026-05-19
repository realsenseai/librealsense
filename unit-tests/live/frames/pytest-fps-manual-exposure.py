# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Test FPS accuracy with manual (forced) exposure for depth and color streams.
Mirrors fps-single-stream but disables auto-exposure and sets exposure to half frame time.
"""

import pytest
import pyrealsense2 as rs
import fps_helper
import platform
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
    pytest.mark.device_exclude("D555"),
    pytest.mark.skip(reason="Test disabled (donotrun)"),
]

TESTED_FPS =          [5,   6,   15,  30,  60,  90]
TIME_TO_TEST_FPS =    [25,  20,  13,  10,  5,   4]


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

        # Decide units based on sensor type: depth -> microseconds; RGB -> 100 microseconds units
        is_color = False
        try:
            for p in sensor.profiles:
                if p.stream_type() == rs.stream.color:
                    is_color = True
                    break
        except Exception:
            is_color = False

        if is_color:
            desired_units = desired_s * 1e6 / 100.0
        else:
            desired_units = desired_s * 1e6

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


def test_depth_fps_manual_exposure(test_device):
    dev, ctx = test_device
    product_line = dev.get_info(rs.camera_info.product_line)
    camera_name = dev.get_info(rs.camera_info.name)
    os_name = platform.system()

    log.info(f"Testing depth fps (manual exposure) {product_line} device - {os_name} OS")
    log.info(f"Device: {camera_name}, firmware: {dev.get_info(rs.camera_info.firmware_version)}")

    ds = dev.first_depth_sensor()
    if product_line == "D400":
        if ds.supports(rs.option.enable_auto_exposure):
            ds.set_option(rs.option.enable_auto_exposure, 0)

    failures = []
    for i in range(len(TESTED_FPS)):
        requested_fps = TESTED_FPS[i]
        try:
            dp = next(p for p in ds.profiles
                      if p.fps() == requested_fps
                      and p.stream_type() == rs.stream.depth
                      and p.format() == rs.format.z16
                      and ((p.as_video_stream_profile().height() == 720 and p.fps() != 60) if "D585S" in camera_name else True))
        except StopIteration:
            log.info(f"Requested fps: {requested_fps:.1f} [Hz], not supported")
            continue

        exposure_val = set_exposure_half_frame_time(ds, requested_fps)
        fps_helper.TIME_TO_COUNT_FRAMES = TIME_TO_TEST_FPS[i]
        fps_dict = fps_helper.measure_fps({ds: [dp]})
        fps = fps_dict.get(dp.stream_name(), 0)
        log.info(f"Exposure: {(exposure_val or 0)/1000:.1f} [msec], requested fps: {requested_fps:.1f} [Hz], actual fps: {fps:.1f} [Hz]")
        delta_Hz = requested_fps * 0.05
        if not (fps >= requested_fps - delta_Hz and fps <= requested_fps + delta_Hz):
            failures.append(f"Depth {requested_fps}Hz: got {fps:.1f}Hz")

    assert not failures, "Depth FPS out of tolerance:\n" + "\n".join(failures)


def test_color_fps_manual_exposure(test_device):
    dev, ctx = test_device
    product_line = dev.get_info(rs.camera_info.product_line)
    product_name = dev.get_info(rs.camera_info.name)
    os_name = platform.system()

    if any(model in product_name for model in ['D421', 'D405']):
        pytest.skip(f"Device {product_name} has no color sensor")

    log.info(f"Testing color fps (manual exposure) {product_line} device - {os_name} OS")

    cs = dev.first_color_sensor()
    if product_line == "D400":
        if cs.supports(rs.option.enable_auto_exposure):
            cs.set_option(rs.option.enable_auto_exposure, 0)
        if cs.supports(rs.option.auto_exposure_priority):
            cs.set_option(rs.option.auto_exposure_priority, 0)

    failures = []
    for i in range(len(TESTED_FPS)):
        requested_fps = TESTED_FPS[i]
        try:
            candidates = [p for p in cs.profiles
                          if p.fps() == requested_fps
                          and p.stream_type() == rs.stream.color
                          and p.format() == rs.format.rgb8]
            if not candidates:
                raise StopIteration
            candidates.sort(key=lambda pr: pr.as_video_stream_profile().width() * pr.as_video_stream_profile().height())
            cp = candidates[len(candidates)//2]
        except StopIteration:
            log.info(f"Requested fps: {requested_fps:.1f} [Hz], not supported")
            continue

        exposure_val = set_exposure_half_frame_time(cs, requested_fps)
        fps_helper.TIME_TO_COUNT_FRAMES = TIME_TO_TEST_FPS[i]
        fps_dict = fps_helper.measure_fps({cs: [cp]})
        fps = fps_dict.get(cp.stream_name(), 0)
        log.info(f"Exposure: {(exposure_val or 0)/1000:.1f} [msec], requested fps: {requested_fps:.1f} [Hz], actual fps: {fps:.1f} [Hz]")
        delta_Hz = requested_fps * (0.10 if requested_fps == 5 else 0.05)
        if not (fps >= requested_fps - delta_Hz and fps <= requested_fps + delta_Hz):
            failures.append(f"Color {requested_fps}Hz: got {fps:.1f}Hz")

    assert not failures, "Color FPS out of tolerance:\n" + "\n".join(failures)
