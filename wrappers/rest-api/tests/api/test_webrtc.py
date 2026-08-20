# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from .conftest import SDP, WEBRTC_CONFIG, client, open_session


def test_create_webrtc_offer(setup_mock_managers):
    response = client.post("/api/v1/webrtc/offer", json=WEBRTC_CONFIG)
    assert response.status_code == 200

    result = response.json()
    assert result["session_id"] == "test-session-device1"
    assert result["type"] == "offer"
    assert result["sdp"]


def test_process_webrtc_answer(setup_mock_managers):
    answer = {"session_id": open_session(), "sdp": SDP, "type": "answer"}

    response = client.post("/api/v1/webrtc/answer", json=answer)
    assert response.status_code == 200
    assert response.json()["success"] == True


def test_add_ice_candidate(setup_mock_managers):
    ice_candidate = {
        "session_id": open_session(),
        "candidate": "candidate:0 1 UDP 2122260223 192.168.1.1 49152 typ host",
        "sdpMid": "0",
        "sdpMLineIndex": 0,
    }

    response = client.post("/api/v1/webrtc/ice-candidates", json=ice_candidate)
    assert response.status_code == 200
    assert response.json()["success"] == True


def test_get_webrtc_session(setup_mock_managers):
    session_id = open_session()

    response = client.get(f"/api/v1/webrtc/sessions/{session_id}")
    assert response.status_code == 200

    result = response.json()
    assert result["session_id"] == session_id
    assert result["device_id"] == "device1"
    assert "depth" in result["stream_types"]


def test_close_webrtc_session(setup_mock_managers):
    session_id = open_session()

    response = client.delete(f"/api/v1/webrtc/sessions/{session_id}")
    assert response.status_code == 200
    assert response.json()["success"] == True

    assert client.get(f"/api/v1/webrtc/sessions/{session_id}").status_code == 404
