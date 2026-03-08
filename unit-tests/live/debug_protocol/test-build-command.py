# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

# Not frequently changing, no need to test for each commit
# test:donotrun:!nightly
# test:device D400*
# test:device each(D555)

import pyrealsense2 as rs
from rspy import log, test, file, repo


#############################################################################################
# Help Functions
#############################################################################################

def convert_bytes_string_to_decimal_list(command):
    command_input = []  # array of uint_8t

    # Parsing the command to array of unsigned integers(size should be < 8bits)
    # threw out spaces
    command = command.lower()
    command = command.split()

    for byte in command:
        command_input.append(int('0x' + byte, 0))

    return command_input


def send_hardware_monitor_command(device, command):
    raw_result = rs.debug_protocol(device).send_and_receive_raw_data(command)
    status = raw_result[:4]
    result = raw_result[4:]
    return status, result


#############################################################################################
# Tests
#############################################################################################

test.start("Init")
try:
    ctx = rs.context()
    dev = ctx.query_devices()[0]
except:
    test.unexpected_exception()
test.finish()

#############################################################################################

test.start("Old Scenario Test")
try:
    # creating a raw data command
    # [msg_length, magic_number, opcode, params, data]
    # all values are in hex - little endian
    msg_length = "14 00"
    magic_number = "ab cd"
    gvd_opcode_as_string = "10 00 00 00"  # gvd opcode = 0x10
    params_and_data = "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"  # empty params and data
    gvd_command = msg_length + " " + magic_number + " " + gvd_opcode_as_string + " " + params_and_data
    raw_command = convert_bytes_string_to_decimal_list(gvd_command)

    status, old_scenario_result = send_hardware_monitor_command(dev, raw_command)

    # expected status in case of success of "send_hardware_monitor_command" is the same as opcode
    expected_status = convert_bytes_string_to_decimal_list(gvd_opcode_as_string)

    test.check_equal_lists(status, expected_status)
except:
    test.unexpected_exception()
test.finish()

#############################################################################################

test.start("New Scenario Test")
try:
    gvd_opcode_as_int = 0x10
    gvd_opcode_as_string = "10 00 00 00"  # little endian

    raw_command = rs.debug_protocol(dev).build_command(gvd_opcode_as_int)
    status, new_scenario_result = send_hardware_monitor_command(dev, raw_command)

    # expected status in case of success of "send_hardware_monitor_command" is the same as opcode
    expected_status = convert_bytes_string_to_decimal_list(gvd_opcode_as_string)

    test.check_equal_lists(status, expected_status)
    product_name = dev.get_info(rs.camera_info.name)
    if 'D457' in product_name:
        # compare only the first 272 bytes since MIPI devices can return bigger buffer with
        # irrelevant data after the first 272 bytes
        test.check_equal_lists(new_scenario_result[:272], old_scenario_result[:272])
    else:
        test.check_equal_lists(new_scenario_result, old_scenario_result)

except:
    test.unexpected_exception()
test.finish()


#############################################################################################

test.print_results_and_exit()
