# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
import logging
from starlette.concurrency import run_in_threadpool

from app.services.rs_manager import RealSenseManager, RealSenseError
from app.api.dependencies import get_realsense_manager

router = APIRouter()


class AdvancedModeUpdate(BaseModel):
    enable: bool


@router.get("/", response_model=dict)
async def get_advanced_mode(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """Return {supported, enabled} for RS400 advanced mode on a device."""
    try:
        return rs_manager.get_advanced_mode_status(device_id)
    except RealSenseError as e:
        raise HTTPException(status_code=e.status_code, detail=e.detail)
    except Exception:
        logging.exception("Unexpected error reading advanced-mode status for %s", device_id)
        raise HTTPException(status_code=500, detail="Unexpected error while reading advanced-mode status")


@router.post("/", response_model=dict)
async def set_advanced_mode(
    device_id: str,
    body: AdvancedModeUpdate,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """Enable/disable advanced mode. NOTE: this restarts the device (blocking)."""
    try:
        return await run_in_threadpool(rs_manager.set_advanced_mode, device_id, body.enable)
    except RealSenseError as e:
        raise HTTPException(status_code=e.status_code, detail=e.detail)
    except Exception:
        logging.exception("Unexpected error toggling advanced mode for %s", device_id)
        raise HTTPException(status_code=500, detail="Unexpected error while toggling advanced mode")
