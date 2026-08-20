# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import numpy as np
from unittest.mock import AsyncMock, MagicMock
from fastapi.testclient import TestClient
from ..mocks.setup_fake_devices import setup_fake_devices
from ..mocks.mock_dependencies import patch_dependencies, DummyOfferStat  # noqa: F401  (autouse fixture)
from ..mocks.pyrealsense_mock import camera_info
from main import app

client = TestClient(app)

STREAM_CONFIG = {
    "configs": [
        {
            "sensor_id": "device1-sensor-0",
            "stream_type": "depth",
            "format": "z16",
            "resolution": {"width": 640, "height": 480},
            "framerate": 30,
        }
    ]
}
WEBRTC_CONFIG = {"device_id": "device1", "stream_types": ["depth"]}
OPTIONS_URL = "/api/v1/devices/device1/sensors/device1-sensor-0/options"
FIRMWARE_URL = "/api/v1/devices/device1/firmware/update_from_file"
SDP = "v=0\r\no=- 0 0 IN IP4 0.0.0.0\r\ns=-\r\nt=0 0\r\n"

FAKE_DEVICES = setup_fake_devices()


def open_session():
    return client.post("/api/v1/webrtc/offer", json=WEBRTC_CONFIG).json()["session_id"]


@pytest.fixture
def setup_mock_managers(patch_dependencies):
    rs_manager = patch_dependencies["rs_manager"]
    webrtc_manager = patch_dependencies["webrtc_manager"]

    def mock_start_stream(device_id, configs, align_to=None, reuse_cache=True):
        rs_manager.active_streams[device_id] = {c.stream_type for c in configs}
        rs_manager.frame_queues[device_id] = {
            c.stream_type: [
                (
                    np.zeros((c.resolution.height, c.resolution.width, 3), dtype=np.uint8),
                    {
                        "timestamp": 12345678,
                        "frame_number": 42,
                        "width": c.resolution.width,
                        "height": c.resolution.height,
                    },
                )
            ]
            for c in configs
        }
        rs_manager.pipelines[device_id] = MagicMock()

        # Shape must match the real start_stream() so the endpoint can subscript result['timings'].
        return {
            "device_id": device_id,
            "is_streaming": True,
            "active_streams": list(rs_manager.active_streams[device_id]),
            "timings": {},
            "config_reused": False,
            "config_signature": "mock-signature",
        }

    def mock_refresh_devices():
        from app.models.device import DeviceInfo

        rs_manager.devices.clear()
        rs_manager.device_infos.clear()
        for dev in FAKE_DEVICES:
            device_id = dev.get_info(camera_info.serial_number)
            rs_manager.devices[device_id] = dev

            sensors = []
            for sensor in dev.sensors:
                try:
                    sensors.append(sensor.get_info(camera_info.name))
                except RuntimeError:
                    pass

            rs_manager.device_infos[device_id] = DeviceInfo(
                device_id=device_id,
                name=dev.get_info(camera_info.name),
                serial_number=device_id,
                firmware_version="1.0.0",
                physical_port="USB",
                usb_type="3.0",
                product_id="001",
                sensors=sensors,
                is_streaming=device_id in rs_manager.pipelines,
            )
        return list(rs_manager.device_infos.values())

    async def mock_create_offer(device_id, stream_types):
        session_id = f"test-session-{device_id}"
        pc = MagicMock()
        pc.getStats = AsyncMock(
            return_value={
                "stat1": DummyOfferStat(type="candidate", id="1234", value=42),
                "stat2": DummyOfferStat(type="track", id="5678", value=99),
            }
        )
        webrtc_manager.sessions[session_id] = {
            "session_id": session_id,
            "device_id": device_id,
            "stream_types": stream_types,
            "connected": False,
            "pc": pc,
        }
        return session_id, {"sdp": SDP, "type": "offer"}

    async def mock_process_answer(session_id, sdp, type):
        return session_id in webrtc_manager.sessions and type == "answer" and bool(sdp)

    async def mock_add_ice_candidate(session_id, candidate, sdpMid, sdpMLineIndex):
        return session_id in webrtc_manager.sessions and bool(candidate)

    async def mock_close_session(session_id):
        webrtc_manager.sessions.pop(session_id, None)
        return True

    rs_manager.start_stream = mock_start_stream
    rs_manager.refresh_devices = mock_refresh_devices
    webrtc_manager.create_offer = mock_create_offer
    webrtc_manager.process_answer = mock_process_answer
    webrtc_manager.add_ice_candidate = mock_add_ice_candidate
    webrtc_manager.close_session = mock_close_session

    rs_manager.refresh_devices()

    return {"rs_manager": rs_manager, "webrtc_manager": webrtc_manager}
