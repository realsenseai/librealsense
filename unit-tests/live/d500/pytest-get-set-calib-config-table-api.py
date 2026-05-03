# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

# test:donotrun  ## TODO: change this when relevant device is connected to libCI

import pyrealsense2 as rs
from rspy import test, log
from rspy import tests_wrapper as tw
import json

#############################################################################################
# Tests
#############################################################################################

test.start("Verify auto calibrated device extension")
ctx = rs.context()
dev = ctx.query_devices()[0]
ac_dev = dev.as_auto_calibrated_device()

test.finish()

#############################################################################################

tw.start_wrapper(dev)


test.start("Writing calibration config, then reading and comparing calibration config JSONs")

# save current calibration config json in order to restore it at the end
original_calib_config = ac_dev.get_calibration_config()

# calibration config JSON String to be written
new_calib_config = '''
{
    "calibration_config":
    {
        "roi_num_of_segments": 0,
        "roi_0":
        {
            "vertex_0": [ 0, 0 ],
            "vertex_1": [ 0, 0 ],
            "vertex_2": [ 0, 0 ],
            "vertex_3": [ 0, 0 ]
        },
        "roi_1": 
        {
            "vertex_0": [ 0, 0 ],
            "vertex_1": [ 0, 0 ],
            "vertex_2": [ 0, 0 ],
            "vertex_3": [ 0, 0 ]
        },
        "roi_2": 
        {
            "vertex_0": [ 0, 0 ],
            "vertex_1": [ 0, 0 ],
            "vertex_2": [ 0, 0 ],
            "vertex_3": [ 0, 0 ]
        },
        "roi_3": 
        {
            "vertex_0": [ 0, 0 ],
            "vertex_1": [ 0, 0 ],
            "vertex_2": [ 0, 0 ],
            "vertex_3": [ 0, 0 ]
        },
        "camera_position":
        {
            "rotation":
            [
                [ 0.0,  0.0,  1.0],
                [-1.0,  0.0,  0.0],
                [ 0.0, -1.0,  0.0]
            ],
            "translation": [0.0, 0.0, 1.0]
        },
        "crypto_signature": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
    }
}
'''

# write the above calib config table to the device
ac_dev.set_calibration_config(new_calib_config)

# read the current calib config table from the device
read_result = ac_dev.get_calibration_config()

# verify the JSON objects are equal (comparing JSON object because
# the JSON string can have different order of inner fields
test.check_equal_jsons(json.loads(read_result), json.loads(new_calib_config))

test.finish()

# #############################################################################################

test.start("Trying to write bad calib config with a missing field")

original_json = json.loads(new_calib_config)

# List of keys in the top-level JSON object
keys = list(original_json["calibration_config"].keys())

# Generate JSON dictionaries, each missing a different key
json_variants = []
for key in keys:
    variant = original_json.copy()
    del variant["calibration_config"][key]
    json_variants.append(variant)

for i, variant in enumerate(json_variants):
    try:
        log.d("Testing set calibration config with a missing field:", keys[i])
        # try to write the above calib config table to the device
        ac_dev.set_calibration_config(json.dumps(variant))
        log.d("Test failed: Exception was expected while setting calibration config with a missing field:" + keys[i])
        test.fail()
    except Exception as e:
        test.check_equal(str(e), "Invalid calibration_config format: calibration_config must include 'roi_num_of_segments', 'roi_0', 'roi_1', 'roi_2', 'roi_3', 'camera_position', and 'crypto_signature'")

test.finish()

#############################################################################################

test.start("Trying to write bad calib config with a missing field from camera position")

original_json = json.loads(new_calib_config)

keys = list(original_json["calibration_config"]["camera_position"].keys())

# Generate JSON dictionaries, each missing a different key from the camera_position section
json_variants = []
for key in keys:
    variant = original_json.copy()
    del variant["calibration_config"]["camera_position"][key]
    json_variants.append(variant)

