# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter, Depends
from typing import Dict, List

from app.models.stream import Extrinsics, StreamName
from app.services.rs_manager import RealSenseManager
from app.api.dependencies import get_camera, get_realsense_manager
from app.services.camera import Camera

router = APIRouter()

@router.get("/", response_model=List[Dict[str, str]])
def get_devices(
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
    #user: dict = Depends(get_current_user)  -> enable this if security is needed
):
    """
    Every connected RealSense device: each camera info it reports, by its SDK name.
    """
    return [c.info for c in list(rs_manager.cameras.values())]  # a copy: the SDK callback may change it

@router.get("/{device_id}", response_model=Dict[str, str])
def get_device(
    camera: Camera = Depends(get_camera),
    #user: dict = Depends(get_current_user) -> enable this if security is needed
):
    """
    Get details of a specific RealSense device.
    """
    return camera.info


@router.get("/{device_id}/is_metadata_enabled", response_model=bool)
def is_metadata_enabled(
    camera: Camera = Depends(get_camera),
):
    """
    Whether the device's frames carry their metadata: false only on Windows until it is enabled there.
    """
    return camera.is_metadata_enabled()


@router.get("/{device_id}/extrinsics", response_model=Extrinsics)
def get_extrinsics(
    from_stream: StreamName,
    from_index: int,
    to_stream: StreamName,
    to_index: int,
    camera: Camera = Depends(get_camera),
):
    """
    The device's calibration from one stream to another, whether streaming or not: as the SDK's
    get_extrinsics_to. Streams are rs2_stream names (any other answers 422), e.g. depth/0 to color/0.
    """
    return camera.extrinsics((from_stream, from_index), (to_stream, to_index))

@router.post("/{device_id}/hardware_reset")
def hardware_reset(
    camera: Camera = Depends(get_camera),
    #user: dict = Depends(get_current_user) -> enable this if security is needed
):
    """
    Perform a hardware reset on a specific RealSense device.
    """
    camera.hardware_reset()
