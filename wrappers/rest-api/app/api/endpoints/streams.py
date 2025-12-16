from fastapi import APIRouter, Depends, HTTPException
from typing import List, Optional


from app.models.stream import StreamStatus, StreamStart
from app.services.rs_manager import RealSenseManager
from app.api.dependencies import get_realsense_manager

router = APIRouter()

@router.post("/start", response_model=StreamStatus)
async def start_stream(
    device_id: str,
    stream_config: StreamStart,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """
    Start streaming from a RealSense device with the specified configuration.
    """
    try:
        return rs_manager.start_stream(device_id, stream_config.configs, stream_config.align_to)
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))

@router.post("/stop", response_model=StreamStatus)
async def stop_stream(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """
    Stop streaming from a RealSense device.
    """
    try:
        return rs_manager.stop_stream(device_id)
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))

@router.get("/status", response_model=StreamStatus)
async def get_stream_status(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager)
):
    """
    Get the streaming status for a RealSense device.
    """
    try:
        return rs_manager.get_stream_status(device_id)
    except Exception as e:
        raise HTTPException(status_code=404, detail=str(e))

@router.get("/depth-at-pixel")
async def get_depth_at_pixel(
    device_id: str,
    x: int,
    y: int,
    rs_manager: RealSenseManager = Depends(get_realsense_manager)
):
    """
    Get depth value (in meters) at specific pixel coordinates.
    Returns null if no depth frame is available or coordinates are out of bounds.
    """
    try:
        depth = rs_manager.get_depth_at_pixel(device_id, x, y)
        return {"depth": depth, "x": x, "y": y, "units": "meters"}
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))

@router.get("/depth-range")
async def get_depth_range(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager)
):
    """
    Calculate dynamic depth range for the legend based on current frame data.
    Uses the same algorithm as the legacy viewer (mean + 1.5*stddev, rounded to nearest 4m).
    """
    try:
        result = rs_manager.get_depth_range(device_id)
        return result
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))