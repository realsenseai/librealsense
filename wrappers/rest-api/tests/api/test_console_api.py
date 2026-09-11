# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from .conftest import client

DEV = "/api/v1/devices/device1"


def test_logs_backfill_and_clear(setup_mock_managers):
    console = setup_mock_managers["rs_manager"].console
    a = console.add("info", "one")
    console.add("warn", "two")
    assert [e["message"] for e in client.get("/api/v1/logs/").json()] == ["one", "two"]
    assert [e["message"] for e in client.get(f"/api/v1/logs/?after={a['id']}").json()] == ["two"]
    assert client.delete("/api/v1/logs/").status_code == 200
    assert client.get("/api/v1/logs/").json() == []


def test_terminal_raw_hex_over_the_mock_hwm(setup_mock_managers):
    response = client.post(f"{DEV}/terminal", json={"line": "10 00 00 00"})
    assert response.status_code == 200
    assert response.json()["output"].startswith("10 00 00 00")  # the mock echoes the opcode
    entries = client.get("/api/v1/logs/").json()
    assert entries[-1]["source"] == "terminal" and entries[-1]["command"] == "10 00 00 00"


def test_terminal_commands_empty_without_an_xml(setup_mock_managers):
    assert client.get("/api/v1/terminal/commands").json() == []


def test_fw_logs_refused_when_the_device_has_none(setup_mock_managers, monkeypatch):
    rs_manager = setup_mock_managers["rs_manager"]
    dev = rs_manager.devices["device1"]
    monkeypatch.setattr(dev, "is_firmware_logger", lambda: False, raising=False)
    assert client.post(f"{DEV}/fw_logs/start").status_code == 400
    assert client.get(f"{DEV}/fw_logs").json() == {"device_id": "device1", "running": False, "parsed": False}
