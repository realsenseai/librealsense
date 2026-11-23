# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter, Depends, WebSocket

from app.api.dependencies import get_realsense_manager
from app.models.stream import StreamType
from app.services.rs_manager import RealSenseManager

router = APIRouter()


@router.websocket("/{stream_type}/{stream_index}")
async def stream_frames(
    websocket: WebSocket,
    device_id: str,
    stream_type: StreamType,
    stream_index: int,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """The stream's frames as the SDK delivers them; see app/services/frame_sender.py for the message layout.

    stream_type and stream_index are as in the sensor's stream profiles, e.g. depth/0 or infrared/1.
    """
    await websocket.accept()
    await rs_manager.frame_sender.serve(websocket, device_id, stream_type, stream_index)
