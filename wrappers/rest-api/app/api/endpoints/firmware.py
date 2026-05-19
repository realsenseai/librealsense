# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter, Depends, HTTPException
import logging

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