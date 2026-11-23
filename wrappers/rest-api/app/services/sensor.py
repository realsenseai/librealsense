# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from typing import Callable, Dict, List

import pyrealsense2 as rs

from app.models.option import FilterInfo, OptionInfo
from app.models.sensor import SensorInfo, StreamProfile
from app.services import options


def profile_row(profile: rs.stream_profile) -> StreamProfile:
    video = profile.as_video_stream_profile()
    return StreamProfile(
        stream_type=profile.stream_type().name,
        stream_index=profile.stream_index(),
        format=profile.format().name,
        width=video.width(),
        height=video.height(),
        fps=profile.fps(),
        default=profile.is_default(),
    )


class Sensor:
    """One sensor of a camera, as the legacy viewer's subdevice_model: its SDK sensor, its options, its
    streaming through the sensor API, and its post-processing filters"""

    def __init__(self, sensor: rs.sensor, index: int):
        self._sensor = sensor
        self.index = index  # in the device's sensors, as the SDK lists them
        self.name = sensor.get_info(rs.camera_info.name)
        # Its post-processing filters, in the SDK's recommended order, each to whether it is on: the SDK holds
        # everything else about a filter, its options included. All start off
        self.filters = {f: False for f in sensor.get_recommended_filters()}
        self._filters_by_name = {f.get_info(rs.camera_info.name): f for f in self.filters}  # the same filters, by name

    def info(self) -> SensorInfo:
        return SensorInfo(
            sensor_index=self.index,
            name=self.name,
            stream_profiles=[profile_row(p) for p in self._sensor.get_stream_profiles()],
            active_streams=self.active_streams(),
        )

    def active_profiles(self) -> List[rs.stream_profile]:
        """What it is opened with, by any client through either API, from open to close."""
        return self._sensor.get_active_streams()

    def active_streams(self) -> List[StreamProfile]:
        return [profile_row(p) for p in self.active_profiles()]

    # ---- options ---------------------------------

    def options(self) -> Dict[str, OptionInfo]:
        return options.all_options(self._sensor)

    def option(self, option_id: str) -> OptionInfo:
        return options.option(self._sensor, option_id)

    def set_option(self, option_id: str, value: float) -> None:
        options.set_option(self._sensor, option_id, value)

    # ---- post-processing ---------------------------------

    def all_filters(self) -> Dict[str, FilterInfo]:
        """Its filters, keyed by filter name. Keyed rather than flat because filters share option names -
        holes_fill exists on Spatial, Temporal and Hole Filling with a different meaning on each."""
        return {name: FilterInfo(enabled=self.filters[f], options=options.all_options(f))
                for name, f in self._filters_by_name.items()}

    def filter_options(self, filter_name: str) -> Dict[str, OptionInfo]:
        """One filter's options"""
        return options.all_options(self._filters_by_name[filter_name])

    def filter_option(self, filter_name: str, field: str) -> OptionInfo:
        """One option of one filter"""
        return options.option(self._filters_by_name[filter_name], field)

    def set_filter_option(self, filter_name: str, field: str, value: float) -> None:
        options.set_option(self._filters_by_name[filter_name], field, value)

    def filter_enabled(self, filter_name: str) -> bool:
        return self.filters[self._filters_by_name[filter_name]]

    def set_filter_enabled(self, filter_name: str, enabled: float) -> None:
        """Bypass or apply one filter"""
        self.filters[self._filters_by_name[filter_name]] = bool(enabled)

    # ---- streaming, through the sensor API ---------------------------------

    def open(self, rows: List[StreamProfile]) -> None:
        """Open it with profiles of its stream_profiles, several if wanted (e.g. depth + IR)"""
        listed = {profile_row(p).model_dump_json(exclude={"default"}): p
                  for p in self._sensor.get_stream_profiles()}
        self._sensor.open([listed[r.model_dump_json(exclude={"default"})] for r in rows])

    def start(self, callback: Callable[[rs.frame], None]) -> None:
        self._sensor.start(callback)

    def stop(self) -> None:
        self._sensor.stop()

    def close(self) -> None:
        self._sensor.close()
