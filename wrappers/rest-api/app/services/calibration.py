# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""On-chip and tare calibration, a port of common/on-chip-calib.cpp.

The flow the legacy tool runs: save the emitter and thermal-loop options, stop what the device
streams, stream depth at the calibration resolution (256x144 @ 90 fps, an internal crop of the
full sensor), emitter on and thermal loop off, run the firmware calibration with the legacy
JSON parameters, make the new table active, then restore streams and options. The result stays
in a per-device session so the user can compare, keep (write to flash) or go back to the old
table.
"""

import json
import logging
import threading
import time
from typing import Any, Callable, Dict, List, Optional

import pyrealsense2 as rs

from app.core.errors import RealSenseError

OCC_TIMEOUT_MS = 9000
TARE_TIMEOUT_MS = 5000
CALIB_WIDTH, CALIB_HEIGHT, CALIB_FPS = 256, 144, 90


def occ_json(speed: int = 3, average_step_count: int = 20, step_count: int = 20, accuracy: int = 2,
             apply_preset: bool = True, intrinsic_scan: bool = True, host_assistance: bool = False) -> str:
    """The legacy "calib type 0" request (on-chip-calib.cpp)."""
    return json.dumps({
        "calib type": 0,
        "host assistance": 1 if host_assistance else 0,
        "speed": int(speed),
        "average step count": int(average_step_count),
        "scan parameter": 0 if intrinsic_scan else 1,
        "step count": int(step_count),
        "apply preset": 1 if apply_preset else 0,
        "accuracy": int(accuracy),
        "scan only": 1 if host_assistance else 0,
        "interactive scan": 0,
    })


def tare_json(average_step_count: int = 20, step_count: int = 20, accuracy: int = 2,
              apply_preset: bool = True, host_assistance: bool = False) -> str:
    return json.dumps({
        "average step count": int(average_step_count),
        "step count": int(step_count),
        "accuracy": int(accuracy),
        "apply preset": 1 if apply_preset else 0,
        "scan parameter": 0,
        "data sampling": 0,
        "host assistance": 1 if host_assistance else 0,
        "scan only": 1 if host_assistance else 0,
    })


def occ_verdict(health: float) -> str:
    """The legacy colour bands for the on-chip health figure."""
    if health < 0:
        return "unknown"
    if health < 0.25:
        return "good"
    if health < 0.75:
        return "ok"
    return "bad"


class CalibrationSession:
    """What a device's last calibration left behind: old and new tables, health, which is active."""

    def __init__(self, kind: str):
        self.kind = kind
        self.state = "running"
        self.health: Optional[List[float]] = None
        self.verdict: Optional[str] = None
        self.old_table: Any = None
        self.new_table: Any = None
        self.active = "old"
        self.written = False
        self.error: Optional[str] = None
        self.started_at = time.time()

    def to_dict(self) -> Dict[str, Any]:
        return {
            "kind": self.kind, "state": self.state, "health": self.health, "verdict": self.verdict,
            "has_new_table": bool(self.new_table), "active": self.active, "written": self.written,
            "error": self.error, "started_at": self.started_at,
        }


