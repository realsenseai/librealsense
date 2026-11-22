# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter
from app.api.endpoints import (advanced_mode, colorizer, devices, events, filters, firmware, frames, health, hwm, options,
                               sensors, streams, system)

api_router = APIRouter()
api_router.include_router(health.router, tags=["health"])
api_router.include_router(events.router, tags=["events"])
api_router.include_router(devices.router, prefix="/devices", tags=["devices"])
api_router.include_router(advanced_mode.router, prefix="/devices/{device_id}/advanced_mode", tags=["advanced_mode"])
api_router.include_router(colorizer.router, prefix="/devices/{device_id}/colorizer", tags=["colorizer"])
api_router.include_router(filters.router, prefix="/devices/{device_id}/sensors/{sensor_index}/filters", tags=["filters"])
api_router.include_router(firmware.router, prefix="/devices/{device_id}/firmware", tags=["firmware"])
api_router.include_router(frames.router, prefix="/devices/{device_id}/frames", tags=["frames"])
api_router.include_router(hwm.router, prefix="/devices/{device_id}/hwm", tags=["hwm"])
api_router.include_router(sensors.router, prefix="/devices/{device_id}/sensors", tags=["sensors"])
api_router.include_router(options.router, prefix="/devices/{device_id}/sensors/{sensor_index}/options", tags=["options"])
api_router.include_router(streams.router, prefix="/devices/{device_id}/stream", tags=["streams"])
api_router.include_router(system.router, prefix="/system", tags=["system"])
