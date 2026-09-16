# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from typing import Any, Dict, List

from fastapi import APIRouter, Depends
from pydantic import BaseModel
from starlette.concurrency import run_in_threadpool

from app.api.dependencies import get_realsense_manager
from app.services.rs_manager import RealSenseManager

logs_router = APIRouter()
device_router = APIRouter()
terminal_router = APIRouter()


@logs_router.get("/", response_model=List[Dict[str, Any]])
async def get_logs(after: int = 0, limit: int = 500, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Console entries with an id greater than ``after`` (backfill after a reconnect)."""
    return rs_manager.console.since(after, limit)


@logs_router.delete("/")
async def clear_logs(rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    rs_manager.console.clear()
    return {"cleared": True}


@device_router.get("/fw_logs")
async def fw_logs_status(device_id: str, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    return await run_in_threadpool(rs_manager.fw_logs_status, device_id)


@device_router.post("/fw_logs/start")
async def start_fw_logs(device_id: str, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Pull firmware logs into the console; parsed when Settings names a firmware-logs XML."""
    return await run_in_threadpool(rs_manager.start_fw_logs, device_id)


@device_router.post("/fw_logs/stop")
async def stop_fw_logs(device_id: str, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    return await run_in_threadpool(rs_manager.stop_fw_logs, device_id)


@device_router.post("/fw_logs/flash")
async def recover_flash_logs(device_id: str, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Read the logs the firmware kept in flash into the console."""
    return await run_in_threadpool(rs_manager.recover_flash_logs, device_id)


class TerminalLine(BaseModel):
    line: str


@device_router.post("/terminal")
async def run_terminal(device_id: str, body: TerminalLine, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Send a console command (raw hex bytes, or a Commands.xml name) to the device."""
    output = await run_in_threadpool(rs_manager.run_terminal, device_id, body.line)
    return {"output": output}


@terminal_router.get("/commands", response_model=List[str])
async def terminal_commands(rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Named commands from the configured Commands.xml, for autocompletion."""
    return await run_in_threadpool(rs_manager.terminal_commands)
