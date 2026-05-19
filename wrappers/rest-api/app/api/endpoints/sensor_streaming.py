# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
REST API endpoints for per-sensor streaming control using the RealSense sensor API.

These endpoints provide finer-grained control than the pipeline-based /streams/* endpoints,
allowing individual sensors (depth, color, IMU) to be started/stopped independently.
"""

import functools
from fastapi import APIRouter, Depends, HTTPException
from typing import List, Optional

from app.models.sensor_streaming import (
    SensorStartRequest,
    SensorStreamStatus,
    BatchSensorStartRequest,
    BatchSensorStopRequest,
    BatchSensorStatus,
)
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


# =============================================================================
# Per-Sensor Endpoints
# =============================================================================

@router.post("/{sensor_id}/start", response_model=SensorStreamStatus)
@rs_exception_handler()
async def start_sensor(
    device_id: str,
    sensor_id: str,
    request: SensorStartRequest,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """
    Start streaming from a specific sensor using the sensor API.

    Supports both single stream (backward compat) and multiple streams
    for opening a sensor with multiple profiles (e.g., depth + IR).

    **Note:** Cannot be used simultaneously with pipeline API (/streams/start).
    Stop all streams before switching between APIs.
    """
    # Support both single config (backward compat) and multi-config
    if request.configs:
        configs = request.configs
    elif request.config:
        configs = [request.config]
    else:
        raise HTTPException(status_code=400, detail="config or configs required")

    return rs_manager.start_sensor(device_id, sensor_id, configs)


@router.post("/{sensor_id}/stop", response_model=SensorStreamStatus)
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


@router.get("/{sensor_id}/status", response_model=SensorStreamStatus)
@rs_exception_handler(default_status=404)
async def get_sensor_status(
    device_id: str,
    sensor_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """
    Get streaming status for a specific sensor.

    Returns information about whether the sensor is streaming,
    and if so, what configuration it is using.
    """
    return rs_manager.get_sensor_status(device_id, sensor_id)


# =============================================================================
# Batch Sensor Endpoints
# =============================================================================

@router.post("/batch/start", response_model=BatchSensorStatus)
@rs_exception_handler()
async def batch_start_sensors(
    device_id: str,
    request: BatchSensorStartRequest,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """
    Start multiple sensors atomically.

    All specified sensors will be started in order. If any sensor fails to start,
    all previously started sensors will be stopped (rollback).

    This is useful for starting depth + color together while maintaining
    independent control.
    """
    return rs_manager.batch_start_sensors(device_id, request.sensors)


@router.post("/batch/stop", response_model=BatchSensorStatus)
@rs_exception_handler()
async def batch_stop_sensors(
    device_id: str,
    request: BatchSensorStopRequest = None,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """
    Stop multiple sensors.

    If sensor_ids is provided, only those sensors will be stopped.
    If sensor_ids is None or empty, all streaming sensors will be stopped.
    """
    sensor_ids = request.sensor_ids if request else None
    return rs_manager.batch_stop_sensors(device_id, sensor_ids)


@router.get("/batch/status", response_model=BatchSensorStatus)
@rs_exception_handler(default_status=404)
async def get_batch_status(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """
    Get streaming status for all sensors on a device.

    Returns the current streaming mode (sensor_api, pipeline_api, or idle)
    and the status of each sensor.
    """
    return rs_manager.get_batch_status(device_id)
