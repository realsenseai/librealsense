# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from .conftest import STREAM_CONFIG, client


def test_start_stream(setup_mock_managers):
    response = client.post("/api/v1/devices/device1/stream/start", json=STREAM_CONFIG)
    assert response.status_code == 200

    result = response.json()
    assert result["device_id"] == "device1"
    assert result["is_streaming"] == True
    assert "depth" in result["active_streams"]


def test_stop_stream(setup_mock_managers):
    response = client.post("/api/v1/devices/device1/stream/stop", json=STREAM_CONFIG)
    assert response.status_code == 200

    result = response.json()
    assert result["device_id"] == "device1"
    assert result["is_streaming"] == False


def test_get_stream_status(setup_mock_managers):
    response = client.get("/api/v1/devices/device1/stream/status")
    assert response.status_code == 200

    status = response.json()
    assert status["device_id"] == "device1"
    assert "is_streaming" in status


def test_latest_metadata_is_readable_over_rest(setup_mock_managers, monkeypatch):
    rs_manager = setup_mock_managers["rs_manager"]
    monkeypatch.setattr(rs_manager, "get_latest_metadata", lambda d, s: {"frame_number": 7, "stream": s, "point_cloud": {"vertices": b"x"}})
    body = client.get("/api/v1/devices/device1/stream/metadata?stream=depth").json()
    assert body["frame_number"] == 7 and body["stream"] == "depth" and "point_cloud" not in body
