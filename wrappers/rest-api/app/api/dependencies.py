# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import Depends
from fastapi.security import OAuth2PasswordBearer
from typing import Generator


from app.services.rs_manager import RealSenseManager
from app.services.webrtc_manager import WebRTCManager
from app.services.socketio import sio

# OAuth2 setup
oauth2_scheme = OAuth2PasswordBearer(tokenUrl="token")

# Singleton instances
_realsense_manager = None
_webrtc_manager = None

def get_realsense_manager() -> RealSenseManager:
    global _realsense_manager
    if _realsense_manager is None:
        _realsense_manager = RealSenseManager(sio)
    return _realsense_manager

def get_webrtc_manager() -> WebRTCManager:
    global _webrtc_manager
    if _webrtc_manager is None:
        _webrtc_manager = WebRTCManager(get_realsense_manager())
    return _webrtc_manager