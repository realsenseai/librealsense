# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from pytest_check import check
import logging
log = logging.getLogger(__name__)

pytestmark = [pytest.mark.device_each("D405")]


def test_d405_explicit_config_ir_color_hd(module_device_setup):
    pipeline = rs.pipeline()
    config = rs.config()
    config.enable_stream(rs.stream.infrared, 1, 1288, 808, rs.format.y16, 15)
    config.enable_stream(rs.stream.infrared, 2, 1288, 808, rs.format.y16, 15)
    config.enable_stream(rs.stream.color, 1280, 720, rs.format.rgb8, 15)
    pipeline.start(config)
    for _ in range(10):
        frames = pipeline.wait_for_frames()
        check.is_true(frames.size() == 3)
        ir_1_stream_found = False
        ir_2_stream_found = False
        color_stream_found = False
        for f in frames:
            profile = f.get_profile()
            if profile.stream_type() == rs.stream.infrared:
                if profile.stream_index() == 1:
                    ir_1_stream_found = True
                elif profile.stream_index() == 2:
                    ir_2_stream_found = True
            elif profile.stream_type() == rs.stream.color:
                color_stream_found = True
        check.is_true(ir_1_stream_found and ir_2_stream_found and color_stream_found)
    pipeline.stop()


def test_d405_explicit_config_ir_color_vga(module_device_setup):
    pipeline = rs.pipeline()
    config = rs.config()
    config.enable_stream(rs.stream.infrared, 1, 1288, 808, rs.format.y16, 15)
    config.enable_stream(rs.stream.infrared, 2, 1288, 808, rs.format.y16, 15)
    config.enable_stream(rs.stream.color, 640, 480, rs.format.rgb8, 15)
    pipeline.start(config)
    for _ in range(10):
        frames = pipeline.wait_for_frames()
        check.is_true(frames.size() == 3)
        ir_1_stream_found = False
        ir_2_stream_found = False
        color_stream_found = False
        for f in frames:
            profile = f.get_profile()
            if profile.stream_type() == rs.stream.infrared:
                if profile.stream_index() == 1:
                    ir_1_stream_found = True
                elif profile.stream_index() == 2:
                    ir_2_stream_found = True
            elif profile.stream_type() == rs.stream.color:
                color_stream_found = True
        check.is_true(ir_1_stream_found and ir_2_stream_found and color_stream_found)
    pipeline.stop()


def test_d405_implicit_config_ir_color(module_device_setup):
    pipeline = rs.pipeline()
    config = rs.config()
    config.enable_stream(rs.stream.infrared, rs.format.y16, 15)
    config.enable_stream(rs.stream.color)
    pipeline.start(config)
    for _ in range(10):
        frames = pipeline.wait_for_frames()
        check.is_true(frames.size() == 2)
        ir_1_stream_found = False
        color_stream_found = False
        for f in frames:
            profile = f.get_profile()
            if profile.stream_type() == rs.stream.infrared:
                if profile.stream_index() == 1:
                    ir_1_stream_found = True
            elif profile.stream_type() == rs.stream.color:
                color_stream_found = True
        check.is_true(ir_1_stream_found and color_stream_found)
    pipeline.stop()
