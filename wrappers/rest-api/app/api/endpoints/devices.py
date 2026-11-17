# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import platform
from fastapi import APIRouter, Depends
from typing import List


from app.models.device import DeviceInfo
from app.models.stream import StreamExtrinsics
from app.services.rs_manager import RealSenseManager
from app.api.dependencies import get_camera, get_realsense_manager
from app.services.camera import Camera

router = APIRouter()


def _device_info(camera: Camera) -> DeviceInfo:
    metadata_enabled = None
    if platform.system() == "Windows":
        try:
            metadata_enabled = camera.is_metadata_enabled()
        except RuntimeError:
            pass
    return DeviceInfo(
        device_id=camera.id,
        name=camera.info.get("name", "Unknown Device"),
        serial_number=camera.id,
        firmware_version=camera.info.get("firmware_version"),
        physical_port=camera.info.get("physical_port"),
        usb_type=camera.info.get("usb_type_descriptor"),
        product_id=camera.info.get("product_id"),
        sensors=[s.name for s in camera.sensors],
        metadata_enabled=metadata_enabled,
    )


@router.get("/", response_model=List[DeviceInfo])
def get_devices(
    force_refresh: bool = False,  # nothing to do: the SDK's devices-changed callback keeps the list current
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
    #user: dict = Depends(get_current_user)  -> enable this if security is needed
):
    """
    Get a list of all connected RealSense devices.
    """
    return [_device_info(c) for c in list(rs_manager.cameras.values())]  # a copy: the SDK callback may change it

@router.get("/{device_id}", response_model=DeviceInfo)
def get_device(
    force_refresh: bool = False,  # nothing to do, as above
    camera: Camera = Depends(get_camera),
    #user: dict = Depends(get_current_user) -> enable this if security is needed
):
    """
    Get details of a specific RealSense device.
    """
    return _device_info(camera)


@router.post("/refresh", response_model=List[DeviceInfo])
def refresh_devices(
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
    #user: dict = Depends(get_current_user)  -> enable this if security is needed
):
    """
    Return the device list; the SDK's devices-changed callback keeps it current.
    """
    return get_devices(rs_manager=rs_manager)

@router.get("/{device_id}/extrinsics", response_model=List[StreamExtrinsics])
def get_extrinsics(
    camera: Camera = Depends(get_camera),
):
    """
    The device's calibration between its streams: from each stream to every other, whether streaming or not.
    """
    return camera.extrinsics()

@router.post("/{device_id}/hw_reset/", response_model=bool)
def hw_reset_device(
    camera: Camera = Depends(get_camera),
    #user: dict = Depends(get_current_user) -> enable this if security is needed
):
    """
    Perform a hardware reset on a specific RealSense device.
    """
    camera.hardware_reset()
    return True
