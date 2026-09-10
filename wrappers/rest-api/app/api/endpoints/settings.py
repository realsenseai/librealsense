# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from typing import Any, Dict

from fastapi import APIRouter, Body, Depends

from app.api.dependencies import get_settings_store
from app.models.settings import ViewerSettings
from app.services.settings import SettingsStore

router = APIRouter()


@router.get("/", response_model=ViewerSettings)
async def get_settings(store: SettingsStore = Depends(get_settings_store)):
    """The server-side viewer settings, all groups."""
    return store.get()


@router.put("/", response_model=ViewerSettings)
async def update_settings(
    patch: Dict[str, Any] = Body(..., description="Any subset of the settings groups/keys"),
    store: SettingsStore = Depends(get_settings_store),
):
    """Merge a partial update into the settings and return the whole result."""
    return store.update(patch)
