# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
from unittest.mock import MagicMock
from fastapi.testclient import TestClient
from ..mocks.setup_fake_devices import setup_fake_devices
from ..mocks.mock_dependencies import patch_dependencies  # noqa: F401  (autouse fixture)
from main import app
import pyrealsense2 as rs

client = TestClient(app)

STREAM_CONFIG = [{"stream": "depth", "index": 0, "width": 640, "height": 480, "format": "z16", "fps": 30}]
OPTIONS_URL = "/api/v1/devices/device1/sensors/0/options"
FIRMWARE_URL = "/api/v1/devices/device1/firmware/update_from_file"

FAKE_DEVICES = setup_fake_devices()


@pytest.fixture
def setup_mock_managers(patch_dependencies, monkeypatch):
    rs_manager = patch_dependencies["rs_manager"]

    monkeypatch.setattr(rs, "pipeline", MagicMock())  # rs.pipeline.set_device takes only a real rs.device
    rs_manager.cameras.clear()
    with rs_manager.lock:
        rs_manager._add_cameras(FAKE_DEVICES)

    return {"rs_manager": rs_manager}
