# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.


from fastapi import APIRouter, Depends
from typing import List

from app.api.dependencies import get_camera
from app.services.camera import Camera
from app.models.hwm import HwmRequest

router = APIRouter()


@router.post("/", response_model=List[int])
def send_hwm_command(
    request: HwmRequest,
    camera: Camera = Depends(get_camera),
):
    """
    Send a hardware monitor (HWM) command to a specific RealSense device.

    Requires the device to support the ``RS2_EXTENSION_DEBUG`` extension.
    Returns the firmware's raw reply, as bytes; 400 if the device does not support hardware monitor commands.
    """
    return camera.send_hwm_command(request)
