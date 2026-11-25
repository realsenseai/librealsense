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
    Returns the firmware's raw reply, as bytes, as the SDK's send_and_receive_raw_data: its first uint32 echoes the
    opcode, or is the firmware's error code, for the caller to compare. 400 if the device has no hardware monitor.
    """
    return camera.send_hwm_command(request)
