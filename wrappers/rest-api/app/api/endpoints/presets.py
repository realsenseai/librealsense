# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from pathlib import Path
from typing import List, Optional

from fastapi import APIRouter, Depends, File, HTTPException, Response, UploadFile
from pydantic import BaseModel
from starlette.concurrency import run_in_threadpool

from app.api.dependencies import get_realsense_manager
from app.services import presets
from app.services.rs_manager import RealSenseManager

router = APIRouter()


class PresetRef(BaseModel):
    path: Optional[str] = None  # a file in the presets folder
    text: Optional[str] = None  # or the JSON itself


class SavePreset(BaseModel):
    name: str


def _folder(rs_manager: RealSenseManager) -> Path:
    configured = rs_manager.settings.get().paths.presets_folder
    return Path(configured) if configured else presets.default_folder()


@router.get("/", response_model=List[dict])
async def list_presets(device_id: str, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Preset files in the presets folder that match this camera model."""
    device = rs_manager.get_device(device_id)
    return presets.list_folder(_folder(rs_manager), device.name)


@router.get("/current")
async def download_current(device_id: str, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """The device's current settings as the JSON preset the legacy viewer saves."""
    device = rs_manager.get_device(device_id)
    text = await run_in_threadpool(rs_manager.serialize_preset, device_id)
    name = f"{presets.model_name(device.name)} preset.json"
    return Response(content=text, media_type="application/json",
                    headers={"Content-Disposition": f'attachment; filename="{name}"'})


@router.post("/load")
async def load_preset(device_id: str, body: PresetRef, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Apply a preset from the folder (path) or from its JSON (text)."""
    text = body.text
    if text is None:
        if not body.path:
            raise HTTPException(status_code=400, detail="Give a preset path or its JSON text")
        path = Path(body.path)
        if not path.is_file():
            raise HTTPException(status_code=404, detail=f"Preset not found: {body.path}")
        text = path.read_text(encoding="utf-8")
    await run_in_threadpool(rs_manager.load_preset, device_id, text)
    return {"loaded": body.path or "json"}


@router.post("/upload")
async def upload_preset(device_id: str, file: UploadFile = File(...), rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Apply a preset file sent from the browser."""
    text = (await file.read()).decode("utf-8", errors="replace")
    await run_in_threadpool(rs_manager.load_preset, device_id, text)
    return {"loaded": file.filename}


@router.post("/save", response_model=List[dict])
async def save_preset(device_id: str, body: SavePreset, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Store the current settings in the presets folder as '<model> <name>.preset'."""
    device = rs_manager.get_device(device_id)
    text = await run_in_threadpool(rs_manager.serialize_preset, device_id)
    folder = _folder(rs_manager)
    folder.mkdir(parents=True, exist_ok=True)
    safe = "".join(c for c in body.name.strip() if c not in '\\/:*?"<>|') or "preset"
    (folder / f"{presets.model_name(device.name)} {safe}.preset").write_text(text, encoding="utf-8")
    return presets.list_folder(folder, device.name)
