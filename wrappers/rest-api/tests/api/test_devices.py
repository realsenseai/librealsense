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


def test_sensor_profiles_carry_the_sdk_default(setup_mock_managers):
    by_type = {s["type"]: s for s in client.get("/api/v1/devices/device1/sensors").json()}
    depth = by_type["Depth Sensor"]["supported_stream_profiles"][0]
    assert depth["default"] == {"resolution": [640, 480], "fps": 30, "format": "z16"}
    color = by_type["RGB Camera"]["supported_stream_profiles"][0]
    assert color["default"] == {"resolution": [1280, 720], "fps": 30, "format": "rgb8"}


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


def test_device_carries_every_camera_info_field(setup_mock_managers):
    rs_manager = setup_mock_managers["rs_manager"]
    from ..mocks.setup_fake_devices import setup_fake_devices
    rs_manager.devices.clear(); rs_manager.device_infos.clear()
    with rs_manager.lock:
        rs_manager._register_new_device(setup_fake_devices()[0])
    info = rs_manager.device_infos["device1"].info
    assert info["serial_number"] == "device1" and info["product_id"] == "0123" and info["usb_type_descriptor"] == "3.0"


def test_stream_profiles_list_every_exact_mode(setup_mock_managers):
    depth = next(s for s in client.get("/api/v1/devices/device1/sensors").json() if s["type"] == "Depth Sensor")
    modes = depth["supported_stream_profiles"][0]["modes"]
    assert [640, 480, 30, "z16"] in modes and [1280, 720, 60, "z16"] in modes and len(modes) == 4


def _counting_ctx(devices):
    """A mock rs.context that counts how often it is enumerated."""
    from ..mocks.pyrealsense_mock import context

    class Counting(context):
        def __init__(self):
            self.enumerations = 0
            self._list = list(devices)

        @property
        def devices(self):
            self.enumerations += 1
            return self._list

    return Counting()


def test_no_enumeration_while_a_camera_streams(setup_mock_managers):
    rs_manager = setup_mock_managers["rs_manager"]
    from ..mocks.setup_fake_devices import setup_fake_devices
    ctx = rs_manager.ctx = _counting_ctx(setup_fake_devices())
    real_refresh = type(rs_manager).refresh_devices  # the fixture stubs the instance method

    rs_manager.streaming_mode["device1"] = "sensor"
    assert [d.device_id for d in real_refresh(rs_manager)] == ["device1", "device2"]
    assert ctx.enumerations == 0

    rs_manager.streaming_mode["device1"] = "idle"
    real_refresh(rs_manager)
    assert ctx.enumerations == 1


def test_refresh_keeps_known_handles_and_drops_the_unplugged(setup_mock_managers):
    rs_manager = setup_mock_managers["rs_manager"]
    from ..mocks.setup_fake_devices import setup_fake_devices
    devs = setup_fake_devices()
    rs_manager.ctx = _counting_ctx(devs)
    real_refresh = type(rs_manager).refresh_devices

    real_refresh(rs_manager)
    before = rs_manager.devices["device1"]
    real_refresh(rs_manager)
    assert rs_manager.devices["device1"] is before  # no vanish-and-reappear for readers

    rs_manager.ctx = _counting_ctx(devs[1:])
    real_refresh(rs_manager)
    assert set(rs_manager.devices) == {"device2"}
