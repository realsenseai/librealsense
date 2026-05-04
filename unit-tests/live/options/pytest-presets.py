# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
import time
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_exclude("D401"),
    pytest.mark.device_each("D500*"),
    pytest.mark.context("nightly"),
    pytest.mark.flaky(retries=2),  # See FW stability issue RSDSO-18908
]


@pytest.mark.dependency(scope='module')
def test_visual_preset_support(test_device_wrapped):
    """Prerequisite: no use continuing if there is no preset support."""
    dev, ctx = test_device_wrapped
    depth_sensor = dev.first_depth_sensor()
    assert depth_sensor.supports(rs.option.visual_preset)


@pytest.mark.dependency(scope='module', depends=["test_visual_preset_support"])
def test_set_presets(test_device_wrapped):
    dev, ctx = test_device_wrapped
    depth_sensor = dev.first_depth_sensor()
    depth_sensor.set_option(rs.option.visual_preset, int(rs.rs400_visual_preset.high_accuracy))
    assert depth_sensor.get_option(rs.option.visual_preset) == rs.rs400_visual_preset.high_accuracy
    depth_sensor.set_option(rs.option.visual_preset, int(rs.rs400_visual_preset.default))
    assert depth_sensor.get_option(rs.option.visual_preset) == rs.rs400_visual_preset.default


@pytest.mark.dependency(scope='module', depends=["test_visual_preset_support"])
def test_save_load_preset(test_device_wrapped):
    dev, ctx = test_device_wrapped
    depth_sensor = dev.first_depth_sensor()
    am_dev = rs.rs400_advanced_mode(dev)
    saved_values = am_dev.serialize_json()
    depth_control_group = am_dev.get_depth_control()
    depth_control_group.textureCountThreshold = 250
    am_dev.set_depth_control(depth_control_group)
    time.sleep(0.1)  # Give camera time to handle the command
    assert depth_sensor.get_option(rs.option.visual_preset) == rs.rs400_visual_preset.custom

    am_dev.load_json(saved_values)
    assert am_dev.get_depth_control().textureCountThreshold != 250


@pytest.mark.dependency(scope='module', depends=["test_visual_preset_support"])
def test_setting_color_options(test_device_wrapped):
    """Setting visual preset should update color sensor options on D400 but not D500.

    Uses Hue (not Gain/Exposure) to avoid auto-exposure interference.
    Skipped on cameras without a color sensor or without Hue support (e.g. D457).
    """
    dev, ctx = test_device_wrapped
    product_line = dev.get_info(rs.camera_info.product_line)
    product_name = dev.get_info(rs.camera_info.name)
    depth_sensor = dev.first_depth_sensor()

    try:
        color_sensor = dev.first_color_sensor()
    except RuntimeError:
        if 'D421' in product_name or 'D405' in product_name:
            pytest.skip("No color sensor")
        raise

    if not color_sensor.supports(rs.option.hue):
        pytest.skip("Color sensor does not support hue option")

    color_sensor.set_option(rs.option.hue, 123)
    assert color_sensor.get_option(rs.option.hue) == 123

    depth_sensor.set_option(rs.option.visual_preset, int(rs.rs400_visual_preset.default))
    if product_line == "D400":
        # D400 devices set color options as part of preset setting
        assert color_sensor.get_option(rs.option.hue) != 123
    elif product_line == "D500":
        # D500 devices do not set color options as part of preset setting
        assert color_sensor.get_option(rs.option.hue) == 123
    else:
        pytest.fail(f"Unsupported product line: {product_line}")
