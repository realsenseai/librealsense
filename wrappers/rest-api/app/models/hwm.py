# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from pydantic import BaseModel, Field
from typing import List, Annotated


class HwmRequest(BaseModel):
    """Request body for a hardware monitor (HWM) command."""

    opcode: int = Field(..., ge=0, le=0xFFFFFFFF, description="HWM opcode (e.g. 0x10 for GVD)")
    param1: int = Field(0, ge=0, le=0xFFFFFFFF, description="First command parameter (default 0)")
    param2: int = Field(0, ge=0, le=0xFFFFFFFF, description="Second command parameter (default 0)")
    param3: int = Field(0, ge=0, le=0xFFFFFFFF, description="Third command parameter (default 0)")
    param4: int = Field(0, ge=0, le=0xFFFFFFFF, description="Fourth command parameter (default 0)")
    data: List[Annotated[int, Field(ge=0, le=255)]] = Field(
        default_factory=list,
        description="Optional payload bytes appended after the header",
    )


class HwmResponse(BaseModel):
    """Response returned by a hardware monitor command."""

    device_id: str = Field(..., description="Serial number of the target device")
    response: List[Annotated[int, Field(ge=0, le=255)]] = Field(
        ..., description="Raw firmware response as a list of byte values"
    )
