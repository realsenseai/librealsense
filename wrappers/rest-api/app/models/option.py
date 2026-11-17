# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from pydantic import BaseModel
from typing import Any, List, Optional, Union

class OptionBase(BaseModel):
    name: str
    description: Optional[str] = None

class OptionCreate(OptionBase):
    pass

class Option(OptionBase):
    option_id: str
    current_value: Any
    default_value: Any
    min_value: Optional[Union[float, int]] = None
    max_value: Optional[Union[float, int]] = None
    step: Optional[Union[float, int]] = None
    units: Optional[str] = None

    class Config:
        from_attributes = True

class OptionUpdate(BaseModel):
    value: float

class OptionInfo(BaseModel):
    option_id: str
    description: Optional[str] = None
    current_value: Any
    default_value: Any
    min_value: float
    max_value: float
    step: Optional[Union[float, int]] = None
    units: Optional[str] = None
    read_only: bool = False
    value_descriptions: Optional[dict] = None  # For enum-type options: {value: description}

class FilterInfo(BaseModel):
    """One post-processing filter; the map holding it names it"""
    enabled: bool
    options: List[OptionInfo]
