# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
from unittest.mock import MagicMock

from .conftest import FIRMWARE_URL, client


def test_update_firmware_from_file_happy_path(patch_dependencies):
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
    response = client.post(FIRMWARE_URL, files=files)
    assert response.status_code == 200
    assert response.json()["status"] == "success"
    # Manager called with device_id + bytes payload
    args, _ = rs_manager.update_firmware_from_bytes.call_args
    assert args[0] == "device1"
    assert args[1] == b"\x00\x01\x02\x03"


@pytest.mark.parametrize(
    "filename,content,status,detail",
    [
        ("firmware.txt", b"hello", 400, ".bin"),
        ("empty.bin", b"", 400, "empty"),
        ("big.bin", b"X" * 32, 413, "too large"),
    ],
    ids=["non_bin_extension", "empty", "oversize"],
)
def test_update_firmware_from_file_rejects(
    patch_dependencies, monkeypatch, filename, content, status, detail
):
    from app.api.endpoints import firmware as firmware_module

    # Shrink the cap so the oversize case doesn't have to allocate 64 MiB.
    monkeypatch.setattr(firmware_module, "MAX_FW_UPLOAD_BYTES", 16)
    rs_manager = patch_dependencies["rs_manager"]
    rs_manager.update_firmware_from_bytes = MagicMock()

    files = {"file": (filename, content, "application/octet-stream")}
    response = client.post(FIRMWARE_URL, files=files)
    assert response.status_code == status
    assert detail in response.json()["detail"].lower()
    rs_manager.update_firmware_from_bytes.assert_not_called()


def test_update_firmware_from_file_propagates_sdk_error(patch_dependencies):
    from app.services.rs_manager import RealSenseError

    patch_dependencies["rs_manager"].update_firmware_from_bytes = MagicMock(
        side_effect=RealSenseError(status_code=400, detail="Firmware is not compatible")
    )
    files = {"file": ("bad.bin", b"\xff" * 8, "application/octet-stream")}
    response = client.post(FIRMWARE_URL, files=files)
    assert response.status_code == 400
    assert response.json()["detail"] == "Firmware is not compatible"
