# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter, Depends
from starlette.concurrency import run_in_threadpool
from typing import List


from app.models.stream import StreamConfig
from app.services.rs_manager import RealSenseManager
from app.api.dependencies import get_realsense_manager

router = APIRouter()

@router.post("/start")
async def start_stream(
    device_id: str,
    configs: List[StreamConfig],
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """
    Start streaming from a RealSense device with the specified configuration.
    """
    await run_in_threadpool(rs_manager.start_stream, device_id, configs)

@router.post("/stop")
async def stop_stream(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """
    Stop streaming from a RealSense device.
    """
    await run_in_threadpool(rs_manager.stop_stream, device_id)
