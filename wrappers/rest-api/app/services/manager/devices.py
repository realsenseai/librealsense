# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Device discovery, registry, hot-plug and lookup helpers."""

import asyncio
import platform
import time
import logging
from typing import Callable, Deque, Dict, List, Optional, Any, Tuple, Set
import pyrealsense2 as rs
from app.core.errors import RealSenseError
from app.models.device import Device, DeviceInfo

_IS_WINDOWS = platform.system() == "Windows"


class DeviceRegistryMixin:
    """Mixed into RealSenseManager; see rs_manager.py."""

    def _on_devices_changed(self, info) -> None:
        """rs.context devices-changed callback. Runs on a pyrealsense2 internal thread."""
        added: List[str] = []
        removed: List[str] = []
        with self.lock:
            new_devs = list(info.get_new_devices())
            for serial, dev in list(self.devices.items()):
                if info.was_removed(dev):
                    removed.append(serial)
            for serial in removed:
                self._remove_device(serial)
            for dev in new_devs:
                serial = self._register_new_device(dev)
                if serial is not None:
                    added.append(serial)

        for serial in removed:
            self.metadata_socket_server.stop_broadcast(serial)

        if added or removed:
            logging.info("devices_changed: +%s -%s", added, removed)
            self._emit_socket_event("devices_changed", {"added": added, "removed": removed})

    def _remove_device(self, serial: str) -> None:
        assert self.lock.locked(), "_remove_device called without self.lock held"
        self.pipelines.pop(serial, None)
        self.configs.pop(serial, None)
        self.active_streams.pop(serial, None)
        self.frame_queues.pop(serial, None)
        self.metadata_queues.pop(serial, None)
        self.depth_frames.pop(serial, None)
        self.last_frames.pop(serial, None)
        self.options_poller.forget(serial)
        self.color_frames.pop(serial, None)
        self.point_clouds.pop(serial, None)
        # Drop the point-cloud-enabled flag so a re-plug of the same serial
        # doesn't silently resume emitting PC metadata that the UI thinks is
        # off (frontend resets to off on disconnect).
        self.is_pointcloud_enabled.pop(serial, None)
        self.colorizers.pop(serial, None)
        self.devices.pop(serial, None)
        self.device_infos.pop(serial, None)
        self._supported_md_by_profile.pop(serial, None)
        self.streaming_mode.pop(serial, None)

    def _register_new_device(self, dev: rs.device) -> Optional[str]:
        """Cache a freshly-discovered rs.device + its DeviceInfo. Caller must hold self.lock."""
        assert self.lock.locked(), "_register_new_device called without self.lock held"
        if not dev.supports(rs.camera_info.serial_number):
            return None
        device_id = dev.get_info(rs.camera_info.serial_number)
        # A recording keeps the recorded camera's serial; give it an id of its own so it can
        # be played back next to that camera.
        is_playback = bool(getattr(dev, "is_playback", lambda: False)())
        file_name = rs.playback(dev).file_name() if is_playback else None
        if is_playback:
            from app.services.manager.record_playback import playback_device_id
            device_id = playback_device_id(file_name)

        if device_id in self.devices:
            return None

        def _info(key, default=None):
            try:
                return dev.get_info(key)
            except RuntimeError:
                return default

        sensors: List[str] = [
            sensor.get_info(rs.camera_info.name)
            for sensor in dev.sensors
            if sensor.supports(rs.camera_info.name)
        ]

        metadata_enabled: Optional[bool] = None
        if _IS_WINDOWS:
            try:
                metadata_enabled = dev.is_metadata_enabled()
            except RuntimeError:
                pass

        # rs.camera_info has a member called "name", so the members are walked by key.
        all_info = {}
        for key, member in rs.camera_info.__members__.items():
            try:
                if dev.supports(member):
                    all_info[key] = dev.get_info(member)
            except RuntimeError:
                pass

        info = DeviceInfo(
            device_id=device_id,
            name=_info(rs.camera_info.name, "Unknown Device"),
            serial_number=device_id,
            firmware_version=_info(rs.camera_info.firmware_version),
            physical_port=_info(rs.camera_info.physical_port),
            usb_type=_info(rs.camera_info.usb_type_descriptor),
            product_id=_info(rs.camera_info.product_id),
            sensors=sensors,
            is_streaming=device_id in self.pipelines,
            metadata_enabled=metadata_enabled,
            info=all_info,
            is_playback=is_playback,
            file_name=file_name,
        )
        # Publish atomically at the end — if anything above raises, no partial
        # cache entry is left behind. Keep new work above this block.
        self.devices[device_id] = dev
        self.device_infos[device_id] = info
        self.streaming_mode.setdefault(device_id, "idle")
        return device_id

    def _emit_socket_event(self, event: str, payload: Dict[str, Any]) -> None:
        """Emit a Socket.IO event from sync contexts using the main FastAPI event loop."""
        loop = type(self)._main_loop
        if not loop or loop.is_closed():
            logging.warning("Socket emit skipped (no main loop): %s", event)
            return
        try:
            asyncio.run_coroutine_threadsafe(self.sio.emit(event, payload), loop)
        except Exception as exc:
            logging.warning("Socket emit failed (%s): %s", event, exc)

    def refresh_devices(self) -> List[DeviceInfo]:
        """Refresh the list of connected devices.

        Public entry point; skips ctx enumeration while a firmware update is in
        progress to avoid the polling thread invalidating the FW thread's
        ``rs.device`` handles (which causes ``null pointer passed for argument
        "device"`` on the subsequent ``update_dev.update(...)`` call). The FW
        thread itself uses ``_refresh_devices_locked`` directly to bypass the
        guard once DFU is complete.
        """
        with self.lock:
            if self._fw_updates_in_progress:
                logging.debug(
                    "refresh_devices: skipping ctx enumeration — FW update(s) in progress: %s",
                    self._fw_updates_in_progress,
                )
                return list(self.device_infos.values())
        return self._refresh_devices_locked()

    def _refresh_devices_locked(self) -> List[DeviceInfo]:
        """Actual device enumeration (no FW-in-progress guard)."""
        with self.lock:
            # Clear existing devices (that aren't streaming); loaded recordings stay.
            for device_id in list(self.devices.keys()):
                if device_id not in self.pipelines and device_id not in self._playbacks:
                    del self.devices[device_id]
                    self.device_infos.pop(device_id, None)
                    self._supported_md_by_profile.pop(device_id, None)

            for dev in self.ctx.devices:
                self._register_new_device(dev)

            # Update cache timestamp after a successful refresh
            import time
            self._last_refresh_time = time.perf_counter()
            return list(self.device_infos.values())

    def get_devices(self, force_refresh: bool = False) -> List[DeviceInfo]:
        """Get all connected devices, with optional forced refresh."""
        if force_refresh or not self.device_infos:
            return self.refresh_devices()
        with self.lock:
            return list(self.device_infos.values())

    def get_device(self, device_id: str, force_refresh: bool = False) -> DeviceInfo:
        """Get a specific device by ID"""
        devices = self.get_devices(force_refresh=force_refresh)
        for device in devices:
            if device.device_id == device_id:
                return device
        raise RealSenseError(status_code=404, detail=f"Device {device_id} not found")

    def _resolve_live_device(self, device_id: str) -> rs.device:
        """Return an rs.device for ``device_id`` enumerated fresh from self.ctx.

        Avoids using ``self.devices[device_id]`` directly: that cached wrapper
        can be invalidated underneath by a concurrent refresh_devices() (the
        polling loop), leaving a truthy Python object backed by a null C++
        pointer. Raises 404 if the device is no longer visible.
        """
        for dev in self.ctx.query_devices():
            try:
                if not dev.supports(rs.camera_info.serial_number):
                    continue
                if dev.get_info(rs.camera_info.serial_number) == device_id:
                    # Refresh the cache so downstream callers (refresh_devices)
                    # observe the same live handle.
                    with self.lock:
                        self.devices[device_id] = dev
                    return dev
            except RuntimeError:
                continue
        raise RealSenseError(status_code=404, detail=f"Device {device_id} not found")

    def _refresh_until_device_returns(
        self, device_id: str, attempts: int = 8,
    ) -> Optional[DeviceInfo]:
        """Re-enumerate until device_id reappears in device_infos.

        Calls ``_refresh_devices_locked`` directly to bypass the
        ``_fw_updates_in_progress`` guard on the public ``refresh_devices``: the
        FW slot is still claimed at this point (released in the outer
        ``finally``), but DFU has finished and we own the FW thread, so it's
        safe — and necessary — to enumerate.
        """
        # The USB re-enumeration may lag behind the SDK's first query. Give it
        # a few seconds to settle, then retry until the device reappears.
        time.sleep(3)
        for attempt in range(attempts):
            self._refresh_devices_locked()
            updated_info = self.device_infos.get(device_id)
            if updated_info:
                logging.info(
                    "Device %s re-appeared after firmware update (attempt %d)",
                    device_id, attempt + 1,
                )
                return updated_info
            logging.debug(
                "Device %s not yet visible after firmware update (attempt %d)",
                device_id, attempt + 1,
            )
            time.sleep(1)
        logging.warning(
            "Device %s did not reappear after firmware update; "
            "frontend will keep polling.", device_id,
        )
        return None

    def reset_device(self, device_id: str) -> bool:
        """Reset a specific device by ID"""
        with self.lock:
            if device_id not in self.devices:
                raise RealSenseError(
                    status_code=404, detail=f"Device {device_id} not found"
                )
            dev = self.devices[device_id]
            self._remove_device(device_id)

        self._emit_socket_event("devices_changed", {"added": [], "removed": [device_id]})

        try:
            dev.hardware_reset()
            return True
        except Exception as e:
            # Reset failed: handle still valid, device still plugged in.
            # Restore cache + announce device back so FE stops showing it as gone.
            with self.lock:
                self._register_new_device(dev)
            self._emit_socket_event(
                "devices_changed", {"added": [device_id], "removed": []},
            )
            raise RealSenseError(
                status_code=500, detail=f"Failed to reset device: {str(e)}"
            )

    def _find_sensor(self, device_id: str, sensor_id: str):
        """Resolve a sensor by its "<serial>-sensor-<index>" id."""
        dev = self._require_device(device_id)
        try:
            index = int(sensor_id.split("-")[-1])
        except ValueError:
            raise RealSenseError(status_code=404, detail=f"Invalid sensor ID format: {sensor_id}")
        if index < 0 or index >= len(dev.sensors):
            raise RealSenseError(status_code=404, detail=f"Sensor {sensor_id} not found")
        return dev.sensors[index]

    def _require_device(self, device_id: str):
        if device_id not in self.devices:
            self.refresh_devices()
        dev = self.devices.get(device_id)
        if dev is None:
            raise RealSenseError(status_code=404, detail=f"Device {device_id} not found")
        return dev

    def _get_sensor_by_id(self, device_id: str, sensor_id: str) -> Tuple[rs.sensor, int]:
        """
        Get sensor object and index from sensor_id.
        
        Returns:
            Tuple of (sensor, sensor_index)
        """
        if device_id not in self.devices:
            self.refresh_devices()
            if device_id not in self.devices:
                raise RealSenseError(
                    status_code=404, detail=f"Device {device_id} not found"
                )
        
        dev = self.devices[device_id]
        
        # Parse sensor index from sensor_id (format: "{device_id}-sensor-{index}")
        try:
            sensor_index = int(sensor_id.split("-")[-1])
            if sensor_index < 0 or sensor_index >= len(dev.sensors):
                raise RealSenseError(
                    status_code=404, detail=f"Sensor {sensor_id} not found"
                )
        except (ValueError, IndexError):
            raise RealSenseError(
                status_code=404, detail=f"Invalid sensor ID format: {sensor_id}"
            )
        
        return dev.sensors[sensor_index], sensor_index