class CalibrationWorkspace:
    """Stop the device's streams, stream depth at the calibration profile with the emitter on and
    the thermal loop off, and put everything back afterwards (on-chip-calib.cpp process_flow)."""

    def __init__(self, manager, device_id: str):
        self.manager = manager
        self.device_id = device_id
        self.previous: List[tuple] = []  # (sensor_id, configs)
        self.depth_sensor_id: Optional[str] = None
        self.saved_options: Dict[Any, float] = {}

    def enter(self, stream: bool = True) -> None:
        m = self.manager
        with m.lock:
            streams = {sid: list(info.get("configs", [])) for sid, info in m.sensor_streams.get(self.device_id, {}).items()
                       if info.get("is_streaming")}
        for sensor_id, configs in streams.items():
            self.previous.append((sensor_id, configs))
            m.stop_sensor(self.device_id, sensor_id)

        dev = m._require_device(self.device_id)
        # dev.sensors hands out fresh wrappers on every access, so find index and sensor together
        found = next(((i, s) for i, s in enumerate(dev.sensors) if s.is_depth_sensor()), None)
        if found is None:
            raise RealSenseError(status_code=400, detail="The device has no depth sensor to calibrate")
        index, depth = found
        self.depth_sensor_id = f"{self.device_id}-sensor-{index}"

        for option in (rs.option.emitter_enabled, rs.option.thermal_compensation):
            try:
                if depth.supports(option):
                    with m.option_lock(self.device_id):
                        self.saved_options[option] = depth.get_option(option)
            except RuntimeError:
                pass

        if stream:
            from app.models.sensor_streaming import SensorStreamConfig
            from app.models.stream import Resolution
            config = SensorStreamConfig(stream_type="depth", format="z16",
                                        resolution=Resolution(width=CALIB_WIDTH, height=CALIB_HEIGHT), framerate=CALIB_FPS)
            m.start_sensor(self.device_id, self.depth_sensor_id, [config])

        for option, value in ((rs.option.emitter_enabled, 1.0), (rs.option.thermal_compensation, 0.0)):
            if option in self.saved_options:
                try:
                    with m.option_lock(self.device_id):
                        depth.set_option(option, value)
                except RuntimeError as exc:
                    logging.warning("calibration: could not set %s: %s", option, exc)

    def exit(self) -> None:
        m = self.manager
        try:
            if self.depth_sensor_id:
                m.stop_sensor(self.device_id, self.depth_sensor_id)
        except Exception as exc:
            logging.warning("calibration: stopping the calibration stream failed: %s", exc)
        try:
            dev = m._require_device(self.device_id)
            depth = next((s for s in dev.sensors if s.is_depth_sensor()), None)
            for option, value in self.saved_options.items():
                try:
                    with m.option_lock(self.device_id):
                        depth.set_option(option, value)
                except RuntimeError as exc:
                    logging.warning("calibration: could not restore %s: %s", option, exc)
        except Exception as exc:
            logging.warning("calibration: option restore failed: %s", exc)
        time.sleep(0.2)  # let the SDK finish stopping before reopening, as the legacy tool does
        for sensor_id, configs in self.previous:
            try:
                m.start_sensor(self.device_id, sensor_id, configs)
            except Exception as exc:
                logging.warning("calibration: could not restart %s: %s", sensor_id, exc)


