# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Test configuration: Requires at least 2 devices
#test:device * *

"""
Device enumeration example showing basic pattern for discovering and verifying all connected devices.

This test enumerates all devices and verifies basic functionality without extensive documentation.
Requires at least 2 devices to run.
"""

import pyrealsense2 as rs
from rspy import test, log

# Query all connected devices directly via RealSense context
ctx = rs.context()
device_list = ctx.query_devices()
device_count = len(device_list)

log.i(f"Found {device_count} connected device(s)")

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

