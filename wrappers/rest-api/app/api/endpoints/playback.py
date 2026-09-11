# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import shutil
from pathlib import Path
from typing import Optional

from fastapi import APIRouter, Depends, File, HTTPException, UploadFile
from starlette.concurrency import run_in_threadpool

from app.api.dependencies import get_realsense_manager, get_settings_store
from app.models.device import DeviceInfo
from app.models.playback import PlaybackAction, PlaybackLoadRequest, PlaybackStatus
from app.services.rs_manager import RealSenseManager
from app.services.settings import SettingsStore

router = APIRouter()


def recordings_folder(store: SettingsStore) -> Path:
    return Path(store.get().record.default_path or str(Path.home() / "Documents"))


@router.post("/load", response_model=DeviceInfo)
async def load_recording(body: PlaybackLoadRequest, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Open a recording already on the server's file system as a playback device."""
    return await run_in_threadpool(rs_manager.load_playback, body.path)


@router.post("/upload", response_model=DeviceInfo)
async def upload_recording(
    file: UploadFile = File(...),
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
    store: SettingsStore = Depends(get_settings_store),
):
    """Receive a recording from the browser into the recordings folder and open it."""
    name = Path(file.filename or "recording.bag").name
    if Path(name).suffix.lower() not in (".bag", ".db3"):
        raise HTTPException(status_code=400, detail="Only .bag and .db3 recordings can be played")
    folder = recordings_folder(store)
    folder.mkdir(parents=True, exist_ok=True)
    target = folder / name
    with target.open("wb") as out:
        await run_in_threadpool(shutil.copyfileobj, file.file, out, 1024 * 1024)
    return await run_in_threadpool(rs_manager.load_playback, str(target))


@router.get("/files", response_model=list)
async def list_recordings(store: SettingsStore = Depends(get_settings_store)):
    """Recordings in the server's recordings folder, newest first."""
    folder = recordings_folder(store)
    if not folder.is_dir():
        return []
    files = [p for p in folder.iterdir() if p.suffix.lower() in (".bag", ".db3")]
    files.sort(key=lambda p: p.stat().st_mtime, reverse=True)
    return [{"path": str(p), "name": p.name, "size": p.stat().st_size, "modified": p.stat().st_mtime} for p in files]


@router.get("/{device_id}", response_model=PlaybackStatus)
async def playback_status(device_id: str, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    return await run_in_threadpool(rs_manager.get_playback_status, device_id)


@router.post("/{device_id}", response_model=PlaybackStatus)
async def playback_control(device_id: str, body: PlaybackAction, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Transport: play, pause, stop, seek (ns), speed (factor), step (+1/-1), repeat (0/1)."""
    return await run_in_threadpool(rs_manager.playback_control, device_id, body.action, body.value)


@router.delete("/{device_id}")
async def unload_recording(device_id: str, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    await run_in_threadpool(rs_manager.unload_playback, device_id)
    return {"unloaded": device_id}
