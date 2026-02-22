# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Test configuration: Device enumeration test that works with all connected devices
#test:device *

"""
Device enumeration example showing basic pattern for discovering and verifying all connected devices.

This test enumerates all devices and verifies basic functionality without extensive documentation.
"""

import pyrealsense2 as rs
from rspy import test, log, devices

# Ensure we have device information
if not devices.all():
    devices.query(recycle_ports=False)

# Get all enabled devices
all_sns = list(devices.enabled())

log.i(f"Found {len(all_sns)} connected device(s)")

if len(all_sns) == 0:
    log.w("No devices connected - skipping test")
else:
    #
    # Enumerate and verify all devices
    #
    with test.closure("Device enumeration and basic verification"):
        
        for i, sn in enumerate(all_sns, 1):
            dev = devices.get(sn).handle
            
            # Get basic info
            name = dev.get_info(rs.camera_info.name) if dev.supports(rs.camera_info.name) else "Unknown"
            log.i(f"Device {i}: {name} (SN: {sn})")
            
            # Verify device is responsive
            sensors = dev.query_sensors()
            test.check(len(sensors) > 0, f"Device {i} should have sensors")
        
        log.i(f"All {len(all_sns)} devices verified successfully")
