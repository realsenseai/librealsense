# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
REST API endpoints for sensor enumeration and per-sensor streaming control.

The streaming endpoints use the RealSense sensor API, which provides finer-grained
control than the pipeline-based /streams/* endpoints, allowing individual sensors
(depth, color, IMU) to be started/stopped independently.
"""

from fastapi import APIRouter, Depends, Path
from typing import Annotated, List

from app.models.sensor import SensorInfo, SensorOpenRequest
from app.api.dependencies import get_camera, get_sensor
from app.services.camera import Camera
from app.services.sensor import Sensor

router = APIRouter()


@router.get("/", response_model=List[SensorInfo])
def get_sensors(
    camera: Camera = Depends(get_camera),
):
    """
    Get a list of all sensors for a specific RealSense device.
    """
    return [s.info() for s in camera.sensors]


@router.get("/{sensor_index}", response_model=SensorInfo)
def get_sensor_info(
    sensor: Sensor = Depends(get_sensor),
):
    """
    Get details of a specific sensor for a RealSense device.
    """
    return sensor.info()


@router.post("/{sensor_index}/open")
def open_sensor(
    sensor_index: Annotated[int, Path(ge=0)],
    request: SensorOpenRequest,
    camera: Camera = Depends(get_camera),
):
    """
    Open a sensor with profiles from its `supported_stream_profiles` (several for e.g. depth + IR),
    as the SDK's sensor.open() does; start it next.
    """
    return camera.open_sensor(sensor_index, request.profiles)


@router.post("/{sensor_index}/start")
def start_sensor(
    sensor_index: Annotated[int, Path(ge=0)],
    camera: Camera = Depends(get_camera),
):
    """Start streaming from an opened sensor, as the SDK's sensor.start() does."""
    return camera.start_sensor(sensor_index)


@router.post("/{sensor_index}/stop")
def stop_sensor(
    sensor_index: Annotated[int, Path(ge=0)],
    camera: Camera = Depends(get_camera),
):
    """Stop streaming from a sensor, as the SDK's sensor.stop() does; it stays open. Others keep streaming."""
    return camera.stop_sensor(sensor_index)


@router.post("/{sensor_index}/close")
def close_sensor(
    sensor_index: Annotated[int, Path(ge=0)],
    camera: Camera = Depends(get_camera),
):
    """Close a stopped sensor, freeing it, as the SDK's sensor.close() does."""
    return camera.close_sensor(sensor_index)
