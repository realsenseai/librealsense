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
