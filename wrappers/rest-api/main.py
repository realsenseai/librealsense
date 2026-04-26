import asyncio
import uvicorn
import sys
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
    reload_enabled = not is_bundled and '--reload' not in sys.argv
    
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
