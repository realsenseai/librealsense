# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import sys
import math
import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.extras_type_exclude("GMSL"),   # the USB dual RGB and GMSL dedicated sensor runs cover it
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
]


@pytest.mark.priority(2)  # Run after fw-update tests
@pytest.mark.skipif(sys.platform != 'win32', reason="Windows only")
def test_metadata_enabled(test_device):
    """Check that metadata is enabled on the device"""
    dev, ctx = test_device
    assert dev.is_metadata_enabled(), "Metadata should be enabled on the device"


def test_connection_type_detected(test_device):
    """Test that connection type can be detected and matches expected type per device"""
    dev, ctx = test_device

    assert dev.supports(rs.camera_info.connection_type), "Device should support connection_type info"
    connection_type = dev.get_info(rs.camera_info.connection_type)
    product_name = dev.get_info(rs.camera_info.name)
    assert connection_type, f"Connection type should not be empty for {product_name}"

    if any(model in product_name for model in ['D457', 'D401']):
        assert connection_type == "GMSL", f"{product_name} should have GMSL connection, got: {connection_type}"
    elif 'D585' in product_name:
        # D585 is connected over USB or GMSL (MIPI on Jetson)
        assert connection_type in ("USB", "GMSL"), f"{product_name} should have USB or GMSL connection, got: {connection_type}"
    elif any(model in product_name for model in ['D555']):
        assert connection_type == "DDS", f"{product_name} should have DDS connection, got: {connection_type}"
    else:
        assert connection_type == "USB", f"{product_name} should have USB connection, got: {connection_type}"


@pytest.mark.device_type("USB")
def test_usb_type_detected(test_device):
    """Test that USB type can be detected on USB-connected devices"""
    dev, ctx = test_device
    assert dev.supports(rs.camera_info.usb_type_descriptor), "Device should support usb_type_descriptor"
    usb_type = dev.get_info(rs.camera_info.usb_type_descriptor)
    assert usb_type and usb_type != "Undefined", f"USB type should be defined, got: {usb_type}"


@pytest.mark.device_type_exclude("DDS")  # DDS devices do not implement RS2_EXTENSION_GLOBAL_TIMER
def test_device_time_is_positive(test_device):
    dev, _ = test_device
    t_ms = dev.get_device_time_ms()
    log.info(f"device time = {t_ms} ms")
    assert math.isfinite(t_ms)
    assert t_ms > 0.0
