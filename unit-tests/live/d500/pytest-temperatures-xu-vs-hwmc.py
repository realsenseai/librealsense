# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

# Not frequently changing, no need to test for each commit
# test:donotrun:!nightly
# test:device D500*

import pyrealsense2 as rs
from rspy import test

# This test checks that the same values of temperature are received whether XU command or HWM Command are used.

dev, _ = test.find_first_device_or_exit()
depth_sensor = dev.first_depth_sensor()
dp_device = dev.as_debug_protocol()

is_projector_option_supported = True
########################################  HELPERS  ##########################################

def get_temperatures_from_xu():
    global is_projector_option_supported
    pvt_temp = -10
    ohm_temp = -10
    proj_temp = -10

    test.check(depth_sensor.supports(rs.option.soc_pvt_temperature))
    test.check(depth_sensor.supports(rs.option.ohm_temperature))
    is_projector_option_supported = depth_sensor.supports(rs.option.projector_temperature)

    pvt_temp = depth_sensor.get_option(rs.option.soc_pvt_temperature)
    ohm_temp = depth_sensor.get_option(rs.option.ohm_temperature)
    if is_projector_option_supported:
        proj_temp = depth_sensor.get_option(rs.option.projector_temperature)

    return pvt_temp, ohm_temp, proj_temp


def parse_temperature_from_hwm(hwm_answer):
    """
    The returned value from gtemp hwmc is a list of uint8_t, with:
          - the 4 first values are the opcode of the request - 2a 0 0 0 in our case
          - the remaining values are temperatures of several components, each represented
            by 2 values: the first is the decimal part, and the second one is the whole value part
            e.g.: if the 2 values are 90 28 (in hex):
            * the whole value part is 0x28 = 40
            * the decimal value part is 0x90 = 144. 144/256 = 0.5625
            So in this example, the resulting temperature is 40.5625 deg.
    This function parses the hwmc returned list to a list of temperatures, parsed as explained above.
    """
    # stepping over the 4 first values (opcode of the request, see above explanation)
    relevant_data = hwm_answer[4:]
    temperatures_list = []

    test.check_equal(len(relevant_data), 20)
    whole_number_part = 0
    decimal_part = 0
    for i in range(len(relevant_data)):
        if i % 2 == 0:
            decimal_part = relevant_data[i]
        else:
            whole_number_part = relevant_data[i]
            current_temp = whole_number_part + decimal_part / 256.0
            temperatures_list.append(current_temp)
            whole_number_part = 0
            decimal_part = 0
    return temperatures_list


def get_temperatures_from_hwm():
    gtemp_opcode = 0x2a

    # getting all the available temperatures
    param_for_all_temp = 0
    all_temp_cmd = dp_device.build_command(opcode=gtemp_opcode, param1=param_for_all_temp)
    all_temp_list = parse_temperature_from_hwm(dp_device.send_and_receive_raw_data(all_temp_cmd))

    # get pvt temperature
    pvt_temp_index = 7
    pvt_temp = all_temp_list[pvt_temp_index - 1]

    # get ohm temperature
    ohm_temp_index = 2
    ohm_temp = all_temp_list[ohm_temp_index - 1]

    # get projector temperature
    proj_temp = -10
    if is_projector_option_supported:
        proj_temp_index = 1
        proj_temp = all_temp_list[proj_temp_index - 1]

    return pvt_temp, ohm_temp, proj_temp


#############################################################################################

test.start("Compare Temperature readings XU vs HWMC")

pvt_temp_xu, ohm_temp_xu, projector_temp_xu = get_temperatures_from_xu()

pvt_temp_hwm, ohm_temp_hwm, projector_temp_hwm = get_temperatures_from_hwm()

# Since PVT in XU is different sensor than PVT in HMC, we increase the tollerance to 3 deg
tolerance = 3.0
test.check_approx_abs(pvt_temp_xu, pvt_temp_hwm, tolerance)
test.check_approx_abs(ohm_temp_xu, ohm_temp_hwm, tolerance)
if is_projector_option_supported:
    test.check_approx_abs(projector_temp_xu, projector_temp_xu, tolerance)

test.finish()
#############################################################################################

test.print_results_and_exit()
