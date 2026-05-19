# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Models for per-sensor streaming control using the RealSense sensor API.

This provides finer-grained control than the pipeline API, allowing
individual sensors (depth, color, IMU) to be started/stopped independently.
"""

from pydantic import BaseModel
from typing import List, Optional
from datetime import datetime

from app.models.stream import Resolution


class SensorStreamConfig(BaseModel):
    """Configuration for starting a single sensor stream."""
    stream_type: str           # e.g., "depth", "color", "infrared-1", "gyro", "accel"
    format: str                # e.g., "z16", "rgb8", "y8", "combined_motion"
    resolution: Resolution     # {width, height}
    framerate: int             # e.g., 30


class SensorStartRequest(BaseModel):
    """Request body for starting a single sensor.
    
    Supports both single config (backward compat) and multiple configs
    for opening a sensor with multiple stream profiles (e.g., depth + IR).
    """
    config: Optional[SensorStreamConfig] = None   # Legacy: single stream
    configs: Optional[List[SensorStreamConfig]] = None  # New: multiple streams


class SensorStartItem(BaseModel):
    """A single sensor configuration for batch operations."""
    sensor_id: str
    config: Optional[SensorStreamConfig] = None   # Legacy: single stream  
    configs: Optional[List[SensorStreamConfig]] = None  # New: multiple streams


class BatchSensorStartRequest(BaseModel):
    """Request body for starting multiple sensors atomically."""
    sensors: List[SensorStartItem]


class BatchSensorStopRequest(BaseModel):
    """Request body for stopping multiple sensors."""
    sensor_ids: Optional[List[str]] = None  # None = stop all sensors


class SensorStreamStatus(BaseModel):
    """Status of a single sensor's streaming state."""
    sensor_id: str
    name: str = ""
    is_streaming: bool
    # Single stream_type for backward compatibility (first stream)
    stream_type: Optional[str] = None
    resolution: Optional[Resolution] = None
    framerate: Optional[int] = None
    format: Optional[str] = None
    # New: multiple streams support
    stream_types: List[str] = []  # All active stream types
    streams: List[SensorStreamConfig] = []  # All active stream configs
    error: Optional[str] = None
    started_at: Optional[datetime] = None


class BatchSensorStatus(BaseModel):
    """Status of all sensors on a device."""
    device_id: str
    mode: str  # "sensor_api" | "pipeline_api" | "idle"
    sensors: List[SensorStreamStatus]
    errors: List[str] = []