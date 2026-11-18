# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import Depends
from fastapi.security import OAuth2PasswordBearer


from app.services.camera import Camera
from app.services.sensor import Sensor
from app.services.rs_manager import RealSenseManager

# OAuth2 setup
oauth2_scheme = OAuth2PasswordBearer(tokenUrl="token")

# One, built before any request: requests run on a thread pool, and two building it at once would leak one
_realsense_manager = RealSenseManager()


def get_realsense_manager() -> RealSenseManager:
    return _realsense_manager


def get_camera(device_id: str, rs_manager: RealSenseManager = Depends(get_realsense_manager)) -> Camera:
    """The connected camera a request's path names; 404 if none"""
    return rs_manager.cameras[device_id]


def get_sensor(sensor_id: str, camera: Camera = Depends(get_camera)) -> Sensor:
    """The sensor of that camera a request's path names; 404 if none"""
    return {s.id: s for s in camera.sensors}[sensor_id]
