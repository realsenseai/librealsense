# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
]


def test_usb_type_detected(test_device):
    """Test that USB type can be detected on USB-connected devices"""
    dev, ctx = test_device

    # Skip non-USB devices (replaces legacy #test:type USB)
    if dev.supports(rs.camera_info.connection_type):
        conn_type = dev.get_info(rs.camera_info.connection_type)
        if conn_type.lower() != "usb":
            pytest.skip(f"Not a USB device (connection type: {conn_type})")

    assert dev.supports(rs.camera_info.usb_type_descriptor), "Device should support usb_type_descriptor"
    usb_type = dev.get_info(rs.camera_info.usb_type_descriptor)
    assert usb_type and usb_type != "Undefined", f"USB type should be defined, got: {usb_type}"
