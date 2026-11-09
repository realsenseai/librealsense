# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
A device that throws on creation must not hide the other devices from the registry.
"""

import pytest

import rspy.devices as dev


class FakeDeviceList:
    """Like pyrealsense2.device_list: the device is created when indexed, and may throw."""
    def __init__(self, entries):
        self._entries = entries

    def __len__(self):
        return len(self._entries)

    def __getitem__(self, i):
        if isinstance(self._entries[i], Exception):
            raise self._entries[i]
        return self._entries[i]


def test_create_devices_skips_failing_device():
    bad = RuntimeError("option 'Filter Type' not in this filter")
    assert dev._create_devices( FakeDeviceList( ['D455', bad, 'D435'] ) ) == ['D455', 'D435']


def test_create_devices_all_good():
    assert dev._create_devices( FakeDeviceList( ['D455', 'D435'] ) ) == ['D455', 'D435']


def test_create_devices_empty():
    assert dev._create_devices( FakeDeviceList( [] ) ) == []
