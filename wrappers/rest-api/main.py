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
    if hasattr(os, "add_dll_directory"):
        for key in target_dirs:
            try:
                os.add_dll_directory(key)
            except (OSError, FileNotFoundError):
                pass


_prefer_local_pyrealsense2()

import asyncio
import uvicorn
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from app.api.router import api_router
from app.core.errors import setup_exception_handlers
from config import settings
import socketio
from app.services.socketio import sio
from app.services.rs_manager import RealSenseManager


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


@app.on_event("startup")
async def startup_event():
    """Store the main event loop for use in synchronous callbacks."""
    loop = asyncio.get_running_loop()
    RealSenseManager.set_event_loop(loop)


# --- Combine FastAPI and Socket.IO into a single ASGI App ---
# Mount the Socket.IO app (`sio`) onto the FastAPI app (`app`)
# The result `combined_app` is what Uvicorn will run.
combined_app = socketio.ASGIApp(socketio_server=sio, other_asgi_app=app, socketio_path='socket')

if __name__ == "__main__":
    # Disable reload when running as a bundled executable (PyInstaller)
    # Reload mode doesn't work in PyInstaller and causes issues with device access
    is_bundled = getattr(sys, 'frozen', False) and hasattr(sys, '_MEIPASS')
    reload_enabled = not is_bundled
    
    if is_bundled:
        # When bundled, pass the app object directly (string import doesn't work)
        # Bind to localhost only to avoid Windows Firewall prompts
        uvicorn.run(
            combined_app,
            host="127.0.0.1",
            port=8000,
            log_level="info"
        )
    else:
        # In development, use string reference to enable reload
        uvicorn.run(
            "main:combined_app",
            host="127.0.0.1",
            port=8000,
            reload=reload_enabled,
            log_level="debug"
        )