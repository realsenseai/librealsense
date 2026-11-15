# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import json
import pyrsutils as rsutils
from rspy.json_compare import check_equal_jsons
from rspy.pytest.device_helpers import require_min_fw_version
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D585"),
    pytest.mark.device_exclude("D585S"),  # D585S does not serve the depth mapping params
]


# First FW that serves the depth mapping params (with a synchronous set); older FW rejects the control
MIN_FW = rsutils.version("7.59.46744")


depth_mapping_params = {
    "camera_position": {
        "rotation": [[0.0, 0.0, 1.0],
                     [-1.0, 0.0, 0.0],
                     [0.0, -1.0, 0.0]],
        "translation": [0.0, 0.0, 0.3]
    },
    "occupancy_grid_params": {
        "grid_cell_size": 60,
        "cell_threshold_factor": 0.03,
        "polynomial_bias": 3.0,
        "surface_height": 0.12
    }
}


def as_str(params):
    # Compact: the whole JSON document has to fit in 1016 bytes
    return json.dumps(params, separators=(",", ":"))


def sent_subset(params, sent):
    # Set is a partial update, so compare only the keys that were sent
    return {section: {key: params[section][key] for key in sent[section]} for section in sent}


@pytest.fixture
def depth_mapping_sensor(test_device):
    """Yield depth_mapping_sensor with the original params saved/restored around the test."""
    dev, _ = test_device
    require_min_fw_version(dev, MIN_FW, "depth mapping params")
    depth_mapping_sensor = dev.first_depth_mapping_sensor()
    original_params = json.loads(depth_mapping_sensor.get_depth_mapping_params())
    # FW omits camera_position until the first write, and a camera can't be made unconfigured again
    if "camera_position" not in original_params:
        pytest.skip("camera never configured; a set could not be undone")
    yield depth_mapping_sensor
    depth_mapping_sensor.set_depth_mapping_params(as_str(original_params))
    log.debug("depth mapping params successfully restored")
    assert check_equal_jsons(json.loads(depth_mapping_sensor.get_depth_mapping_params()), original_params)


def test_get_depth_mapping_params(test_device):
    dev, _ = test_device
    require_min_fw_version(dev, MIN_FW, "depth mapping params")
    params = json.loads(dev.first_depth_mapping_sensor().get_depth_mapping_params())
    log.debug("depth mapping params successfully downloaded")
    assert isinstance(params, dict)


def test_set_depth_mapping_params(depth_mapping_sensor):
    depth_mapping_sensor.set_depth_mapping_params(as_str(depth_mapping_params))
    log.debug("depth mapping params successfully uploaded")
    params = json.loads(depth_mapping_sensor.get_depth_mapping_params())
    assert check_equal_jsons(sent_subset(params, depth_mapping_params), depth_mapping_params)


def test_set_unknown_key_is_rejected(depth_mapping_sensor):
    before = json.loads(depth_mapping_sensor.get_depth_mapping_params())
    with pytest.raises(RuntimeError, match="UNKNOWN_KEY"):
        depth_mapping_sensor.set_depth_mapping_params(as_str({"occupancy_grid_params": {"no_such_key": 1}}))
    # A rejected set leaves the params as they were
    assert check_equal_jsons(json.loads(depth_mapping_sensor.get_depth_mapping_params()), before)


def test_set_out_of_range_is_rejected(depth_mapping_sensor):
    with pytest.raises(RuntimeError, match="OUT_OF_RANGE"):
        depth_mapping_sensor.set_depth_mapping_params(as_str({"occupancy_grid_params": {"grid_cell_size": -1}}))
