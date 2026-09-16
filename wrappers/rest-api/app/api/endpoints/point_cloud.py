# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from starlette.concurrency import run_in_threadpool
from fastapi import APIRouter, Depends, HTTPException
from fastapi.responses import Response
from pydantic import BaseModel
from typing import List, Optional


from app.models.stream import PointCloudStatus, StreamStatus
from app.services.rs_manager import RealSenseManager
from app.api.dependencies import get_realsense_manager

router = APIRouter()

@router.post("/activate", response_model=PointCloudStatus)
async def activate_point_cloud(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    try:
        return await run_in_threadpool(rs_manager.activate_point_cloud, device_id, True)
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))

@router.post("/deactivate", response_model=PointCloudStatus)
async def deactivate_point_cloud(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    try:
        return await run_in_threadpool(rs_manager.activate_point_cloud, device_id, False)
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))

@router.get("/geometry")
async def get_point_cloud_geometry(
    device_id: str,
    texture: Optional[str] = "color",
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """Depth intrinsics and units plus the texture stream's intrinsics and depth->texture
    extrinsics for the running streams, so the client can build the point cloud itself."""
    try:
        return await run_in_threadpool(rs_manager.get_point_cloud_geometry, device_id, texture)
    except Exception as e:
        raise HTTPException(status_code=getattr(e, "status_code", 400), detail=str(getattr(e, "detail", e)))


class PlyExportRequest(BaseModel):
    mesh: bool = True
    normals: bool = False
    binary: bool = True


@router.post("/export")
async def export_point_cloud(
    device_id: str,
    body: PlyExportRequest = PlyExportRequest(),
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """Download the newest depth frame as a PLY file: mesh or points, with or without normals,
    binary or ASCII - the legacy viewer's export dialog."""
    try:
        data = await run_in_threadpool(rs_manager.export_point_cloud, device_id, body.mesh, body.normals, body.binary)
    except Exception as e:
        raise HTTPException(status_code=getattr(e, "status_code", 400), detail=str(getattr(e, "detail", e)))
    return Response(content=data, media_type="application/octet-stream",
                    headers={"Content-Disposition": f'attachment; filename="{device_id}.ply"'})


@router.get("/status", response_model=PointCloudStatus)
async def get_stream_status(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager)
):
    try:
        return await run_in_threadpool(rs_manager.get_point_cloud_status, device_id)
    except Exception as e:
        raise HTTPException(status_code=404, detail=str(e))