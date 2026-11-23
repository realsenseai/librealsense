# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from unittest.mock import MagicMock

import pyrealsense2 as rs

from .conftest import client

URL = "/api/v1/devices/device1/firmware"
IMAGE = {"content": b"\x00\x01\x02\x03", "headers": {"Content-Type": "application/octet-stream"}}


def _sdk(setup_mock_managers, monkeypatch):
    """The camera's SDK updatable, a mock; and the camera"""
    camera = setup_mock_managers["rs_manager"].cameras["device1"]
    updatable = MagicMock()
    monkeypatch.setattr(rs, "updatable", lambda dev: updatable, raising=False)
    return updatable, camera


def test_check_firmware_compatibility_answers_as_the_sdk(setup_mock_managers, monkeypatch):
    updatable, _ = _sdk(setup_mock_managers, monkeypatch)
    updatable.check_firmware_compatibility.return_value = False
    assert client.post(f"{URL}/check_firmware_compatibility", **IMAGE).json() is False
    updatable.check_firmware_compatibility.assert_called_once_with(bytearray(b"\x00\x01\x02\x03"))


def test_enter_update_state_switches_the_camera(setup_mock_managers, monkeypatch):
    updatable, _ = _sdk(setup_mock_managers, monkeypatch)
    assert client.post(f"{URL}/enter_update_state").status_code == 200
    updatable.enter_update_state.assert_called_once_with()


def test_update_writes_the_image_reporting_each_percent_once(setup_mock_managers, monkeypatch):
    _, camera = _sdk(setup_mock_managers, monkeypatch)

    def update(image, on_progress):
        for tick in range(1001):  # the SDK ticks far more often than each percent
            on_progress(tick / 1000)
    monkeypatch.setattr(camera.dev, "as_update_device", lambda: MagicMock(update=update), raising=False)
    monkeypatch.setattr(camera, "_events", MagicMock())

    assert client.post(f"{URL}/update", **IMAGE).status_code == 200
    progress = [c.args[1]["progress"] for c in camera._events.emit.call_args_list]
    assert len(progress) == 101 and progress[-1] == 1.0


def test_a_camera_tells_whether_it_is_in_recovery_mode(setup_mock_managers):
    assert client.get("/api/v1/devices/device1/is_update_device").json() is False
