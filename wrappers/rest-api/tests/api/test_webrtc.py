# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import asyncio
import time

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


class _FakePeer:
    """Just enough of an RTCPeerConnection for the session sweep."""

    def __init__(self, state="connected"):
        self.connectionState = state
        self.closed = False

    async def close(self):
        self.closed = True
        self.connectionState = "closed"


def test_sessions_whose_peer_is_gone_are_dropped(setup_mock_managers):
    """A browser that navigates away or reloads leaves its peer connection behind; every one
    kept alive goes on encoding frames on the event loop until the server crawls."""
    manager = setup_mock_managers["webrtc_manager"]
    now = time.time()
    peers = {
        "live": _FakePeer("connected"),
        "failed": _FakePeer("failed"),
        "unanswered": _FakePeer("new"),
        "fresh-offer": _FakePeer("new"),
    }
    manager.sessions = {
        "live": {"device_id": "device1", "stream_types": ["depth"], "pc": peers["live"], "connected": True, "created_at": now},
        "failed": {"device_id": "device1", "stream_types": ["depth"], "pc": peers["failed"], "connected": True, "created_at": now},
        "unanswered": {"device_id": "device1", "stream_types": ["depth"], "pc": peers["unanswered"], "connected": False,
                       "created_at": now - manager.UNANSWERED_SESSION_S - 5},
        "fresh-offer": {"device_id": "device1", "stream_types": ["depth"], "pc": peers["fresh-offer"], "connected": False, "created_at": now},
    }

    asyncio.run(manager._cleanup_sessions())

    assert sorted(manager.sessions) == ["fresh-offer", "live"]
    assert peers["failed"].closed and peers["unanswered"].closed
    assert not peers["live"].closed


def test_closing_a_session_does_not_deadlock_when_the_peer_reports_it(setup_mock_managers):
    """Closing the peer fires its state-change handler, which closes the session again; the
    lock must already be free or the event loop stops for good."""
    manager = setup_mock_managers["webrtc_manager"]

    class _Reentrant(_FakePeer):
        async def close(self):
            await manager.close_session("reentrant")  # what the handler does
            await super().close()

    peer = _Reentrant()
    manager.sessions = {"reentrant": {"device_id": "device1", "stream_types": ["depth"], "pc": peer,
                                      "connected": True, "created_at": time.time()}}

    async def run():
        return await asyncio.wait_for(manager.close_session("reentrant"), timeout=5)

    assert asyncio.run(run()) is True
    assert manager.sessions == {}
