# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from pathlib import Path

import pyrealsense2 as rs
from fastapi import APIRouter


def _check_debug_sdk_build() -> str:
    """Warn when the loaded pyrealsense2 is a Debug build from this repo's build/. Only the path parts below
    build/ are checked, so a venv living under a directory named Debug is not flagged"""
    build_dir = Path(__file__).resolve().parents[5] / "build"
    module_file = Path(rs.__file__).resolve()
    if not module_file.is_relative_to(build_dir):
        return ""
    if any(part.lower() == "debug" for part in module_file.relative_to(build_dir).parts):
        return ("The server is running a Debug build of the RealSense SDK - "
                "streaming performance is degraded. Build Release for full speed.")
    return ""


# The loaded binary's version, not pip metadata: main.py may load a local build. Wheels before de49033e6
# have it only on the inner module
_SDK_VERSION = getattr(getattr(rs, "pyrealsense2", rs), "__full_version__", "unknown")
SDK_WARNINGS = [w for w in [_check_debug_sdk_build()] if w]

router = APIRouter()


@router.get("/health")
def health_check():
    """sdk_version drives the viewer's first-run banner; warnings (e.g. a Debug SDK build) are shown by the client"""
    return {
        "sdk_version": _SDK_VERSION,
        "warnings": SDK_WARNINGS,
    }
