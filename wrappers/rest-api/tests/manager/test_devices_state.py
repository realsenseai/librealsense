# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Unit tests for RealSenseManager internals: targeted hot-plug.

Uses stubs/mocks against the real pyrealsense2 enum — no live device needed.
"""

import pytest
import pyrealsense2 as rs
from unittest.mock import MagicMock

from app.services.rs_manager import RealSenseManager


@pytest.fixture(autouse=True)
def _mock_pipeline(monkeypatch):
    """Registering a device gives it a pipeline, and rs.pipeline.set_device takes only a real rs.device."""
    monkeypatch.setattr(rs, "pipeline", MagicMock())


# --- Hot-plug: targeted register / per-device eviction ---

def _make_mock_device(serial):
    info_map = {
        rs.camera_info.serial_number: serial,
        rs.camera_info.name: f"Test Device {serial}",
        rs.camera_info.firmware_version: "5.0.0.0",
        rs.camera_info.physical_port: f"port-{serial}",
        rs.camera_info.usb_type_descriptor: "3.2",
        rs.camera_info.product_id: "0B5C",
        rs.camera_info.firmware_update_id: serial,
    }
    dev = MagicMock()
    dev.supports.return_value = True
    dev.get_info.side_effect = lambda k: info_map.get(k) if k in info_map else (_ for _ in ()).throw(RuntimeError())
    dev.is_metadata_enabled.return_value = True
    dev.is_update_device.return_value = False
    return dev


def test_metadata_enabled_is_reread():
    """Enabling metadata writes the registry while the device stays connected: the next read reports it"""
    mgr = RealSenseManager()
    dev = _make_mock_device("serial-A")
    dev.is_metadata_enabled.return_value = False
    with mgr.lock:
        mgr._add_cameras([dev])
    assert mgr.cameras["serial-A"].is_metadata_enabled() is False
    dev.is_metadata_enabled.return_value = True
    assert mgr.cameras["serial-A"].is_metadata_enabled() is True


def test_a_device_the_sdk_cannot_open_is_skipped():
    mgr = RealSenseManager()
    rs.pipeline.side_effect = RuntimeError("pipeline failed")
    with mgr.lock:
        assert mgr._add_cameras([_make_mock_device("serial-A")]) == []
    assert "serial-A" not in mgr.cameras


# --- Hardware reset ---

def _seed_device(mgr, serial):
    dev = _make_mock_device(serial)
    info_add = MagicMock()
    info_add.get_new_devices.return_value = [dev]
    info_add.was_removed.return_value = False
    mgr._on_devices_changed(info_add)
    return dev


def test_reset_device_resets_it():
    mgr = RealSenseManager()
    dev = _seed_device(mgr, "serial-A")

    mgr.cameras["serial-A"].hardware_reset()
    dev.hardware_reset.assert_called_once()


def test_an_unknown_camera_is_404():
    with pytest.raises(KeyError):  # answered 404
        RealSenseManager().cameras["nope"]


def test_on_devices_changed_remove_evicts_only_target():
    """Survivors keep their entries when one device is unplugged."""
    mgr = RealSenseManager()
    dev_a = _make_mock_device("serial-A")
    dev_b = _make_mock_device("serial-B")

    info_add = MagicMock()
    info_add.get_new_devices.return_value = [dev_a, dev_b]
    info_add.was_removed.return_value = False
    mgr._on_devices_changed(info_add)

    info_remove = MagicMock()
    info_remove.get_new_devices.return_value = []
    info_remove.was_removed.side_effect = lambda d: d is dev_a
    mgr._on_devices_changed(info_remove)

    assert "serial-A" not in mgr.cameras and "serial-B" in mgr.cameras


def test_a_camera_in_recovery_mode_is_listed_by_its_firmware_update_id():
    """No serial number and no sensors in recovery mode: listed so that a client can flash it back"""
    mgr = RealSenseManager()
    dfu = MagicMock()
    dfu.is_update_device.return_value = True
    info_map = {rs.camera_info.firmware_update_id: "UPD-1", rs.camera_info.name: "Intel RealSense D4XX Recovery"}
    dfu.get_info.side_effect = lambda k: info_map.get(k) if k in info_map else (_ for _ in ()).throw(RuntimeError())
    with mgr.lock:
        assert mgr._add_cameras([dfu]) == ["UPD-1"]
    camera = mgr.cameras["UPD-1"]
    assert (camera.info.get("serial_number"), camera.info["firmware_update_id"]) == (None, "UPD-1")
