# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Tests for rspy.pytest.device_helpers.require_min_fw_on_d585_gmsl.

The FW gate applies only to a D585 connected over GMSL: an older FW skips the test there,
while other SKUs, other transports and new enough FW are never skipped.
"""

import pytest

rs = pytest.importorskip("pyrealsense2")
pytest.importorskip("pyrsutils")

from rspy.pytest import device_helpers


class FakePyrsDevice:
    """Minimal stand-in for pyrealsense2.device: supports() + get_info() only."""
    def __init__(self, name, connection_type, fw, sn):
        self._info = {
            rs.camera_info.name: name,
            rs.camera_info.connection_type: connection_type,
            rs.camera_info.firmware_version: fw,
            rs.camera_info.serial_number: sn,
        }

    def supports(self, info):
        return self._info.get(info) is not None

    def get_info(self, info):
        return self._info[info]


@pytest.fixture(autouse=True)
def clear_fw_cache():
    device_helpers._fw_version_cache.clear()
    yield
    device_helpers._fw_version_cache.clear()


@pytest.mark.parametrize("name, connection_type, fw", [
    ("D585 Prototype", "GMSL", "7.62.0.0"),      # exactly the minimum
    ("D585 Prototype", "GMSL", "7.62.47274.18338"),
    ("D585 Prototype", "USB", "7.58.46322.15891"),   # not GMSL
    ("D401", "GMSL", "5.17.5.15"),                   # not a D585
])
def test_not_skipped(name, connection_type, fw):
    dev = FakePyrsDevice(name, connection_type, fw, "1")
    device_helpers.require_min_fw_on_d585_gmsl(dev, "feature")


@pytest.mark.parametrize("fw", ["7.58.46322.15891", "7.59.46979.17549", "7.61.47150.0"])
def test_skipped_on_old_fw(fw):
    dev = FakePyrsDevice("D585 Prototype", "GMSL", fw, "2")
    with pytest.raises(pytest.skip.Exception, match="does not meet minimum"):
        device_helpers.require_min_fw_on_d585_gmsl(dev, "feature")


def test_custom_min_version():
    dev = FakePyrsDevice("D585 Prototype", "GMSL", "7.62.47274.18338", "3")
    with pytest.raises(pytest.skip.Exception):
        device_helpers.require_min_fw_on_d585_gmsl(dev, "feature", min_version="7.63.0.0")
