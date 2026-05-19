# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter, Depends, HTTPException
from typing import List


from app.models.device import DeviceInfo
from app.services.rs_manager import RealSenseManager
from app.api.dependencies import get_realsense_manager

router = APIRouter()

@router.get("/", response_model=List[DeviceInfo])
async def get_devices(
    force_refresh: bool = False,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
    #user: dict = Depends(get_current_user)  -> enable this if security is needed
):
    """
    Get a list of all connected RealSense devices. Set force_refresh=true to re-enumerate.
    """
    return rs_manager.get_devices(force_refresh=force_refresh)

@router.get("/{device_id}", response_model=DeviceInfo)
async def get_device(
    device_id: str,
    force_refresh: bool = False,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
    #user: dict = Depends(get_current_user) -> enable this if security is needed
):
    """
    Get details of a specific RealSense device.
    """
    try:
        return rs_manager.get_device(device_id, force_refresh=force_refresh)
    except Exception as e:
        raise HTTPException(status_code=404, detail=str(e))


@router.post("/refresh", response_model=List[DeviceInfo])
async def refresh_devices(
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
    #user: dict = Depends(get_current_user)  -> enable this if security is needed
):
    """
    Force device re-enumeration and return the updated list.
    Delegates to get_devices(force_refresh=True) so the underlying logic lives in one place.
    """
    return await get_devices(force_refresh=True, rs_manager=rs_manager)

@router.post("/{device_id}/hw_reset", response_model=bool)
async def hw_reset_device(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
    #user: dict = Depends(get_current_user) -> enable this if security is needed
):
    """
    Perform a hardware reset on a specific RealSense device.
    """
    try:
        return rs_manager.reset_device(device_id)
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))