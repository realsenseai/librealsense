# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Device enumeration discovering and verifying the connected devices.
Requires 2 D400 series devices to run.
"""

import pytest
import pyrealsense2 as rs
from rspy.pytest.device_helpers import is_jetson_platform
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.skipif(is_jetson_platform(), reason="Not supported on Jetson"),
    pytest.mark.device("D400*", "D400*"),
]


def test_device_enumeration(test_devices):
    """Enumerate devices and verify each has sensors"""
    device_list, ctx = test_devices

    log.info(f"Found {len(device_list)} connected device(s)")

    for i, dev in enumerate(device_list):
        sn = dev.get_info(rs.camera_info.serial_number) if dev.supports(rs.camera_info.serial_number) else "Unknown"
        name = dev.get_info(rs.camera_info.name) if dev.supports(rs.camera_info.name) else "Unknown"
        log.info(f"Device {i+1}: {name} (SN: {sn})")

        sensors = dev.query_sensors()
        assert len(sensors) > 0, f"Device {i+1} should have sensors"

    log.info(f"All {len(device_list)} devices verified successfully")
