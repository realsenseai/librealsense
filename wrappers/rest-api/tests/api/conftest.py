# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
from unittest.mock import MagicMock
from fastapi.testclient import TestClient
from ..mocks.setup_fake_devices import setup_fake_devices
from ..mocks.mock_dependencies import patch_dependencies  # noqa: F401  (autouse fixture)
from ..mocks.pyrealsense_mock import camera_info
from main import app

client = TestClient(app)

STREAM_CONFIG = [{"stream": "depth", "index": 0, "width": 640, "height": 480, "format": "z16", "fps": 30}]
OPTIONS_URL = "/api/v1/devices/device1/sensors/device1-sensor-0/options"
FIRMWARE_URL = "/api/v1/devices/device1/firmware/update_from_file"

FAKE_DEVICES = setup_fake_devices()


@pytest.fixture
def setup_mock_managers(patch_dependencies):
    rs_manager = patch_dependencies["rs_manager"]

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
            )
            rs_manager.pipelines[device_id] = MagicMock()
        return list(rs_manager.device_infos.values())

    rs_manager.refresh_devices = mock_refresh_devices

    rs_manager.refresh_devices()

    return {"rs_manager": rs_manager}
