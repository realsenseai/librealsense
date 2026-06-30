# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import logging
import pytest
import numpy as np
from unittest.mock import AsyncMock, MagicMock, patch
from fastapi.testclient import TestClient
from fastapi import FastAPI
from .setup_fake_devices import setup_fake_devices
from .mock_dependencies import patch_dependencies, DummyOfferStat
from .pyrealsense_mock import camera_info
from main import app

log = logging.getLogger(__name__)

# Create test client
client = TestClient(app)

class TestRealSenseAPI:
    @classmethod
    def setup_class(cls):
        mock_setup = setup_fake_devices()
        cls.fake_devices = mock_setup["devices"]
        cls.depth_sensors = mock_setup["depth_sensors"]
        cls.color_sensors = mock_setup["color_sensors"]


    @pytest.fixture
    def setup_mock_managers(self, patch_dependencies):
        rs_manager = patch_dependencies["rs_manager"]
        webrtc_manager = patch_dependencies["webrtc_manager"]

        # Configure mock RealSenseManager
        def mock_start_stream(device_id, configs, align_to=None, reuse_cache=True):
            # Set up mock frame queues
            rs_manager.active_streams[device_id] = set(
                [config.stream_type for config in configs]
            )
            rs_manager.frame_queues[device_id] = {}

            for config in configs:
                stream_type = config.stream_type
                rs_manager.frame_queues[device_id][stream_type] = []

                # Add a fake frame to the queue
                if stream_type.lower() == "depth":
                    # Create a fake depth frame (grayscale)
                    frame = np.random.randint(
                        0,
                        255,
                        (config.resolution.height, config.resolution.width),
                        dtype=np.uint8,
                    )
                    # Colorize it to simulate depth colorization
                    frame_rgb = np.zeros(
                        (config.resolution.height, config.resolution.width, 3),
                        dtype=np.uint8,
                    )
                    frame_rgb[:, :, 2] = (
                        frame  # Add to blue channel for colorization effect
                    )
                    frame_data = frame_rgb
                elif stream_type.lower() == "color":
                    # Create a fake color frame
                    frame_data = np.random.randint(
                        0,
                        255,
                        (config.resolution.height, config.resolution.width, 3),
                        dtype=np.uint8,
                    )
                else:
                    # Generic frame for other stream types
                    frame_data = np.random.randint(
                        0,
                        255,
                        (config.resolution.height, config.resolution.width, 3),
                        dtype=np.uint8,
                    )

                metadata = {
                    "timestamp": 12345678,
                    "frame_number": 42,
                    "width": config.resolution.width,
                    "height": config.resolution.height,
                }

                rs_manager.frame_queues[device_id][stream_type].append(
                    (frame_data, metadata)
                )

            # Update pipelines to indicate streaming
            rs_manager.pipelines[device_id] = MagicMock()

            # Return a dict matching the real start_stream() shape so the endpoint
            # can subscript result['timings'] without a TypeError.
            return {
                "device_id": device_id,
                "is_streaming": True,
                "active_streams": list(rs_manager.active_streams[device_id]),
                "timings": {},
                "config_reused": False,
                "config_signature": "mock-signature",
            }

        def mock_refresh_devices():
            # Populate the devices dictionary with our mock devices
            rs_manager.devices.clear()
            rs_manager.device_infos.clear()
            for dev in self.fake_devices:
                device_id = dev.get_info(camera_info.serial_number)
                rs_manager.devices[device_id] = dev

                # Create device info
                name = dev.get_info(camera_info.name)
                firmware_version = "1.0.0"  # Mock firmware version

                # Get sensors
                sensors = []
                for sensor in dev.sensors:
                    try:
                        sensor_name = sensor.get_info(camera_info.name)
                        sensors.append(sensor_name)
                    except RuntimeError:
                        pass

                # Create device info object
                device_info = {
                    "device_id": device_id,
                    "name": name,
                    "serial_number": device_id,
                    "firmware_version": firmware_version,
                    "physical_port": "USB",
                    "usb_type": "3.0",
                    "product_id": "001",
                    "sensors": sensors,
                    "is_streaming": device_id in rs_manager.pipelines,
                }

                # Convert to DeviceInfo model
                from app.models.device import DeviceInfo

                rs_manager.device_infos[device_id] = DeviceInfo(**device_info)

            return list(rs_manager.device_infos.values())

        # Replace the actual methods
        rs_manager.start_stream = mock_start_stream
        rs_manager.refresh_devices = mock_refresh_devices

        # Configure mock WebRTCManager
        async def mock_create_offer(device_id, stream_types):
            session_id = f"test-session-{device_id}"

            mock_stats_dict = {
                "stat1": DummyOfferStat(type="candidate", id="1234", value=42),
                "stat2": DummyOfferStat(type="track", id="5678", value=99),
            }
            pc = MagicMock()
            pc.getStats = AsyncMock(return_value=mock_stats_dict)

            webrtc_manager.sessions[session_id] = {
                "session_id": session_id,
                "device_id": device_id,
                "stream_types": stream_types,
                "connected": False,
                "pc": pc,
            }

            # Mock offer
            offer = {
                "sdp": "v=0\r\no=- 0 0 IN IP4 0.0.0.0\r\ns=-\r\nt=0 0\r\n",
                "type": "offer",
            }

            return session_id, offer

        webrtc_manager.create_offer = mock_create_offer

        # Add methods to handle webrtc operations
        async def mock_process_answer(session_id, sdp, type):
            if session_id not in webrtc_manager.sessions or type != "answer" or not sdp:
                return False
            return True

        async def mock_add_ice_candidate(session_id, candidate, sdpMid, sdpMLineIndex):
            if session_id not in webrtc_manager.sessions or not candidate:
                return False
            return True

        async def mock_close_session(session_id):
            if session_id in webrtc_manager.sessions:
                del webrtc_manager.sessions[session_id]
            return True

        webrtc_manager.process_answer = mock_process_answer
        webrtc_manager.add_ice_candidate = mock_add_ice_candidate
        webrtc_manager.close_session = mock_close_session

        # Populate with initial data
        rs_manager.refresh_devices()

        return {"rs_manager": rs_manager, "webrtc_manager": webrtc_manager}

    # ----- Tests for the RealSense API -----

    def test_get_devices(self, setup_mock_managers):
        # Test the /devices endpoint
        response = client.get("/api/v1/devices")
        assert response.status_code == 200

        devices = response.json()
        assert len(devices) == 2
        assert devices[0]["name"] == "Test Device 1"
        assert devices[1]["name"] == "Test Device 2"

    def test_get_device_by_id(self, setup_mock_managers):
        # Test the /devices/{device_id} endpoint
        response = client.get("/api/v1/devices/device1")
        assert response.status_code == 200

        device = response.json()
        assert device["device_id"] == "device1"
        assert device["name"] == "Test Device 1"

        # Test with non-existent device
        response = client.get("/api/v1/devices/nonexistent")
        assert response.status_code == 404

    def test_get_sensors(self, setup_mock_managers):
        # Test the /devices/{device_id}/sensors endpoint
        response = client.get("/api/v1/devices/device1/sensors")
        assert response.status_code == 200

        sensors = response.json()
        assert len(sensors) == 2
        assert sensors[0]["type"] in ["Depth Sensor", "RGB Camera"]
        assert sensors[1]["type"] in ["Depth Sensor", "RGB Camera"]

    def test_get_sensor_by_id(self, setup_mock_managers):
        # Test the /devices/{device_id}/sensors/{sensor_id} endpoint
        response = client.get("/api/v1/devices/device1/sensors/device1-sensor-0")
        assert response.status_code == 200

        sensor = response.json()
        assert sensor["sensor_id"] == "device1-sensor-0"

        # Test with non-existent sensor
        response = client.get("/api/v1/devices/device1/sensors/nonexistent")
        assert response.status_code == 404

    def test_get_sensor_options(self, setup_mock_managers):
        # Test the /devices/{device_id}/sensors/{sensor_id}/options endpoint
        response = client.get("/api/v1/devices/device1/sensors/device1-sensor-0/options")
        assert response.status_code == 200

        options = response.json()
        assert len(options) > 0

    def test_get_option_by_id(self, setup_mock_managers):
        # Test the /devices/{device_id}/sensors/{sensor_id}/options/{option_id} endpoint
        # First get available options
        response = client.get("/api/v1/devices/device1/sensors/device1-sensor-0/options")
        options = response.json()
        option_id = options[0]["option_id"]

        response = client.get(
            f"/api/v1/devices/device1/sensors/device1-sensor-0/options/{option_id}"
        )
        assert response.status_code == 200

        option = response.json()
        assert option["option_id"] == option_id

    def test_set_option(self, setup_mock_managers):
        # Test the /devices/{device_id}/sensors/{sensor_id}/options/{option_id} PUT endpoint
        # First get available options
        response = client.get("/api/v1/devices/device1/sensors/device1-sensor-0/options")
        options = response.json()
        option_id = options[0]["option_id"]

        response = client.put(
            f"/api/v1/devices/device1/sensors/device1-sensor-0/options/{option_id}",
            json={"value": 0.5},
        )
        assert response.status_code == 200

    def test_start_stream(self, setup_mock_managers):
        # Test the /devices/{device_id}/stream POST endpoint
        stream_config = {
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

        response = client.post("/api/v1/devices/device1/stream/start", json=stream_config)
        assert response.status_code == 200

        result = response.json()
        assert result["device_id"] == "device1"
        assert result["is_streaming"] == True
        assert "depth" in result["active_streams"]

    def test_stop_stream(self, setup_mock_managers):
        # Test the /devices/{device_id}/stream DELETE endpoint
        # First start streaming
        stream_config = {
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
        response = client.post("/api/v1/devices/device1/stream/stop", json=stream_config)

        assert response.status_code == 200

        result = response.json()
        assert result["device_id"] == "device1"
        assert result["is_streaming"] == False

    def test_get_stream_status(self, setup_mock_managers):
        # Test the /devices/{device_id}/stream GET endpoint
        response = client.get("/api/v1/devices/device1/stream/status")
        assert response.status_code == 200

        status = response.json()
        assert status["device_id"] == "device1"
        assert "is_streaming" in status

    # ----- Tests for the firmware update_from_file endpoint -----

    def test_update_firmware_from_file_happy_path(self, patch_dependencies):
        rs_manager = patch_dependencies["rs_manager"]
        rs_manager.update_firmware_from_bytes = MagicMock(
            return_value={
                "device_id": "device1",
                "progress": 1.0,
                "firmware_version": "1.2.3",
                "status": "success",
            }
        )
        files = {"file": ("D4XX_FW.bin", b"\x00\x01\x02\x03", "application/octet-stream")}
        response = client.post(
            "/api/v1/devices/device1/firmware/update_from_file", files=files
        )
        assert response.status_code == 200
        assert response.json()["status"] == "success"
        # Manager called with device_id + bytes payload
        args, _ = rs_manager.update_firmware_from_bytes.call_args
        assert args[0] == "device1"
        assert args[1] == b"\x00\x01\x02\x03"

    def test_update_firmware_from_file_rejects_non_bin_extension(self, patch_dependencies):
        rs_manager = patch_dependencies["rs_manager"]
        rs_manager.update_firmware_from_bytes = MagicMock()
        files = {"file": ("firmware.txt", b"hello", "text/plain")}
        response = client.post(
            "/api/v1/devices/device1/firmware/update_from_file", files=files
        )
        assert response.status_code == 400
        assert ".bin" in response.json()["detail"]
        rs_manager.update_firmware_from_bytes.assert_not_called()

    def test_update_firmware_from_file_rejects_empty(self, patch_dependencies):
        rs_manager = patch_dependencies["rs_manager"]
        rs_manager.update_firmware_from_bytes = MagicMock()
        files = {"file": ("empty.bin", b"", "application/octet-stream")}
        response = client.post(
            "/api/v1/devices/device1/firmware/update_from_file", files=files
        )
        assert response.status_code == 400
        assert "empty" in response.json()["detail"].lower()
        rs_manager.update_firmware_from_bytes.assert_not_called()

    def test_update_firmware_from_file_rejects_oversize(self, patch_dependencies):
        from app.api.endpoints import firmware as firmware_module

        rs_manager = patch_dependencies["rs_manager"]
        rs_manager.update_firmware_from_bytes = MagicMock()
        # Temporarily shrink the cap so we don't have to allocate 64 MiB just to test the path.
        original_cap = firmware_module.MAX_FW_UPLOAD_BYTES
        firmware_module.MAX_FW_UPLOAD_BYTES = 16
        try:
            files = {"file": ("big.bin", b"X" * 32, "application/octet-stream")}
            response = client.post(
                "/api/v1/devices/device1/firmware/update_from_file", files=files
            )
        finally:
            firmware_module.MAX_FW_UPLOAD_BYTES = original_cap
        assert response.status_code == 413
        rs_manager.update_firmware_from_bytes.assert_not_called()

    def test_update_firmware_from_file_propagates_sdk_error(self, patch_dependencies):
        from app.services.rs_manager import RealSenseError

        rs_manager = patch_dependencies["rs_manager"]
        rs_manager.update_firmware_from_bytes = MagicMock(
            side_effect=RealSenseError(status_code=400, detail="Firmware is not compatible")
        )
        files = {"file": ("bad.bin", b"\xff" * 8, "application/octet-stream")}
        response = client.post(
            "/api/v1/devices/device1/firmware/update_from_file", files=files
        )
        assert response.status_code == 400
        assert response.json()["detail"] == "Firmware is not compatible"

    # ----- Tests for the WebRTC API -----

    @pytest.mark.asyncio
    async def test_create_webrtc_offer(self, setup_mock_managers):
        # First start streaming
        stream_config = {
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
        client.post("/api/v1/devices/device1/stream", json=stream_config)

        # Test the /webrtc/offer POST endpoint
        webrtc_config = {"device_id": "device1", "stream_types": ["depth"]}

        response = client.post("/api/v1/webrtc/offer", json=webrtc_config)
        assert response.status_code == 200

        result = response.json()
        assert "session_id" in result
        assert "sdp"
        assert "type"
        assert result["session_id"] == "test-session-device1"
        assert result["type"] == "offer"

    @pytest.mark.asyncio
    async def test_process_webrtc_answer(self, setup_mock_managers):
        # First create offer
        webrtc_config = {"device_id": "device1", "stream_types": ["depth"]}
        response = client.post("/api/v1/webrtc/offer", json=webrtc_config)
        session_id = response.json()["session_id"]

        # Test the /webrtc/sessions/{session_id}/answer POST endpoint
        answer = {
            "session_id": session_id,
            "sdp": "v=0\r\no=- 0 0 IN IP4 0.0.0.0\r\ns=-\r\nt=0 0\r\n",
            "type": "answer",
        }

        response = client.post(f"/api/v1/webrtc/answer", json=answer)
        assert response.status_code == 200
        assert response.json()["success"] == True

    @pytest.mark.asyncio
    async def test_add_ice_candidate(self, setup_mock_managers):
        # First create offer
        webrtc_config = {"device_id": "device1", "stream_types": ["depth"]}
        response = client.post("/api/v1/webrtc/offer", json=webrtc_config)
        session_id = response.json()["session_id"]

        # Test the /webrtc/sessions/{session_id}/ice POST endpoint
        ice_candidate = {
            "session_id": session_id,
            "candidate": "candidate:0 1 UDP 2122260223 192.168.1.1 49152 typ host",
            "sdpMid": "0",
            "sdpMLineIndex": 0,
        }

        response = client.post(f"/api/v1/webrtc/ice-candidates", json=ice_candidate)
        assert response.status_code == 200
        assert response.json()["success"] == True

    @pytest.mark.asyncio
    async def test_get_webrtc_session(self, setup_mock_managers):
        # First create offer
        webrtc_config = {"device_id": "device1", "stream_types": ["depth"]}
        response = client.post("/api/v1/webrtc/offer", json=webrtc_config)
        session_id = response.json()["session_id"]

        # Test the /webrtc/sessions/{session_id} GET endpoint
        response = client.get(f"/api/v1/webrtc/sessions/{session_id}")
        assert response.status_code == 200

        result = response.json()
        assert result["session_id"] == session_id
        assert result["device_id"] == "device1"
        assert "depth" in result["stream_types"]

    # ----- Tests for the HWM API -----

    def test_hwm_command_basic(self, setup_mock_managers):
        """POST /devices/{id}/hwm with a minimal request returns 200 and a response list."""
        response = client.post(
            "/api/v1/devices/device1/hwm",
            json={"opcode": 0xA6},
        )
        assert response.status_code == 200
        body = response.json()
        assert body["device_id"] == "device1"
        assert isinstance(body["response"], list)
        # Mock echoes opcode (0xA6 = 166) as little-endian uint32 in the first 4 bytes.
        assert body["response"] == [0xA6, 0, 0, 0]

    def test_hwm_command_with_params(self, setup_mock_managers):
        """Params and data payload are accepted without error."""
        response = client.post(
            "/api/v1/devices/device1/hwm",
            json={
                "opcode": 0x14,
                "param1": 1,
                "param2": 0xC0DE,
                "param3": 0,
                "param4": 0,
                "data": [0x01, 0x02, 0x03],
            },
        )
        assert response.status_code == 200
        body = response.json()
        assert body["device_id"] == "device1"
        assert isinstance(body["response"], list)

    def test_hwm_command_unknown_device(self, setup_mock_managers):
        """POST to an unknown device_id returns 404."""
        response = client.post(
            "/api/v1/devices/no-such-device/hwm",
            json={"opcode": 0xA6},
        )
        assert response.status_code == 404

    def test_hwm_command_unsupported_device(self, setup_mock_managers):
        """POST to a device that lacks debug_protocol extension returns 400."""
        from .pyrealsense_mock import device as MockDevice
        rs_manager = setup_mock_managers["rs_manager"]

        # Plain device (not a debug_protocol subclass) does not expose the extension.
        no_debug_device = MockDevice(serial_number="no-debug", name="Limited Device")
        rs_manager.devices["no-debug"] = no_debug_device

        response = client.post(
            "/api/v1/devices/no-debug/hwm",
            json={"opcode": 0xA6},
        )
        assert response.status_code == 400
        assert "does not support" in response.json()["detail"].lower()

    def test_hwm_firmware_error(self, setup_mock_managers):
        """When firmware echoes back a non-matching opcode the endpoint returns 500."""
        from .pyrealsense_mock import debug_protocol as MockDebugDevice
        rs_manager = setup_mock_managers["rs_manager"]

        # fw_error_code differs from opcode 0xA6 so the opcode check must fire.
        error_device = MockDebugDevice(
            serial_number="fw-error",
            name="Error Device",
            fw_error_code=0x00000009,
        )
        rs_manager.devices["fw-error"] = error_device

        response = client.post(
            "/api/v1/devices/fw-error/hwm",
            json={"opcode": 0xA6},
        )
        assert response.status_code == 500
        detail = response.json()["detail"]
        assert "error code" in detail.lower()
        assert "0x00000009" in detail  # returned error code
        assert "0x000000A6" in detail  # expected opcode echo

    def test_hwm_response_too_short(self, setup_mock_managers):
        """When the firmware response is shorter than 4 bytes the endpoint returns 500."""
        from .pyrealsense_mock import debug_protocol as MockDebugDevice
        rs_manager = setup_mock_managers["rs_manager"]

        short_device = MockDebugDevice(
            serial_number="short-resp",
            name="Short Response Device",
            short_hwm_response=True,
        )
        rs_manager.devices["short-resp"] = short_device

        response = client.post(
            "/api/v1/devices/short-resp/hwm",
            json={"opcode": 0xA6},
        )
        assert response.status_code == 500
        assert "too short" in response.json()["detail"].lower()

    def test_hwm_no_deadlock_on_unknown_device(self, patch_dependencies):
        """send_hwm_command must not deadlock when the device is absent and refresh_devices is called.

        Uses the real refresh_devices (not the mock) so that its internal lock
        acquisition actually happens.  With the buggy implementation the call
        would block forever; the 2-second timeout makes that a hard failure.
        """
        import threading
        from app.core.errors import RealSenseError

        rs_manager = patch_dependencies["rs_manager"]
        rs_manager.devices.clear()
        rs_manager.device_infos.clear()

        caught = []

        def _call():
            try:
                rs_manager.send_hwm_command("no-such-device", opcode=0xA6)
            except RealSenseError as e:
                caught.append(e)

        t = threading.Thread(target=_call, daemon=True)
        t.start()
        t.join(timeout=2.0)

        assert not t.is_alive(), "send_hwm_command deadlocked — refresh_devices was called while holding self.lock"
        assert caught and caught[0].status_code == 404

    @pytest.mark.asyncio
    async def test_close_webrtc_session(self, setup_mock_managers):
        # First create offer
        webrtc_config = {"device_id": "device1", "stream_types": ["depth"]}
        response = client.post("/api/v1/webrtc/offer", json=webrtc_config)
        session_id = response.json()["session_id"]

        # Test the /webrtc/sessions/{session_id} DELETE endpoint
        response = client.delete(f"/api/v1/webrtc/sessions/{session_id}")
        assert response.status_code == 200
        assert response.json()["success"] == True

        # Verify session is closed
        response = client.get(f"/api/v1/webrtc/sessions/{session_id}")
        assert response.status_code == 404

    # ----- /system/enable-metadata -----

    def test_enable_metadata_noop_on_non_windows(self):
        with patch("app.api.endpoints.system.platform.system", return_value="Linux"):
            response = client.post("/api/v1/system/enable-metadata")
        assert response.status_code == 200
        body = response.json()
        assert body["status"] == "noop"
        assert "Windows-only" in body["note"]

    def test_enable_metadata_windows_ok(self):
        with patch("app.api.endpoints.system.platform.system", return_value="Windows"), \
             patch("app.api.endpoints.system.subprocess.run") as mock_run:
            mock_run.return_value.returncode = 0
            response = client.post("/api/v1/system/enable-metadata")
        assert response.status_code == 200
        assert response.json()["status"] == "ok"

    def test_enable_metadata_windows_declined(self):
        with patch("app.api.endpoints.system.platform.system", return_value="Windows"), \
             patch("app.api.endpoints.system.subprocess.run") as mock_run:
            mock_run.return_value.returncode = 1223
            response = client.post("/api/v1/system/enable-metadata")
        assert response.status_code == 200
        assert response.json()["status"] == "declined"

    def test_enable_metadata_windows_failure(self):
        with patch("app.api.endpoints.system.platform.system", return_value="Windows"), \
             patch("app.api.endpoints.system.subprocess.run") as mock_run:
            mock_run.return_value.returncode = 99
            response = client.post("/api/v1/system/enable-metadata")
        assert response.status_code == 500
        assert "exit 99" in response.json()["detail"]

    def test_enable_metadata_windows_timeout(self):
        import subprocess as _subprocess
        with patch("app.api.endpoints.system.platform.system", return_value="Windows"), \
             patch("app.api.endpoints.system.subprocess.run",
                   side_effect=_subprocess.TimeoutExpired(cmd="powershell.exe", timeout=120)):
            response = client.post("/api/v1/system/enable-metadata")
        assert response.status_code == 504


class TestRealSenseAPIIntegration:
    """
    Integration tests that work against actual RealSense devices.
    These tests bypass mocking and test the real API functionality.
    """

    @classmethod
    def setup_class(cls):
        # Create a separate test client that doesn't use mocked dependencies
        from main import app
        from fastapi.testclient import TestClient

        # Clear any existing mock patches for these tests
        cls.real_client = TestClient(app)

    @pytest.fixture
    def real_rs_manager(self):
        """Create a real RealSenseManager instance for integration tests"""
        from app.services.socketio import sio
        from app.services.rs_manager import RealSenseManager

        # Create a real RealSenseManager (not mocked)
        manager = RealSenseManager(sio)
        assert manager.get_devices(), "RealSenseManager sees no devices — hub/USB enumeration likely failed"
        return manager

    def test_get_device_rs(self, real_rs_manager):
        """Test getting device information using real RealSense API"""
        # Get real devices
        devices = real_rs_manager.get_devices()


        # Test getting the first device
        device = devices[0]
        assert device.device_id is not None
        assert device.name is not None
        assert device.serial_number is not None
        assert isinstance(device.sensors, list)
        assert len(device.sensors) > 0

        # Test getting device by ID
        retrieved_device = real_rs_manager.get_device(device.device_id)
        assert retrieved_device.device_id == device.device_id
        assert retrieved_device.name == device.name
        assert retrieved_device.serial_number == device.serial_number

    def test_get_sensor_rs(self, real_rs_manager):
        """Test getting sensor information using real RealSense API"""
        # Get real devices
        devices = real_rs_manager.get_devices()

        device = devices[0]
        device_id = device.device_id

        # Get sensors for the device
        sensors = real_rs_manager.get_sensors(device_id)
        assert len(sensors) > 0

        # Test getting the first sensor
        sensor = sensors[0]
        assert sensor.sensor_id is not None
        assert sensor.name is not None
        assert sensor.type is not None
        assert isinstance(sensor.supported_stream_profiles, list)
        assert isinstance(sensor.options, list)

        # Test getting sensor by ID
        retrieved_sensor = real_rs_manager.get_sensor(device_id, sensor.sensor_id)
        assert retrieved_sensor.sensor_id == sensor.sensor_id
        assert retrieved_sensor.name == sensor.name
        assert retrieved_sensor.type == sensor.type

    def test_set_option_rs(self, real_rs_manager):
        """Test setting sensor options using real RealSense API"""
        # Get real devices
        devices = real_rs_manager.get_devices()

        device = devices[0]
        device_id = device.device_id

        # Get sensors for the device
        sensors = real_rs_manager.get_sensors(device_id)

        # Find a sensor with writable options
        writable_option = None
        sensor_id = None
        original_value = None

        for sensor in sensors:
            for option in sensor.options:
                if not option.read_only and option.min_value != option.max_value:
                    writable_option = option
                    sensor_id = sensor.sensor_id
                    original_value = option.current_value
                    break
            if writable_option:
                break

        # Test setting option to a different value
        option_id = writable_option.option_id

        # Calculate a safe test value within the range
        min_val = writable_option.min_value
        max_val = writable_option.max_value
        step = writable_option.step

        # Choose a value different from current, respecting step if > 0
        if step > 0:
            test_value = min_val + step
            if test_value == original_value and (min_val + 2 * step) <= max_val:
                test_value = min_val + 2 * step
        else:
            # For continuous values, use midpoint
            test_value = (min_val + max_val) / 2
            if abs(test_value - original_value) < 0.001:  # Too close to original
                test_value = min_val + (max_val - min_val) * 0.75

        # Ensure test value is within bounds
        test_value = max(min_val, min(max_val, test_value))

        try:
            # Set the option
            success = real_rs_manager.set_sensor_option(device_id, sensor_id, option_id, test_value)
            assert success == True

            # Verify the option was set by reading it back
            updated_option = real_rs_manager.get_sensor_option(device_id, sensor_id, option_id)

            # For stepped values, check exact match; for continuous, allow small tolerance
            if step > 0:
                assert updated_option.current_value == test_value
            else:
                assert abs(updated_option.current_value - test_value) < 0.01

        finally:
            # Restore original value
            try:
                real_rs_manager.set_sensor_option(device_id, sensor_id, option_id, original_value)
            except Exception:
                # If restoration fails, don't fail the test
                pass
    # Additional integration tests for API endpoints
    def test_devices_endpoint_rs(self):
        """Test the /api/devices endpoint with real devices"""
        # Temporarily bypass mocking by creating a new app instance
        import importlib
        import sys

        # Remove mock patches from the dependencies module
        if 'app.api.dependencies' in sys.modules:
            importlib.reload(sys.modules['app.api.dependencies'])

        from main import app
        from fastapi.testclient import TestClient

        real_client = TestClient(app)

        response = real_client.get("/api/v1/devices")

        if response.status_code == 500:
            pytest.skip("No RealSense devices connected or RealSense library issue")

        assert response.status_code == 200
        devices = response.json()

        if devices:  # Only test if devices are connected
            assert isinstance(devices, list)
            device = devices[0]
            assert "device_id" in device
            assert "name" in device
            assert "serial_number" in device
            assert "sensors" in device

    def test_sensors_endpoint_rs(self):
        """Test the /api/devices/{device_id}/sensors endpoint with real devices"""
        import importlib
        import sys

        # Remove mock patches from the dependencies module
        if 'app.api.dependencies' in sys.modules:
            importlib.reload(sys.modules['app.api.dependencies'])

        from main import app
        from fastapi.testclient import TestClient

        real_client = TestClient(app)

        # First get devices
        response = real_client.get("/api/v1/devices")
        assert response.status_code == 200, f"/api/v1/devices returned {response.status_code}: {response.text}"

        devices = response.json()
        assert devices, "/api/v1/devices returned no devices — hub/USB enumeration likely failed"
        device_id = devices[0]["device_id"]

        # Test sensors endpoint
        response = real_client.get(f"/api/v1/devices/{device_id}/sensors")
        assert response.status_code == 200

        sensors = response.json()
        assert isinstance(sensors, list)
        assert len(sensors) > 0

        sensor = sensors[0]
        assert "sensor_id" in sensor
        assert "name" in sensor
        assert "type" in sensor
        assert "supported_stream_profiles" in sensor
        assert "options" in sensor

    def test_options_endpoint_rs(self):
        """Test the options endpoints with real devices"""
        import importlib
        import sys

        # Remove mock patches from the dependencies module
        if 'app.api.dependencies' in sys.modules:
            importlib.reload(sys.modules['app.api.dependencies'])

        from main import app
        from fastapi.testclient import TestClient

        real_client = TestClient(app)

        # First get devices
        response = real_client.get("/api/v1/devices")
        assert response.status_code == 200, f"/api/v1/devices returned {response.status_code}: {response.text}"

        devices = response.json()
        assert devices, "/api/v1/devices returned no devices — hub/USB enumeration likely failed"

        device_id = devices[0]["device_id"]

        # Get sensors
        response = real_client.get(f"/api/v1/devices/{device_id}/sensors")
        sensors = response.json()
        sensor_id = sensors[0]["sensor_id"]

        # Test options endpoint
        response = real_client.get(f"/api/v1/devices/{device_id}/sensors/{sensor_id}/options")
        assert response.status_code == 200

        options = response.json()
        assert isinstance(options, list)
        assert len(options) > 0

        option = options[0]
        assert "option_id" in option
        assert "name" in option
        assert "current_value" in option
        assert "min_value" in option
        assert "max_value" in option

        # Test getting specific option
        option_id = option["option_id"]
        response = real_client.get(f"/api/v1/devices/{device_id}/sensors/{sensor_id}/options/{option_id}")
        assert response.status_code == 200

        retrieved_option = response.json()
        assert retrieved_option["option_id"] == option_id

    @staticmethod
    def _parse_gvd_d400(data):
        """Parse the first 6 fields of a D400-series GVD response."""
        if len(data) < 70:
            return {"raw": data}
        return {
            "version":           data[4],
            "gvd_version":       data[6],
            "fw_version":        f"{data[19]}.{data[18]}.{data[17]}.{data[16]}",
            "is_camera_locked":  bool(data[29]),
            "module_serial":     "".join(f"{b:02X}" for b in data[52:58]),
            "module_asic_serial":"".join(f"{b:02X}" for b in data[68:74]),
        }

    def test_hwm_command_gvd_rs(self):
        """Test sending a hardware monitor command (GVD opcode) to a real device."""
        import importlib
        import sys

        if 'app.api.dependencies' in sys.modules:
            importlib.reload(sys.modules['app.api.dependencies'])

        from main import app
        from fastapi.testclient import TestClient

        real_client = TestClient(app)

        response = real_client.get("/api/v1/devices")
        if response.status_code == 500:
            pytest.skip("No RealSense devices connected or RealSense library issue")

        devices = response.json()
        if not devices:
            pytest.skip("No RealSense devices connected")

        device_id = devices[0]["device_id"]
        log.info("Testing HWM command on device: %s", device_id)

        # GVD (Get Version and Date) is opcode 0x10 — safe read-only command
        hwm_response = real_client.post(
            f"/api/v1/devices/{device_id}/hwm",
            json={"opcode": 0x10},
        )
        body = hwm_response.json()
        parsed = self._parse_gvd_d400(body.get("response", []))
        log.info("HWM response: status=%s parsed=%s", hwm_response.status_code, parsed)

        if hwm_response.status_code == 400:
            pytest.skip(f"Device {device_id} does not support hardware monitor commands")

        assert hwm_response.status_code == 200
        assert body["device_id"] == device_id
        assert isinstance(body["response"], list)
        assert len(body["response"]) > 0