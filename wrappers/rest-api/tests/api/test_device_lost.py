# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import time

from .conftest import client


def _wait(predicate, timeout=5.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if predicate():
            return True
        time.sleep(0.05)
    return predicate()


def test_lost_device_is_dropped_announced_and_picked_up_again(setup_mock_managers):
    rs_manager = setup_mock_managers["rs_manager"]
    from ..mocks.setup_fake_devices import setup_fake_devices

    class _Ctx:
        def __init__(self, devices):
            self.devices = devices

    rs_manager.ctx = _Ctx(setup_fake_devices())
    events = []
    rs_manager._emit_socket_event = lambda ev, payload: events.append((ev, payload))
    stopped = []
    rs_manager.stop_sensor = lambda d, s: stopped.append(s)
    rs_manager.sensor_streams["device1"] = {"device1-sensor-0": {"is_streaming": True, "configs": []}}
    old_handle = rs_manager.devices["device1"]

    rs_manager.device_lost("device1", "The video recording device is no longer present")
    assert _wait(lambda: ("devices_changed", {"added": ["device1"], "removed": []}) in events, timeout=8)

    assert stopped == ["device1-sensor-0"]
    assert ("devices_changed", {"added": [], "removed": ["device1"]}) in events
    assert rs_manager.devices["device1"] is not old_handle  # a fresh handle from enumeration
    assert client.get("/api/v1/devices/device1").status_code == 200
    messages = [e["message"] for e in rs_manager.console.since()]
    assert any("Lost contact" in m for m in messages) and any("is back" in m for m in messages)


def test_lost_device_recovery_runs_once_and_ignores_unknown_ids(setup_mock_managers):
    rs_manager = setup_mock_managers["rs_manager"]
    rs_manager.device_lost("nope", "whatever")  # unknown: nothing happens
    assert "nope" not in getattr(rs_manager, "_lost_in_progress", set())
    assert rs_manager.is_device_lost_error(RuntimeError("hr returned: HResult 0xc00d3ea2: The video recording device is no longer present."))
    assert not rs_manager.is_device_lost_error(RuntimeError("Frame did not arrive in time!"))


def test_unreadable_advanced_controls_are_a_503(setup_mock_managers, monkeypatch):
    rs_manager = setup_mock_managers["rs_manager"]

    def boom(device_id):
        raise RuntimeError("QueryInterface returned: HResult 0x80004002: No such interface supported")

    monkeypatch.setattr(rs_manager, "get_advanced_controls", boom)
    response = client.get("/api/v1/devices/device1/advanced_mode/controls/")
    assert response.status_code == 503
    assert "did not answer" in response.json()["detail"]
