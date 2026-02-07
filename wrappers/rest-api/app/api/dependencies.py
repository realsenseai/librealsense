from fastapi import Depends, HTTPException, status
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

async def get_current_user(token: str = Depends(oauth2_scheme)) -> dict:
    """
    Validate the OAuth2 token and return the current user.
    This enforces authentication on all protected endpoints.
    """
    if not token:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Not authenticated",
            headers={"WWW-Authenticate": "Bearer"},
        )
    # Token validation logic should be implemented here
    # For now, we enforce that a token must be present
    # In production, validate token against your auth service/database
    return {"token": token}

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