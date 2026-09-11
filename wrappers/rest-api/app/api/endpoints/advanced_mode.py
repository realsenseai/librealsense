# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
from typing import Dict, List
import logging
from starlette.concurrency import run_in_threadpool

from app.models.option import OptionInfo, OptionUpdate
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
        return await run_in_threadpool(rs_manager.get_advanced_mode_status, device_id)
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


@router.get("/controls/", response_model=Dict[str, List[OptionInfo]])
async def get_advanced_controls(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """Every RS400 advanced-mode control, keyed by group. Requires advanced mode enabled."""
    try:
        return await run_in_threadpool(rs_manager.get_advanced_controls, device_id)
    except RealSenseError as e:
        raise HTTPException(status_code=e.status_code, detail=e.detail)
    except Exception:
        logging.exception("Unexpected error reading advanced controls for %s", device_id)
        raise HTTPException(status_code=500, detail="Unexpected error while reading advanced controls")


@router.put("/controls/{group}/{field}/", response_model=OptionInfo)
async def set_advanced_control(
    device_id: str,
    group: str,
    field: str,
    body: OptionUpdate,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """Set one advanced control; returns it as the device reports it afterwards."""
    try:
        return await run_in_threadpool(rs_manager.set_advanced_control, device_id, group, field, body.value)
    except RealSenseError as e:
        raise HTTPException(status_code=e.status_code, detail=e.detail)
    except Exception:
        logging.exception("Unexpected error writing advanced controls for %s", device_id)
        raise HTTPException(status_code=500, detail="Unexpected error while writing advanced controls")
