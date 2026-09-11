# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest

import pyrealsense2 as rs
from app.services.manager import record_playback
from .conftest import client

REC = "/api/v1/devices/device1/record"


class _FakeRecorder:
    instances = []

    def __init__(self, path, dev, compress=None):
        self.path, self.dev, self.compress, self.paused, self.closed = path, dev, compress, False, False
        _FakeRecorder.instances.append(self)

    def pause(self):
        self.paused = True

    def resume(self):
        self.paused = False


def _streaming(rs_manager):
    rs_manager.streaming_mode["device1"] = "sensor"
    rs_manager.sensor_streams["device1"] = {
        "device1-sensor-0": {"is_streaming": True, "stream_types": ["depth"], "name": "Depth Sensor", "configs": []},
    }


def test_recording_requires_streaming(setup_mock_managers):
    assert client.post(f"{REC}/start").status_code == 409


def test_record_start_pause_resume_stop(setup_mock_managers, monkeypatch, tmp_path):
    rs_manager = setup_mock_managers["rs_manager"]
    _streaming(rs_manager)
    monkeypatch.setattr(record_playback.rs, "recorder", _FakeRecorder)
    rs_manager.settings.update({"record": {"default_path": str(tmp_path), "compression": "auto"}})

    started = client.post(f"{REC}/start").json()
    assert started["recording"] is True and started["file"].startswith(str(tmp_path)) and started["file"].endswith(".db3")
    assert _FakeRecorder.instances[-1].compress is None  # "auto": the SDK decides
    assert client.get(f"{REC}/").json()["recording"] is True
    assert client.post(f"{REC}/start").status_code == 409

    assert client.post(f"{REC}/pause").json()["paused"] is True
    assert _FakeRecorder.instances[-1].paused is True
    assert client.post(f"{REC}/resume").json()["paused"] is False

    stopped = client.post(f"{REC}/stop").json()
    assert stopped == {"device_id": "device1", "recording": False, "paused": False, "file": started["file"]}
    assert client.post(f"{REC}/stop").status_code == 409


def test_record_compression_setting_is_passed_through(setup_mock_managers, monkeypatch, tmp_path):
    rs_manager = setup_mock_managers["rs_manager"]
    _streaming(rs_manager)
    monkeypatch.setattr(record_playback.rs, "recorder", _FakeRecorder)
    rs_manager.settings.update({"record": {"compression": "never"}})
    client.post(f"{REC}/start", json={"path": str(tmp_path / "x.db3")})
    assert _FakeRecorder.instances[-1].compress is False
    client.post(f"{REC}/stop")


class _FakePlayback:
    """Stands in for rs.playback around a loaded device."""
    by_device = {}

    def __init__(self, dev):
        self.dev = dev
        self.state = self.by_device.setdefault(id(dev), {"status": rs.playback_status.stopped, "pos": 0, "speed": 1.0, "cb": None})

    def file_name(self):
        return self.dev.file

    def current_status(self):
        return self.state["status"]

    def get_position(self):
        return self.state["pos"]

    def get_duration(self):
        return 5_000_000_000

    def resume(self):
        self.state["status"] = rs.playback_status.playing

    def pause(self):
        self.state["status"] = rs.playback_status.paused

    def stop(self):
        self.state["status"] = rs.playback_status.stopped
        self.state["pos"] = 0

    def seek(self, delta):
        assert hasattr(delta, "total_seconds"), "seek takes a timedelta"
        self.state["pos"] = int(delta.total_seconds() * 1e9)

    def set_playback_speed(self, speed):
        self.state["speed"] = speed

    def set_real_time(self, _rt):
        pass

    def set_status_changed_callback(self, cb):
        self.state["cb"] = cb


