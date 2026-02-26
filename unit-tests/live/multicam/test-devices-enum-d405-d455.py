# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Test configuration: Enumerate D405 and D455 devices only
#test:device D405
#test:device D455

"""
Device enumeration test specifically for D405 and D455 devices.

This test enumerates and verifies D405 and D455 devices as configured by the test infrastructure.
The #test:device directives ensure the test only runs when D405 or D455 devices are available.
The test code filters to only process D405 and D455 devices from all connected devices.
"""

import pyrealsense2 as rs
from rspy import test, log

# Query all connected devices and filter to D405/D455 only
ctx = rs.context()
all_devices = ctx.query_devices()

device_list = [dev for dev in all_devices 
               if dev.supports(rs.camera_info.name) and 
               ('D405' in dev.get_info(rs.camera_info.name) or 
                'D455' in dev.get_info(rs.camera_info.name))]

device_count = len(device_list)

log.i(f"Found {device_count} D405/D455 device(s) ({len(all_devices)} total connected)")

if device_count == 0:
    with test.closure("No devices found"):
        log.e("No devices connected - test cannot proceed")
        test.check(False, "At least one device required")
else:
    #
    # Enumerate and verify all devices
    #
    with test.closure("Device enumeration and basic verification"):
        
        for i in range(device_count):
            dev = device_list[i]
            
            # Get basic info
            sn = dev.get_info(rs.camera_info.serial_number) if dev.supports(rs.camera_info.serial_number) else "Unknown"
            name = dev.get_info(rs.camera_info.name) if dev.supports(rs.camera_info.name) else "Unknown"
            log.i(f"Device {i+1}: {name} (SN: {sn})")
            
            # Verify device is responsive
            sensors = dev.query_sensors()
            test.check(len(sensors) > 0, f"Device {i+1} should have sensors")
        
        log.i(f"All {device_count} devices verified successfully")

test.print_results_and_exit()

