# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from pydantic import AfterValidator, BaseModel, Field
from typing import Annotated, List, Literal
import pyrealsense2 as rs

StreamName = Literal[tuple(rs.stream.__members__)]  # an rs2_stream name: nothing else gets through
StreamType = Annotated[StreamName, AfterValidator(rs.stream.__members__.get)]
Format = Annotated[Literal[tuple(rs.format.__members__)], AfterValidator(rs.format.__members__.get)]

class StreamConfig(BaseModel):
    """The arguments of rs.config.enable_stream, defaulting to any like its overloads do."""
    stream: StreamType
    index: int = -1
    width: int = 0
    height: int = 0
    format: Format = Field("any", validate_default=True)
    fps: int = 0

class Extrinsics(BaseModel):
    """From one stream to another, as rs2_extrinsics"""
    rotation: List[float]  # 3x3, column-major
    translation: List[float]  # meters
