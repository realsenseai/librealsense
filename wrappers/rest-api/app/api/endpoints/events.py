# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter, Depends, WebSocket

from app.api.dependencies import get_realsense_manager
from app.services.rs_manager import RealSenseManager

router = APIRouter()


@router.websocket("/events")
async def events(websocket: WebSocket, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """What changes on the server, as JSON messages; see app/services/events.py for the events."""
    await websocket.accept()
    await rs_manager.events.serve(websocket)
