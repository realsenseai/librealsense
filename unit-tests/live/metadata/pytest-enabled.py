# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import sys
import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
    pytest.mark.priority(2),  # Run after fw-update tests
    pytest.mark.skipif(sys.platform != 'win32', reason="Windows only"),
]


def test_metadata_enabled(test_device):
    """Check that metadata is enabled on the device"""
    dev, ctx = test_device
    assert dev.is_metadata_enabled(), "Metadata should be enabled on the device"
