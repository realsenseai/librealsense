# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from typing import Any, Dict

from fastapi import APIRouter, Body, Depends
from starlette.concurrency import run_in_threadpool

from app.api.dependencies import get_realsense_manager
from app.services.rs_manager import RealSenseManager

router = APIRouter()


@router.get("/", response_model=dict)
async def get_hdr(device_id: str, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """HDR preset support, exposure/gain ranges and the sequence the device holds."""
    return await run_in_threadpool(rs_manager.get_hdr, device_id)


@router.put("/", response_model=dict)
async def apply_hdr(device_id: str, preset: Dict[str, Any] = Body(...), rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Apply an HDR sequence (the legacy hdr_model preset struct); answers the device's view."""
    return await run_in_threadpool(rs_manager.apply_hdr, device_id, preset)
