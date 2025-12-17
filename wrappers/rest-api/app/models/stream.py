from pydantic import BaseModel, Field
from typing import List, Optional, Tuple

class Resolution(BaseModel):
    width: int
    height: int

class StreamConfigBase(BaseModel):
    stream_type: str
    format: str  # format/encoding
    resolution: Resolution
    framerate: int

class StreamConfig(StreamConfigBase):
    sensor_id: str
    enable: bool = True

class StreamStart(BaseModel):
    configs: List[StreamConfig]
    align_to: Optional[str] = None  # Align to specific stream
    apply_filters: bool = False
    reuse_cache: bool = True  # allow callers to force rebuild when needed

class StreamStatus(BaseModel):
    device_id: str
    is_streaming: bool
    active_streams: List[str] = []
    framerate: Optional[float] = None
    duration: Optional[float] = None  # Time in seconds

class PointCloudStatus(BaseModel):
    device_id: str
    is_active: bool

class StreamStartTiming(BaseModel):
    device_id: str
    is_streaming: bool
    active_streams: List[str] = []
    timings: dict
    framerate: Optional[float] = None
    duration: Optional[float] = None  # Time in seconds
    config_reused: Optional[bool] = None
    config_signature: Optional[str] = None