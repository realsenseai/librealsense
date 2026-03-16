# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

# Not frequently changing, using FW return codes. Can be checked weekly.
# test:donotrun:!weekly
# test:device each(D400*)
# test:device each(D500*)

# This UT tests the HWM error reporting mechanism.
# When we send HWM command and it is successful we expect the command opcode to be reflected in the first bytes of the reply.
# In case of failure a negative value will be returned, indicating the failure reason.


import pyrealsense2 as rs
from rspy import devices, log, test, file, repo
from rspy import tests_wrapper as tw


#############################################################################################
# Help Functions
#############################################################################################

def convert_bytes_string_to_decimal_list(command):
    command_input = []  # array of uint_8t

    # Parsing the command to array of unsigned integers(size should be < 8bits)
    # throw out spaces
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
    product_line = dev.get_info(rs.camera_info.product_line)
    tw.start_wrapper(dev)
except:
    test.unexpected_exception()
test.finish()

test.start("Invalid command")
try:
    command_opcode_as_int = 0xee
    failure_opcode_as_string = "ff ff ff ff"

    raw_command = rs.debug_protocol(dev).build_command(command_opcode_as_int)
    status, result = send_hardware_monitor_command(dev, raw_command)

    expected_status = convert_bytes_string_to_decimal_list(failure_opcode_as_string)
    test.check_equal_lists(status, expected_status)

except:
    test.unexpected_exception()
test.finish()

if product_line == "D400": # D500 doesn't have "No Data To Return" error code
    test.start("No Data to Return")
    try:
        command_opcode_as_int = 0x7d # GETSUBPRESETID
        failure_opcode_as_string = "eb ff ff ff" # NoDataToReturn = -21 = 0xeb

        raw_command = rs.debug_protocol(dev).build_command(command_opcode_as_int)
        status, result = send_hardware_monitor_command(dev, raw_command)

        expected_status = convert_bytes_string_to_decimal_list(failure_opcode_as_string)
        test.check_equal_lists(status, expected_status)

    except:
        test.unexpected_exception()
    test.finish()

test.start("Wrong Parameter")
try:
    command_opcode_as_int = 0x2b # SET_ADV
    failure_opcode_as_string = "fa ff ff ff" # WRONG_PARAM = -6 = 0xfa
    if product_line == "D500":
        command_opcode_as_int = 0x69 # SET_CAM_SYNC
        failure_opcode_as_string = "fe ff ff ff" # INVALID_PARAM = -2 = 0xfe

    raw_command = rs.debug_protocol(dev).build_command(command_opcode_as_int)
    raw_command[9] = 9
    status, result = send_hardware_monitor_command(dev, raw_command)

    expected_status = convert_bytes_string_to_decimal_list(failure_opcode_as_string)
    test.check_equal_lists(status, expected_status)

except:
    test.unexpected_exception()
test.finish()

tw.stop_wrapper(dev)
test.print_results_and_exit()
