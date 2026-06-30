# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# we initialize the SIC table, before all other safety tests will run

import pytest
import time
import json
import pyrealsense2 as rs
from rspy import devices
from rspy import tests_wrapper as tw
from rspy.json_compare import check_equal_jsons
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D585S"),
    pytest.mark.priority(9),
    pytest.mark.context("nightly"),
]


valid_sic_table_as_json_str = """
{
    "safety_interface_config":
    {
        "m12_safety_pins_configuration":
        {
            "power":
            {
                "direction": "In",
                "functionality": "p24VDC"
            },
            "ossd1_b":
            {
                "direction": "Out",
                "functionality": "pOSSD1_B"
            },
            "ossd1_a":
            {
                "direction": "Out",
                "functionality": "pOSSD1_A"
            },
            "preset3_a":
            {
                "direction": "In",
                "functionality": "pPresetSelect3_A"
            },
            "preset3_b":
            {
                "direction": "In",
                "functionality": "pPresetSelect3_B"
            },
            "preset4_a":
            {
                "direction": "In",
                "functionality": "pPresetSelect4_A"
            },
            "preset1_b":
            {
                "direction": "In",
                "functionality": "pPresetSelect1_B"
            },
            "preset1_a":
            {
                "direction": "In",
                "functionality": "pPresetSelect1_A"
            },
            "gpio_0":
            {
                "direction": "In",
                "functionality": "pPresetSelect5_A"
            },
            "gpio_1":
            {
                "direction": "In",
                "functionality": "pPresetSelect5_B"
            },
            "gpio_3":
            {
                "direction": "In",
                "functionality": "pPresetSelect6_B"
            },
            "gpio_2":
            {
                "direction": "In",
                "functionality": "pPresetSelect6_A"
            },
            "preset2_b":
            {
                "direction": "In",
                "functionality": "pPresetSelect2_B"
            },
            "gpio_4":
            {
                "direction": "Out",
                "functionality": "pDeviceReady"
            },
            "preset2_a":
            {
                "direction": "In",
                "functionality": "pPresetSelect2_A"
            },
            "preset4_b":
            {
                "direction": "In",
                "functionality": "pPresetSelect4_B"
            },
            "ground":
            {
                "direction": "In",
                "functionality": "pGND"
            }
        },
        "gpio_stabilization_interval" : 150,
        "camera_position":
        {
            "rotation":
            [
                [ 0.0,  0.0,  1.0],
                [-1.0,  0.0,  0.0],
                [ 0.0, -1.0,  0.0]
            ],
            "translation": [0.0, 0.0, 0.27]
        },
        "occupancy_grid_params":
        {
            "grid_cell_size" : 70,
            "cell_threshold_factor" : 1.0,
            "polynomial_bias" : 0.0,
            "surface_height" : 0.05
        },
        "smcu_arbitration_params":
        {
            "l_0_total_threshold": 100,
            "l_0_sustained_rate_threshold": 20,
            "l_1_total_threshold": 100,
            "l_1_sustained_rate_threshold": 20,
            "l_2_total_threshold": 10,
            "hkr_stl_timeout": 15,
            "mcu_stl_timeout": 10,
            "sustained_aicv_frame_drops": 90,
            "ossd_self_test_pulse_width": 23
        },
        "crypto_signature": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
    }
}
"""


# Module-level state shared across the 3 ordered tests in this file.
# Test 1 writes a known config, then Test 2 verifies it survives a HW reset, then Test 3 compares ram vs flash.
_state = {"json_we_write": None}


def change_config(sic_table_as_json_str):
    sic_as_json_object = json.loads(sic_table_as_json_str)
    sic_as_json_object["safety_interface_config"]["smcu_arbitration_params"]["l_0_total_threshold"] = 90
    sic_as_json_object["safety_interface_config"]["occupancy_grid_params"]["cell_threshold_factor"] = 1.5
    return json.dumps(sic_as_json_object)


def test_valid_get_set_scenario(test_device):
    dev, _ = test_device
    safety_sensor = dev.first_safety_sensor()
    tw.start_wrapper(dev)

    safety_sensor.set_safety_interface_config(valid_sic_table_as_json_str)

    # We read the table from the device, modify it and write it back
    # This way we are sure that the write process worked ()
    config_we_write = change_config(valid_sic_table_as_json_str)

    # write changed config to the device
    safety_sensor.set_safety_interface_config(config_we_write)

    # read the config in the device
    config_we_read = safety_sensor.get_safety_interface_config()

    # Add debugging info
    log.debug("config we write:")
    json_we_write = json.loads(config_we_write)
    print(json_we_write)

    log.debug("config we read:")
    json_we_read = json.loads(config_we_read)
    print(json_we_read)

    # checking the requested config has been written to the device
    assert check_equal_jsons(json_we_write, json_we_read)

    _state["json_we_write"] = json_we_write


def test_verify_same_table_after_camera_reboot(test_device):
    assert _state["json_we_write"] is not None, "previous test did not run"
    dev, _ = test_device

    log.debug("Sending HW-reset command")
    dev.hardware_reset()

    log.debug("sleep to give some time for the device to reconnect")
    time.sleep(devices.MAX_ENUMERATION_TIME)

    log.debug("Fetching new device")
    new_ctx = rs.context()
    new_devs = list(new_ctx.devices)
    assert new_devs, "device did not re-appear after hardware_reset"
    new_dev = new_devs[0]
    safety_sensor = new_dev.first_safety_sensor()

    log.debug("Setting operational mode to service")
    safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.service)
    assert safety_sensor.get_option(rs.option.safety_mode) == float(rs.safety_mode.service)

    config_after_reboot = safety_sensor.get_safety_interface_config(rs.calib_location.flash)

    # Add debugging info
    log.debug("config we write:")
    print(_state["json_we_write"])

    log.debug("config after reboot:")
    json_we_read_after_reboot = json.loads(config_after_reboot)
    print(json_we_read_after_reboot)

    # checking our last write stay the same after reboot
    assert check_equal_jsons(_state["json_we_write"], json_we_read_after_reboot)


def test_config_same_in_flash_and_ram(test_device):
    dev, _ = test_device
    safety_sensor = dev.first_safety_sensor()

    # getting config from ram
    config_from_ram = safety_sensor.get_safety_interface_config()

    # getting config from flash
    config_from_flash = safety_sensor.get_safety_interface_config(rs.calib_location.flash)

    # checking config is the same in flash and in ram
    assert check_equal_jsons(json.loads(config_from_ram), json.loads(config_from_flash))

    tw.stop_wrapper(dev)
