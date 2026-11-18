# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter, Depends
from typing import List


from app.models.option import OptionInfo, OptionUpdate
from app.api.dependencies import get_sensor
from app.services.sensor import Sensor

router = APIRouter()

@router.get("/", response_model=List[OptionInfo])
def get_options(
    sensor: Sensor = Depends(get_sensor),
):
    """
    Get a list of all options for a specific sensor.
    """
    return sensor.options()

@router.get("/{option_id}/", response_model=OptionInfo)
def get_option(
    option_id: str,
    sensor: Sensor = Depends(get_sensor),
):
    """
    Get details of a specific option for a sensor.
    """
    return sensor.option(option_id)

@router.put("/{option_id}/", response_model=OptionInfo)
def update_option(
    option_id: str,
    option_update: OptionUpdate,
    sensor: Sensor = Depends(get_sensor),
):
    """
    Update the value of a specific option for a sensor; returns it as the device holds it.
    """
    return sensor.set_option(option_id, option_update.value)
