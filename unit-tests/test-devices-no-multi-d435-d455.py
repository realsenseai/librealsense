# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Test configuration: Enumerate D435 and D455 devices only (without multi_device directive)
#test:device D435 D455

"""
Device enumeration test specifically for D435 and D455 devices using standard #test:device directive.

This test demonstrates the difference between #test:device and #test:multi_device directives.
Unlike test-devices-enum-d435-d455.py which uses #test:multi_device, this test uses the
standard #test:device directive which requires both device types to be present but treats
them as a standard device configuration rather than a multi-device test.
"""

import pyrealsense2 as rs
from rspy import test, log

# Query all connected devices and filter to D435/D455 only
ctx = rs.context()
all_devices = ctx.query_devices()

device_list = [dev for dev in all_devices 
               if dev.supports(rs.camera_info.name) and 
               ('D435' in dev.get_info(rs.camera_info.name) or 
                'D455' in dev.get_info(rs.camera_info.name))]

device_count = len(device_list)

log.i(f"Found {device_count} D435/D455 device(s) ({len(all_devices)} total connected)")

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
