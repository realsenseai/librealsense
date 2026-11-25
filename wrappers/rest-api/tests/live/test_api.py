# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Live rest-api tests: run against a physically connected RealSense device.

Split out of test_api_service.py so the mocked suite there stays runnable on any
machine (GHA) with no hardware attached.
"""

import importlib
import logging
import queue
import sys
import threading

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
    return devices[0]["serial_number"]


class TestRealSenseAPIIntegration:
    """
    Integration tests that work against actual RealSense devices.
    These tests bypass mocking and test the real API functionality.
    """

    @pytest.fixture
    def real_rs_manager(self):
        """Create a real RealSenseManager instance for integration tests"""
        from app.services.rs_manager import RealSenseManager

        manager = RealSenseManager()
        assert manager.cameras, "RealSenseManager sees no devices — hub/USB enumeration likely failed"
        return manager

    def test_get_device_rs(self, real_rs_manager):
        """Test getting device information using real RealSense API"""
        device_id, camera = next(iter(real_rs_manager.cameras.items()))
        assert camera.id == device_id
        assert camera.info["name"] is not None
        assert len(camera.sensors) > 0

    def test_get_sensor_rs(self, real_rs_manager):
        """Test getting sensor information using real RealSense API"""
        device_id = next(iter(real_rs_manager.cameras))

        sensors = [s.info() for s in real_rs_manager.cameras[device_id].sensors]
        assert len(sensors) > 0

        sensor = sensors[0]
        assert sensor.sensor_index is not None
        assert sensor.name is not None
        assert isinstance(sensor.stream_profiles, list)

        retrieved_sensor = real_rs_manager.cameras[device_id].sensors[sensor.sensor_index].info()
        assert retrieved_sensor.sensor_index == sensor.sensor_index
        assert retrieved_sensor.name == sensor.name

    def test_set_option_rs(self, real_rs_manager):
        """Test setting sensor options using real RealSense API"""
        device_id = next(iter(real_rs_manager.cameras))

        writable_option = None
        sensor_index = None
        original_value = None

        for sensor in real_rs_manager.cameras[device_id].sensors:
            for option_id, option in sensor.options().items():
                if not option.read_only and option.min_value != option.max_value:
                    writable_option = option
                    sensor_index = sensor.index
                    original_value = option.current_value
                    break
            if writable_option:
                break

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
            real_rs_manager.cameras[device_id].sensors[sensor_index].set_option(option_id, test_value)

            updated_option = real_rs_manager.cameras[device_id].sensors[sensor_index].option(option_id)

            # For stepped values, check exact match; for continuous, allow small tolerance
            if step > 0:
                assert updated_option.current_value == test_value
            else:
                assert abs(updated_option.current_value - test_value) < 0.01

        finally:
            try:
                real_rs_manager.cameras[device_id].sensors[sensor_index].set_option(option_id, original_value)
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
            for key in ("name", "serial_number"):
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

        for key in ("sensor_index", "name", "stream_profiles", "active_streams"):
            assert key in sensors[0]

    def test_start_sensor_default_profile_rs(self):
        """Start a sensor with its SDK default profile, as listed by the sensors endpoint"""
        client = real_client()
        device_id = first_device_id(client)

        sensor = client.get(f"/api/v1/devices/{device_id}/sensors").json()[0]
        default = next(p for p in sensor["stream_profiles"] if p["default"])
        url = f"/api/v1/devices/{device_id}/sensors/{sensor['sensor_index']}"

        response = client.post(f"{url}/open", json={"profiles": [default]})
        assert response.status_code == 200, response.text
        try:
            response = client.post(f"{url}/start")
            try:
                assert response.status_code == 200, response.text
            finally:
                client.post(f"{url}/stop")
        finally:
            client.post(f"{url}/close")

    def test_start_pipeline_default_profile_rs(self):
        """Start the pipeline with a sensor's SDK default profile, as rs.config.enable_stream takes it"""
        client = real_client()
        device_id = first_device_id(client)

        sensor = client.get(f"/api/v1/devices/{device_id}/sensors").json()[0]
        p = next(p for p in sensor["stream_profiles"] if p["default"])
        config = {"stream": p["stream_type"], "index": p["stream_index"], "width": p["width"],
                  "height": p["height"], "format": p["format"], "fps": p["fps"]}
        url = f"/api/v1/devices/{device_id}/stream"

        response = client.post(f"{url}/start", json=[config])
        try:
            assert response.status_code == 200, response.text
        finally:
            client.post(f"{url}/stop")

    def test_sdk_refuses_sensor_calls_out_of_order_rs(self):
        """The SDK's own order, open -> start -> stop -> close: start before open and close while streaming are 400"""
        client = real_client()
        device_id = first_device_id(client)

        sensor = client.get(f"/api/v1/devices/{device_id}/sensors").json()[0]
        default = next(p for p in sensor["stream_profiles"] if p["default"])
        url = f"/api/v1/devices/{device_id}/sensors/{sensor['sensor_index']}"

        assert client.post(f"{url}/start").status_code == 400
        assert client.post(f"{url}/open", json={"profiles": [default]}).status_code == 200
        try:
            assert client.post(f"{url}/start").status_code == 200
            try:
                assert client.post(f"{url}/close").status_code == 400
            finally:
                client.post(f"{url}/stop")
        finally:
            client.post(f"{url}/close")

    def test_options_endpoint_rs(self):
        """Test the options endpoints with real devices"""
        client = real_client()
        device_id = first_device_id(client)

        sensor_index = client.get(f"/api/v1/devices/{device_id}/sensors").json()[0]["sensor_index"]

        response = client.get(f"/api/v1/devices/{device_id}/sensors/{sensor_index}/options")
        assert response.status_code == 200

        options = response.json()
        assert isinstance(options, dict)
        assert len(options) > 0

        option_id, option = next(iter(options.items()))
        for key in ("current_value", "min_value", "max_value"):
            assert key in option

        response = client.get(f"/api/v1/devices/{device_id}/sensors/{sensor_index}/options/{option_id}")
        assert response.status_code == 200
        assert response.json() == option

    def test_sdk_reports_an_option_write_with_its_new_value_rs(self):
        """A sensor option written is reported to the clients by the SDK's options watcher, with the new value"""
        client = real_client()
        device_id = first_device_id(client)
        sensors = client.get(f"/api/v1/devices/{device_id}/sensors").json()
        index = next(s["sensor_index"] for s in sensors
                     if "exposure" in client.get(f"/api/v1/devices/{device_id}/sensors/{s['sensor_index']}/options").json())
        url = f"/api/v1/devices/{device_id}/sensors/{index}/options"
        exposure = client.get(f"{url}/exposure").json()
        value = exposure["min_value"] if exposure["current_value"] != exposure["min_value"] else exposure["max_value"]

        with client.websocket_connect("/api/v1/events") as events:
            received = queue.Queue()

            def read():  # the socket gives no timeout of its own
                try:
                    while True:
                        received.put(events.receive_json())
                except Exception:  # closed at the test's end
                    pass
            threading.Thread(target=read, daemon=True).start()
            try:
                assert client.put(f"{url}/exposure", json={"value": value}).status_code == 200
                message = received.get(timeout=5)  # the SDK reads the options about once a second
                assert message["event"] == "options_changed" and message["group"] == f"sensors/{index}/options" and message["options"]["exposure"] == value
            finally:
                client.put(f"{url}/exposure", json={"value": exposure["current_value"]})

    def test_filters_endpoint_rs(self):
        """Every filter route of a sensor with filters: each read agrees with the list, and a write reads back"""
        client = real_client()
        device_id = first_device_id(client)
        url = next(u for u in (f"/api/v1/devices/{device_id}/sensors/{s['sensor_index']}/filters"
                               for s in client.get(f"/api/v1/devices/{device_id}/sensors").json())
                   if client.get(f"{u}/").json())

        filters = client.get(f"{url}/").json()
        name, options = next((n, f["options"]) for n, f in filters.items() if f["options"])
        assert client.get(f"{url}/{name}/").json() == options
        field, option = next((f, o) for f, o in options.items() if not o["read_only"])
        assert client.get(f"{url}/{name}/{field}").json() == option

        assert filters[name]["enabled"] is False
        assert client.get(f"{url}/{name}/enabled").json() is False
        assert client.put(f"{url}/{name}/enabled", json={"value": True}).status_code == 200
        try:
            assert client.get(f"{url}/{name}/enabled").json() is True
        finally:
            client.put(f"{url}/{name}/enabled", json={"value": False})

        assert client.put(f"{url}/{name}/{field}", json={"value": option["current_value"]}).status_code == 200
        assert client.get(f"{url}/{name}/{field}").json() == option

    def test_colorizer_and_advanced_reads_rs(self):
        """Each colorizer option, advanced group and advanced control reads alone as the full read has it"""
        client = real_client()
        url = f"/api/v1/devices/{first_device_id(client)}"

        colorizer = client.get(f"{url}/colorizer/").json()
        field, option = next(iter(colorizer.items()))
        assert client.get(f"{url}/colorizer/{field}").json() == option
        assert client.get(f"{url}/colorizer/nope").status_code == 404

        if client.get(url).json().get("advanced_mode") != "YES":
            pytest.skip("advanced mode is off")
        controls = client.get(f"{url}/advanced_mode/controls/").json()
        group, fields = next(iter(controls.items()))
        assert client.get(f"{url}/advanced_mode/controls/{group}/").json() == fields
        field, control = next(iter(fields.items()))
        assert client.get(f"{url}/advanced_mode/controls/{group}/{field}").json() == control
        assert client.get(f"{url}/advanced_mode/controls/nope/").status_code == 404
        assert client.put(f"{url}/advanced_mode/controls/{group}/nope", json={"value": 1}).status_code == 404
        assert client.put(f"{url}/advanced_mode/controls/nope/{field}", json={"value": 1}).status_code == 404

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

        device_id = devices[0]["serial_number"]
        log.info("Testing HWM command on device: %s", device_id)

        # GVD (Get Version and Date) is opcode 0x10 — safe read-only command
        hwm_response = client.post(f"/api/v1/devices/{device_id}/hwm", json={"opcode": 0x10})
        body = hwm_response.json()
        parsed = self._parse_gvd_d400(body if isinstance(body, list) else [])
        log.info("HWM response: status=%s parsed=%s", hwm_response.status_code, parsed)

        if hwm_response.status_code == 400:
            pytest.skip(f"Device {device_id} does not support hardware monitor commands")

        assert hwm_response.status_code == 200
        assert isinstance(body, list)
        assert len(body) > 0
