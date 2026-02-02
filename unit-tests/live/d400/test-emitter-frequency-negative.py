# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#test:device each(D400*)

import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy import test, log

device, _ = test.find_first_device_or_exit()
depth_sensor = device.first_depth_sensor()
device_name = device.get_info(rs.camera_info.name)
fw_version = rsutils.version(device.get_info(rs.camera_info.firmware_version))

# List of SKUs that support emitter frequency from FW 5.14.0.0 onwards
SUPPORTED_SKUS = ["D455", "D457"]
MIN_FW_VERSION = rsutils.version(5, 14, 0, 0)

################################################################################################
test.start("Verify emitter frequency support based on SKU and FW version")

is_supported_sku = any(sku in device_name for sku in SUPPORTED_SKUS)
has_sufficient_fw = fw_version > MIN_FW_VERSION
should_support = is_supported_sku and has_sufficient_fw

actual_support = depth_sensor.supports(rs.option.emitter_frequency)
test.check_equal(actual_support, should_support)

if should_support:
    log.i(f"{device_name} with FW {fw_version} supports emitter frequency as expected")
else:
    if is_supported_sku and not has_sufficient_fw:
        log.i(f"{device_name} with FW {fw_version} does not support emitter frequency (FW too old)")
    else:
        log.i(f"{device_name} does not support emitter frequency as expected (not a supported SKU)")

test.finish()
################################################################################################
test.print_results_and_exit()
