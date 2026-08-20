# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from .pyrealsense_mock import create_mock_device


def setup_fake_devices():
    """Creates the fake RealSense devices used by the mocked API tests."""
    return [
        create_mock_device("device1", "Test Device 1", with_depth=True, with_color=True),
        create_mock_device("device2", "Test Device 2", with_depth=True, with_color=True),
    ]
