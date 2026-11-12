# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import platform
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

# Nothing is set by the user: the reported gyro sensitivity is the FW value where the SDK can read it (Linux) and the
# SDK fallback otherwise (Windows). Both must equal EXPECTED_DEFAULT, so this fails on Linux when the FW default changes.
# Then: update the fallback in the code for that device (gyro_sensitivity_feature default_value for D400,
# fallback_index in d500_motion::register_gyro_sensitivity for D500) and the value below to the one this test reports.
pytestmark = [
    pytest.mark.device_each("D455", "D500*"),
    pytest.mark.device_type_exclude("DDS", "GMSL"),
    pytest.mark.skipif(platform.machine() == "aarch64", reason="D455 not available on CI Jetson"),
]

# product name fragment -> expected RS2_OPTION_GYRO_SENSITIVITY value (the first test of a file runs on a recycled
# device, so FW holds its own default). The first matching fragment wins, so D585S must stay before D585.
EXPECTED_DEFAULT = {
    "D585S": 4.0,  # 125 dps, the code fallback; not checked against FW (the lab D585S FW has no gyro sensitivity option)
    "D455": 1.0,  # 1000 dps
    "D585": 1.0,  # 1000 dps
}


def test_gyro_sensitivity_default(test_device):
    dev, _ = test_device
    ms = dev.first_motion_sensor()
    if not ms.supports(rs.option.gyro_sensitivity):
        pytest.skip("gyro sensitivity not supported on this device/FW")
    name = dev.get_info(rs.camera_info.name)
    expected = next((v for k, v in EXPECTED_DEFAULT.items() if k in name), None)
    if expected is None:
        pytest.skip(f"no expected default gyro sensitivity for '{name}', add it to EXPECTED_DEFAULT")

    assert ms.get_option(rs.option.gyro_sensitivity) == expected
