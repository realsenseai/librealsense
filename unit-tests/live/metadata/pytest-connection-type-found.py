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


def test_connection_type_detected(test_device):
    """Test that connection type can be detected and matches expected type per device"""
    dev, ctx = test_device

    assert dev.supports(rs.camera_info.connection_type), "Device should support connection_type info"
    connection_type = dev.get_info(rs.camera_info.connection_type)
    product_name = dev.get_info(rs.camera_info.name)
    assert connection_type, f"Connection type should not be empty for {product_name}"

    if any(model in product_name for model in ['D457', 'D401']):
        assert connection_type == "GMSL", f"{product_name} should have GMSL connection, got: {connection_type}"
    elif any(model in product_name for model in ['D555']):
        assert connection_type == "DDS", f"{product_name} should have DDS connection, got: {connection_type}"
    else:
        assert connection_type == "USB", f"{product_name} should have USB connection, got: {connection_type}"
