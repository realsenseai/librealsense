# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter
from app.api.endpoints import devices, sensors, options, streams, webrtc, point_cloud, firmware, sensor_streaming, hwm, system


def _get_sdk_version() -> str:
    """Return the version of the actually-loaded pyrealsense2 binary, or 'unknown'.

    Read it from the loaded module (RS2_API_*, bound as __full_version__) rather than
    pip metadata: main.py may load a locally-built extension whose version differs from
    any installed wheel. The source-built wrapper re-exports __full_version__ on the
    package, while the PyPI wheel exposes it only on the extension submodule, so check
    the inner module first, then the package.
    """
    try:
        import pyrealsense2 as rs
        return getattr(getattr(rs, "pyrealsense2", rs), "__full_version__", "unknown")
    except Exception:
        return "unknown"


_SDK_VERSION = _get_sdk_version()

api_router = APIRouter()

# Health check endpoint
@api_router.get("/health")
async def health_check():
    """Health check endpoint for monitoring the backend service.

    Returns the installed RealSense SDK version so the frontend can show a
    welcome banner the first time the user opens it on a new SDK version.
    """
    return {"status": "ok", "service": "realsense-api", "sdk_version": _SDK_VERSION}

# Register firmware route before devices to avoid conflicts with /{device_id} catch-all
api_router.include_router(firmware.router, prefix="/devices", tags=["firmware"])
api_router.include_router(devices.router, prefix="/devices", tags=["devices"])
api_router.include_router(sensors.router, prefix="/devices/{device_id}/sensors", tags=["sensors"])
api_router.include_router(options.router, prefix="/devices/{device_id}/sensors/{sensor_id}/options", tags=["options"])
api_router.include_router(hwm.router, prefix="/devices/{device_id}/hwm", tags=["hwm"])
api_router.include_router(streams.router, prefix="/devices/{device_id}/stream", tags=["streams"])
api_router.include_router(point_cloud.router, prefix="/devices/{device_id}/point_cloud", tags=["point_cloud"])
api_router.include_router(webrtc.router, prefix="/webrtc", tags=["webrtc"])
api_router.include_router(system.router, prefix="/system", tags=["system"])

# Per-sensor streaming control (sensor API - independent sensor start/stop)
api_router.include_router(
    sensor_streaming.router, 
    prefix="/devices/{device_id}/sensors", 
    tags=["sensor-streaming"]
)