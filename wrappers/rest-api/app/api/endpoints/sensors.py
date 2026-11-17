# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
REST API endpoints for sensor enumeration and per-sensor streaming control.

The streaming endpoints use the RealSense sensor API, which provides finer-grained
control than the pipeline-based /streams/* endpoints, allowing individual sensors
(depth, color, IMU) to be started/stopped independently.
"""

from fastapi import APIRouter, Depends, HTTPException
from typing import List

from app.models.sensor import SensorInfo, SensorStartRequest
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


@router.get("/{sensor_id}", response_model=SensorInfo)
def get_sensor_info(
    sensor: Sensor = Depends(get_sensor),
):
    """
    Get details of a specific sensor for a RealSense device.
    """
    return sensor.info()


@router.post("/{sensor_id}/start")
def start_sensor(
    request: SensorStartRequest,
    camera: Camera = Depends(get_camera),
    sensor: Sensor = Depends(get_sensor),
):
    """
    Start streaming from a specific sensor using the sensor API.

    Opens the sensor with the given profiles from its `supported_stream_profiles`
    (several for e.g. depth + IR).

    **Note:** Cannot be used simultaneously with pipeline API (/streams/start).
    Stop all streams before switching between APIs.
    """
    try:
        camera.open_sensor(sensor.index, request.profiles)
    except Exception as e:  # a sensor another client holds refuses open(), and is not ours to close
        raise HTTPException(status_code=500, detail=f"Failed to start sensor: {e}")
    try:
        camera.start_sensor(sensor.index)
    except Exception as e:
        camera.close_sensor(sensor.index)
        raise HTTPException(status_code=500, detail=f"Failed to start sensor: {e}")
    sensor.started = True


@router.post("/{sensor_id}/stop")
def stop_sensor(
    camera: Camera = Depends(get_camera),
    sensor: Sensor = Depends(get_sensor),
):
    """
    Stop streaming from a specific sensor.

    The sensor will be stopped and closed, freeing its resources.
    Other sensors on the same device will continue streaming.
    A sensor the pipeline API runs is refused with 409: stop it with /stream/stop.
    """
    if not sensor.started:
        if sensor.active_profiles():
            raise HTTPException(status_code=409,
                                detail="The sensor streams through the pipeline; stop it with /stream/stop")
        return
    camera.stop_sensor(sensor.index)
    camera.close_sensor(sensor.index)
    sensor.started = False
