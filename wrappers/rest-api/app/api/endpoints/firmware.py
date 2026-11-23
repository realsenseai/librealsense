# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter, Body, Depends
from typing import Annotated

from app.api.dependencies import get_camera
from app.services.camera import Camera

router = APIRouter()

# The request body is the image itself, as the SDK takes it
Image = Annotated[bytes, Body(media_type="application/octet-stream")]


@router.post("/check_firmware_compatibility", response_model=bool)
def check_firmware_compatibility(
    image: Image,
    camera: Camera = Depends(get_camera),
):
    """Whether the SDK takes the image for this camera. Asked before enter_update_state: in recovery mode the
    SDK asks again, and an image refused there leaves the camera in it"""
    # A copy the SDK binding needs: it takes a list of ints, which bytes are not
    return camera.check_firmware_compatibility(bytearray(image))


@router.post("/enter_update_state")
def enter_update_state(
    camera: Camera = Depends(get_camera),
):
    """Switch the camera to recovery mode: devices_changed reports it leaving and coming back in it"""
    camera.enter_update_state()


@router.post("/update")
def update_firmware(
    image: Image,
    camera: Camera = Depends(get_camera),
):
    """Write the image to a camera in recovery mode, answering once written; firmware_progress events report
    it. devices_changed then reports the camera leaving recovery mode"""
    camera.update_firmware(bytearray(image))
