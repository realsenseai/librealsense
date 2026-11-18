# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import sys
from pathlib import Path


def _prefer_local_pyrealsense2() -> None:
    # Prefer locally-built pyrealsense2 (e.g. build/Release/) over any PyPI
    # wheel — newer devices (e.g. D585 Prototype, PID 0x0C08) may not be
    # recognized by the published wheel and only show up when loading the
    # module built from this branch.
    import os
    repo_root = Path(__file__).resolve().parent.parent.parent
    build_dir = repo_root / "build"
    if not build_dir.is_dir():
        return
    candidates = []
    for pattern in ("pyrealsense2*.pyd", "pyrealsense2*.so"):
        candidates.extend(build_dir.rglob(pattern))
    candidates.extend(build_dir.rglob("pyrealsense2/__init__.py"))
    if not candidates:
        return
    candidates.sort(key=lambda p: p.stat().st_mtime, reverse=True)
    seen, target_dirs = set(), []
    for path in candidates:
        target_dir = path.parent.parent if path.name == "__init__.py" else path.parent
        key = str(target_dir)
        if key in seen:
            continue
        seen.add(key)
        target_dirs.append(key)
    sys.path[0:0] = target_dirs
    # print(), not logging.info(): this runs before logging is configured,
    # and an unconfigured root logger drops INFO records.
    print(f"[pyrealsense2] preferring local build: {target_dirs[0]}")
    if hasattr(os, "add_dll_directory"):
        for key in target_dirs:
            try:
                os.add_dll_directory(key)
            except (OSError, FileNotFoundError):
                pass


_prefer_local_pyrealsense2()

import logging

import uvicorn
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles

from app.api.endpoints.health import SDK_WARNINGS
from app.api.router import api_router
from app.core.errors import setup_exception_handlers
from config import settings

for warning in SDK_WARNINGS:  # a Debug SDK build: the client shows these too
    logging.warning(warning)


# --- Create FastAPI App ---
# Initialize FastAPI app with title and OpenAPI URL
app = FastAPI(
    title=settings.PROJECT_NAME,
    openapi_url=f"{settings.API_V1_STR}/openapi.json",
)

# Set up CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.CORS_ORIGINS,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Set up routers
app.include_router(api_router, prefix=settings.API_V1_STR)

# Set up exception handlers
setup_exception_handlers(app)

# If the react viewer is built, mount it on root (/)
_static_dir = Path(__file__).parent / "static"
if (_static_dir / "index.html").exists():
    app.router.default = StaticFiles(directory=_static_dir, html=True)


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=settings.PROJECT_NAME)
    # localhost by default, which also avoids Windows Firewall prompts
    parser.add_argument("--host", default="127.0.0.1", help="interface to listen on; 0.0.0.0 accepts remote clients")
    parser.add_argument("--port", type=int, choices=range(1, 65536), metavar="PORT", default=8000, help="port to listen on (1-65535)")
    args = parser.parse_args()

    # Disable reload when running as a bundled executable (PyInstaller)
    # Reload mode doesn't work in PyInstaller and causes issues with device access
    is_bundled = getattr(sys, 'frozen', False) and hasattr(sys, '_MEIPASS')
    reload_enabled = not is_bundled
    
    if is_bundled:
        # When bundled, pass the app object directly (string import doesn't work)
        uvicorn.run(
            app,
            host=args.host,
            port=args.port,
            log_level="info",
            ws_per_message_deflate=False,  # compressing every raw frame costs a core and caps the fps
        )
    else:
        # In development, use string reference to enable reload
        uvicorn.run(
            "main:app",
            host=args.host,
            port=args.port,
            reload=reload_enabled,
            log_level="debug",
            ws_per_message_deflate=False,  # compressing every raw frame costs a core and caps the fps
        )
