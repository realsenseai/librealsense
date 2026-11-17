# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest

pytestmark = [
    pytest.mark.device_each("D585"),
    pytest.mark.device_exclude("D585S"),  # D585S does not serve the depth mapping params
    pytest.mark.device_type("GMSL"),
]


def test_get_depth_mapping_params_is_unsupported(test_device):
    # Remove this test, and the GMSL exclusion in pytest-depth-mapping-params-get-set.py, once the driver has a control for the params
    dev, _ = test_device
    with pytest.raises(RuntimeError, match="not supported over GMSL"):
        dev.first_depth_mapping_sensor().get_depth_mapping_params()


def test_set_depth_mapping_params_is_unsupported(test_device):
    dev, _ = test_device
    with pytest.raises(RuntimeError, match="not supported over GMSL"):
        dev.first_depth_mapping_sensor().set_depth_mapping_params("{}")
