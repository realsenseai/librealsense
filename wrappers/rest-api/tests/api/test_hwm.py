# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest

from .conftest import client
from ..mocks.pyrealsense_mock import camera_info, debug_protocol as MockDebugDevice


def test_hwm_command_basic(setup_mock_managers):
    """POST /devices/{id}/hwm with a minimal request returns 200 and a response list."""
    response = client.post("/api/v1/devices/device1/hwm", json={"opcode": 0xA6})
    assert response.status_code == 200
    # Mock echoes opcode (0xA6 = 166) as little-endian uint32 in the first 4 bytes.
    assert response.json() == [0xA6, 0, 0, 0]


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
    assert isinstance(response.json(), list)


def test_a_firmware_error_reply_is_returned_as_is(setup_mock_managers):
    """As the SDK returns it: its first uint32 is the firmware's error code instead of the opcode echo"""
    device = MockDebugDevice(serial_number="fw-error", name="Error Device", fw_error_code=0x00000009)
    with setup_mock_managers["rs_manager"].lock:
        setup_mock_managers["rs_manager"]._add_cameras([device])
    response = client.post("/api/v1/devices/fw-error/hwm", json={"opcode": 0xA6})
    assert (response.status_code, response.json()) == (200, [0x09, 0, 0, 0])


@pytest.mark.parametrize("body", [{"opcode": -1}, {"opcode": 0x10, "param1": 0x100000000}, {"opcode": 0x10, "data": [256]}])
def test_a_value_out_of_the_sdks_uint32_or_byte_is_refused(setup_mock_managers, body):
    """The SDK's build_command takes uint32 and uint8: anything else is the client's error (422), not a 500"""
    assert client.post("/api/v1/devices/device1/hwm", json=body).status_code == 422