class CalibrationMixin:
    """Mixed into RealSenseManager; see rs_manager.py."""

    def _calibration_sessions(self) -> Dict[str, CalibrationSession]:
        sessions = getattr(self, "_calib_sessions", None)
        if sessions is None:
            sessions = self._calib_sessions = {}
        return sessions

    def get_calibration(self, device_id: str) -> Dict[str, Any]:
        self._require_device(device_id)
        session = self._calibration_sessions().get(device_id)
        return session.to_dict() if session else {"kind": None, "state": "idle", "health": None, "verdict": None,
                                                  "has_new_table": False, "active": "old", "written": False,
                                                  "error": None, "started_at": None}

    def _start_calibration(self, device_id: str, kind: str, run: Callable[[Any, Callable[[float], None]], tuple]) -> Any:
        """Run `run(calib_dev, progress)` -> (table, health list) in a job thread inside the workspace."""
        dev = self._require_device(device_id)
        sessions = self._calibration_sessions()
        current = sessions.get(device_id)
        if current and current.state == "running":
            raise RealSenseError(status_code=409, detail="A calibration is already running on this device")
        session = CalibrationSession(kind)
        sessions[device_id] = session
        job = self.jobs.create(f"calibration_{kind}", device_id)

        def worker() -> None:
            workspace = CalibrationWorkspace(self, device_id)
            error: Optional[str] = None
            try:
                job.progress(0.0, "Preparing the device")
                workspace.enter()
                calib_dev = rs.auto_calibrated_device(dev)
                session.old_table = calib_dev.get_calibration_table()
                job.progress(0.05, "Calibrating")
                table, health = run(calib_dev, lambda p: job.progress(0.05 + 0.9 * max(0.0, min(1.0, p / 100.0)), "Calibrating"))
                if not table:
                    raise RealSenseError(status_code=500, detail="Calibration did not converge")
                session.new_table = table
                session.health = [float(h) for h in health]
                session.verdict = occ_verdict(session.health[0]) if kind == "occ" else None
                calib_dev.set_calibration_table(table)  # the legacy tool makes the new table active
                session.active = "new"
            except Exception as exc:
                error = str(getattr(exc, "detail", exc))
                logging.warning("calibration %s failed on %s: %s", kind, device_id, error)
            finally:
                job.progress(0.97, "Restoring the device")
                workspace.exit()  # streams and options are back before the job reports completion
            if error is None:
                session.state = "done"
                job.done(session.to_dict(), "Calibration completed")
            else:
                session.state = "failed"
                session.error = error
                job.fail(error)

        threading.Thread(target=worker, name=f"calibration-{device_id}", daemon=True).start()
        return job.info

    def start_on_chip_calibration(self, device_id: str, params: Dict[str, Any]) -> Any:
        request = occ_json(**params)
        timeout = OCC_TIMEOUT_MS

        def run(calib_dev, progress):
            table, health = calib_dev.run_on_chip_calibration(request, progress, timeout)
            return table, list(health)

        return self._start_calibration(device_id, "occ", run)

    def start_tare_calibration(self, device_id: str, ground_truth_mm: float, params: Dict[str, Any]) -> Any:
        request = tare_json(**params)

        def run(calib_dev, progress):
            table, health = calib_dev.run_tare_calibration(float(ground_truth_mm), request, progress, TARE_TIMEOUT_MS)
            return table, [h * 100 for h in health]  # the legacy tool shows tare health in percent

        return self._start_calibration(device_id, "tare", run)

    def apply_calibration(self, device_id: str, use_new: bool) -> Dict[str, Any]:
        dev = self._require_device(device_id)
        session = self._calibration_sessions().get(device_id)
        if not session or not session.new_table:
            raise RealSenseError(status_code=409, detail="No calibration result to apply")
        table = session.new_table if use_new else session.old_table
        with self.option_lock(device_id):
            rs.auto_calibrated_device(dev).set_calibration_table(table)
        session.active = "new" if use_new else "old"
        return session.to_dict()

    def keep_calibration(self, device_id: str) -> Dict[str, Any]:
        """Write the active table to flash (the legacy "Keep")."""
        if not self.settings.get().calibration.enable_writing:
            raise RealSenseError(status_code=403, detail="Writing calibration to the device is disabled in Settings")
        dev = self._require_device(device_id)
        session = self._calibration_sessions().get(device_id)
        if not session or not session.new_table:
            raise RealSenseError(status_code=409, detail="No calibration result to keep")
        with self.option_lock(device_id):
            rs.auto_calibrated_device(dev).write_calibration()
        session.written = True
        return session.to_dict()

    def get_calibration_table(self, device_id: str) -> Dict[str, Any]:
        """The device's coefficients table, parsed (calibration-model.cpp)."""
        from app.services import calibration_table
        dev = self._require_device(device_id)
        with self.option_lock(device_id):
            raw = bytes(rs.auto_calibrated_device(dev).get_calibration_table())
        return calibration_table.parse(raw)

    def set_calibration_table(self, device_id: str, patch: Dict[str, Any], write: bool) -> Dict[str, Any]:
        """Edit fields of the table, make it active and optionally write it to flash."""
        from app.services import calibration_table
        if write and not self.settings.get().calibration.enable_writing:
            raise RealSenseError(status_code=403, detail="Writing calibration to the device is disabled in Settings")
        dev = self._require_device(device_id)
        calib_dev = rs.auto_calibrated_device(dev)
        with self.option_lock(device_id):
            raw = bytes(calib_dev.get_calibration_table())
            edited = calibration_table.apply_patch(raw, patch)
            calib_dev.set_calibration_table(list(edited))  # the binding takes a list of bytes
            if write:
                calib_dev.write_calibration()
        return calibration_table.parse(edited)

    def reset_factory_calibration(self, device_id: str) -> Dict[str, Any]:
        if not self.settings.get().calibration.enable_writing:
            raise RealSenseError(status_code=403, detail="Writing calibration to the device is disabled in Settings")
        dev = self._require_device(device_id)
        with self.option_lock(device_id):
            rs.auto_calibrated_device(dev).reset_to_factory_calibration()
        self._calibration_sessions().pop(device_id, None)
        return self.get_calibration(device_id)
