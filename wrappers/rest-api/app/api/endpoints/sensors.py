# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
REST API endpoints for sensor enumeration and per-sensor streaming control.

The streaming endpoints use the RealSense sensor API, which provides finer-grained
control than the pipeline-based /streams/* endpoints, allowing individual sensors
(depth, color, IMU) to be started/stopped independently.
"""

import functools
from fastapi import APIRouter, Depends, HTTPException
from typing import List

from app.models.sensor import SensorInfo, SensorStartRequest
from app.services.rs_manager import RealSenseManager
from app.api.dependencies import get_realsense_manager

router = APIRouter()


def _handle_rs_exception(e: Exception, default_status: int = 400) -> None:
    if hasattr(e, 'status_code'):
        raise HTTPException(status_code=e.status_code, detail=str(e.detail))
    raise HTTPException(status_code=default_status, detail=str(e))


def rs_exception_handler(default_status: int = 400):
    """Convert any non-HTTPException raised by RealSenseManager into an HTTPException.

    Eliminates the repeated `try / except Exception: _handle_rs_exception(e, ...)` shell
    around every endpoint body. HTTPExceptions raised inside the wrapped function (e.g.,
    explicit 400s for bad input) propagate unchanged.
    """
    def decorator(fn):
        @functools.wraps(fn)
        async def wrapper(*args, **kwargs):
            try:
                return await fn(*args, **kwargs)
            except HTTPException:
                raise
            except Exception as e:
                _handle_rs_exception(e, default_status=default_status)

        return wrapper

    return decorator


@router.get("/", response_model=List[SensorInfo])
@rs_exception_handler(default_status=404)
async def get_sensors(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """
    Get a list of all sensors for a specific RealSense device.
    """
    return rs_manager.get_sensors(device_id)


@router.get("/{sensor_id}", response_model=SensorInfo)
@rs_exception_handler(default_status=404)
async def get_sensor(
    device_id: str,
    sensor_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """
    Get details of a specific sensor for a RealSense device.
    """
    return rs_manager.get_sensor(device_id, sensor_id)


@router.post("/{sensor_id}/start")
@rs_exception_handler()
async def start_sensor(
    device_id: str,
    sensor_id: str,
    request: SensorStartRequest,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """
    Start streaming from a specific sensor using the sensor API.

    Opens the sensor with the given profiles from its `supported_stream_profiles`
    (several for e.g. depth + IR).

    **Note:** Cannot be used simultaneously with pipeline API (/streams/start).
    Stop all streams before switching between APIs.
    """
    return rs_manager.start_sensor(device_id, sensor_id, request.profiles)


@router.post("/{sensor_id}/stop")
@rs_exception_handler()
async def stop_sensor(
    device_id: str,
    sensor_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """
    Stop streaming from a specific sensor.

    The sensor will be stopped and closed, freeing its resources.
    Other sensors on the same device will continue streaming.
    """
    return rs_manager.stop_sensor(device_id, sensor_id)
