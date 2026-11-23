# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from pydantic import BaseModel, Field
from typing import Annotated, List

# The SDK's build_command takes these as uint32 and uint8: out of range it raises TypeError, so refuse them here (422)
UInt32 = Annotated[int, Field(ge=0, le=0xFFFFFFFF)]
Byte = Annotated[int, Field(ge=0, le=0xFF)]


class HwmRequest(BaseModel):
    """Request body for a hardware monitor (HWM) command."""

    opcode: UInt32 = Field(..., description="HWM opcode (e.g. 0x10 for GVD)")
    param1: UInt32 = Field(0, description="First command parameter (default 0)")
    param2: UInt32 = Field(0, description="Second command parameter (default 0)")
    param3: UInt32 = Field(0, description="Third command parameter (default 0)")
    param4: UInt32 = Field(0, description="Fourth command parameter (default 0)")
    data: List[Byte] = Field(
        default_factory=list,
        description="Optional payload bytes appended after the header",
    )

