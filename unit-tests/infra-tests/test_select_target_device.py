# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Tests for select_target_device (rspy/pytest/device_helpers.py).

select_target_device is the per-test device picker used by the test_device and
function_scoped_device fixtures in unit-tests/conftest.py. It exists so that on
hub-less multi-device CI rigs (e.g. Jetson with D457 on MIPI and D436 on USB,
both always visible), a test parametrized for one device doesn't silently end
up running against the other one because of enumeration order.

Covered:
- str setup: returns the device whose SN matches.
- None setup: no device parametrization -> falls back to devices_list[0].
- str setup, SN not present in devices_list -> pytest.fail with a useful message.
- list setup (multi-device marker misuse): warns and falls back to devices_list[0].
"""

import pytest

try:
    import pyrealsense2 as rs
except ImportError:
    pytestmark = pytest.mark.skip(reason="pyrealsense2 not available")
    rs = None

from rspy.pytest.device_helpers import select_target_device


class FakePyrsDevice:
    """Minimal stand-in for pyrealsense2.device.

    Only implements supports()/get_info() for camera_info.serial_number (the only
    attribute select_target_device queries). Pass has_sn=False to simulate a device
    that doesn't expose its serial number.
    """
    def __init__(self, sn, has_sn=True):
        self._sn = sn
        self._has_sn = has_sn

    def supports(self, info):
        return info == rs.camera_info.serial_number and self._has_sn

    def get_info(self, info):
        if info == rs.camera_info.serial_number and self._has_sn:
            return self._sn
        return None


class TestSelectTargetDevice:

    def test_picks_matching_sn(self):
        """str setup: the device with the matching SN is returned, regardless of position."""
        d0 = FakePyrsDevice('111')
        d1 = FakePyrsDevice('222')
        d2 = FakePyrsDevice('333')
        assert select_target_device([d0, d1, d2], '222') is d1

    def test_picks_matching_sn_when_not_first(self):
        """str setup: the last device in the list still gets picked when its SN matches."""
        d0 = FakePyrsDevice('111')
        d1 = FakePyrsDevice('222')
        assert select_target_device([d0, d1], '222') is d1

    def test_no_parametrization_falls_back_to_first(self):
        """None setup (no device() marker) -> devices_list[0]."""
        d0 = FakePyrsDevice('111')
        d1 = FakePyrsDevice('222')
        assert select_target_device([d0, d1], None) is d0

    def test_missing_sn_calls_pytest_fail(self):
        """str setup, target SN absent -> pytest.fail with both the missing SN and the visible SNs."""
        d0 = FakePyrsDevice('111')
        d1 = FakePyrsDevice('222')
        with pytest.raises(pytest.fail.Exception) as exc_info:
            select_target_device([d0, d1], '999')
        msg = str(exc_info.value)
        assert '999' in msg, f"missing target SN not surfaced in message: {msg!r}"
        assert '111' in msg and '222' in msg, f"visible SNs not listed in message: {msg!r}"

    def test_devices_without_sn_skipped_during_match(self):
        """str setup: a device that doesn't expose its SN is skipped, not matched."""
        d_no_sn = FakePyrsDevice('111', has_sn=False)
        d_target = FakePyrsDevice('222')
        assert select_target_device([d_no_sn, d_target], '222') is d_target

    def test_list_setup_warns_and_returns_first(self, caplog):
        """list setup means a multi-device marker was used -> warn + fallback to devices_list[0]."""
        d0 = FakePyrsDevice('111')
        d1 = FakePyrsDevice('222')
        with caplog.at_level('WARNING', logger='librealsense'):
            result = select_target_device([d0, d1], ['111', '222'])
        assert result is d0
        assert any('multi-device marker' in r.message for r in caplog.records), \
            f"expected a 'multi-device marker' warning, got: {[r.message for r in caplog.records]}"
