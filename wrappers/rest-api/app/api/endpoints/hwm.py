# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import struct

from fastapi import APIRouter, Depends, HTTPException

from app.api.dependencies import get_camera
from app.models.hwm import HwmRequest, HwmResponse
from app.services.camera import Camera

router = APIRouter()


@router.post("/", response_model=HwmResponse)
def send_hwm_command(
    request: HwmRequest,
    camera: Camera = Depends(get_camera),
):
    """
    Send a hardware monitor (HWM) command to a specific RealSense device.

    Requires the device to support the ``RS2_EXTENSION_DEBUG`` extension.
    Returns 400 if the device does not support hardware monitor commands.
    """
    # is_debug_protocol() is the correct way to check extension support before casting.
    # as_debug_protocol() does not raise when unsupported — it returns an empty handle
    # whose methods would fail later with a harder-to-diagnose error.
    if not camera.dev.is_debug_protocol():
        raise HTTPException(status_code=400,
                            detail=f"Device {camera.id} does not support hardware monitor commands")
    try:
        response = camera.send_hwm_command(request)
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"HWM command failed: {e}")
    # The raw response starts with a 4-byte little-endian uint32 that echoes
    # the sent opcode on success or contains a firmware error code on failure.
    # send_and_receive_raw_data does not raise on firmware-level errors, so we
    # must inspect the opcode ourselves.
    if len(response) < 4:
        raise HTTPException(status_code=500, detail="HWM command failed: response too short")
    response_opcode, = struct.unpack_from('<I', bytes(response[:4]))
    if response_opcode != request.opcode:
        raise HTTPException(
            status_code=500,
            detail=f"HWM command failed: firmware returned error code 0x{response_opcode:08X} "
                   f"(expected opcode echo 0x{request.opcode:08X})",
        )
    return HwmResponse(device_id=camera.id, response=response)
