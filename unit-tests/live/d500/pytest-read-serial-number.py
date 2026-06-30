# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# GVD structure and HWM don't often change

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D500*"),
    pytest.mark.context("weekly"),
]


# This test verifies we read a 12 digits serial number from GVD, and it matches the SDK reported device serial number
def test_serial_number_matches_gvd(test_device):
    dev, _ = test_device
    dp_device = dev.as_debug_protocol()

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

    dev_sn_from_sdk = dev.get_info(rs.camera_info.serial_number)
    dev_sn_from_gvd = extract_device_serial_number_from_gvd()

    assert dev_sn_from_gvd == dev_sn_from_sdk
