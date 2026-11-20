# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Test for the no-hub guard of unit-tests/live/hw-reset/pytest-hub-recycle-imu.py.

The module skips (rather than fails) when devices.hub is None, so plain local runs without an
Acroname/Ykush/UniFi don't surface a misleading failure. The guard is a module skipif, so the
camera is never powered for it.

The test file uses kebab-case, so we load it via importlib with rspy.devices.hub patched.
"""

import importlib.util
from pathlib import Path

import pytest
from rspy import devices

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


@pytest.mark.parametrize("hub, skipped", [(None, True), (object(), False)])
def test_skipped_only_without_hub(monkeypatch, hub, skipped):
    """No hub anywhere (Jetson, Windows dev, clean Linux, etc.) -> the module skips cleanly."""
    monkeypatch.setattr(devices, 'hub', hub)
    skipif = next(m for m in _load_test_module().pytestmark if m.name == 'skipif')
    assert skipif.args == (skipped,)
    assert "no hub configured" in skipif.kwargs['reason']
