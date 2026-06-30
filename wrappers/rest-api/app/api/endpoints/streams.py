# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter, Depends, HTTPException
from typing import List, Optional


from app.models.stream import StreamStatus, StreamStart, StreamStartTiming
from app.services.rs_manager import RealSenseManager
from app.api.dependencies import get_realsense_manager

router = APIRouter()

@router.post("/start", response_model=StreamStartTiming)
async def start_stream(
    device_id: str,
    stream_config: StreamStart,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """
    Start streaming from a RealSense device with the specified configuration.
    Returns timing info for diagnostics.
    """
    import time
    import logging
    import traceback
    t0 = time.perf_counter()
    try:
        result = rs_manager.start_stream(
            device_id,
            stream_config.configs,
            stream_config.align_to,
            reuse_cache=stream_config.reuse_cache,
        )
        result['timings']['endpoint_total'] = time.perf_counter() - t0
        return result
    except Exception as e:
        error_msg = str(e) if str(e) else repr(e)
        logging.error(f"[PIPELINE] Start stream failed: {error_msg}")
        logging.error(traceback.format_exc())
        for cfg in stream_config.configs:
            logging.error(f"  Config: {cfg.stream_type} {cfg.format} {cfg.resolution.width}x{cfg.resolution.height}@{cfg.framerate}fps")
        raise HTTPException(status_code=400, detail=error_msg)

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