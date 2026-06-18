# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Tests for rspy.test.find_first_device_or_exit.

The harness (run-unit-tests.py / rspy.devices) registers each connected device
by `camera_info.serial_number` when available, and by `camera_info.firmware_update_id`
when the device is in DFU/recovery mode and the regular serial number isn't exposed.
find_first_device_or_exit has to match against either, otherwise a SN derived from a
recovery-mode device fails the lookup -- which is what test-fw-update.py hit when
two devices were left in recovery on a multi-device rig.
"""

import pytest

rs = pytest.importorskip("pyrealsense2")

from rspy import test as rspy_test


class FakePyrsDevice:
    """Minimal stand-in for pyrealsense2.device: supports() + get_info() only.

    The two camera_info keys we care about for matching are wired up via the
    sn / fwid kwargs. Pass either or both; absent ones return False from supports().
    """
    def __init__(self, sn=None, fwid=None):
        self._sn = sn
        self._fwid = fwid

    def supports(self, info):
        if info == rs.camera_info.serial_number:
            return self._sn is not None
        if info == rs.camera_info.firmware_update_id:
            return self._fwid is not None
        return False

    def get_info(self, info):
        if info == rs.camera_info.serial_number:
            return self._sn
        if info == rs.camera_info.firmware_update_id:
            return self._fwid
        return None


class FakeContext:
    """Minimal stand-in for rs.context(): exposes devices as both list and .size()."""
    def __init__(self, devs):
        self._devs = devs

    @property
    def devices(self):
        return _DeviceList(self._devs)


class _DeviceList(list):
    """list with rs-style .size() method."""
    def size(self):
        return len(self)


@pytest.fixture
def _patch_context(monkeypatch):
    """Replace rs.context with a factory that returns whatever FakeContext we set up."""
    holder = {"ctx": None}

    def _ctx_factory(*args, **kwargs):
        return holder["ctx"]

    monkeypatch.setattr(rs, "context", _ctx_factory)
    return holder


@pytest.fixture
def _silence_log_f(monkeypatch):
    """log.f normally exits. Replace with a raising stub so tests can assert on it."""
    class _LogF(Exception):
        pass

    def _raise(*args, **kwargs):
        raise _LogF(" ".join(str(a) for a in args))

    monkeypatch.setattr(rspy_test.log, "f", _raise)
    return _LogF


class TestFindFirstDeviceOrExit:

    def test_no_serial_returns_first_device(self, _patch_context):
        """Without a serial_number arg, returns devices[0]."""
        d0, d1 = FakePyrsDevice(sn="111"), FakePyrsDevice(sn="222")
        _patch_context["ctx"] = FakeContext([d0, d1])

        dev, ctx = rspy_test.find_first_device_or_exit()
        assert dev is d0

    def test_matches_by_serial_number(self, _patch_context):
        """When serial_number is supplied and a device exposes it, return that device."""
        d_match = FakePyrsDevice(sn="222")
        d_other = FakePyrsDevice(sn="111")
        _patch_context["ctx"] = FakeContext([d_other, d_match])

        dev, _ = rspy_test.find_first_device_or_exit(serial_number="222")
        assert dev is d_match

    def test_matches_by_firmware_update_id_when_serial_not_exposed(self, _patch_context):
        """Recovery-mode case: device doesn't expose camera_info.serial_number, so
        the lookup must fall back to camera_info.firmware_update_id."""
        d_recovery = FakePyrsDevice(sn=None, fwid="204423060494")
        _patch_context["ctx"] = FakeContext([d_recovery])

        dev, _ = rspy_test.find_first_device_or_exit(serial_number="204423060494")
        assert dev is d_recovery

    def test_mixed_recovery_and_normal_devices_each_resolvable(self, _patch_context):
        """Two devices, one normal and one in recovery -- each should resolve from its own ID."""
        d_normal = FakePyrsDevice(sn="111")
        d_recovery = FakePyrsDevice(sn=None, fwid="222-fwid")
        _patch_context["ctx"] = FakeContext([d_normal, d_recovery])

        dev_a, _ = rspy_test.find_first_device_or_exit(serial_number="111")
        assert dev_a is d_normal

        dev_b, _ = rspy_test.find_first_device_or_exit(serial_number="222-fwid")
        assert dev_b is d_recovery

    def test_serial_takes_precedence_over_firmware_update_id(self, _patch_context, _silence_log_f):
        """A device that exposes BOTH should match against serial_number, and must NOT
        also be matchable via its firmware_update_id value -- otherwise a SN that happens
        to collide with another device's fwid would resolve to the wrong handle."""
        d = FakePyrsDevice(sn="sn-value", fwid="fwid-value")
        _patch_context["ctx"] = FakeContext([d])

        # Positive: matches via serial_number.
        dev, _ = rspy_test.find_first_device_or_exit(serial_number="sn-value")
        assert dev is d

        # Negative: searching by the fwid value must NOT find the device, because the
        # SN-exposed branch wins and returns "sn-value", not "fwid-value".
        with pytest.raises(_silence_log_f):
            rspy_test.find_first_device_or_exit(serial_number="fwid-value")

    def test_no_match_calls_log_f(self, _patch_context, _silence_log_f):
        """No device with the requested SN or fwid -> log.f (test fail)."""
        d = FakePyrsDevice(sn="111", fwid="aaa")
        _patch_context["ctx"] = FakeContext([d])

        with pytest.raises(_silence_log_f) as exc_info:
            rspy_test.find_first_device_or_exit(serial_number="999")
        assert "999" in str(exc_info.value)

    def test_empty_context_calls_log_f(self, _patch_context, _silence_log_f):
        """No devices visible at all -> log.f ('No device found')."""
        _patch_context["ctx"] = FakeContext([])

        with pytest.raises(_silence_log_f) as exc_info:
            rspy_test.find_first_device_or_exit()
        assert "No device found" in str(exc_info.value)
