# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from typing import Optional

from fastapi import APIRouter, Depends
from pydantic import BaseModel, Field
from starlette.concurrency import run_in_threadpool

from app.api.dependencies import get_realsense_manager
from app.models.job import JobInfo
from app.services.rs_manager import RealSenseManager

router = APIRouter()


class OccRequest(BaseModel):
    """The legacy on-chip calibration parameters (on-chip-calib.cpp)."""
    speed: int = Field(3, ge=0, le=4)  # very fast .. white wall
    average_step_count: int = Field(20, ge=1, le=30)
    step_count: int = Field(20, ge=1, le=30)
    accuracy: int = Field(2, ge=0, le=3)  # very high .. low
    apply_preset: bool = True
    intrinsic_scan: bool = True
    host_assistance: bool = False


class TareRequest(BaseModel):
    ground_truth_mm: float = Field(..., gt=0)
    average_step_count: int = Field(20, ge=1, le=30)
    step_count: int = Field(20, ge=1, le=30)
    accuracy: int = Field(2, ge=0, le=3)
    apply_preset: bool = True
    host_assistance: bool = False


class ApplyRequest(BaseModel):
    use_new: bool = True


class RectParamsPatch(BaseModel):
    index: Optional[int] = Field(None, ge=0, le=15)
    resolution: Optional[str] = None
    fx: Optional[float] = None
    fy: Optional[float] = None
    ppx: Optional[float] = None
    ppy: Optional[float] = None


class TablePatch(BaseModel):
    """Fields of the D400 coefficients table the legacy editor lets the user change."""
    baseline: Optional[float] = None
    intrinsic_left: Optional[list] = None
    intrinsic_right: Optional[list] = None
    world2left_rot: Optional[list] = None
    world2right_rot: Optional[list] = None
    rect_params: Optional[list[RectParamsPatch]] = None
    write: bool = False


@router.get("/", response_model=dict)
async def get_calibration(device_id: str, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """The device's calibration session: state, health, which table is active."""
    return await run_in_threadpool(rs_manager.get_calibration, device_id)


@router.post("/occ", response_model=JobInfo)
async def start_occ(device_id: str, body: OccRequest = OccRequest(), rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Start on-chip calibration as a job; follow it on the `job` socket event or GET /jobs/{id}."""
    return await run_in_threadpool(rs_manager.start_on_chip_calibration, device_id, body.model_dump())


@router.post("/tare", response_model=JobInfo)
async def start_tare(device_id: str, body: TareRequest, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Start tare calibration against a flat target at a known distance."""
    params = body.model_dump()
    ground_truth = params.pop("ground_truth_mm")
    return await run_in_threadpool(rs_manager.start_tare_calibration, device_id, ground_truth, params)


@router.post("/apply", response_model=dict)
async def apply_calibration(device_id: str, body: ApplyRequest = ApplyRequest(), rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Make the new (or the old) table active without writing it to flash."""
    return await run_in_threadpool(rs_manager.apply_calibration, device_id, body.use_new)


@router.post("/keep", response_model=dict)
async def keep_calibration(device_id: str, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Write the active table to the device's flash (gated by Settings > calibration)."""
    return await run_in_threadpool(rs_manager.keep_calibration, device_id)


@router.get("/table", response_model=dict)
async def get_table(device_id: str, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """The D400 coefficients table: intrinsics, rotations, baseline, rectified parameters."""
    return await run_in_threadpool(rs_manager.get_calibration_table, device_id)


@router.put("/table", response_model=dict)
async def set_table(device_id: str, body: TablePatch, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Edit table fields; the table becomes active, and with `write` it is stored in flash."""
    patch = body.model_dump(exclude_none=True)
    write = patch.pop("write", False)
    return await run_in_threadpool(rs_manager.set_calibration_table, device_id, patch, write)


@router.post("/reset_factory", response_model=dict)
async def reset_factory(device_id: str, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    return await run_in_threadpool(rs_manager.reset_factory_calibration, device_id)
