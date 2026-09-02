# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter, Depends, HTTPException, UploadFile, File
import logging
from starlette.concurrency import run_in_threadpool

from app.services.rs_manager import RealSenseManager, RealSenseError
from app.api.dependencies import get_realsense_manager

router = APIRouter()


# Cap user-supplied firmware uploads. D4XX images are well under 10 MiB, so 64 MiB
# leaves headroom for future products without letting an unrelated file through.
MAX_FW_UPLOAD_BYTES = 64 * 1024 * 1024


@router.get("/", response_model=dict)
async def get_recommended_firmware(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """Get the firmware version the online versions DB recommends for a device."""
    try:
        # Off the event loop: an unreachable versions DB would stall every other request.
        return await run_in_threadpool(rs_manager.get_recommended_firmware, device_id)
    except RealSenseError as e:
        raise HTTPException(status_code=e.status_code, detail=e.detail)
    except Exception:
        logging.exception("Unexpected error fetching recommended firmware for %s", device_id)
        raise HTTPException(status_code=500, detail="Unexpected error while fetching recommended firmware")


_FW_UPLOAD_CHUNK = 1024 * 1024  # 1 MiB


@router.post("/update_from_file", response_model=dict)
async def update_firmware_from_file(
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

    # Stream the upload in chunks and fail early if it exceeds the cap, so a
    # huge POST can't materialize a huge bytes object in memory.
    fw_buf = bytearray()
    try:
        while True:
            chunk = await file.read(_FW_UPLOAD_CHUNK)
            if not chunk:
                break
            fw_buf.extend(chunk)
            if len(fw_buf) > MAX_FW_UPLOAD_BYTES:
                raise HTTPException(
                    status_code=413,
                    detail=f"Firmware file too large (max {MAX_FW_UPLOAD_BYTES // (1024 * 1024)} MiB)",
                )
    except HTTPException:
        raise
    except Exception:
        logging.exception("Failed to read uploaded firmware for %s", device_id)
        raise HTTPException(status_code=400, detail="Could not read uploaded firmware file")
    finally:
        await file.close()

    if not fw_buf:
        raise HTTPException(status_code=400, detail="Uploaded firmware file is empty")

    try:
        # Firmware update is blocking; run it off the event loop so Socket.IO
        # can deliver progress / completion events in real time.
        return await run_in_threadpool(rs_manager.update_firmware_from_bytes, device_id, bytes(fw_buf))
    except RealSenseError as e:
        raise HTTPException(status_code=e.status_code, detail=e.detail)
    except Exception:
        logging.exception("Unexpected error updating firmware for %s", device_id)
        raise HTTPException(status_code=500, detail="Unexpected error while updating firmware")


@router.post("/update_from_recommended", response_model=dict)
async def update_firmware_from_recommended(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """Download the device's recommended firmware (from the online versions DB) and flash it.

    One-click alternative to update_from_file: no upload — the backend fetches the .bin
    and reuses the same DFU flow (and Socket.IO progress events).
    """
    try:
        return await run_in_threadpool(rs_manager.update_firmware_from_recommended, device_id)
    except RealSenseError as e:
        raise HTTPException(status_code=e.status_code, detail=e.detail)
    except Exception:
        logging.exception("Unexpected error updating firmware from recommended for %s", device_id)
        raise HTTPException(status_code=500, detail="Unexpected error while updating firmware")
