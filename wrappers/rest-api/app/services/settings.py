# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Persisted viewer settings: one JSON file, partial updates merged group by group."""

import json
import logging
import os
import threading
from pathlib import Path
from typing import Any, Dict

from pydantic import ValidationError

from app.core.errors import RealSenseError
from app.models.settings import ViewerSettings

DEFAULT_PATH = Path.home() / ".realsense" / "rest-api-settings.json"


def _merge(base: Dict[str, Any], patch: Dict[str, Any]) -> Dict[str, Any]:
    out = dict(base)
    for key, value in patch.items():
        out[key] = _merge(base[key], value) if isinstance(value, dict) and isinstance(base.get(key), dict) else value
    return out


class SettingsStore:
    def __init__(self, path: Path = DEFAULT_PATH):
        self._path = Path(path)
        self._lock = threading.Lock()
        self._settings = self._load()

    def _load(self) -> ViewerSettings:
        try:
            return ViewerSettings.model_validate(json.loads(self._path.read_text(encoding="utf-8")))
        except FileNotFoundError:
            return ViewerSettings()
        except (OSError, ValueError, ValidationError) as exc:
            # A damaged file must not take the server down; the defaults are always valid.
            logging.warning("Ignoring unreadable settings file %s: %s", self._path, exc)
            return ViewerSettings()

    def get(self) -> ViewerSettings:
        with self._lock:
            return self._settings

    def update(self, patch: Dict[str, Any]) -> ViewerSettings:
        """Merge ``patch`` into the settings, validate, persist, and return the result."""
        with self._lock:
            try:
                merged = ViewerSettings.model_validate(_merge(self._settings.model_dump(), patch))
            except ValidationError as exc:
                raise RealSenseError(status_code=422, detail=exc.errors(include_url=False))
            self._settings = merged
            self._save()
            return merged

    def _save(self) -> None:
        self._path.parent.mkdir(parents=True, exist_ok=True)
        tmp = self._path.with_suffix(".tmp")
        tmp.write_text(self._settings.model_dump_json(indent=2), encoding="utf-8")
        os.replace(tmp, self._path)  # readers never see a half-written file
