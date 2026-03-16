# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#test:device D585S
#test:priority 10
# Feature not frequently changing, moving to weekly checks
# test:donotrun:!weekly

# Add retries as occasionally HKR FW fails during this initialization
#test:retries 3


import pyrealsense2 as rs
import random
from rspy import test, log
import json
from rspy import tests_wrapper as tw

#############################################################################################
# Helper Functions
#############################################################################################

#############################################################################################
# Tests
#############################################################################################

test.start("Verify Safety Sensor Extension")
ctx = rs.context()
dev = ctx.query_devices()[0]
safety_sensor = dev.first_safety_sensor()
test.finish()

tw.start_wrapper(dev)
#############################################################################################

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
                    "p1": {"x": 0.8, "y":  0.1},
                    "p2": {"x": 0.8, "y": -0.1},
                    "p3": {"x": 0.5, "y": -0.1}
                },
                "safety_trigger_confidence": 3,
                "reserved": [0, 0, 0, 0, 0, 0, 0]
            },
            "warning_zone":
            {
                "zone_polygon":
                {
                    "p0": {"x": 0.8, "y":  0.1},
                    "p1": {"x": 1.2, "y":  0.1},
                    "p2": {"x": 1.2, "y": -0.1},
                    "p3": {"x": 0.8, "y": -0.1}
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
                "minimal_range": 0,
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
            "surface_height": 0.05,
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

#############################################################################################

test.start("Init all safety zones")
for x in range(64):
    log.d("Init preset ID =", x)
    safety_sensor.set_safety_preset(x, valid_sp_json_str)
test.finish()

#############################################################################################

test.start("Writing safety preset to random index, then reading and comparing safety presets JSONs")

# changing some small value to create new SP
safety_preset_json_obj = json.loads(valid_sp_json_str)
safety_preset_json_obj["safety_preset"]["environment"]["diagnostic_zone_fill_rate_threshold"] = 99
new_safety_preset = json.dumps(safety_preset_json_obj)

# generate random index
index = random.randint(1, 63)
log.out( "writing to index = ", index )

# Save previous safety preset to restore it at the end
previous_result = safety_sensor.get_safety_preset(index)

# write the above sp table to the device
safety_sensor.set_safety_preset(index, new_safety_preset)

# read the table from the device
read_result = safety_sensor.get_safety_preset(index)

# verify the tables are equal
test.check_equal_jsons(json.loads(new_safety_preset), json.loads(read_result))

# restore original table
safety_sensor.set_safety_preset(index, previous_result)
test.finish()

#############################################################################################
tw.stop_wrapper(dev)
test.print_results_and_exit()
