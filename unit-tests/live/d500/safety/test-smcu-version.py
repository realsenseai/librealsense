# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

# Not frequently changing, no need to test for each commit
# test:donotrun:!nightly
# test:device D585S


import os, sys
import subprocess
import re
import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy import test, log, repo


with test.closure("Test to read SMCU Version from rsutils FW version, HWMC Commands and rs-fw-update tool, "
                    "Compare against 0.0.0.0 and check if all the methods provide same result"):
    versions_dict ={}

    # Find the first RealSense device or exit if not found
    device, _ = test.find_first_device_or_exit()

    ########################  FW version using rs camera info  #######################

    # Get the firmware version using rs_camera_info
    fw_version = rsutils.version(device.get_info(rs.camera_info.smcu_fw_version))

    # Print the firmware version
    log.d("SMCU Version Using rs_camera_info: ", fw_version)
    versions_dict["smcu_version_rsutils_fw_version"] = str(fw_version)

    ########################  FW version using rs camera info  #######################


    ############################  FW version using HMC  ################################

    # Get the firmware version using GVD cmd
    dp_device = device.as_debug_protocol()
    gvd_data = dp_device.send_and_receive_raw_data(dp_device.build_command(0x10))
    #log.d ("HMC Command GVD " + str(gvd_data))
    substring = gvd_data[275:279]
    hmc_smcu_version = ".".join(str(char) for char in substring)
    log.d("SMCU Version Using HMC: ", hmc_smcu_version)
    versions_dict["smcu_version_hmc"] = str(hmc_smcu_version)

    ############################  FW version using HMC  ################################


    ########################## FW version using rs-fw-update  ###########################

    # Get the firmware version using rs-fw-update
    rs_fw_update_tool_path = repo.find_built_exe( 'tools/fw-update', 'rs-fw-update' )
    log.d ("rs-fw-update path in the directory :",rs_fw_update_tool_path)

    version = subprocess.check_output(rs_fw_update_tool_path).decode("utf-8")

    rs_fw_update_smcu_version = re.search('SMCU firmware version: ([\d.]+)',version)
    if rs_fw_update_smcu_version:
        smcu_version_rs_fw_update_tool= rs_fw_update_smcu_version.group(1)
        log.d("SMCU version using rs-fw-update: ", smcu_version_rs_fw_update_tool)
        versions_dict["smcu_version_rs_fw_update"] = str(smcu_version_rs_fw_update_tool)

    ########################## FW version using rs-fw-update  ###########################
    log.d ("Actual Dictonary is " + str(versions_dict.values()))

    # Consider SMCU Versions fetched from one of the methods as reference
    test_val = versions_dict["smcu_version_rsutils_fw_version"]
    log.d ("SMCU Version From one of the method = " + str(test_val))
    for key, value in versions_dict.items():
        test.info(key, value)
        # check the smcu version is not zero
        test.check(value != "0.0.0.0", "SMCU Version is 0.0.0.0!", test.LOG)
        # Check the smcu version is same across multiple methods
        test.check(value == test_val, "SMCU Version is different than expected!", test.LOG)

test.print_results_and_exit()
