# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Feature not changing frequently, no need to run on each commit

import pytest
import pyrealsense2 as rs
from pytest_check import check
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D585S"),
    pytest.mark.context("nightly"),
]


# D500 devices support an extended buffer (> 1 KB) on HWMC for reading / writing calibration tables.
# This test only test the 'read' part as we don't want to ruin our calibration tables in the device.

@pytest.fixture
def dp_device(test_device):
    dev, _ = test_device
    return dev.as_debug_protocol()


def test_ds5_standard_buffer(dp_device):
    # getting gvd
    gvd_opcode = 0x10
    gvd_opcode_size = 4  # bytes
    gvd_header_size = 8
    gvd_expected_full_size = 602
    gvd_expected_payload_size = gvd_expected_full_size - gvd_header_size
    gvd_payload_size_offset = 2
    gvd_payload_size_element_size = 2
    expected_gvd_version = "2.0"

    cmd = dp_device.build_command(opcode=gvd_opcode)
    ans = dp_device.send_and_receive_raw_data(cmd)

    # returns 4 bytes with opcode, and then the requested buffer
    check.equal(ans[0], gvd_opcode)
    # remove first 4 bytes of opcode and continue testing the GVD message itself
    rcv_gvd = ans[gvd_opcode_size:]
    current_gvd_version = str(rcv_gvd[0]) + "." + str(rcv_gvd[1])

    log.debug("GVD Version: %s", current_gvd_version)

    check.equal(current_gvd_version, expected_gvd_version)
    check.equal(len(rcv_gvd), gvd_expected_full_size)

    rcv_gvd_payload_size = rcv_gvd[gvd_payload_size_offset: gvd_payload_size_offset + gvd_payload_size_element_size]
    rcv_gvd_payload_size_integer = int.from_bytes(rcv_gvd_payload_size, byteorder='little')
    check.equal(rcv_gvd_payload_size_integer, gvd_expected_payload_size)


def test_get_buffer_less_than_1kb(dp_device):
    # getting depth_calibration_table - size is 512
    depth_calib_table_id = 0xb4
    depth_calib_table_size = 512
    get_hkr_config_table_opcode = 0xa7
    cmd = dp_device.build_command(opcode=get_hkr_config_table_opcode, param1=0, param2=depth_calib_table_id, param3=0)
    ans = dp_device.send_and_receive_raw_data(cmd)

    check.equal(ans[0], get_hkr_config_table_opcode)
    check.equal(len(ans), depth_calib_table_size + 4)


def test_get_buffer_more_than_1kb_whole(dp_device):
    # getting rgb_lens_shading table - size is 1088
    rgb_lens_shading_table_id = 0xb2
    rgb_lens_shading_table_size = 1088
    get_hkr_config_table_opcode = 0xa7
    cmd = dp_device.build_command(opcode=get_hkr_config_table_opcode, param1=0, param2=rgb_lens_shading_table_id, param3=0)
    ans = dp_device.send_and_receive_raw_data(cmd)
    check.equal(ans[0], get_hkr_config_table_opcode)
    check.equal(len(ans), rgb_lens_shading_table_size + 4)


def test_get_buffer_more_than_1kb_chunks(dp_device):
    # getting rgb_lens_shading table - size is 1088
    rgb_lens_shading_table_id = 0xb2
    rgb_lens_shading_table_size = 1088
    get_hkr_config_table_opcode = 0xa7
    first_chunk_from_two_param = 0x10000
    cmd1 = dp_device.build_command(opcode=get_hkr_config_table_opcode, param1=0, param2=rgb_lens_shading_table_id, param3=0, param4=first_chunk_from_two_param)
    ans1 = dp_device.send_and_receive_raw_data(cmd1)
    check.equal(ans1[0], get_hkr_config_table_opcode)

    second_chunk_from_two_param = 0x10001
    cmd2 = dp_device.build_command(opcode=get_hkr_config_table_opcode, param1=0, param2=rgb_lens_shading_table_id, param3=0, param4=second_chunk_from_two_param)
    ans2 = dp_device.send_and_receive_raw_data(cmd2)
    check.equal(ans2[0], get_hkr_config_table_opcode)

    ans = ans1 + ans2
    twice_opcode_length = 8
    check.equal(len(ans), rgb_lens_shading_table_size + twice_opcode_length)
