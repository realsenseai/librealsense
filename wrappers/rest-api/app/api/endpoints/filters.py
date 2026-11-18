# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from fastapi import APIRouter, Depends
from typing import Dict

from app.models.option import FilterInfo, OptionInfo, OptionUpdate
from app.api.dependencies import get_sensor
from app.services.sensor import Sensor

router = APIRouter()


@router.get("/", response_model=Dict[str, FilterInfo])
def get_sensor_filters(
    sensor: Sensor = Depends(get_sensor),
):
    """A sensor's post-processing filters, keyed by filter name, each with its options."""
    return sensor.all_filters()


# Ahead of the per-option route below, which would otherwise take "enabled" for a field.
@router.put("/{filter_name}/enabled/")
def set_filter_enabled(
    filter_name: str,
    body: OptionUpdate,
    sensor: Sensor = Depends(get_sensor),
):
    """Bypass or apply one filter."""
    sensor.set_filter_enabled(filter_name, body.value)


@router.put("/{filter_name}/{field}/", response_model=OptionInfo)
def set_filter_option(
    filter_name: str,
    field: str,
    body: OptionUpdate,
    sensor: Sensor = Depends(get_sensor),
):
    """Set one option on one filter; returns it as the device reports it afterwards."""
    return sensor.set_filter_option(filter_name, field, body.value)
