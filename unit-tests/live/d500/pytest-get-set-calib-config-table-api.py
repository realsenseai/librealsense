# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from rspy import tests_wrapper as tw
from rspy.json_compare import check_equal_jsons
import json
import re
import logging
log = logging.getLogger(__name__)

# TODO: re-enable when relevant device is connected to libCI
pytestmark = [
    pytest.mark.skip(reason="TODO: enable when relevant device is connected to libCI"),
]


# calibration config JSON String to be written
NEW_CALIB_CONFIG = '''
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


@pytest.fixture
def ac_dev(test_device):
    dev, _ = test_device
    ac_dev = dev.as_auto_calibrated_device()
    tw.start_wrapper(dev)
    # save current calibration config json in order to restore it at the end
    original_calib_config = ac_dev.get_calibration_config()
    yield ac_dev
    # restore original calibration config table
    ac_dev.set_calibration_config(original_calib_config)
    read_result = ac_dev.get_calibration_config()
    assert check_equal_jsons(json.loads(read_result), json.loads(original_calib_config))
    tw.stop_wrapper(dev)


def test_auto_calibrated_extension(test_device):
    dev, _ = test_device
    ac_dev = dev.as_auto_calibrated_device()
    assert ac_dev is not None


def test_write_and_read_calib_config(ac_dev):
    # write the above calib config table to the device
    ac_dev.set_calibration_config(NEW_CALIB_CONFIG)

    # read the current calib config table from the device
    read_result = ac_dev.get_calibration_config()

    # verify the JSON objects are equal (comparing JSON object because
    # the JSON string can have different order of inner fields
    assert check_equal_jsons(json.loads(read_result), json.loads(NEW_CALIB_CONFIG))


def test_set_bad_calib_config_missing_top_level_field(ac_dev):
    original_json = json.loads(NEW_CALIB_CONFIG)

    # List of keys in the top-level JSON object
    keys = list(original_json["calibration_config"].keys())

    # Generate JSON dictionaries, each missing a different key
    json_variants = []
    for key in keys:
        variant = original_json.copy()
        del variant["calibration_config"][key]
        json_variants.append(variant)

    for i, variant in enumerate(json_variants):
        log.debug("Testing set calibration config with a missing field: %s", keys[i])
        with pytest.raises(Exception, match=re.escape("Invalid calibration_config format: calibration_config must include 'roi_num_of_segments', 'roi_0', 'roi_1', 'roi_2', 'roi_3', 'camera_position', and 'crypto_signature'")):
            ac_dev.set_calibration_config(json.dumps(variant))


def test_set_bad_calib_config_missing_camera_position_field(ac_dev):
    original_json = json.loads(NEW_CALIB_CONFIG)

    keys = list(original_json["calibration_config"]["camera_position"].keys())

    # Generate JSON dictionaries, each missing a different key from the camera_position section
    json_variants = []
    for key in keys:
        variant = original_json.copy()
        del variant["calibration_config"]["camera_position"][key]
        json_variants.append(variant)

    for i, variant in enumerate(json_variants):
        log.debug("Testing set calibration config with a missing field: %s", keys[i])
        with pytest.raises(Exception, match=re.escape("Invalid camera_position format: camera_position must include rotation and translation fields")):
            ac_dev.set_calibration_config(json.dumps(variant))


def test_set_bad_calib_config_bad_roi_values(ac_dev):
    log.debug("Testing set calibration config with a missing element in roi_0")
    original_json = json.loads(NEW_CALIB_CONFIG)
    variant = original_json.copy()
    del variant["calibration_config"]["roi_0"]["vertex_0"]
    with pytest.raises(Exception, match=re.escape("Invalid ROI format: missing field: vertex_0")):
        ac_dev.set_calibration_config(json.dumps(variant))

    log.debug("Testing set calibration config with a missing element in roi_0[vertex_0]")
    original_json = json.loads(NEW_CALIB_CONFIG)
    variant = original_json.copy()
    del variant["calibration_config"]["roi_0"]["vertex_0"][0]
    with pytest.raises(Exception, match=re.escape("Invalid Vertex format: each vertex should be an array of size=2")):
        ac_dev.set_calibration_config(json.dumps(variant))

    log.debug("Testing set calibration config with an invalid type in roi")
    original_json = json.loads(NEW_CALIB_CONFIG)
    variant = original_json.copy()
    variant["calibration_config"]["roi_0"]["vertex_0"][0] = 1.2
    with pytest.raises(Exception, match=re.escape("Invalid Vertex type: Each vertex must include only unsigned integers")):
        ac_dev.set_calibration_config(json.dumps(variant))


def test_set_bad_calib_config_bad_crypto_signature(ac_dev):
    log.debug("Testing set calibration config with a missing element in crypto_signature")
    original_json = json.loads(NEW_CALIB_CONFIG)
    variant = original_json.copy()
    del variant["calibration_config"]["crypto_signature"][0]
    with pytest.raises(Exception, match=re.escape("Invalid crypto_signature format: crypto_signature must be an array of size=32")):
        ac_dev.set_calibration_config(json.dumps(variant))

    log.debug("Testing set calibration config with a wrong element type in crypto_signature")
    original_json = json.loads(NEW_CALIB_CONFIG)
    variant = original_json.copy()
    variant["calibration_config"]["crypto_signature"][0] = 0.5
    with pytest.raises(Exception, match=re.escape("Invalid crypto_signature type: all elements in crypto_signature array must be unsigned integers")):
        ac_dev.set_calibration_config(json.dumps(variant))
