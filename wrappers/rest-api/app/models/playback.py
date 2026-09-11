# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from typing import Literal, Optional

from pydantic import BaseModel

PlaybackState = Literal["unknown", "playing", "paused", "stopped"]


class PlaybackStatus(BaseModel):
    device_id: str
    file_name: str
    state: PlaybackState
    position_ns: int
    duration_ns: int
    speed: float = 1.0
    repeat: bool = False


class PlaybackAction(BaseModel):
    """Transport request: play, pause, stop, seek (value: ns), speed (value: factor),
    step (value: +1 / -1 frames), repeat (value: 0 / 1)."""
    action: Literal["play", "pause", "stop", "seek", "speed", "step", "repeat"]
    value: Optional[float] = None


class PlaybackLoadRequest(BaseModel):
    path: str  # a recording on the server's file system


class RecordStatus(BaseModel):
    device_id: str
    recording: bool
    paused: bool = False
    file: Optional[str] = None


class RecordStartRequest(BaseModel):
    path: Optional[str] = None  # default: settings.record.default_path + auto name
