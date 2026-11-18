# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from pydantic import BaseModel
from typing import Dict, Optional, Union

class OptionUpdate(BaseModel):
    value: float

class OptionInfo(BaseModel):
    """One option; the map holding it, or the URL it came from, names it"""
    description: Optional[str] = None
    current_value: Union[float, int, bool, str]  # by the option's type in the SDK
    default_value: float
    min_value: float
    max_value: float
    step: Optional[Union[float, int]] = None
    units: Optional[str] = None
    read_only: bool = False
    value_descriptions: Optional[dict] = None  # For enum-type options: {value: description}

class FilterInfo(BaseModel):
    """One post-processing filter; the map holding it names it"""
    enabled: bool
    options: Dict[str, OptionInfo]
