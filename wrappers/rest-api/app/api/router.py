# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from typing import List

from fastapi import APIRouter
from app.api.endpoints import advanced_mode, devices, firmware, hwm, options, point_cloud, sensors, streams, system, webrtc


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


def _check_debug_sdk_build() -> str:
    """Warn when the loaded pyrealsense2 is a Debug build from this repo's build/.

    Only path components below the repo build directory are examined, so a
    checkout or venv that merely lives under a directory named "Debug" is not
    flagged.
    """
    from pathlib import Path
    import pyrealsense2 as rs
    module_file = getattr(getattr(rs, "pyrealsense2", rs), "__file__", "") or ""
    if not module_file:
        return ""
    build_dir = Path(__file__).resolve().parents[4] / "build"
    try:
        relative = Path(module_file).resolve().relative_to(build_dir)
    except ValueError:
        return ""  # not loaded from this repo's build tree
    if any(part.lower() == "debug" for part in relative.parts):
        return (
            "The server is running a Debug build of the RealSense SDK - "
            "streaming performance is degraded. Build Release for full speed."
        )
    return ""


_WARNING_CHECKS = (_check_debug_sdk_build,)


def _get_sdk_warnings() -> List[str]:
    """Collect environment warnings worth surfacing in the client UI."""
    warnings = []
    for check in _WARNING_CHECKS:
        try:
            message = check()
        except Exception:
            continue
        if message:
            warnings.append(message)
    return warnings


_SDK_VERSION = _get_sdk_version()
_SDK_WARNINGS = _get_sdk_warnings()

api_router = APIRouter()

# Health check endpoint
@api_router.get("/health")
async def health_check():
    """Health check endpoint for monitoring the backend service.

    Returns the installed RealSense SDK version so the frontend can show a
    welcome banner the first time the user opens it on a new SDK version, and
    environment warnings (e.g. Debug SDK build) for the client to display.
    """
    return {
        "status": "ok",
        "service": "realsense-api",
        "sdk_version": _SDK_VERSION,
        "warnings": _SDK_WARNINGS,
    }

api_router.include_router(devices.router, prefix="/devices", tags=["devices"])
api_router.include_router(advanced_mode.router, prefix="/devices/{device_id}/advanced_mode", tags=["advanced_mode"])
api_router.include_router(firmware.router, prefix="/devices/{device_id}/firmware", tags=["firmware"])
api_router.include_router(hwm.router, prefix="/devices/{device_id}/hwm", tags=["hwm"])
api_router.include_router(point_cloud.router, prefix="/devices/{device_id}/point_cloud", tags=["point_cloud"])
api_router.include_router(sensors.router, prefix="/devices/{device_id}/sensors", tags=["sensors"])
api_router.include_router(options.router, prefix="/devices/{device_id}/sensors/{sensor_id}/options", tags=["options"])
api_router.include_router(streams.router, prefix="/devices/{device_id}/stream", tags=["streams"])
api_router.include_router(system.router, prefix="/system", tags=["system"])
api_router.include_router(webrtc.router, prefix="/webrtc", tags=["webrtc"])
