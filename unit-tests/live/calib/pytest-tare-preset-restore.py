# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
import logging
from calibrations_common import on_calib_cb, TARE_TIMEOUT_MS

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.context("nightly"),
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D555"),  # The only D500 supporting tare
    pytest.mark.device_exclude("D401"),
    pytest.mark.device_type_exclude("GMSL"),  # Tare over GMSL requires host assistance, enough to test USB and DDS, code path identical
    pytest.mark.context("calibration"),
]

CUSTOM_MEDIAN_THRESHOLD = 600  # Differs from both Default (500) and High Accuracy (796) presets
GROUND_TRUTH_MM = 1000
TARE_JSON = '{"host assistance": 0, "speed": 3, "scan parameter": 0, "step count": 20, "apply preset": 1, "accuracy": 2, "depth": 0}'


def preset_name(sensor):
    # USB devices report the preset as a number, DDS devices as its name
    value = sensor.get_option_value(rs.option.visual_preset).value
    return value if isinstance(value, str) else sensor.get_option_value_description(rs.option.visual_preset, value)


@pytest.fixture
def depth_sensor(test_device):
    dev, _ = test_device
    # Skip before yield, the teardown cannot set the preset without advanced mode
    if not rs.rs400_advanced_mode(dev).is_enabled():
        pytest.skip("Custom visual preset requires advanced mode")
    sensor = dev.first_depth_sensor()
    yield sensor
    sensor.set_option(rs.option.visual_preset, int(rs.rs400_visual_preset.default))


def test_tare_restores_custom_preset(test_device, depth_sensor):
    dev, ctx = test_device
    am = rs.rs400_advanced_mode(dev)

    depth_sensor.set_option(rs.option.visual_preset, int(rs.rs400_visual_preset.default))
    depth_control = am.get_depth_control()
    depth_control.deepSeaMedianThreshold = CUSTOM_MEDIAN_THRESHOLD
    am.set_depth_control(depth_control)  # Changing an advanced-mode control switches the preset to Custom
    assert preset_name(depth_sensor) == "Custom"

    pipeline = rs.pipeline(ctx)
    config = rs.config()
    config.enable_device(dev.get_info(rs.camera_info.serial_number))
    config.enable_stream(rs.stream.depth, 256, 144, rs.format.z16, 90)
    pipeline.start(config)
    try:
        pipeline.wait_for_frames()
        try:
            rs.auto_calibrated_device(dev).run_tare_calibration(GROUND_TRUTH_MM, TARE_JSON, on_calib_cb, TARE_TIMEOUT_MS)
        except RuntimeError as e:
            # Calibration outcome depends on the scene; the preset must be restored either way
            log.info(f"Tare calibration did not succeed: {e}")
    finally:
        pipeline.stop()

    assert preset_name(depth_sensor) == "Custom"
    assert am.get_depth_control().deepSeaMedianThreshold == CUSTOM_MEDIAN_THRESHOLD
