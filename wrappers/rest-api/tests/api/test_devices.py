# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from .conftest import OPTIONS_URL, client


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
