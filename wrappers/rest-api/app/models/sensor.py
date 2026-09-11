# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from pydantic import BaseModel
from typing import List, Optional, Tuple

from app.models.option import OptionInfo

class SensorBase(BaseModel):
    name: str
    type: str  # color, depth, IMU, etc.

class SensorCreate(SensorBase):
    pass

class Sensor(SensorBase):
    sensor_id: str
    supported_formats: List[str] = []
    options: List[str] = []

    class Config:
        from_attributes = True

class DefaultProfile(BaseModel):
    """The profile the SDK marks default for a stream - what the viewer starts with."""
    resolution: tuple[int, int]
    fps: int
    format: str

class SupportedStreamProfile(BaseModel):
    stream_type: str
    resolutions: List[tuple[int, int]] # List of tuples (width, height)
    fps: List[int] # List of frames per second
    formats: List[str] # List of supported formats
    default: Optional[DefaultProfile] = None
    modes: List[Tuple[int, int, int, str]] = []  # every (width, height, fps, format) the SDK lists

class SensorInfo(BaseModel):
    sensor_id: str
    name: str
    type: str
    supported_stream_profiles: List[SupportedStreamProfile] = []
    options: List[OptionInfo] = []