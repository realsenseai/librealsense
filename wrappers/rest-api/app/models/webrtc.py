# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from pydantic import BaseModel
from typing import Dict, List, Optional, Any

class WebRTCOffer(BaseModel):
    device_id: str
    stream_types: List[str]  # Types of streams to include (color, depth, etc.)

class WebRTCSession(BaseModel):
    session_id: str
    device_id: str
    stream_types: List[str]

class WebRTCAnswer(BaseModel):
    session_id: str
    sdp: str
    type: str

class ICECandidate(BaseModel):
    session_id: str
    candidate: str
    sdpMid: str
    sdpMLineIndex: int

class WebRTCStatus(BaseModel):
    session_id: str
    device_id: str
    connected: bool
    streaming: bool
    stream_types: List[str]
    stats: Optional[Dict[str, Any]] = None