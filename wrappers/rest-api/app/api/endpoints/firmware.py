from fastapi import APIRouter, Depends, HTTPException
import logging
from starlette.concurrency import run_in_threadpool

from app.services.rs_manager import RealSenseManager, RealSenseError
from app.api.dependencies import get_realsense_manager

router = APIRouter()


@router.get("/{device_id}/status", response_model=dict)
async def get_firmware_status(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """Get firmware status for a specific device."""
    try:
        return rs_manager.get_firmware_status(device_id)
    except RealSenseError as e:
        raise HTTPException(status_code=e.status_code, detail=e.detail)
    except Exception as e:
        logging.exception("Unexpected error fetching firmware status for %s", device_id)
        raise HTTPException(status_code=500, detail="Unexpected error while fetching firmware status")


@router.post("/{device_id}/update", response_model=dict)
async def update_firmware(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """Trigger firmware update using bundled image (no upload)."""
    try:
        # IMPORTANT: firmware update is blocking; run it off the event loop so
        # Socket.IO can deliver progress/completion events in real time.
        return await run_in_threadpool(rs_manager.update_firmware, device_id)
    except RealSenseError as e:
        raise HTTPException(status_code=e.status_code, detail=e.detail)
    except Exception as e:
        logging.exception("Unexpected error updating firmware for %s", device_id)
        raise HTTPException(status_code=500, detail="Unexpected error while updating firmware")
