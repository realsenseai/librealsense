# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import logging
from typing import Dict, List

import pyrealsense2 as rs

from app.models.hwm import HwmRequest
from app.models.option import AdvancedControl, OptionInfo
from app.models.sensor import StreamProfile
from app.models.stream import Extrinsics, StreamConfig, StreamExtrinsics
from app.services import advanced_mode, options
from app.services.events import Events
from app.services.frame_sender import STOPPED, FrameSender
from app.services.sensor import Sensor


class Camera:
    """One connected camera and everything about it for as long as it is connected, as the legacy viewer's
    device_model: its SDK device and camera info, and what clients set up on it"""

    def __init__(self, dev: rs.device, ctx: rs.context, events: Events, frame_sender: FrameSender):
        self.dev = dev
        # Every camera info it reports, by its SDK name, as the legacy viewer reads them
        self.info: Dict[str, str] = {}
        for name, key in rs.camera_info.__members__.items():
            try:
                if dev.supports(key):
                    self.info[name] = dev.get_info(key)
            except RuntimeError:
                pass
        # Its serial number. A device without one, such as one in recovery (DFU) mode, raises, and is not listed
        self.id: str = dev.get_info(rs.camera_info.serial_number)
        self.pipeline = rs.pipeline(ctx)
        self.pipeline.set_device(dev)  # skips re-creating the device on start
        self._events = events
        self._frame_sender = frame_sender
        # One colorizer, so depth-visualization options (color scheme, min/max distance, histogram eq)
        # set on it affect the streamed depth image
        self.colorizer = rs.colorizer()
        # The SDK keeps an options callback on the sensor object it was given, and dev.sensors makes new ones: so the
        # callbacks go on the very objects each Sensor keeps
        sensors = dev.sensors
        self.sensors = [Sensor(s, i) for i, s in enumerate(sensors)]
        for i, s in enumerate(sensors):
            try:
                s.on_options_changed(self._options_callback(i))
            except RuntimeError:  # a sensor the SDK watches no options of, such as a software device's
                pass

    def is_metadata_enabled(self) -> bool:
        """False only where Windows keeps its frame metadata off until it is enabled (the SDK says True elsewhere);
        read from the registry each call"""
        return self.dev.is_metadata_enabled()

    def hardware_reset(self) -> None:
        """The devices-changed callback reports it leaving and coming back, as in the legacy viewer"""
        self.dev.hardware_reset()

    def extrinsics(self) -> List[StreamExtrinsics]:
        """From each of its streams to every other; calibration, so they hold whatever streams."""
        streams = {(p.stream_type(), p.stream_index()): p  # per stream, not per resolution
                   for sensor in self.dev.sensors for p in sensor.get_stream_profiles()}
        result = []
        for p in streams.values():
            to = []
            for other in streams.values():
                if other is p:
                    continue
                try:
                    e = p.get_extrinsics_to(other)
                except RuntimeError:  # no calibration between them
                    continue
                to.append(Extrinsics(stream_type=other.stream_type().name, stream_index=other.stream_index(),
                                     rotation=list(e.rotation), translation=list(e.translation)))
            result.append(StreamExtrinsics(stream_type=p.stream_type().name, stream_index=p.stream_index(), to=to))
        return result

    # ---- colorizer ---------------------------------

    def colorizer_options(self) -> Dict[str, OptionInfo]:
        return options.all_options(self.colorizer)

    def colorizer_option(self, field: str) -> OptionInfo:
        return options.option(self.colorizer, field)

    def set_colorizer_option(self, field: str, value: float) -> None:
        options.set_option(self.colorizer, field, value)

    # ---- advanced mode ---------------------------------

    def set_advanced_mode(self, enable: bool) -> None:
        advanced_mode.toggle(self.dev, enable)

    def advanced_controls(self) -> Dict[str, Dict[str, AdvancedControl]]:
        return advanced_mode.controls(self.dev)

    def advanced_group(self, group: str) -> Dict[str, AdvancedControl]:
        return advanced_mode.group(self.dev, group)

    def advanced_control(self, group: str, field: str) -> AdvancedControl:
        return self.advanced_group(group)[field]

    def set_advanced_control(self, group: str, field: str, value: float) -> None:
        advanced_mode.set_control(self.dev, group, field, value)

    def _options_callback(self, sensor_index: int):
        """What the SDK calls when options of the sensor change, by any client's write or by the camera itself (auto
        exposure), as the legacy viewer is told: every client gets the new values"""
        events, device_id, group = self._events, self.id, f"sensors/{sensor_index}/options"

        def changed(values):
            # Never use self in here: a camera the callback held would never be freed once unplugged
            events.emit("options_changed", {"device_id": device_id, "group": group,
                                            "options": {v.id.name: v.value for v in values}})  # None: unreadable now
        return changed

    # ---- streaming ---------------------------------

    def _streaming_changed(self) -> None:
        """Tell every client what each of its sensors streams now, as a start or stop by any client changed it."""
        self._events.emit("streaming_changed", {"device_id": self.id, "sensors": [
            [r.model_dump() for r in s.active_streams()] for s in self.sensors]})

    def _stopped(self, profiles: List[rs.stream_profile]) -> None:
        """Close the sockets of the streams a stop ended, and tell every client"""
        self._frame_sender.close(self.id, STOPPED, [(p.stream_type(), p.stream_index()) for p in profiles])
        self._streaming_changed()

    def _frame_callback(self):
        """What the SDK calls with frames, pipeline and sensor alike: sends each frame after its sensor's
        enabled filters, as the legacy viewer does"""
        # Each sensor's filters by its name within the device, the way the SDK compares sensors
        filters = {s.name: s.filters for s in self.sensors}
        sender, device_id, colorizer = self._frame_sender, self.id, self.colorizer
        failed = set()

        def send(frames):
            # Never use self in here, only the locals above: the SDK holds this callback for good, and a camera
            # it held would never be freed once unplugged
            # A synced set arrives as one frameset; unsynced streams (motion) and sensor streams as single frames
            for f in frames.as_frameset() if frames.is_frameset() else [frames]:
                if f.get_profile().stream_type() == rs.stream.depth:  # the filters process depth only
                    for flt, on in filters[rs.sensor.from_frame(f).get_info(rs.camera_info.name)].items():
                        if on:
                            try:
                                f = flt.process(f)
                            except RuntimeError as e:  # a failing filter is skipped, not the whole frame
                                if flt not in failed:  # logged once, not on every frame
                                    failed.add(flt)
                                    logging.warning("Filter %s failed, skipped: %s", flt.get_info(rs.camera_info.name), e)
                sender.send(device_id, f, colorizer)
        return send

    def start_stream(self, configs: List[StreamConfig]) -> None:
        config = rs.config()
        for c in configs:
            config.enable_stream(c.stream, c.index, c.width, c.height, c.format, c.fps)
        self.pipeline.start(config, self._frame_callback())
        self._streaming_changed()

    def stop_stream(self) -> None:
        profiles = self.pipeline.get_active_profile().get_streams()
        self.pipeline.stop()
        self._stopped(profiles)

    # Clients are told after start and after close: an opened or stopped sensor still lists its profiles as active

    def open_sensor(self, sensor_index: int, rows: List[StreamProfile]) -> None:
        self.sensors[sensor_index].open(rows)

    def start_sensor(self, sensor_index: int) -> None:
        self.sensors[sensor_index].start(self._frame_callback())
        self._streaming_changed()

    def stop_sensor(self, sensor_index: int) -> None:
        self.sensors[sensor_index].stop()

    def close_sensor(self, sensor_index: int) -> None:
        profiles = self.sensors[sensor_index].active_profiles()
        self.sensors[sensor_index].close()
        self._stopped(profiles)

    # ---- hardware monitor ---------------------------------

    def send_hwm_command(self, request: HwmRequest) -> List[int]:
        """The firmware's raw reply, as the SDK's debug_protocol returns it: its first uint32 echoes the opcode,
        or is the firmware's error code"""
        debug = self.dev.as_debug_protocol()
        return list(debug.send_and_receive_raw_data(debug.build_command(**request.model_dump())))
