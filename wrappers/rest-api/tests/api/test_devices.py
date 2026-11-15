# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from unittest.mock import MagicMock

from .conftest import FAKE_DEVICES, OPTIONS_URL, client


def test_get_devices(setup_mock_managers):
    response = client.get("/api/v1/devices")
    assert response.status_code == 200

    devices = response.json()
    assert len(devices) == 2
    assert devices[0]["name"] == "Test Device 1"
    assert devices[1]["name"] == "Test Device 2"


def test_get_device_by_id(setup_mock_managers):
    response = client.get("/api/v1/devices/device1")
    assert response.status_code == 200

    device = response.json()
    assert device["device_id"] == "device1"
    assert device["name"] == "Test Device 1"

    assert client.get("/api/v1/devices/nonexistent").status_code == 404


def test_get_sensors(setup_mock_managers):
    response = client.get("/api/v1/devices/device1/sensors")
    assert response.status_code == 200

    sensors = response.json()
    assert len(sensors) == 2
    assert sensors[0]["type"] in ["Depth Sensor", "RGB Camera"]
    assert sensors[1]["type"] in ["Depth Sensor", "RGB Camera"]


def test_get_sensor_by_id(setup_mock_managers):
    response = client.get("/api/v1/devices/device1/sensors/device1-sensor-0")
    assert response.status_code == 200
    assert response.json()["sensor_id"] == "device1-sensor-0"

    assert client.get("/api/v1/devices/device1/sensors/nonexistent").status_code == 404


def test_sensor_lists_each_profile(setup_mock_managers):
    profiles = client.get("/api/v1/devices/device1/sensors/device1-sensor-0").json()["supported_stream_profiles"]
    assert [(p["width"], p["height"], p["fps"], p["default"]) for p in profiles] == [
        (640, 480, 30, False),
        (640, 480, 60, False),
        (1280, 720, 30, True),
        (1280, 720, 60, False),
    ]
    assert {(p["stream_type"], p["stream_index"], p["format"]) for p in profiles} == {("depth", 0, "z16")}


def test_start_sensor_opens_listed_profile(setup_mock_managers):
    url = "/api/v1/devices/device1/sensors/device1-sensor-0"
    profile = client.get(url).json()["supported_stream_profiles"][3]
    assert client.post(f"{url}/start", json={"profiles": [profile]}).status_code == 200
    sensor = FAKE_DEVICES[0].sensors[0]
    assert sensor.opened_profiles == [sensor.get_stream_profiles()[3]]
    status_url = "/api/v1/devices/device1/stream/status"
    assert client.get(status_url).json()["active_streams"] == ["depth"]
    assert client.post(f"{url}/stop").status_code == 200
    assert client.get(status_url).json()["active_streams"] == []


def test_refresh_keeps_a_sensor_streaming(setup_mock_managers):
    rs_manager = setup_mock_managers["rs_manager"]
    url = "/api/v1/devices/device1/sensors/device1-sensor-0"
    profile = client.get(url).json()["supported_stream_profiles"][0]
    assert client.post(f"{url}/start", json={"profiles": [profile]}).status_code == 200
    sensor = FAKE_DEVICES[0].sensors[0]

    rs_manager.ctx = MagicMock(devices=[])  # re-enumerate the real refresh, finding no new devices
    rs_manager._refresh_devices_locked()
    assert "device1" in rs_manager.devices and "device2" not in rs_manager.devices

    assert client.post(f"{url}/stop").status_code == 200
    assert sensor.get_active_streams() == []


def test_get_sensor_options(setup_mock_managers):
    response = client.get(OPTIONS_URL)
    assert response.status_code == 200
    assert len(response.json()) > 0


def test_get_option_by_id(setup_mock_managers):
    option_id = client.get(OPTIONS_URL).json()[0]["option_id"]

    response = client.get(f"{OPTIONS_URL}/{option_id}")
    assert response.status_code == 200
    assert response.json()["option_id"] == option_id


def test_set_option(setup_mock_managers):
    option_id = client.get(OPTIONS_URL).json()[0]["option_id"]

    response = client.put(f"{OPTIONS_URL}/{option_id}", json={"value": 0.5})
    assert response.status_code == 200
