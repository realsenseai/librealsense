from fastapi import APIRouter, Depends, HTTPException
from typing import List


from app.models.sensor import SensorInfo
from app.services.rs_manager import RealSenseManager
from app.api.dependencies import get_realsense_manager, get_current_user

router = APIRouter()

@router.get("/", response_model=List[SensorInfo])
async def get_sensors(
    device_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
    user: dict = Depends(get_current_user),
):
    """
    Get a list of all sensors for a specific RealSense device.
    Requires authentication.
    """
    try:
        return rs_manager.get_sensors(device_id)
    except Exception as e:
        raise HTTPException(status_code=404, detail=str(e))

@router.get("/{sensor_id}", response_model=SensorInfo)
async def get_sensor(
    device_id: str,
    sensor_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
    user: dict = Depends(get_current_user),
):
    """
    Get details of a specific sensor for a RealSense device.
    Requires authentication.
    """
    try:
        return rs_manager.get_sensor(device_id, sensor_id)
    except Exception as e:
        raise HTTPException(status_code=404, detail=str(e))