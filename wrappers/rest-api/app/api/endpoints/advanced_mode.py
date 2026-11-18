# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter, Depends
from pydantic import BaseModel
from typing import Dict

from app.models.option import AdvancedControl, OptionUpdate
from app.services import advanced_mode
from app.api.dependencies import get_camera
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


@router.post("/")
def set_advanced_mode(
    body: AdvancedModeUpdate,
    camera: Camera = Depends(get_camera),
):
    """Enable/disable advanced mode. This resets the device: devices_changed reports it leaving and coming back"""
    return camera.set_advanced_mode(body.enable)


@router.get("/controls/", response_model=Dict[str, Dict[str, AdvancedControl]])
def get_advanced_controls(
    camera: Camera = Depends(get_camera),
):
    """Every RS400 advanced-mode control, keyed by group. Requires advanced mode enabled."""
    return camera.advanced_controls()


@router.get("/controls/{group}/", response_model=Dict[str, AdvancedControl])
def get_advanced_group(
    group: str,
    camera: Camera = Depends(get_camera),
):
    """One group of advanced controls, by field"""
    return camera.advanced_group(group)


@router.get("/controls/{group}/{field}", response_model=AdvancedControl)
def get_advanced_control(
    group: str,
    field: str,
    camera: Camera = Depends(get_camera),
):
    """One advanced control"""
    return camera.advanced_control(group, field)


@router.put("/controls/{group}/{field}")
def set_advanced_control(
    group: str,
    field: str,
    body: OptionUpdate,
    camera: Camera = Depends(get_camera),
):
    """Set one advanced control."""
    camera.set_advanced_control(group, field, body.value)
