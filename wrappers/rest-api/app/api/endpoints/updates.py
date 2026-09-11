# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter, Depends
from starlette.concurrency import run_in_threadpool

from app.api.dependencies import get_realsense_manager
from app.core.sdk_info import sdk_version
from app.services import updates
from app.services.firmware import SERVER_VERSIONS_DB_URL
from app.services.rs_manager import RealSenseManager

router = APIRouter()


def versions_db_url(rs_manager: RealSenseManager) -> str:
    """The official DB, or the custom URL (http(s):// or file://) from Settings."""
    update = rs_manager.settings.get().update
    return SERVER_VERSIONS_DB_URL if update.sw_update_official_server or not update.sw_update_url else update.sw_update_url


@router.get("/{device_id}", response_model=dict)
async def check_updates(device_id: str, rs_manager: RealSenseManager = Depends(get_realsense_manager)):
    """Firmware and librealsense update candidates for a device, with an up-to-date /
    recommended / essential verdict for each."""
    device = rs_manager.get_device(device_id)
    return await run_in_threadpool(updates.check, versions_db_url(rs_manager), device.name, device.firmware_version, sdk_version())
