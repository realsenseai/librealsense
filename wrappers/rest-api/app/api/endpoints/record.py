# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter, Depends
from starlette.concurrency import run_in_threadpool

from app.api.dependencies import get_realsense_manager
from app.models.playback import RecordStartRequest, RecordStatus
from app.services.rs_manager import RealSenseManager

router = APIRouter()


@router.get("/", response_model=RecordStatus)
async def record_status(device_id: str, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    return rs_manager.get_record_status(device_id)


@router.post("/start", response_model=RecordStatus)
async def start_recording(device_id: str, body: RecordStartRequest = RecordStartRequest(),
                          rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Record every streaming sensor of the device to a ROS-bag. Requires streaming."""
    return await run_in_threadpool(rs_manager.start_recording, device_id, body.path)


@router.post("/pause", response_model=RecordStatus)
async def pause_recording(device_id: str, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    return await run_in_threadpool(rs_manager.set_recording_paused, device_id, True)


@router.post("/resume", response_model=RecordStatus)
async def resume_recording(device_id: str, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    return await run_in_threadpool(rs_manager.set_recording_paused, device_id, False)


@router.post("/stop", response_model=RecordStatus)
async def stop_recording(device_id: str, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Finalize the file; the device keeps streaming."""
    return await run_in_threadpool(rs_manager.stop_recording, device_id)
