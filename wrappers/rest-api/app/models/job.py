# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from typing import Any, Literal, Optional

from pydantic import BaseModel

JobState = Literal["running", "done", "failed", "cancelled"]


class JobInfo(BaseModel):
    """A long-running server operation (firmware update, calibration, export...)."""
    id: str
    kind: str
    device_id: Optional[str] = None
    state: JobState = "running"
    progress: float = 0.0  # 0..1
    message: Optional[str] = None  # phase or status text for the UI
    result: Optional[Any] = None
    error: Optional[str] = None
    created_at: float
    updated_at: float
