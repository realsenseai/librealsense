# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from pydantic import BaseModel
from typing import List

class StreamProfile(BaseModel):
    stream_type: str
    stream_index: int
    format: str
    width: int  # 0 for motion, as the SDK reports it
    height: int
    fps: int
    default: bool = False  # the SDK's is_default: the server tells it, an open request may leave it out

class SensorOpenRequest(BaseModel):
    profiles: List[StreamProfile]  # from the sensor's stream_profiles, e.g. depth + IR

class SensorInfo(BaseModel):
    sensor_index: int  # in the device's sensors, as the SDK lists them
    name: str
    stream_profiles: List[StreamProfile]
    active_streams: List[StreamProfile]  # what it is opened with, by any API, from open to close
