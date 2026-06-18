# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import logging

from fastapi import APIRouter, Depends, HTTPException

from app.api.dependencies import get_realsense_manager
from app.models.hwm import HwmRequest, HwmResponse
from app.services.rs_manager import RealSenseManager, RealSenseError

router = APIRouter()


@router.post("/", response_model=HwmResponse)
async def send_hwm_command(
    device_id: str,
    request: HwmRequest,
    rs_manager: RealSenseManager = Depends(get_realsense_manager)
):
    """
    Send a hardware monitor (HWM) command to a specific RealSense device.

    Requires the device to support the ``RS2_EXTENSION_DEBUG`` extension.
    Returns 400 if the device does not support hardware monitor commands.
    """
    try:
        response_bytes = rs_manager.send_hwm_command(
            device_id,
            request.opcode,
            request.param1,
            request.param2,
            request.param3,
            request.param4,
            request.data,
        )
        return HwmResponse(device_id=device_id, response=response_bytes)
    except RealSenseError as e:
        raise HTTPException(status_code=e.status_code, detail=e.detail)
    except Exception as e:
        logging.exception("Unexpected error sending HWM command to %s", device_id)
        raise HTTPException(status_code=500, detail=f"Unexpected error while sending HWM command: {e}")
