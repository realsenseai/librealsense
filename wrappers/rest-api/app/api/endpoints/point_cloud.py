from fastapi import APIRouter, Depends, HTTPException
from typing import List, Optional


from app.models.stream import PointCloudStatus, StreamStatus
from app.services.rs_manager import RealSenseManager
from app.api.dependencies import get_realsense_manager, get_current_user

router = APIRouter()

@router.post("/activate", response_model=PointCloudStatus)
async def activate_point_cloud(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
    user: dict = Depends(get_current_user),
):
    """
    Activate point cloud processing for a device.
    Requires authentication.
    """
    try:
        return rs_manager.activate_point_cloud(device_id, True)
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))

@router.post("/deactivate", response_model=PointCloudStatus)
async def deactivate_point_cloud(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
    user: dict = Depends(get_current_user),
):
    """
    Deactivate point cloud processing for a device.
    Requires authentication.
    """
    try:
        return rs_manager.activate_point_cloud(device_id, False)
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))

@router.get("/status", response_model=PointCloudStatus)
async def get_stream_status(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
    user: dict = Depends(get_current_user),
):
    """
    Get point cloud processing status for a device.
    Requires authentication.
    """
    try:
        return rs_manager.get_point_cloud_status(device_id)
    except Exception as e:
        raise HTTPException(status_code=404, detail=str(e))