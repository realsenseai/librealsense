# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Test for the no-hub guard added to unit-tests/live/hw-reset/pytest-hub-recycle-imu.py.

The live test now skips (rather than fails) when devices.hub is None, so plain
local runs without an Acroname/Ykush/UniFi don't surface a misleading failure.

The test file uses kebab-case, so we load it via importlib and call the function
directly with a fake device + a stubbed rspy.devices module.
"""

import importlib.util
import types
from pathlib import Path
from unittest.mock import MagicMock

import pytest

try:
    import pyrealsense2 as rs
except ImportError:
    pytestmark = pytest.mark.skip(reason="pyrealsense2 not available")
    rs = None


def _load_test_module():
    path = Path(__file__).resolve().parent.parent / "live" / "hw-reset" / "pytest-hub-recycle-imu.py"
    spec = importlib.util.spec_from_file_location("pytest_hub_recycle_imu", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _fake_dev_with_motion(sn='111'):
    sensor = MagicMock()
    sensor.get_info.return_value = "Motion Module"
    dev = MagicMock()
    dev.query_sensors.return_value = [sensor]
    dev.get_info.return_value = sn
    return dev


def test_hub_none_skips(monkeypatch):
    """No hub anywhere -> skip cleanly (true on Jetson, Windows dev, clean Linux, etc.)."""
    mod = _load_test_module()
    monkeypatch.setattr(mod, 'devices', types.SimpleNamespace(hub=None))
    with pytest.raises(pytest.skip.Exception, match="no hub configured"):
        mod.test_hub_recycle_imu_presence(
            (_fake_dev_with_motion(), MagicMock()), 'nightly'
        )