@pytest.fixture
def recording(setup_mock_managers, monkeypatch, tmp_path):
    from ..mocks.pyrealsense_mock import create_mock_device
    rs_manager = setup_mock_managers["rs_manager"]
    bag = tmp_path / "walk.bag"
    bag.write_bytes(b"not really a bag")
    pb_dev = create_mock_device("rec-serial", "RealSense D455")
    pb_dev.file = str(bag)
    pb_dev.is_playback = lambda: True
    loaded, unloaded = [], []

    class _Ctx:  # the real rs.context refuses monkeypatching
        devices = []

        def load_device(self, path):
            loaded.append(path)
            pb_dev.file = path  # a real playback device reports the file it was opened from
            return pb_dev

        def unload_device(self, path):
            unloaded.append(path)

    rs_manager.ctx = _Ctx()
    monkeypatch.setattr(record_playback.rs, "playback", _FakePlayback)
    _FakePlayback.by_device.clear()
    return {"bag": bag, "dev": pb_dev, "loaded": loaded, "unloaded": unloaded, "rs_manager": rs_manager}


def test_load_registers_a_playback_device_with_its_own_id(recording):
    info = client.post("/api/v1/playback/load", json={"path": str(recording["bag"])}).json()
    assert info["device_id"] == "playback-walk.bag" and info["is_playback"] is True and info["file_name"].endswith("walk.bag")
    assert recording["loaded"] == [str(recording["bag"])]
    assert any(d["device_id"] == "playback-walk.bag" for d in client.get("/api/v1/devices/").json())
    # the recorded camera's serial does not collide with a live camera of the same serial
    assert "rec-serial" not in recording["rs_manager"].devices


def test_load_of_a_missing_file_is_404(recording):
    assert client.post("/api/v1/playback/load", json={"path": "C:/nope/none.bag"}).status_code == 404


def test_transport_actions_drive_the_playback(recording):
    client.post("/api/v1/playback/load", json={"path": str(recording["bag"])})
    url = "/api/v1/playback/playback-walk.bag"
    assert client.get(url).json()["state"] == "stopped"
    assert client.post(url, json={"action": "play"}).json()["state"] == "playing"
    assert client.post(url, json={"action": "pause"}).json()["state"] == "paused"
    assert client.post(url, json={"action": "seek", "value": 2_000_000_000}).json()["position_ns"] == 2_000_000_000
    assert client.post(url, json={"action": "speed", "value": 0.5}).json()["speed"] == 0.5
    assert client.post(url, json={"action": "repeat", "value": 1}).json()["repeat"] is True
    stepped = client.post(url, json={"action": "step", "value": 1}).json()
    assert stepped["state"] == "paused"
    assert abs(stepped["position_ns"] - (2_000_000_000 + 1e9 / 30)) <= 1000  # seek has microsecond granularity
    assert client.post(url, json={"action": "stop"}).json() == {**client.get(url).json(), "state": "stopped", "position_ns": 0}


def test_status_callback_is_forwarded_and_unload_removes_the_device(recording):
    rs_manager = recording["rs_manager"]
    emitted = []
    rs_manager._emit_socket_event = lambda ev, payload: emitted.append((ev, payload))
    client.post("/api/v1/playback/load", json={"path": str(recording["bag"])})

    _FakePlayback.by_device[id(recording["dev"])]["cb"](rs.playback_status.paused)
    assert ("playback_status", {"device_id": "playback-walk.bag", "state": "paused"}) in emitted

    assert client.delete("/api/v1/playback/playback-walk.bag").status_code == 200
    assert recording["unloaded"] == [str(recording["bag"])]
    assert "playback-walk.bag" not in rs_manager.devices
    assert ("devices_changed", {"added": [], "removed": ["playback-walk.bag"]}) in emitted
    assert client.get("/api/v1/playback/playback-walk.bag").status_code == 404


def test_uploaded_recording_lands_in_the_recordings_folder(recording, tmp_path):
    rs_manager = recording["rs_manager"]
    rs_manager.settings.update({"record": {"default_path": str(tmp_path / "recs")}})
    response = client.post("/api/v1/playback/upload", files={"file": ("clip.bag", b"bag bytes", "application/octet-stream")})
    assert response.status_code == 200
    assert (tmp_path / "recs" / "clip.bag").read_bytes() == b"bag bytes"
    assert recording["loaded"][-1] == str(tmp_path / "recs" / "clip.bag")
    assert client.post("/api/v1/playback/upload", files={"file": ("clip.txt", b"x", "text/plain")}).status_code == 400
    files = client.get("/api/v1/playback/files").json()
    assert [f["name"] for f in files] == ["clip.bag"]
