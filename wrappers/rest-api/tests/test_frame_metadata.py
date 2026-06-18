# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Unit tests for RealSenseManager internals: frame-metadata cache + targeted hot-plug.

Uses stubs/mocks against the real pyrealsense2 enum — no live device needed.
"""

import pyrealsense2 as rs
from unittest.mock import MagicMock

from app.services.rs_manager import RealSenseManager


_ALL_MD = list(rs.frame_metadata_value.__members__.values())


class _StubFrame:
    def __init__(self, supported_set, *, profile_uid=1, frame_number=2):
        self.supported_set = set(supported_set)
        self._profile_uid = profile_uid
        self._frame_number = frame_number

    def supports_frame_metadata(self, md):
        return md in self.supported_set

    def get_frame_metadata(self, md):
        return int(md)

    def get_frame_number(self):
        return self._frame_number

    def get_profile(self):
        outer = self
        class _P:
            def unique_id(self):
                return outer._profile_uid
        return _P()


def test_returns_only_supported_keys():
    mgr = RealSenseManager(MagicMock())
    supported = _ALL_MD[:3]
    frame = _StubFrame(supported)

    attrs = mgr._get_frame_metadata(frame, "dev-A")

    assert set(attrs.keys()) == {md.name for md in supported}


def test_cache_is_per_profile_uid():
    mgr = RealSenseManager(MagicMock())
    supported_a = _ALL_MD[:2]
    supported_b = _ALL_MD[2:5]
    frame_a = _StubFrame(supported_a, profile_uid=1)
    frame_b = _StubFrame(supported_b, profile_uid=2)

    attrs_a = mgr._get_frame_metadata(frame_a, "dev-A")
    attrs_b = mgr._get_frame_metadata(frame_b, "dev-A")

    assert set(attrs_a.keys()) == {md.name for md in supported_a}
    assert set(attrs_b.keys()) == {md.name for md in supported_b}


def test_cache_is_per_device():
    """Same profile_uid on different devices must not share the supported-md cache."""
    mgr = RealSenseManager(MagicMock())
    supported_a = _ALL_MD[:2]
    supported_b = _ALL_MD[2:5]
    # Same profile_uid=1, different devices.
    frame_a = _StubFrame(supported_a, profile_uid=1)
    frame_b = _StubFrame(supported_b, profile_uid=1)

    attrs_a = mgr._get_frame_metadata(frame_a, "dev-A")
    attrs_b = mgr._get_frame_metadata(frame_b, "dev-B")

    assert set(attrs_a.keys()) == {md.name for md in supported_a}
    assert set(attrs_b.keys()) == {md.name for md in supported_b}
    # Sanity: cache entries are kept separately per device.
    assert "dev-A" in mgr._supported_md_by_profile
    assert "dev-B" in mgr._supported_md_by_profile
    assert mgr._supported_md_by_profile["dev-A"][1] != mgr._supported_md_by_profile["dev-B"][1]


def test_first_frame_does_not_populate_cache():
    """Cache build is deferred to frame #2 because delta-metadata (e.g. actual_fps)
    is not available on the first frame."""
    mgr = RealSenseManager(MagicMock())
    frame_first = _StubFrame(_ALL_MD[:2], profile_uid=7, frame_number=1)

    attrs = mgr._get_frame_metadata(frame_first, "dev-A")

    assert attrs == {}
    assert "dev-A" not in mgr._supported_md_by_profile


# --- Hot-plug: targeted register / per-device eviction ---

def _make_mock_sensor(name="Stereo Module"):
    sensor = MagicMock()
    sensor.get_info.side_effect = lambda k: name if k == rs.camera_info.name else (_ for _ in ()).throw(RuntimeError())
    return sensor


def _make_mock_device(serial):
    info_map = {
        rs.camera_info.serial_number: serial,
        rs.camera_info.name: f"Test Device {serial}",
        rs.camera_info.firmware_version: "5.0.0.0",
        rs.camera_info.physical_port: f"port-{serial}",
        rs.camera_info.usb_type_descriptor: "3.2",
        rs.camera_info.product_id: "0B5C",
    }
    dev = MagicMock()
    dev.supports.return_value = True
    dev.get_info.side_effect = lambda k: info_map.get(k) if k in info_map else (_ for _ in ()).throw(RuntimeError())
    dev.sensors = [_make_mock_sensor()]
    dev.is_metadata_enabled.return_value = True
    return dev


def test_register_new_device_is_atomic_on_sensors_raise():
    """If sensor enumeration raises, the exception bubbles up but no partial state is published."""
    import pytest as _pytest
    mgr = RealSenseManager(MagicMock())

    class _ExplodingSensorList:
        def __iter__(self):
            raise RuntimeError("sensor enumeration failed")

    dev = _make_mock_device("serial-A")
    dev.sensors = _ExplodingSensorList()

    with mgr.lock:
        with _pytest.raises(RuntimeError):
            mgr._register_new_device(dev)

    assert "serial-A" not in mgr.devices
    assert "serial-A" not in mgr.device_infos


def test_on_devices_changed_remove_evicts_only_target():
    """Survivors keep their entries when one device is unplugged."""
    mgr = RealSenseManager(MagicMock())
    mgr.metadata_socket_server = MagicMock()
    dev_a = _make_mock_device("serial-A")
    dev_b = _make_mock_device("serial-B")

    info_add = MagicMock()
    info_add.get_new_devices.return_value = [dev_a, dev_b]
    info_add.was_removed.return_value = False
    mgr._on_devices_changed(info_add)

    # Seed the per-device frame-metadata cache.
    mgr._supported_md_by_profile["serial-A"] = {1: ["x"]}
    mgr._supported_md_by_profile["serial-B"] = {2: ["y"]}

    info_remove = MagicMock()
    info_remove.get_new_devices.return_value = []
    info_remove.was_removed.side_effect = lambda d: d is dev_a
    mgr._on_devices_changed(info_remove)

    assert "serial-A" not in mgr.devices
    assert "serial-A" not in mgr.device_infos
    assert "serial-A" not in mgr._supported_md_by_profile
    assert "serial-B" in mgr.devices
    assert "serial-B" in mgr.device_infos
    assert mgr._supported_md_by_profile["serial-B"] == {2: ["y"]}
