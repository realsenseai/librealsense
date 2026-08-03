# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Not frequently changing, no need to test for each commit

import time
import pytest
import pyrealsense2 as rs
from pytest_check import check
from rspy.timer import Timer
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D500*"),
    pytest.mark.context("nightly"),
]


# This test checks that the same values of temperature are received whether XU command or HWM Command are used.
def test_temperatures_xu_vs_hwmc(test_device):
    dev, _ = test_device
    depth_sensor = dev.first_depth_sensor()
    dp_device = dev.as_debug_protocol()

    ########################################  HELPERS  ##########################################

    def get_temperatures_from_xu(depth_sensor, is_projector_option_supported):
        pvt_temp = -10
        ohm_temp = -10
        proj_temp = -10

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
              - the whole value part is signed (int8_t), to support sub-zero temperatures.
              - a pair of 0xff 0xff means the component's temperature is unavailable, and is
                reported as 0 deg, mirroring temperature_option::query() in d500-options.cpp.
        This function parses the hwmc returned list to a list of temperatures, parsed as explained above.
        """
        # stepping over the 4 first values (opcode of the request, see above explanation)
        relevant_data = hwm_answer[4:]
        temperatures_list = []

        check.equal(len(relevant_data), 20)
        whole_number_part = 0
        decimal_part = 0
        for i in range(len(relevant_data)):
            if i % 2 == 0:
                decimal_part = relevant_data[i]
            else:
                whole_number_part = relevant_data[i]
                if decimal_part == 0xff or whole_number_part == 0xff:
                    assert decimal_part == 0xff and whole_number_part == 0xff, \
                        f"Partial unavailability sentinel: decimal=0x{decimal_part:02x} whole=0x{whole_number_part:02x}"
                    current_temp = 0.0
                else:
                    signed_whole_number_part = whole_number_part - 256 if whole_number_part > 127 else whole_number_part
                    current_temp = signed_whole_number_part + decimal_part / 256.0
                temperatures_list.append(current_temp)
                whole_number_part = 0
                decimal_part = 0
        return temperatures_list


    def get_temperatures_from_hwm(dp_device, is_projector_option_supported):
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


    check.is_true(depth_sensor.supports(rs.option.soc_pvt_temperature))
    check.is_true(depth_sensor.supports(rs.option.ohm_temperature))
    is_projector_option_supported = depth_sensor.supports(rs.option.projector_temperature)

    # Since PVT in XU is different sensor than PVT in HMC, we increase the tolerance to 3 deg
    tolerance = 3.0

    # Allow the system some time to read the correct value after power up
    warmup_timeout = 10  # seconds
    timer = Timer(warmup_timeout)
    timer.start()
    while True:
        pvt_temp_xu, ohm_temp_xu, projector_temp_xu = get_temperatures_from_xu(
            depth_sensor, is_projector_option_supported)
        pvt_temp_hwm, ohm_temp_hwm, projector_temp_hwm = get_temperatures_from_hwm(
            dp_device, is_projector_option_supported)
        converged = (abs(pvt_temp_xu - pvt_temp_hwm) <= tolerance
                     and abs(ohm_temp_xu - ohm_temp_hwm) <= tolerance
                     and (not is_projector_option_supported
                          or abs(projector_temp_xu - projector_temp_hwm) <= tolerance))
        if converged or timer.has_expired():
            break
        time.sleep(1)

    check.almost_equal(pvt_temp_xu, pvt_temp_hwm, abs=tolerance)
    check.almost_equal(ohm_temp_xu, ohm_temp_hwm, abs=tolerance)
    if is_projector_option_supported:
        check.almost_equal(projector_temp_xu, projector_temp_hwm, abs=tolerance)
