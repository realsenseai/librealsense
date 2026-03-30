# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Test FPS accuracy for single depth and color streams at various frame rates.
Verify that actual fps is within 5% of requested.
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
    pytest.mark.device_exclude("D401"),
    pytest.mark.context("nightly"),
]

TESTED_FPS =          [5,  6,  15, 30, 60, 90]
TIME_TO_TEST_FPS =    [8,  8,  5,  4,  3,  2]

fps_helper.TIME_FOR_STEADY_STATE = 1.2  # t2ff KPI is 1 second + some extra


def test_depth_fps(test_device):
    dev, ctx = test_device
    product_line = dev.get_info(rs.camera_info.product_line)
    camera_name = dev.get_info(rs.camera_info.name)
    os_name = platform.system()

    log.info(f"Testing depth fps {product_line} device - {os_name} OS")

    ds = dev.first_depth_sensor()
    if product_line == "D400":
        if ds.supports(rs.option.enable_auto_exposure):
            ds.set_option(rs.option.enable_auto_exposure, 1)

    failures = []
    for i in range(len(TESTED_FPS)):
        requested_fps = TESTED_FPS[i]
        try:
            dp = next(p for p in ds.profiles
                      if p.fps() == requested_fps
                      and p.stream_type() == rs.stream.depth
                      and p.format() == rs.format.z16
                      # On D585S the operational depth resolution is 1280x720
                      # 1280x960 is also available but only allowed in service mode
                      # 60 fps is only available in bypass mode
                      and ((p.as_video_stream_profile().height() == 720 and p.fps() != 60) if "D585S" in camera_name else True))
        except StopIteration:
            log.info(f"Requested fps: {requested_fps:.1f} [Hz], not supported")
            continue

        fps_helper.TIME_TO_COUNT_FRAMES = TIME_TO_TEST_FPS[i]
        fps_dict = fps_helper.measure_fps({ds: [dp]})
        fps = fps_dict.get(dp.stream_name(), 0)
        log.info(f"Requested fps: {requested_fps:.1f} [Hz], actual fps: {fps:.1f} [Hz]")
        delta_Hz = requested_fps * 0.05
        if not (fps >= requested_fps - delta_Hz and fps <= requested_fps + delta_Hz):
            failures.append(f"Depth {requested_fps}Hz: got {fps:.1f}Hz")

    assert not failures, "Depth FPS out of tolerance:\n" + "\n".join(failures)


def test_color_fps(test_device):
    dev, ctx = test_device
    product_line = dev.get_info(rs.camera_info.product_line)
    product_name = dev.get_info(rs.camera_info.name)
    os_name = platform.system()

    if any(model in product_name for model in ['D421', 'D405']):
        pytest.skip(f"Device {product_name} has no color sensor")

    log.info(f"Testing color fps {product_line} device - {os_name} OS")

    cs = dev.first_color_sensor()
    if product_line == "D400":
        if cs.supports(rs.option.enable_auto_exposure):
            cs.set_option(rs.option.enable_auto_exposure, 1)
        if cs.supports(rs.option.auto_exposure_priority):
            cs.set_option(rs.option.auto_exposure_priority, 0)

    failures = []
    for i in range(len(TESTED_FPS)):
        requested_fps = TESTED_FPS[i]
        try:
            cp = next(p for p in cs.profiles
                      if p.fps() == requested_fps
                      and p.stream_type() == rs.stream.color
                      and p.format() == rs.format.rgb8)
        except StopIteration:
            log.info(f"Requested fps: {requested_fps:.1f} [Hz], not supported")
            continue

        fps_helper.TIME_TO_COUNT_FRAMES = TIME_TO_TEST_FPS[i]
        fps_dict = fps_helper.measure_fps({cs: [cp]})
        fps = fps_dict.get(cp.stream_name(), 0)
        log.info(f"Requested fps: {requested_fps:.1f} [Hz], actual fps: {fps:.1f} [Hz]")
        # Validation KPI is 5% for all non 5 FPS rate
        delta_Hz = requested_fps * (0.10 if requested_fps == 5 else 0.05)
        if not (fps >= requested_fps - delta_Hz and fps <= requested_fps + delta_Hz):
            failures.append(f"Color {requested_fps}Hz: got {fps:.1f}Hz")

    assert not failures, "Color FPS out of tolerance:\n" + "\n".join(failures)
