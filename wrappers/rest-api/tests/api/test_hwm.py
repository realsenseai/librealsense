# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest

from .conftest import client
from ..mocks.pyrealsense_mock import camera_info, device as MockDevice, debug_protocol as MockDebugDevice


def test_hwm_command_basic(setup_mock_managers):
    """POST /devices/{id}/hwm with a minimal request returns 200 and a response list."""
    response = client.post("/api/v1/devices/device1/hwm", json={"opcode": 0xA6})
    assert response.status_code == 200
    body = response.json()
    assert body["device_id"] == "device1"
    assert isinstance(body["response"], list)
    # Mock echoes opcode (0xA6 = 166) as little-endian uint32 in the first 4 bytes.
    assert body["response"] == [0xA6, 0, 0, 0]


def test_hwm_command_with_params(setup_mock_managers):
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


@pytest.mark.parametrize(
    "device,status,details",
    [
        (None, 404, []),
        # Plain device (not a debug_protocol subclass) does not expose the extension.
        (MockDevice(serial_number="no-debug", name="Limited Device"), 400, ["does not support"]),
        # fw_error_code differs from opcode 0xA6 so the opcode check must fire; the
        # detail carries both the returned error code and the expected opcode echo.
        (
            MockDebugDevice(serial_number="fw-error", name="Error Device", fw_error_code=0x00000009),
            500,
            ["error code", "0x00000009", "0x000000a6"],
        ),
        (
            MockDebugDevice(serial_number="short-resp", name="Short Response Device", short_hwm_response=True),
            500,
            ["too short"],
        ),
    ],
    ids=["unknown_device", "unsupported_device", "firmware_error", "response_too_short"],
)
def test_hwm_command_errors(setup_mock_managers, device, status, details):
    device_id = "no-such-device"
    if device is not None:
        device_id = device.get_info(camera_info.serial_number)
        setup_mock_managers["rs_manager"].devices[device_id] = device

    response = client.post(f"/api/v1/devices/{device_id}/hwm", json={"opcode": 0xA6})
    assert response.status_code == status
    detail = response.json()["detail"].lower()
    assert all(d in detail for d in details)


def test_hwm_no_deadlock_on_unknown_device(patch_dependencies):
    """Uses the real refresh_devices so its lock acquisition actually happens: with the
    buggy implementation the absent-device path blocks forever."""
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
