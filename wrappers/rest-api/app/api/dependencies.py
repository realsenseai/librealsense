# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import Depends
from fastapi.security import OAuth2PasswordBearer
from typing import Generator


from app.services.rs_manager import RealSenseManager

# OAuth2 setup
oauth2_scheme = OAuth2PasswordBearer(tokenUrl="token")

# Singleton instances
_realsense_manager = None

def get_realsense_manager() -> RealSenseManager:
    global _realsense_manager
    if _realsense_manager is None:
        _realsense_manager = RealSenseManager()
    return _realsense_manager