# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pyrealsense2 as rs
from fastapi import APIRouter, Depends, WebSocket

from app.api.dependencies import get_realsense_manager
from app.services.rs_manager import RealSenseManager

router = APIRouter()


@router.websocket("/{stream_type}/{stream_index}")
async def stream_frames(
    websocket: WebSocket,
    device_id: str,
    stream_type: str,
    stream_index: int,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """The stream's frames as the SDK delivers them; see app/services/frame_sender.py for the message layout.

    stream_type and stream_index are as in the sensor's stream profiles, e.g. depth/0 or infrared/1.
    """
    stream = rs.stream.__members__.get(stream_type)
    if stream is None:
        await websocket.close()  # before accept, uvicorn answers 403 and drops code and reason
        return
    await websocket.accept()
    await rs_manager.frame_sender.serve(websocket, device_id, stream, stream_index,
                                        lambda: rs_manager.is_streaming(device_id, stream, stream_index))
