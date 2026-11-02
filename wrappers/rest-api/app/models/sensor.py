# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from pydantic import BaseModel
from typing import List

from app.models.option import OptionInfo

class StreamProfile(BaseModel):
    stream_type: str
    stream_index: int
    format: str
    width: int  # 0 for motion, as the SDK reports it
    height: int
    fps: int
    default: bool

class SensorStartRequest(BaseModel):
    profiles: List[StreamProfile]  # from the sensor's supported_stream_profiles, e.g. depth + IR

class SensorInfo(BaseModel):
    sensor_id: str
    name: str
    type: str
    supported_stream_profiles: List[StreamProfile] = []
    options: List[OptionInfo] = []
