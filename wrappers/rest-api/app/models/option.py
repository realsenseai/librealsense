# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from pydantic import BaseModel
from typing import Dict, Union

class OptionUpdate(BaseModel):
    value: float

class OptionInfo(BaseModel):
    """One option, as rs2::options reports it; the map holding it, or the URL it came from, names it"""
    description: str
    current_value: Union[float, int, bool, str]  # by the option's type in the SDK
    default_value: float
    min_value: float
    max_value: float
    step: float
    read_only: bool
    value_descriptions: Dict[str, str]  # each value's label, as far as the SDK has them; often none

class AdvancedControl(BaseModel):
    """One advanced-mode field, as the SDK reports its group: value, minimum and maximum"""
    current_value: Union[int, float]
    min_value: Union[int, float]
    max_value: Union[int, float]
    integer: bool  # an int field: JSON alone loses the difference between 10 and 10.0

class FilterInfo(BaseModel):
    """One post-processing filter; the map holding it names it"""
    enabled: bool
    options: Dict[str, OptionInfo]
