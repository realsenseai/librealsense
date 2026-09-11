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
