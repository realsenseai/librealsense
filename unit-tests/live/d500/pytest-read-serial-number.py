# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

# GVD structure and HWM don't often change
# test:donotrun:!weekly
# test:device D500*

import pyrealsense2 as rs
from rspy import test

# This test verifies we read a 12 digits serial number from GVD, and it matches the SDK reported device serial number

dev, _ = test.find_first_device_or_exit()
dp_device = dev.as_debug_protocol()

#############################################################################################
def extract_device_serial_number_from_gvd():
    # define constants
    gvd_opcode = 0x10
    gvd_size = 602
    data_start_offset = 4
    sn_offset = 84
    size_of_sn = 6  # [bytes]

    # get GVD RAW buffer
    cmd = dp_device.build_command(opcode=gvd_opcode)
    raw_gvd = dp_device.send_and_receive_raw_data(cmd)

    # extract the serial number string from the GVD
    dev_sn_from_gvd = ""
    for i in range(size_of_sn):  # handling high and low nibbles on same iteration
        byte = raw_gvd[data_start_offset + sn_offset + i]
        high_nibble, low_nibble = byte >> 4, byte & 0x0F
        dev_sn_from_gvd += str(high_nibble)
        dev_sn_from_gvd += str(low_nibble)

    return dev_sn_from_gvd


test.start("Verify D585S serial number")

dev_sn_from_sdk = dev.get_info(rs.camera_info.serial_number)
dev_sn_from_gvd = extract_device_serial_number_from_gvd()

test.check_equal( dev_sn_from_gvd , dev_sn_from_sdk)

test.finish()
#############################################################################################

test.print_results_and_exit()
