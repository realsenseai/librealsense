# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Test configuration: Requires 2 D400 series devices
#test:device:!jetson D400* D400*

"""
Device enumeration discovering and verifying the connected devices.

This test enumerates the devices and verifies basic functionality.
Requires 2 D400 series devices to run.
"""

import pyrealsense2 as rs
from rspy import test, log

# Find exactly 2 devices or skip the test
device_list, ctx = test.find_n_devices_or_exit(2)

log.i(f"Found {len(device_list)} connected device(s)")

#
# Enumerate and verify devices
#
with test.closure("Device enumeration and basic verification"):
    for i in range(len(device_list)):
        dev = device_list[i]
        
        # Get basic info
        sn = dev.get_info(rs.camera_info.serial_number) if dev.supports(rs.camera_info.serial_number) else "Unknown"
        name = dev.get_info(rs.camera_info.name) if dev.supports(rs.camera_info.name) else "Unknown"
        log.i(f"Device {i+1}: {name} (SN: {sn})")
        
        # Verify device is responsive
        sensors = dev.query_sensors()
        test.check(len(sensors) > 0, f"Device {i+1} should have sensors")
    
    log.i(f"All {len(device_list)} devices verified successfully")

test.print_results_and_exit()
