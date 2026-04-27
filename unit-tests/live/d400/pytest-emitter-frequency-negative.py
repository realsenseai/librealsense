# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
import pyrsutils as rsutils
import logging
log = logging.getLogger(__name__)

pytestmark = [pytest.mark.device_each("D400*")]

# List of SKUs that support emitter frequency from FW 5.14.0.0 onwards
SUPPORTED_SKUS = ["D455", "D457"]
MIN_FW_VERSION = rsutils.version(5, 14, 0, 0)


def test_emitter_frequency_support_based_on_sku_and_fw(test_device):
    dev, _ = test_device
    depth_sensor = dev.first_depth_sensor()
    device_name = dev.get_info(rs.camera_info.name)
    fw_version = rsutils.version(dev.get_info(rs.camera_info.firmware_version))

    is_supported_sku = any(sku in device_name for sku in SUPPORTED_SKUS)
    has_sufficient_fw = fw_version > MIN_FW_VERSION
    should_support = is_supported_sku and has_sufficient_fw

    actual_support = depth_sensor.supports(rs.option.emitter_frequency)
    assert actual_support == should_support

    if should_support:
        log.info(f"{device_name} with FW {fw_version} supports emitter frequency as expected")
    else:
        if is_supported_sku and not has_sufficient_fw:
            log.info(f"{device_name} with FW {fw_version} does not support emitter frequency (FW too old)")
        else:
            log.info(f"{device_name} does not support emitter frequency as expected (not a supported SKU)")
