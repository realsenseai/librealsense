# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Live rest-api tests: run against a physically connected RealSense device.

Split out of test_api_service.py so the mocked suite there stays runnable on any
machine (GHA) with no hardware attached.
"""

import importlib
import logging
import sys

import pytest

log = logging.getLogger(__name__)


def real_client():
    """A TestClient whose dependencies module has been reloaded, dropping the mocked-suite patches."""
    if 'app.api.dependencies' in sys.modules:
        importlib.reload(sys.modules['app.api.dependencies'])

    from main import app
    from fastapi.testclient import TestClient

    return TestClient(app)


def first_device_id(client):
    response = client.get("/api/v1/devices")
    assert response.status_code == 200, f"/api/v1/devices returned {response.status_code}: {response.text}"

    devices = response.json()
    assert devices, "/api/v1/devices returned no devices — hub/USB enumeration likely failed"
    return devices[0]["device_id"]


class TestRealSenseAPIIntegration:
    """
    Integration tests that work against actual RealSense devices.
    These tests bypass mocking and test the real API functionality.
    """

    @pytest.fixture
    def real_rs_manager(self):
        """Create a real RealSenseManager instance for integration tests"""
        from app.services.socketio import sio
        from app.services.rs_manager import RealSenseManager

        manager = RealSenseManager(sio)
        assert manager.get_devices(), "RealSenseManager sees no devices — hub/USB enumeration likely failed"
        return manager

    def test_get_device_rs(self, real_rs_manager):
        """Test getting device information using real RealSense API"""
        device = real_rs_manager.get_devices()[0]
        assert device.device_id is not None
        assert device.name is not None
        assert device.serial_number is not None
        assert isinstance(device.sensors, list)
        assert len(device.sensors) > 0

        retrieved_device = real_rs_manager.get_device(device.device_id)
        assert retrieved_device.device_id == device.device_id
        assert retrieved_device.name == device.name
        assert retrieved_device.serial_number == device.serial_number

    def test_get_sensor_rs(self, real_rs_manager):
        """Test getting sensor information using real RealSense API"""
        device_id = real_rs_manager.get_devices()[0].device_id

        sensors = real_rs_manager.get_sensors(device_id)
        assert len(sensors) > 0

        sensor = sensors[0]
        assert sensor.sensor_id is not None
        assert sensor.name is not None
        assert sensor.type is not None
        assert isinstance(sensor.supported_stream_profiles, list)
        assert isinstance(sensor.options, list)

        retrieved_sensor = real_rs_manager.get_sensor(device_id, sensor.sensor_id)
        assert retrieved_sensor.sensor_id == sensor.sensor_id
        assert retrieved_sensor.name == sensor.name
        assert retrieved_sensor.type == sensor.type

    def test_set_option_rs(self, real_rs_manager):
        """Test setting sensor options using real RealSense API"""
        device_id = real_rs_manager.get_devices()[0].device_id

        writable_option = None
        sensor_id = None
        original_value = None

        for sensor in real_rs_manager.get_sensors(device_id):
            for option in sensor.options:
                if not option.read_only and option.min_value != option.max_value:
                    writable_option = option
                    sensor_id = sensor.sensor_id
                    original_value = option.current_value
                    break
            if writable_option:
                break

        option_id = writable_option.option_id
        min_val = writable_option.min_value
        max_val = writable_option.max_value
        step = writable_option.step

        # Pick a value that differs from the current one, respecting step if > 0.
        if step > 0:
            test_value = min_val + step
            if test_value == original_value and (min_val + 2 * step) <= max_val:
                test_value = min_val + 2 * step
        else:
            test_value = (min_val + max_val) / 2
            if abs(test_value - original_value) < 0.001:
                test_value = min_val + (max_val - min_val) * 0.75

        test_value = max(min_val, min(max_val, test_value))

        try:
            success = real_rs_manager.set_sensor_option(device_id, sensor_id, option_id, test_value)
            assert success == True

            updated_option = real_rs_manager.get_sensor_option(device_id, sensor_id, option_id)

            # For stepped values, check exact match; for continuous, allow small tolerance
            if step > 0:
                assert updated_option.current_value == test_value
            else:
                assert abs(updated_option.current_value - test_value) < 0.01

        finally:
            try:
                real_rs_manager.set_sensor_option(device_id, sensor_id, option_id, original_value)
            except Exception:
                pass

    def test_devices_endpoint_rs(self):
        """Test the /api/devices endpoint with real devices"""
        response = real_client().get("/api/v1/devices")

        if response.status_code == 500:
            pytest.skip("No RealSense devices connected or RealSense library issue")

        assert response.status_code == 200
        devices = response.json()

        if devices:  # Only test if devices are connected
            assert isinstance(devices, list)
            for key in ("device_id", "name", "serial_number", "sensors"):
                assert key in devices[0]

    def test_sensors_endpoint_rs(self):
        """Test the /api/devices/{device_id}/sensors endpoint with real devices"""
        client = real_client()
        device_id = first_device_id(client)

        response = client.get(f"/api/v1/devices/{device_id}/sensors")
        assert response.status_code == 200

        sensors = response.json()
        assert isinstance(sensors, list)
        assert len(sensors) > 0

        for key in ("sensor_id", "name", "type", "supported_stream_profiles", "options"):
            assert key in sensors[0]

    def test_options_endpoint_rs(self):
        """Test the options endpoints with real devices"""
        client = real_client()
        device_id = first_device_id(client)

        sensor_id = client.get(f"/api/v1/devices/{device_id}/sensors").json()[0]["sensor_id"]

        response = client.get(f"/api/v1/devices/{device_id}/sensors/{sensor_id}/options")
        assert response.status_code == 200

        options = response.json()
        assert isinstance(options, list)
        assert len(options) > 0

        option = options[0]
        for key in ("option_id", "name", "current_value", "min_value", "max_value"):
            assert key in option

        option_id = option["option_id"]
        response = client.get(f"/api/v1/devices/{device_id}/sensors/{sensor_id}/options/{option_id}")
        assert response.status_code == 200
        assert response.json()["option_id"] == option_id

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
        client = real_client()

        response = client.get("/api/v1/devices")
        if response.status_code == 500:
            pytest.skip("No RealSense devices connected or RealSense library issue")

        devices = response.json()
        if not devices:
            pytest.skip("No RealSense devices connected")

        device_id = devices[0]["device_id"]
        log.info("Testing HWM command on device: %s", device_id)

        # GVD (Get Version and Date) is opcode 0x10 — safe read-only command
        hwm_response = client.post(f"/api/v1/devices/{device_id}/hwm", json={"opcode": 0x10})
        body = hwm_response.json()
        parsed = self._parse_gvd_d400(body.get("response", []))
        log.info("HWM response: status=%s parsed=%s", hwm_response.status_code, parsed)

        if hwm_response.status_code == 400:
            pytest.skip(f"Device {device_id} does not support hardware monitor commands")

        assert hwm_response.status_code == 200
        assert body["device_id"] == device_id
        assert isinstance(body["response"], list)
        assert len(body["response"]) > 0
