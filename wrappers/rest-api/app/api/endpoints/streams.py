# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter, Depends
from typing import List


from app.models.stream import StreamConfig
from app.api.dependencies import get_camera
from app.services.camera import Camera

router = APIRouter()

@router.post("/start")
def start_stream(
    configs: List[StreamConfig],
    camera: Camera = Depends(get_camera),
):
    """
    Start streaming from a RealSense device with the specified configuration.
    """
    camera.start_stream(configs)

@router.post("/stop")
def stop_stream(
    camera: Camera = Depends(get_camera),
):
    """
    Stop streaming from a RealSense device.
    """
    camera.stop_stream()
