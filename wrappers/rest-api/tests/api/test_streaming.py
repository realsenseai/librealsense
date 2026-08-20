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
