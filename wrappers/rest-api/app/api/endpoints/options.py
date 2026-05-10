from fastapi import APIRouter, Depends, HTTPException
from typing import List, Any


from app.models.option import OptionInfo, OptionUpdate
from app.services.rs_manager import RealSenseManager, RealSenseError
from app.api.dependencies import get_realsense_manager

router = APIRouter()

@router.get("/", response_model=List[OptionInfo])
async def get_options(
    device_id: str,
    sensor_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """
    Get a list of all options for a specific sensor.
    """
    try:
        return rs_manager.get_sensor_options(device_id, sensor_id)
    except Exception as e:
        raise HTTPException(status_code=404, detail=str(e))

@router.get("/{option_id}", response_model=OptionInfo)
async def get_option(
    device_id: str,
    sensor_id: str,
    option_id: str,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """
    Get details of a specific option for a sensor.
    """
    try:
        return rs_manager.get_sensor_option(device_id, sensor_id, option_id)
    except Exception as e:
        raise HTTPException(status_code=404, detail=str(e))

@router.put("/{option_id}", response_model=dict)
async def update_option(
    device_id: str,
    sensor_id: str,
    option_id: str,
    option_update: OptionUpdate,
    rs_manager: RealSenseManager = Depends(get_realsense_manager),
):
    """
    Update the value of a specific option for a sensor.
    """
    try:
        result = rs_manager.set_sensor_option(device_id, sensor_id, option_id, option_update.value)
        return {"success": result}
    except RealSenseError as e:
        # Preserve the original status code from RealSenseError
        raise HTTPException(status_code=e.status_code, detail=e.detail)
    except Exception as e:
        # Log unexpected errors for debugging
        import traceback
        traceback.print_exc()
        raise HTTPException(status_code=500, detail=f"Unexpected error: {str(e)}")