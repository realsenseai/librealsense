# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Feature not frequently changing, moving to weekly checks

import pytest
import random
import json
from rspy import tests_wrapper as tw
from rspy.json_compare import check_equal_jsons
import logging
log = logging.getLogger(__name__)

# Add retries as occasionally HKR FW fails during this initialization
pytestmark = [
    pytest.mark.device_each("D585S"),
    pytest.mark.priority(10),
    pytest.mark.context("weekly"),
    pytest.mark.flaky(retries=3),
]


# Safety Preset JSON String representation to be written on all indexes
valid_sp_json_str = """
{
    "safety_preset":
    {
        "platform_config":
        {
            "transformation_link":
            {
                "rotation":
                [
                    [ 0.0,  0.0,  1.0],
                    [-1.0,  0.0,  0.0],
                    [ 0.0, -1.0,  0.0]
                ],
                "translation": [0.0, 0.0, 0.27]
            },
            "robot_height": 1.0,
            "reserved": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
        },
        "safety_zones":
        {
            "danger_zone":
            {
                "zone_polygon":
                {
                    "p0": {"x": 0.5, "y":  0.1},
                    "p1": {"x": 1.2, "y":  0.1},
                    "p2": {"x": 1.2, "y": -0.1},
                    "p3": {"x": 0.5, "y": -0.1}
                },
                "safety_trigger_confidence": 3,
                "reserved": [0, 0, 0, 0, 0, 0, 0]
            },
            "warning_zone":
            {
                "zone_polygon":
                {
                    "p0": {"x": 0.3, "y":  0.1},
                    "p1": {"x": 0.5, "y":  0.1},
                    "p2": {"x": 0.5, "y": -0.1},
                    "p3": {"x": 0.3, "y": -0.1}
                },
                "safety_trigger_confidence": 3,
                "reserved": [0, 0, 0, 0, 0, 0, 0]
            }
        },
        "masking_zones":
        {
            "0":
            {
                "attributes": 0,
                "minimal_range": 0.5,
                "region_of_interests":
                {
                    "vertex_0": [0, 0],
                    "vertex_1": [0, 320],
                    "vertex_2": [200, 320],
                    "vertex_3": [200, 0]
                }
            },
            "1":
            {
                "attributes": 0,
                "minimal_range": 0.5,
                "region_of_interests":
                {
                    "vertex_0": [0, 0],
                    "vertex_1": [0, 320],
                    "vertex_2": [200, 320],
                    "vertex_3": [200, 0]
                }
            },
            "2":
            {
                "attributes": 0,
                "minimal_range": 0.5,
                "region_of_interests":
                {
                    "vertex_0": [0, 0],
                    "vertex_1": [0, 320],
                    "vertex_2": [200, 320],
                    "vertex_3": [200, 0]
                }
            },
            "3":
            {
                "attributes": 0,
                "minimal_range": 0.5,
                "region_of_interests":
                {
                    "vertex_0": [0, 0],
                    "vertex_1": [0, 320],
                    "vertex_2": [200, 320],
                    "vertex_3": [200, 0]
                }
            },
            "4":
            {
                "attributes": 0,
                "minimal_range": 0.5,
                "region_of_interests":
                {
                    "vertex_0": [0, 0],
                    "vertex_1": [0, 320],
                    "vertex_2": [200, 320],
                    "vertex_3": [200, 0]
                }
            },
            "5":
            {
                "attributes": 0,
                "minimal_range": 0.5,
                "region_of_interests":
                {
                    "vertex_0": [0, 0],
                    "vertex_1": [0, 320],
                    "vertex_2": [200, 320],
                    "vertex_3": [200, 0]
                }
            },
            "6":
            {
                "attributes": 0,
                "minimal_range": 0.5,
                "region_of_interests":
                {
                    "vertex_0": [0, 0],
                    "vertex_1": [0, 320],
                    "vertex_2": [200, 320],
                    "vertex_3": [200, 0]
                }
            },
            "7":
            {
                "attributes": 0,
                "minimal_range": 0.5,
                "region_of_interests":
                {
                    "vertex_0": [0, 0],
                    "vertex_1": [0, 320],
                    "vertex_2": [200, 320],
                    "vertex_3": [200, 0]
                }
            }
        },
        "reserved": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        "environment":
        {
            "safety_trigger_duration": 1.0,
            "zero_safety_monitoring": 0,
            "hara_history_continuation": 0,
            "reserved1": [0, 0],
            "angular_velocity": 0.0,
            "payload_weight": 0.0,
            "surface_inclination": 15.0,
            "diagnostic_zone_fill_rate_threshold": 90,
            "floor_fill_threshold": 0,
            "depth_fill_threshold": 20,
            "diagnostic_zone_height_median_threshold": 255,
            "vision_hara_persistency": 2,
            "crypto_signature": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            "reserved2": [0, 0, 0]
        }
    }
}
"""


@pytest.fixture
def safety_sensor(test_device):
    dev, _ = test_device
    safety_sensor = dev.first_safety_sensor()
    tw.start_wrapper(dev)
    yield safety_sensor
    tw.stop_wrapper(dev)


def test_init_all_safety_zones(safety_sensor):
    for x in range(64):
        log.debug("Init preset ID = %s", x)
        safety_sensor.set_safety_preset(x, valid_sp_json_str)


def test_write_random_index_and_compare(safety_sensor):
    # changing some small value to create new SP
    safety_preset_json_obj = json.loads(valid_sp_json_str)
    safety_preset_json_obj["safety_preset"]["environment"]["diagnostic_zone_fill_rate_threshold"] = 99
    new_safety_preset = json.dumps(safety_preset_json_obj)

    # generate random index
    index = random.randint(1, 63)
    log.info("writing to index = %s", index)

    # Save previous safety preset to restore it at the end
    previous_result = safety_sensor.get_safety_preset(index)

    # write the above sp table to the device
    safety_sensor.set_safety_preset(index, new_safety_preset)

    # read the table from the device
    read_result = safety_sensor.get_safety_preset(index)

    # verify the tables are equal
    assert check_equal_jsons(json.loads(new_safety_preset), json.loads(read_result))

    # restore original table
    safety_sensor.set_safety_preset(index, previous_result)
