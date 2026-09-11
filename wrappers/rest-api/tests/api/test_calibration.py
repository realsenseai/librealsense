# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import json
import time

from app.services import calibration
from .conftest import client

URL = "/api/v1/devices/device1/calibration"


class _FakeCalibDevice:
    """rs.auto_calibrated_device with a scripted result."""
    health = (0.12, 0.0)
    table = b"new-table"
    calls = []

    def __init__(self, dev):
        self.dev = dev

    def get_calibration_table(self):
        return b"old-table"

    def run_on_chip_calibration(self, request, progress, timeout_ms):
        _FakeCalibDevice.calls.append(("occ", json.loads(request), timeout_ms))
        progress(50.0)
        progress(100.0)
        return _FakeCalibDevice.table, _FakeCalibDevice.health

    def run_tare_calibration(self, ground_truth, request, progress, timeout_ms):
        _FakeCalibDevice.calls.append(("tare", ground_truth, json.loads(request)))
        return b"tare-table", (0.01, -0.02)

    def set_calibration_table(self, table):
        _FakeCalibDevice.calls.append(("set", table))

    def write_calibration(self):
        _FakeCalibDevice.calls.append(("write",))

    def reset_to_factory_calibration(self):
        _FakeCalibDevice.calls.append(("reset",))


def _wait_job(job_id, timeout=5.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        job = client.get(f"/api/v1/jobs/{job_id}").json()
        if job["state"] != "running":
            return job
        time.sleep(0.05)
    raise AssertionError("calibration job did not finish")


def _arm(setup_mock_managers, monkeypatch):
    rs_manager = setup_mock_managers["rs_manager"]
    monkeypatch.setattr(calibration.rs, "auto_calibrated_device", _FakeCalibDevice)
    _FakeCalibDevice.calls = []
    _FakeCalibDevice.table = b"new-table"
    streams = []
    rs_manager.start_sensor = lambda d, s, c: streams.append(("start", s, [x.model_dump() for x in c]))
    rs_manager.stop_sensor = lambda d, s: streams.append(("stop", s))
    return rs_manager, streams


def test_idle_session(setup_mock_managers):
    assert client.get(URL + "/").json()["state"] == "idle"


def test_occ_runs_as_a_job_inside_the_calibration_workspace(setup_mock_managers, monkeypatch):
    rs_manager, streams = _arm(setup_mock_managers, monkeypatch)
    # The device was streaming color; the workspace must stop it and bring it back
    from app.models.sensor_streaming import SensorStreamConfig
    from app.models.stream import Resolution
    color = SensorStreamConfig(stream_type="color", format="rgb8", resolution=Resolution(width=640, height=480), framerate=30)
    rs_manager.sensor_streams["device1"] = {"device1-sensor-1": {"is_streaming": True, "configs": [color]}}

    job = client.post(URL + "/occ", json={"speed": 4, "accuracy": 1}).json()
    assert job["kind"] == "calibration_occ"
    done = _wait_job(job["id"])
    assert done["state"] == "done", done
    assert done["result"]["health"] == [0.12, 0.0] and done["result"]["verdict"] == "good"
    assert done["result"]["active"] == "new" and done["result"]["has_new_table"] is True

    occ = next(c for c in _FakeCalibDevice.calls if c[0] == "occ")
    assert occ[1]["speed"] == 4 and occ[1]["accuracy"] == 1 and occ[1]["calib type"] == 0 and occ[2] == calibration.OCC_TIMEOUT_MS
    assert ("set", b"new-table") in _FakeCalibDevice.calls

    # stop color -> start depth 256x144@90 -> stop it -> restart color
    assert streams[0] == ("stop", "device1-sensor-1")
    assert streams[1][0:2] == ("start", "device1-sensor-0") and streams[1][2][0]["resolution"] == {"width": 256, "height": 144} and streams[1][2][0]["framerate"] == 90
    assert streams[2] == ("stop", "device1-sensor-0")
    assert streams[3][0:2] == ("start", "device1-sensor-1")

    status = client.get(URL + "/").json()
    assert status["state"] == "done" and status["verdict"] == "good"


def test_apply_old_and_keep_and_reset(setup_mock_managers, monkeypatch):
    rs_manager, _ = _arm(setup_mock_managers, monkeypatch)
    _wait_job(client.post(URL + "/occ", json={}).json()["id"])
    _FakeCalibDevice.calls = []

    assert client.post(URL + "/apply", json={"use_new": False}).json()["active"] == "old"
    assert ("set", b"old-table") in _FakeCalibDevice.calls
    assert client.post(URL + "/keep").json()["written"] is True
    assert ("write",) in _FakeCalibDevice.calls

    rs_manager.settings.update({"calibration": {"enable_writing": False}})
    assert client.post(URL + "/keep").status_code == 403
    assert client.post(URL + "/reset_factory").status_code == 403
    rs_manager.settings.update({"calibration": {"enable_writing": True}})
    assert client.post(URL + "/reset_factory").json()["state"] == "idle"
    assert ("reset",) in _FakeCalibDevice.calls


def test_a_calibration_that_does_not_converge_fails_the_job(setup_mock_managers, monkeypatch):
    _arm(setup_mock_managers, monkeypatch)
    _FakeCalibDevice.table = b""
    done = _wait_job(client.post(URL + "/occ", json={}).json()["id"])
    assert done["state"] == "failed" and "converge" in done["error"]
    assert client.get(URL + "/").json()["state"] == "failed"
    assert client.post(URL + "/apply", json={}).status_code == 409


def test_tare_reports_health_in_percent(setup_mock_managers, monkeypatch):
    _arm(setup_mock_managers, monkeypatch)
    done = _wait_job(client.post(URL + "/tare", json={"ground_truth_mm": 1000}).json()["id"])
    assert done["state"] == "done"
    assert done["result"]["health"] == [1.0, -2.0]
    tare = next(c for c in _FakeCalibDevice.calls if c[0] == "tare")
    assert tare[1] == 1000.0
