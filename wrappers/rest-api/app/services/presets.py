# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Device JSON presets (device-model.cpp load/save JSON): the serialized advanced-mode state,
plus the presets folder the legacy viewer scans for "<model> <name>.preset" files."""

import platform
from pathlib import Path
from typing import Dict, List

import pyrealsense2 as rs

from app.core.errors import RealSenseError
from app.services import advanced_mode

CUSTOM_PRESET = 0  # RS2_RS400_VISUAL_PRESET_CUSTOM


def default_folder() -> Path:
    home = Path.home()
    return (home / "Documents" if platform.system() == "Windows" else home) / "librealsense2" / "presets"


def _serializable(dev):
    if not advanced_mode.status(dev)["enabled"]:
        # The D400 serializer reads the advanced-mode tables; firmware refuses otherwise.
        raise RealSenseError(status_code=409, detail="Advanced mode must be enabled to use JSON presets")
    return rs.serializable_device(dev)


def serialize(dev) -> str:
    return _serializable(dev).serialize_json()


def load(dev, text: str) -> None:
    """Apply a preset and report the preset combo as Custom, as the legacy viewer does."""
    _serializable(dev).load_json(text)
    for sensor in dev.sensors:
        if not sensor.supports(rs.option.visual_preset):
            continue
        try:
            if sensor.get_option_value_description(rs.option.visual_preset, CUSTOM_PRESET) == "Custom":
                sensor.set_option(rs.option.visual_preset, CUSTOM_PRESET)
        except RuntimeError:
            pass


def model_name(device_name: str) -> str:
    """'RealSense D455' -> 'D455': the prefix the presets folder convention keys files by."""
    return device_name.split()[-1] if device_name else ""


def list_folder(folder: Path, device_name: str) -> List[Dict[str, str]]:
    """Presets in the folder for this camera model, by file name."""
    if not folder.is_dir():
        return []
    model = model_name(device_name).lower()
    files = [p for p in folder.iterdir() if p.suffix.lower() in (".preset", ".json")
             and (not model or p.stem.lower().startswith(model))]
    files.sort(key=lambda p: p.name.lower())
    return [{"path": str(p), "name": p.stem[len(model):].strip() if model and p.stem.lower().startswith(model) else p.stem}
            for p in files]
