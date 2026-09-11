# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Recording to a ROS2 db3 file and playing recordings (db3 or legacy bag) back, the legacy viewer's record button and
playback panel (device-model.cpp start_recording / draw_playback_controls)."""

import logging
import threading
import time
from datetime import datetime, timedelta
from pathlib import Path
from typing import Any, Dict, Optional

import pyrealsense2 as rs

from app.core.errors import RealSenseError
from app.models.device import DeviceInfo
from app.models.playback import PlaybackStatus, RecordStatus

STATE_NAMES = {
    rs.playback_status.unknown: "unknown",
    rs.playback_status.playing: "playing",
    rs.playback_status.paused: "paused",
    rs.playback_status.stopped: "stopped",
}


def _ns(nanoseconds: float) -> timedelta:
    """rs.playback.seek() takes a timedelta; positions are reported in nanoseconds."""
    return timedelta(microseconds=nanoseconds / 1000)


def playback_device_id(file_name: str) -> str:
    return f"playback-{Path(file_name).name}"


class RecordPlaybackMixin:
    """Mixed into RealSenseManager; see rs_manager.py."""

    # ---- recording ------------------------------------------------------------------

    def _default_recording_path(self, device_id: str) -> Path:
        folder = self.settings.get().record.default_path or str(Path.home() / "Documents")
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        return Path(folder) / f"{device_id}_{stamp}.db3"  # the SDK records ROS2 db3; .bag is playback-only

    def start_recording(self, device_id: str, path: Optional[str] = None) -> RecordStatus:
        """Wrap the streaming device in an rs.recorder: frames already flowing get written."""
        dev = self._require_device(device_id)
        if device_id in self._recorders:
            raise RealSenseError(status_code=409, detail="Already recording")
        if not self.get_stream_status(device_id).is_streaming:
            raise RealSenseError(status_code=409, detail="Start streaming before recording")
        target = Path(path) if path else self._default_recording_path(device_id)
        target.parent.mkdir(parents=True, exist_ok=True)
        compression = self.settings.get().record.compression
        try:
            recorder = rs.recorder(str(target), dev) if compression == "auto" else rs.recorder(str(target), dev, compression == "always")
        except RuntimeError as exc:
            raise RealSenseError(status_code=500, detail=f"Failed to start recording: {exc}")
        self._recorders[device_id] = {"recorder": recorder, "paused": False, "file": str(target)}
        return self.get_record_status(device_id)

    def set_recording_paused(self, device_id: str, paused: bool) -> RecordStatus:
        entry = self._recorders.get(device_id)
        if entry is None:
            raise RealSenseError(status_code=409, detail="Not recording")
        entry["recorder"].pause() if paused else entry["recorder"].resume()
        entry["paused"] = paused
        return self.get_record_status(device_id)

    def stop_recording(self, device_id: str) -> RecordStatus:
        """Release the recorder, which finalizes the file; the device keeps streaming."""
        entry = self._recorders.pop(device_id, None)
        if entry is None:
            raise RealSenseError(status_code=409, detail="Not recording")
        entry.pop("recorder")  # last reference: the SDK closes the bag
        return RecordStatus(device_id=device_id, recording=False, file=entry["file"])

    def get_record_status(self, device_id: str) -> RecordStatus:
        entry = self._recorders.get(device_id)
        if entry is None:
            return RecordStatus(device_id=device_id, recording=False)
        return RecordStatus(device_id=device_id, recording=True, paused=entry["paused"], file=entry["file"])

    # ---- playback -------------------------------------------------------------------

    def load_playback(self, path: str) -> DeviceInfo:
        """Open a recording as a device; it then streams through the per-sensor API."""
        if not Path(path).is_file():
            raise RealSenseError(status_code=404, detail=f"Recording not found: {path}")
        device_id = playback_device_id(path)
        if device_id in self._playbacks:
            return self.device_infos[device_id]
        dev = self.devices.get(device_id)
        if dev is None:
            try:
                dev = self.ctx.load_device(path)
            except RuntimeError as exc:
                raise RealSenseError(status_code=400, detail=f"Failed to load recording: {exc}")
            # load_device fires the devices-changed callback, which usually registers the
            # device before this line runs; registering again is then a no-op.
            with self.lock:
                self._register_new_device(dev)
            if device_id not in self.devices:
                raise RealSenseError(status_code=500, detail="Recording loaded but could not be registered")
            dev = self.devices[device_id]
        try:
            playback = rs.playback(dev)
            playback.set_real_time(True)
            playback.set_status_changed_callback(lambda status, d=device_id: self._on_playback_status(d, status))
        except RuntimeError as exc:
            # Leave nothing half-registered behind; the next load starts clean.
            with self.lock:
                self._remove_device(device_id)
            try:
                self.ctx.unload_device(path)
            except RuntimeError:
                pass
            raise RealSenseError(status_code=400, detail=f"Recording could not be set up for playback: {exc}")
        self._playbacks[device_id] = {"speed": 1.0, "repeat": False, "path": path}
        self._emit_socket_event("devices_changed", {"added": [device_id], "removed": []})
        return self.device_infos[device_id]

    def unload_playback(self, device_id: str) -> None:
        entry = self._playbacks.pop(device_id, None)
        if entry is None:
            raise RealSenseError(status_code=404, detail=f"{device_id} is not a loaded recording")
        for sensor_id, info in list(self.sensor_streams.get(device_id, {}).items()):
            if info.get("is_streaming"):
                self.stop_sensor(device_id, sensor_id)
        with self.lock:
            self._remove_device(device_id)
        try:
            self.ctx.unload_device(entry["path"])
            # Enumerating right after an unload deadlocked inside the SDK against the
            # playback device's teardown; keep enumeration off while it settles.
            self._enumeration_hold_until = time.monotonic() + 3.0
        except RuntimeError as exc:
            logging.warning("unload_device(%s): %s", entry["path"], exc)
        self._emit_socket_event("devices_changed", {"added": [], "removed": [device_id]})

    def _playback(self, device_id: str):
        if device_id not in self._playbacks:
            raise RealSenseError(status_code=404, detail=f"{device_id} is not a loaded recording")
        return rs.playback(self._require_device(device_id))

    def get_playback_status(self, device_id: str) -> PlaybackStatus:
        pb = self._playback(device_id)
        entry = self._playbacks[device_id]
        return PlaybackStatus(
            device_id=device_id, file_name=pb.file_name(), state=STATE_NAMES.get(pb.current_status(), "unknown"),
            position_ns=int(pb.get_position()), duration_ns=int(pb.get_duration().total_seconds() * 1e9)
            if hasattr(pb.get_duration(), "total_seconds") else int(pb.get_duration()),
            speed=entry["speed"], repeat=entry["repeat"],
        )

    def playback_control(self, device_id: str, action: str, value: Optional[float]) -> PlaybackStatus:
        pb = self._playback(device_id)
        entry = self._playbacks[device_id]
        if action == "play":
            if pb.current_status() == rs.playback_status.stopped and self._playback_streams(device_id):
                # A recording that ran to its end only plays again once its sensors are
                # reopened (the legacy play button does the same); resume() alone stays stopped.
                self._restart_playback(device_id, settle=0.0)
            else:
                pb.resume()
        elif action == "pause":
            pb.pause()
        elif action == "stop":
            pb.stop()
        elif action == "seek":
            pb.seek(_ns(value or 0))
        elif action == "speed":
            entry["speed"] = float(value or 1.0)
            pb.set_playback_speed(entry["speed"])
        elif action == "repeat":
            entry["repeat"] = bool(value)
        elif action == "step":
            # One frame at the fastest streaming rate, like the legacy step buttons, while paused.
            pb.pause()
            fps = max([c.framerate for info in self.sensor_streams.get(device_id, {}).values()
                       for c in info.get("configs", [])] or [30])
            pb.seek(_ns(max(0, int(pb.get_position()) + int((1 if (value or 1) > 0 else -1) * 1e9 / fps))))
        return self.get_playback_status(device_id)

    def _on_playback_status(self, device_id: str, status) -> None:
        """SDK playback status callback (reading thread): forward it, and loop when asked."""
        name = STATE_NAMES.get(status, "unknown")
        self._emit_socket_event("playback_status", {"device_id": device_id, "state": name})
        entry = self._playbacks.get(device_id)
        if name == "stopped" and entry and entry["repeat"]:
            threading.Thread(target=self._restart_playback, args=(device_id,), daemon=True).start()

    def _playback_streams(self, device_id: str):
        """(sensor_id, configs) of the recording's sensors that are streaming."""
        return [(sensor_id, info["configs"]) for sensor_id, info in self.sensor_streams.get(device_id, {}).items()
                if info.get("configs")]

    def _restart_playback(self, device_id: str, settle: float = 0.2) -> None:
        """Restart every sensor that was streaming so the recording plays again from the top."""
        if settle:
            time.sleep(settle)  # let the SDK finish stopping before reopening
        for sensor_id, configs in self._playback_streams(device_id):
            try:
                self.stop_sensor(device_id, sensor_id)
                self.start_sensor(device_id, sensor_id, configs)
            except Exception as exc:
                logging.warning("playback repeat failed on %s: %s", sensor_id, exc)
