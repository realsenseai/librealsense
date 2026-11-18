# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter, Depends
from typing import Dict

from app.models.option import OptionInfo, OptionUpdate
from app.api.dependencies import get_camera
from app.services.camera import Camera

router = APIRouter()


@router.get("/", response_model=Dict[str, OptionInfo])
def get_colorizer_options(
    camera: Camera = Depends(get_camera),
):
    """The depth colorizer's controls for a device: color scheme, distances, histogram eq."""
    return camera.colorizer_options()


@router.get("/{field}", response_model=OptionInfo)
def get_colorizer_option(
    field: str,
    camera: Camera = Depends(get_camera),
):
    """One colorizer option"""
    return camera.colorizer_option(field)


@router.put("/{field}", response_model=OptionInfo)
def set_colorizer_option(
    field: str,
    body: OptionUpdate,
    camera: Camera = Depends(get_camera),
):
    """Set one colorizer option; returns it as the device reports it afterwards."""
    return camera.set_colorizer_option(field, body.value)
