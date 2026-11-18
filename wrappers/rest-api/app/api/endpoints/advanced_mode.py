# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter, Depends
from pydantic import BaseModel
from typing import Dict, List

from app.models.option import OptionInfo, OptionUpdate
from app.services import advanced_mode
from app.services.rs_manager import RealSenseManager
from app.api.dependencies import get_camera, get_realsense_manager
from app.services.camera import Camera

router = APIRouter()


class AdvancedModeUpdate(BaseModel):
    enable: bool


@router.get("/", response_model=Dict[str, bool])
def get_advanced_mode(
    camera: Camera = Depends(get_camera),
):
    """Return {supported, enabled} for RS400 advanced mode on a device."""
    return advanced_mode.status(camera.dev)


@router.post("/", response_model=Dict[str, bool])
def set_advanced_mode(
    device_id: str,
    body: AdvancedModeUpdate,
    camera: Camera = Depends(get_camera),
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """Enable/disable advanced mode. NOTE: this restarts the device (blocking)."""
    camera.set_advanced_mode(body.enable)
    # Report what the restarted device says rather than what was asked for
    rs_manager.wait_until_device_returns(device_id)
    return advanced_mode.status(rs_manager.cameras[device_id].dev)


@router.get("/controls/", response_model=Dict[str, List[OptionInfo]])
def get_advanced_controls(
    camera: Camera = Depends(get_camera),
):
    """Every RS400 advanced-mode control, keyed by group. Requires advanced mode enabled."""
    return camera.advanced_controls()


@router.put("/controls/{group}/{field}/", response_model=OptionInfo)
def set_advanced_control(
    group: str,
    field: str,
    body: OptionUpdate,
    camera: Camera = Depends(get_camera),
):
    """Set one advanced control; returns it as the device reports it afterwards."""
    return camera.set_advanced_control(group, field, body.value)
