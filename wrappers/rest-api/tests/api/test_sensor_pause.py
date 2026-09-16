# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import time

from .conftest import client

SENSOR = "/api/v1/devices/device1/sensors/device1-sensor-0"


def _streaming(rs_manager, paused=False):
    rs_manager.streaming_mode["device1"] = "sensor"
    rs_manager.sensor_streams["device1"] = {
        "device1-sensor-0": {"is_streaming": True, "paused": paused, "stream_types": ["depth"], "name": "Depth Sensor"},
    }


def test_pause_and_resume_flip_the_status_flag(setup_mock_managers):
    _streaming(setup_mock_managers["rs_manager"])

    assert client.get(f"{SENSOR}/status").json()["paused"] is False
    paused = client.post(f"{SENSOR}/pause")
    assert paused.status_code == 200
    assert paused.json()["paused"] is True and paused.json()["is_streaming"] is True
    assert client.get(f"{SENSOR}/status").json()["paused"] is True
    assert client.post(f"{SENSOR}/resume").json()["paused"] is False


def test_sensor_changes_are_announced_to_every_client(setup_mock_managers):
    """Pause and stop (and start, through the same helper) emit `sensor_status`, so a UI that
    did not cause the change (another tab, a calibration job, a lost device) stops showing a
    stale Stop button."""
    rs_manager = setup_mock_managers["rs_manager"]
    _streaming(rs_manager)
    events = []
    rs_manager._emit_socket_event = lambda ev, payload: events.append((ev, payload))

    assert client.post(f"{SENSOR}/pause").status_code == 200
    assert client.post(f"{SENSOR}/stop").status_code == 200

    statuses = [p for ev, p in events if ev == "sensor_status"]
    assert [(s["device_id"], s["sensor_id"]) for s in statuses] == [("device1", "device1-sensor-0")] * 2
    assert [(s["status"]["is_streaming"], s["status"]["paused"]) for s in statuses] == [(True, True), (False, False)]
    assert client.get(f"{SENSOR}/status").json()["is_streaming"] is False


def test_status_names_the_streams_a_client_arriving_mid_stream_must_draw(setup_mock_managers):
    """A page that opens while the camera runs learns what it is running from here; without
    the stream list it shows an idle camera and draws no tile."""
    from app.models.sensor_streaming import SensorStreamConfig
    from app.models.stream import Resolution

    configs = [SensorStreamConfig(stream_type="depth", format="z16", resolution=Resolution(width=848, height=480), framerate=30),
               SensorStreamConfig(stream_type="infrared-1", format="y8", resolution=Resolution(width=848, height=480), framerate=30)]
    setup_mock_managers["rs_manager"].sensor_streams["device1"] = {
        "device1-sensor-0": {"is_streaming": True, "paused": False, "stream_types": ["depth", "infrared-1"],
                             "configs": configs, "name": "Stereo Module"},
    }

    status = client.get(f"{SENSOR}/status").json()

    assert status["is_streaming"] is True
    assert status["stream_types"] == ["depth", "infrared-1"]
    assert [s["stream_type"] for s in status["streams"]] == ["depth", "infrared-1"]
    assert status["resolution"] == {"width": 848, "height": 480} and status["framerate"] == 30


def test_pause_refused_while_not_streaming(setup_mock_managers):
    response = client.post(f"{SENSOR}/pause")
    assert response.status_code == 409


def test_viewer_info_carries_the_server_arrival_time(setup_mock_managers):
    class _Profile:
        def fps(self):
            return 30

        def format(self):
            class _F:
                name = "z16"
            return _F()

        def as_video_stream_profile(self):
            raise RuntimeError("motion")

    class _Frame:
        def get_profile(self):
            return _Profile()

        def supports_frame_metadata(self, _md):
            return False

        def get_timestamp(self):
            return 1.0

        def get_frame_number(self):
            return 7

        def get_frame_timestamp_domain(self):
            class _D:
                name = "global_time"
            return _D()

    before = time.time()
    info = setup_mock_managers["rs_manager"]._build_viewer_info(_Frame())
    assert before <= info["received_at"] <= time.time()


ROI = "/api/v1/devices/device1/sensors/device1-sensor-0/roi"


class _Roi:
    def __init__(self):
        self.min_x, self.min_y, self.max_x, self.max_y = 0, 0, 639, 479


class _RoiSensor:
    def __init__(self):
        self.roi = _Roi()

    def get_region_of_interest(self):
        return self.roi

    def set_region_of_interest(self, roi):
        self.roi = roi


def test_roi_unsupported_on_a_plain_sensor(setup_mock_managers):
    assert client.get(ROI).json() == {"supported": False}
    assert client.put(ROI, json={"min_x": 0, "min_y": 0, "max_x": 10, "max_y": 10}).status_code == 400


def test_roi_roundtrip_normalizes_corners(setup_mock_managers, monkeypatch):
    sensor = setup_mock_managers["rs_manager"].devices["device1"].sensors[0]
    roi_sensor = _RoiSensor()
    monkeypatch.setattr(sensor, "is_roi_sensor", lambda: True, raising=False)
    monkeypatch.setattr(sensor, "as_roi_sensor", lambda: roi_sensor, raising=False)

    assert client.get(ROI).json() == {"supported": True, "min_x": 0, "min_y": 0, "max_x": 639, "max_y": 479}
    body = client.put(ROI, json={"min_x": 300, "min_y": 200, "max_x": 100, "max_y": 50}).json()
    assert body == {"supported": True, "min_x": 100, "min_y": 50, "max_x": 300, "max_y": 200}


def test_starting_an_already_streaming_sensor_is_a_no_op_or_a_restart(setup_mock_managers):
    from app.models.sensor_streaming import SensorStreamConfig
    from app.models.stream import Resolution
    rs_manager = setup_mock_managers["rs_manager"]
    cfg = SensorStreamConfig(stream_type="depth", format="z16", resolution=Resolution(width=640, height=480), framerate=30)
    rs_manager.streaming_mode["device1"] = "sensor"
    rs_manager.sensor_streams["device1"] = {
        "device1-sensor-0": {"is_streaming": True, "paused": False, "stream_types": ["depth"], "name": "Depth Sensor", "configs": [cfg]},
    }
    calls = []
    rs_manager.stop_sensor = lambda d, s: calls.append(("stop", s))

    same = client.post(f"{SENSOR}/start", json={"configs": [cfg.model_dump()]})
    assert same.status_code == 200 and same.json()["is_streaming"] is True
    assert calls == []  # the same configuration: nothing to do

    other = cfg.model_copy(update={"framerate": 15})
    client.post(f"{SENSOR}/start", json={"configs": [other.model_dump()]})
    assert calls == [("stop", "device1-sensor-0")]  # a different one restarts through stop_sensor
