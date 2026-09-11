# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Detects option values the camera changed on its own (a preset rewriting exposure, an
auto-exposure toggle) and reports them as ``options_changed``.

One thread reads every sensor's options in turn, under the device's option lock that REST
reads and writes also take. The SDK's own option watcher (one thread per sensor) is not used:
on the Windows backend its concurrent reads across sensors were measured to leave the D455
unable to accept option writes until a hardware reset.
"""

import logging
import threading
from typing import Any, Callable, Dict, Iterable, Optional, Tuple

import pyrealsense2 as rs

Values = Dict[str, float]


class OptionsPoller:
    def __init__(
        self,
        devices: Callable[[], Iterable[Tuple[str, Any]]],
        lock_for: Callable[[str], threading.Lock],
        emit: Callable[[str, Dict[str, Any]], None],
        interval: float = 1.0,
    ):
        self._devices = devices
        self._lock_for = lock_for
        self._emit = emit
        self._interval = interval
        self._known: Dict[str, Dict[str, Values]] = {}  # device -> sensor -> option -> value
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None

    def start(self) -> None:
        if self._thread is None:
            self._thread = threading.Thread(target=self._run, name="options-poller", daemon=True)
            self._thread.start()
            logging.info("options poller started (every %.1fs)", self._interval)

    def stop(self) -> None:
        self._stop.set()

    def forget(self, device_id: str) -> None:
        self._known.pop(device_id, None)

    def note_written(self, device_id: str, sensor_id: str, option: str, value: float) -> None:
        """A value the server itself wrote is not a change worth reporting."""
        self._known.setdefault(device_id, {}).setdefault(sensor_id, {})[option] = value

    def _run(self) -> None:
        while not self._stop.wait(self._interval):
            try:
                self.poll_once()
            except Exception:
                logging.exception("options poll failed")

    def poll_once(self) -> None:
        for device_id, dev in list(self._devices()):
            try:
                sensors = list(dev.sensors)
            except Exception:
                continue  # gone or resetting
            for index, sensor in enumerate(sensors):
                sensor_id = f"{device_id}-sensor-{index}"
                try:
                    with self._lock_for(device_id):
                        values = self._read(sensor)
                except Exception as exc:
                    logging.debug("options poll skipped %s: %s", sensor_id, exc)
                    continue
                known = self._known.setdefault(device_id, {})
                previous = known.get(sensor_id)
                known[sensor_id] = values
                if previous is None:
                    continue  # first sight: nothing to compare with
                changed = [{"option_id": k, "current_value": v} for k, v in values.items() if previous.get(k) != v]
                if changed:
                    logging.info("options changed on %s: %s", sensor_id, [c["option_id"] for c in changed])
                    self._emit("options_changed", {"device_id": device_id, "sensor_id": sensor_id, "options": changed})

    @staticmethod
    def _read(sensor) -> Values:
        values: Values = {}
        for opt in sensor.get_supported_options():
            try:
                values[opt.name] = sensor.get_option(opt)
            except RuntimeError:
                continue  # an option the firmware refuses right now
        return values
