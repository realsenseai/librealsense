# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.


#test:device each(D400*) !D401
#test:device each(D500*)


import pyrealsense2 as rs
from rspy import test


test.start("Testing connection type can be detected")

dev, _ = test.find_first_device_or_exit()

if test.check(dev.supports(rs.camera_info.connection_type)):
    connection_type = dev.get_info(rs.camera_info.connection_type)
    camera_name = dev.get_info(rs.camera_info.name)
    if test.check(connection_type):
        if 'D457' in camera_name:
            test.check(connection_type == "GMSL")
        elif 'D555' in camera_name:
            test.check(connection_type == "DDS")
        else:
            test.check(connection_type == "USB")

test.finish()
test.print_results_and_exit()