for i, variant in enumerate(json_variants):
    try:
        log.d("Testing set calibration config with a missing field:", keys[i])
        # try to write the above calib config table to the device
        ac_dev.set_calibration_config(json.dumps(variant))
        log.d("Test failed: Exception was expected while setting calibration config with a missing field:" + keys[i])
        test.fail()
    except Exception as e:
        test.check_equal(str(e), "Invalid camera_position format: camera_position must include rotation and translation fields")

test.finish()

############################################################################################

test.start("Trying to write bad calib config with a missing roi values, or wrong roi types")

log.d("Testing set calibration config with a missing element in roi_0")
original_json = json.loads(new_calib_config)
variant = original_json.copy()
del variant["calibration_config"]["roi_0"]["vertex_0"]
try:
    # try to write the above calib config table to the device
    ac_dev.set_calibration_config(json.dumps(variant))
    log.d("Test failed: Exception was expected while setting calibration config with a missing field in roi_0")
    test.fail()
except Exception as e:
    test.check_equal(str(e), "Invalid ROI format: missing field: vertex_0")


log.d("Testing set calibration config with a missing element in roi_0[vertex_0]")
original_json = json.loads(new_calib_config)
variant = original_json.copy()
del variant["calibration_config"]["roi_0"]["vertex_0"][0]
try:
    # try to write the above calib config table to the device
    ac_dev.set_calibration_config(json.dumps(variant))
    log.d("Test failed: Exception was expected while setting calibration config with a missing field in roi")
    test.fail()
except Exception as e:
    test.check_equal(str(e), "Invalid Vertex format: each vertex should be an array of size=2")


log.d("Testing set calibration config with an invalid type in roi")
original_json = json.loads(new_calib_config)
variant = original_json.copy()
variant["calibration_config"]["roi_0"]["vertex_0"][0] = 1.2
try:
    # try to write the above calib config table to the device
    ac_dev.set_calibration_config(json.dumps(variant))
    log.d("Test failed: Exception was expected while setting calibration config with an invalid type in roi_0[vertex_0]")
    test.fail()
except Exception as e:
    test.check_equal(str(e), "Invalid Vertex type: Each vertex must include only unsigned integers")

test.finish()

############################################################################################

test.start("Trying to write bad calib config with a wrong crypto_signature values")

log.d("Testing set calibration config with a missing element in crypto_signature")
original_json = json.loads(new_calib_config)
variant = original_json.copy()
del variant["calibration_config"]["crypto_signature"][0]
try:
    # try to write the above calib config table to the device
    ac_dev.set_calibration_config(json.dumps(variant))
    log.d("Test failed: Exception was expected while setting calibration config with a missing element in crypto_signature")
    test.fail()
except Exception as e:
    test.check_equal(str(e), "Invalid crypto_signature format: crypto_signature must be an array of size=32")


log.d("Testing set calibration config with a wrong element type in crypto_signature")
original_json = json.loads(new_calib_config)
variant = original_json.copy()
variant["calibration_config"]["crypto_signature"][0] = 0.5
try:
    # try to write the above calib config table to the device
    ac_dev.set_calibration_config(json.dumps(variant))
    log.d("Test failed: Exception was expected while setting calibration config with a wrong element type in crypto_signature")
    test.fail()
except Exception as e:
    test.check_equal(str(e), "Invalid crypto_signature type: all elements in crypto_signature array must be unsigned integers")


test.finish()
############################################################################################

test.start("Restore original calibration config table")

# restore original calibration config table
ac_dev.set_calibration_config(original_calib_config)

# read the current calib config table from the device
read_result = ac_dev.get_calibration_config()

# verify the JSON objects are equal (comparing JSON object because
# the JSON string can have different order of inner fields
test.check_equal_jsons(json.loads(read_result), json.loads(original_calib_config))

test.finish()

#############################################################################################
tw.stop_wrapper(dev)

test.print_results_and_exit()
