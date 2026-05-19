# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from pydantic import BaseModel, Field
from typing import List, Optional

class DeviceBase(BaseModel):
    name: str
    serial_number: str
    firmware_version: Optional[str] = None
    recommended_firmware_version: Optional[str] = None
    firmware_status: Optional[str] = None  # up_to_date | outdated | missing_file | unknown
    firmware_file_available: Optional[bool] = None
    physical_port: Optional[str] = None


class Device(DeviceBase):
    device_id: str
    sensors: List[str] = []
    is_streaming: bool = False

    class Config:
        from_attributes = True

class DeviceInfo(BaseModel):
    device_id: str
    name: str
    serial_number: str
    firmware_version: Optional[str] = None
    recommended_firmware_version: Optional[str] = None
    firmware_status: Optional[str] = None
    firmware_file_available: Optional[bool] = None
    physical_port: Optional[str] = None
    usb_type: Optional[str] = None
    product_id: Optional[str] = None
    sensors: List[str] = []
    is_streaming: bool = False