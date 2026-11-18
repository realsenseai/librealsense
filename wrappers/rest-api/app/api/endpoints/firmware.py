# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter, Depends, HTTPException, UploadFile, File

from app.services.rs_manager import RealSenseManager
from app.api.dependencies import get_realsense_manager

router = APIRouter()


# Cap user-supplied firmware uploads. D4XX images are well under 10 MiB, so 64 MiB
# leaves headroom for future products without letting an unrelated file through.
MAX_FW_UPLOAD_BYTES = 64 * 1024 * 1024


@router.get("/", response_model=dict)
def get_recommended_firmware(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """Get the firmware version the online versions DB recommends for a device."""
    return rs_manager.get_recommended_firmware(device_id)


_FW_UPLOAD_CHUNK = 1024 * 1024  # 1 MiB


@router.post("/update_from_file", response_model=dict)
def update_firmware_from_file(
    device_id: str,
    file: UploadFile = File(...),
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """Trigger firmware update from a user-supplied .bin image.

    Accepts multipart/form-data with a single ``file`` field. The SDK's
    ``check_firmware_compatibility`` gates whether the image is actually applied;
    we only do cheap sanity checks here (filename suffix and size).
    """
    filename = file.filename or ""
    if not filename.lower().endswith(".bin"):
        raise HTTPException(status_code=400, detail="Firmware file must have a .bin extension")

    # Read the upload in chunks and fail early if it exceeds the cap, so a
    # huge POST can't materialize a huge bytes object in memory.
    fw_buf = bytearray()
    while chunk := file.file.read(_FW_UPLOAD_CHUNK):
        fw_buf.extend(chunk)
        if len(fw_buf) > MAX_FW_UPLOAD_BYTES:
            raise HTTPException(
                status_code=413,
                detail=f"Firmware file too large (max {MAX_FW_UPLOAD_BYTES // (1024 * 1024)} MiB)",
            )

    if not fw_buf:
        raise HTTPException(status_code=400, detail="Uploaded firmware file is empty")

    return rs_manager.update_firmware_from_bytes(device_id, bytes(fw_buf))


@router.post("/update_from_recommended", response_model=dict)
def update_firmware_from_recommended(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """Download the device's recommended firmware (from the online versions DB) and flash it.

    One-click alternative to update_from_file: no upload — the backend fetches the .bin
    and reuses the same DFU flow (and its progress events).
    """
    return rs_manager.update_firmware_from_recommended(device_id)
