# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from pydantic import AfterValidator, BaseModel, Field
from typing import Annotated, List, Literal
import pyrealsense2 as rs

StreamType = Annotated[Literal[tuple(rs.stream.__members__)], AfterValidator(rs.stream.__members__.get)]
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
    """To the stream named by stream_type (an rs2_stream name, as in StreamProfile) and stream_index."""
    stream_type: str
    stream_index: int
    rotation: List[float]  # 3x3, column-major, as rs2_extrinsics
    translation: List[float]  # meters

class StreamExtrinsics(BaseModel):
    """From one stream to every other stream of its device it is calibrated with."""
    stream_type: str
    stream_index: int
    to: List[Extrinsics]
