# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Unit tests for RealSenseManager internals: targeted hot-plug.

Uses stubs/mocks against the real pyrealsense2 enum — no live device needed.
"""

import pytest
import pyrealsense2 as rs
from unittest.mock import MagicMock

from app.services.rs_manager import RealSenseManager, RealSenseError


@pytest.fixture(autouse=True)
def _mock_pipeline(monkeypatch):
    """Registering a device gives it a pipeline, and rs.pipeline.set_device takes only a real rs.device."""
    monkeypatch.setattr(rs, "pipeline", MagicMock())


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
    mgr = RealSenseManager()

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


# --- Hardware reset ---

def _seed_device(mgr, serial):
    dev = _make_mock_device(serial)
    info_add = MagicMock()
    info_add.get_new_devices.return_value = [dev]
    info_add.was_removed.return_value = False
    mgr._on_devices_changed(info_add)
    return dev


def test_reset_device_success_evicts_state_and_emits():
    mgr = RealSenseManager()
    dev = _seed_device(mgr, "serial-A")
    emitted = []
    mgr.events.emit = lambda ev, payload: emitted.append((ev, payload))

    assert mgr.reset_device("serial-A") is True

    dev.hardware_reset.assert_called_once()
    assert "serial-A" not in mgr.devices
    assert "serial-A" not in mgr.device_infos
    assert any(ev == "devices_changed" and p == {"added": [], "removed": ["serial-A"]} for ev, p in emitted)


def test_reset_device_not_found_raises_404():
    import pytest as _pytest
    mgr = RealSenseManager()

    with _pytest.raises(RealSenseError) as exc:
        mgr.reset_device("nope")
    assert exc.value.status_code == 404


def test_reset_device_hardware_error_restores_state_and_raises_500():
    """Reset failure: device handle is still valid + device still plugged in,
    so the cache entry is restored and an `added` event is emitted before
    propagating the 500 to the client."""
    import pytest as _pytest
    mgr = RealSenseManager()
    dev = _seed_device(mgr, "serial-A")
    dev.hardware_reset.side_effect = RuntimeError("boom")
    emitted = []
    mgr.events.emit = lambda ev, payload: emitted.append((ev, payload))

    with _pytest.raises(RealSenseError) as exc:
        mgr.reset_device("serial-A")
    assert exc.value.status_code == 500
    assert "boom" in exc.value.detail
    # Restored.
    assert "serial-A" in mgr.devices
    assert "serial-A" in mgr.device_infos
    # Both events emitted, in order.
    assert emitted == [
        ("devices_changed", {"added": [], "removed": ["serial-A"]}),
        ("devices_changed", {"added": ["serial-A"], "removed": []}),
    ]


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

    assert "serial-A" not in mgr.devices
    assert "serial-A" not in mgr.device_infos
    assert "serial-B" in mgr.devices
    assert "serial-B" in mgr.device_infos
