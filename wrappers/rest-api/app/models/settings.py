# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Server-side viewer settings.

The groups and keys follow the legacy viewer's configurations namespace (common/model-views.h)
where a setting has a counterpart there, so a user of both finds the same names.
"""

from typing import Literal

from pydantic import BaseModel, Field


class RecordSettings(BaseModel):
    file_save_mode: Literal["auto", "ask"] = "auto"
    default_path: str = ""  # empty: the server's recordings folder
    compression: Literal["auto", "always", "never"] = "auto"


class UpdateSettings(BaseModel):
    sw_update_official_server: bool = True
    sw_update_url: str = ""  # used when the official server is off; file:// allowed
    recommend_calibration: bool = True


class ConsoleSettings(BaseModel):
    max_entries: int = Field(1000, ge=10, le=100000)
    log_to_file: bool = False
    log_filename: str = ""
    log_severity: Literal["debug", "info", "warn", "error"] = "info"


class PathSettings(BaseModel):
    hwlogger_xml: str = ""  # firmware-log parser definitions
    commands_xml: str = ""  # terminal command definitions


class ContextSettings(BaseModel):
    dds_enabled: bool = False
    dds_domain: int = Field(0, ge=0, le=232)


class CalibrationSettings(BaseModel):
    enable_writing: bool = True


class PostProcessingSettings(BaseModel):
    performance_mode: bool = False  # on: every filter starts disabled to spare the server CPU


class ViewerPrefs(BaseModel):
    metric_system: bool = True


class ViewerSettings(BaseModel):
    record: RecordSettings = RecordSettings()
    update: UpdateSettings = UpdateSettings()
    console: ConsoleSettings = ConsoleSettings()
    paths: PathSettings = PathSettings()
    context: ContextSettings = ContextSettings()
    calibration: CalibrationSettings = CalibrationSettings()
    post_processing: PostProcessingSettings = PostProcessingSettings()
    viewer: ViewerPrefs = ViewerPrefs()
