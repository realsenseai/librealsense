# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from typing import List, Optional

from fastapi import APIRouter, Depends

from app.api.dependencies import get_realsense_manager
from app.models.job import JobInfo
from app.services.rs_manager import RealSenseManager

router = APIRouter()


@router.get("/", response_model=List[JobInfo])
async def list_jobs(device_id: Optional[str] = None, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Running and recently finished long operations, optionally for one device."""
    return rs_manager.jobs.list(device_id)


@router.get("/{job_id}", response_model=JobInfo)
async def get_job(job_id: str, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    return rs_manager.jobs.get(job_id).info


@router.post("/{job_id}/cancel", response_model=JobInfo)
async def cancel_job(job_id: str, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Request cancellation; the job reports 'cancelled' once its worker stops."""
    return rs_manager.jobs.cancel(job_id)
