# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

# Supported on D400 USB devices with FW version 5.12.10.11 and above. https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-device.cpp#L1026
# test:device D400*
# test:type USB

import pyrealsense2 as rs
from rspy import test

# 1. Scenario 1:
    #          - Change control value few times
    #         - Turn toggle off
    #          - Turn toggle on
    #          - Check that control limit value is the latest value
# 2. Scenario 2:
    #       - Init 2 devices
    #        - toggle on both dev1 and dev2 and set two distinct values for the auto-exposure/gain.
    #        - toggle both dev1and dev2 off.
    #        2.1. toggle dev1 on :
    #                  * verify that the limit value is the value that was stored (cached) in dev1.
    #                  * verify that for dev2 both the limit and the toggle values are similar to those of dev1
    #        2.2. toggle dev2 on :
    #                  * verify that the limit value is the value that was stored (cached) in dev2.

ctx = rs.context()
device_list = ctx.query_devices()

fw_version = rsutils.version(device_list.front().get_info(rs.camera_info.firmware_version))
if fw_version < rsutils.version(5,12,10,11):
    log.i(f"FW version {fw_version} does not support AUTO EXPOSURE LIMIT option, skipping test...")
    test.print_results_and_exit()
    
#############################################################################################
with test.closure("Auto Exposure toggle one device"):
    # Scenario 1:
    device = device_list.front()
    sensor = device.first_depth_sensor()
    option_range = sensor.get_option_range(rs.option.auto_exposure_limit)
    values = [option_range.min + 5.0, option_range.max / 4.0, option_range.max - 5.0]
    for val in values:
        sensor.set_option(rs.option.auto_exposure_limit, val)
    sensor.set_option(rs.option.auto_exposure_limit_toggle, 0.0)  # off
    sensor.set_option(rs.option.auto_exposure_limit_toggle, 1.0)  # on
    limit = sensor.get_option(rs.option.auto_exposure_limit)
    test.check_equal(limit, values[2])
#############################################################################################
with test.closure("Auto Exposure two devices"):
    # Scenario 2:
    device1 = device_list.front()
    s1 = device1.first_depth_sensor()
    device2 = device_list.front()
    s2 = device2.first_depth_sensor()

    option_range = s1.get_option_range(rs.option.auto_exposure_limit) #should be same range from both sensors
    s1.set_option(rs.option.auto_exposure_limit, option_range.max / 4.0)
    s1.set_option(rs.option.auto_exposure_limit_toggle, 0.0)  # off
    s2.set_option(rs.option.auto_exposure_limit, option_range.max - 5.0)
    s2.set_option(rs.option.auto_exposure_limit_toggle, 0.0)  # off

    #2.1
    s1.set_option(rs.option.auto_exposure_limit_toggle, 1.0)  # on
    limit1 = s1.get_option(rs.option.auto_exposure_limit)
    test.check_equal(limit1, option_range.max / 4.0)
    # keep toggle of dev2 off
    limit2 = s2.get_option(rs.option.auto_exposure_limit)
    test.check_equal(limit1, limit2)

    # 2.2
    s2.set_option(rs.option.auto_exposure_limit_toggle, 1.0)  # on
    limit2 = s2.get_option(rs.option.auto_exposure_limit)
    test.check_equal(limit2, option_range.max - 5.0)

#############################################################################################
with test.closure("Gain toggle one device"):
    # Scenario 1:
    device = device_list.front()
    sensor = device.first_depth_sensor()
    option_range = sensor.get_option_range(rs.option.auto_gain_limit)
    # 1. Scenario 1:
    # - Change control value few times
    # - Turn toggle off
    # - Turn toggle on
    # - Check that control limit value is the latest value
    values = [option_range.min + 5.0, option_range.max / 4.0, option_range.max - 5.0]
    for val in values:
        sensor.set_option(rs.option.auto_gain_limit, val)
    sensor.set_option(rs.option.auto_gain_limit_toggle, 0.0)  # off
    sensor.set_option(rs.option.auto_gain_limit_toggle, 1.0)  # on
    limit = sensor.get_option(rs.option.auto_gain_limit)
    test.check_equal(limit, values[2])
#############################################################################################
with test.closure("Gain toggle two devices"):
    # Scenario 2:
    device1 = device_list.front()
    s1 = device1.first_depth_sensor()
    device2 = device_list.front()
    s2 = device2.first_depth_sensor()

    option_range = s1.get_option_range(rs.option.auto_gain_limit) #should be same range from both sensors
    s1.set_option(rs.option.auto_gain_limit, option_range.max / 4.0)
    s1.set_option(rs.option.auto_gain_limit_toggle, 0.0)  # off
    s2.set_option(rs.option.auto_gain_limit, option_range.max - 5.0)
    s2.set_option(rs.option.auto_gain_limit_toggle, 0.0)  # off

    #2.1
    s1.set_option(rs.option.auto_gain_limit_toggle, 1.0)  # on
    limit1 = s1.get_option(rs.option.auto_gain_limit)
    test.check_equal(limit1, option_range.max / 4.0)
    # keep toggle of dev2 off
    limit2 = s2.get_option(rs.option.auto_gain_limit)
    test.check_equal(limit1, limit2)

    # 2.2
    s2.set_option(rs.option.auto_gain_limit_toggle, 1.0)  # on
    limit2 = s2.get_option(rs.option.auto_gain_limit)
    test.check_equal(limit2, option_range.max - 5.0)

#############################################################################################
test.print_results_and_exit()
