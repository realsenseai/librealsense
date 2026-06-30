# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import asyncio
import platform
import struct
import threading
import time
import logging
from collections import deque
from typing import Callable, Deque, Dict, List, Optional, Any, Tuple, Set
import pyrealsense2 as rs
import numpy as np
import cv2
from app.core.errors import RealSenseError
from app.models.device import Device, DeviceInfo
from app.models.sensor import Sensor, SensorInfo, SupportedStreamProfile
from app.models.option import Option, OptionInfo
from app.models.stream import PointCloudStatus, StreamConfig, StreamStatus, Resolution
from app.models.sensor_streaming import (
    SensorStreamConfig,
    SensorStartItem,
    SensorStreamStatus,
    BatchSensorStatus,
)
import socketio
from datetime import datetime

from app.services.metadata_socket_server import MetadataSocketServer


_IS_WINDOWS = platform.system() == "Windows"

FW_STATUS_UNKNOWN = "unknown"

# Recent color frames retained per device for texturing the 3D point cloud.
# Five frames at 30 fps covers the typical depth/color SDK-latency offset
# (~one capture interval); the picker selects the entry whose timestamp is
# closest to the depth frame being processed.
COLOR_FRAME_HISTORY = 5


class RealSenseManager:
    # Class-level event loop reference for async operations from sync contexts
    _main_loop: Optional[asyncio.AbstractEventLoop] = None
    
    @classmethod
    def set_event_loop(cls, loop: asyncio.AbstractEventLoop):
        """Store reference to main event loop for use in sync callbacks."""
        cls._main_loop = loop
    
    def __init__(self, sio: socketio.AsyncServer):
        self.ctx = rs.context()
        self.devices: Dict[str, rs.device] = {}
        self.device_infos: Dict[str, DeviceInfo] = {}
        self.pipelines: Dict[str, rs.pipeline] = {}
        self.configs: Dict[str, rs.config] = {}
        self.active_streams: Dict[str, Set[str]] = (
            {}
        )  # device_id -> set of stream types
        self.frame_queues: Dict[str, Dict[str, List]] = (
            {}
        )  # device_id -> stream_type -> list of frames
        self.metadata_queues: Dict[str, Dict[str, List[Dict]]] = (
            {}
        )  # device_id -> stream_type -> list of metadata dicts
        self.lock = threading.Lock()
        self.max_queue_size = 5
        self.is_pointcloud_enabled: Dict[str, bool] = {}
        # One rs.pointcloud() per device: pc.map_to(color) mutates internal
        # state, and depth/color threads from different devices would race on
        # a shared instance (texturing device A's cloud with device B's color).
        self.point_clouds: Dict[str, "rs.pointcloud"] = {}

        # Caches for pipeline/config reuse to reduce startup cost
        self.config_cache: Dict[str, Dict[str, rs.config]] = {}  # device -> signature -> config
        self.pipeline_cache: Dict[str, rs.pipeline] = {}  # device -> last pipeline object
        self.pipeline_signatures: Dict[str, str] = {}  # device -> active signature

        # Stop coordination
        self.stopping: Set[str] = set()

        # Store latest raw depth frames for pixel depth queries
        self.depth_frames: Dict[str, Any] = {}  # device_id -> rs.depth_frame

        # Short history of recent color frames per device for texturing the
        # 3D point cloud. Sensor-mode runs depth/color on independent threads
        # so the depth thread looks up the closest-by-timestamp color frame
        # here via ``_pick_color_for_depth``. Cross-sensor matching by
        # ``frame.get_timestamp()`` only works because ``start_sensor`` sets
        # ``global_time_enabled`` on the sensor — without that, different
        # sensors can report timestamps in different clock domains (a fixed
        # multi-second offset). A deque(maxlen=N) gives O(1) bounded append
        # and atomic-under-GIL pop-on-overflow, which the previous
        # list+pop(0) pattern did not.
        self.color_frames: Dict[str, Deque[Any]] = {}  # device_id -> deque of rs.video_frame, oldest first

        # Cache of supported frame_metadata values keyed by device_id then profile uid.
        # Nested so a single device's profiles can be evicted without wiping others.
        # Avoids re-probing every metadata key on every frame in the hot loop.
        self._supported_md_by_profile: Dict[str, Dict[int, list]] = {}

        # Firmware update tracking (one update at a time per device)
        self._fw_updates_in_progress: Set[str] = set()

        self.sio = sio
        self.metadata_socket_server = MetadataSocketServer(sio, self)

        # Device discovery cache metadata
        self._last_refresh_time: float = 0.0

        # --- Per-Sensor Streaming State (Sensor API) ---
        # Tracks which mode each device is using: "pipeline", "sensor", or "idle"
        self.streaming_mode: Dict[str, str] = {}  # device_id -> mode
        # Per-sensor streaming info: device_id -> sensor_id -> SensorStreamInfo dict
        self.sensor_streams: Dict[str, Dict[str, dict]] = {}
        # Per-sensor frame queues: device_id -> sensor_id -> list of frames
        self.sensor_frame_queues: Dict[str, Dict[str, List]] = {}
        # Per-sensor metadata queues: device_id -> sensor_id -> list of metadata dicts

        # --- Post-Processing Filters ---
        # Stores filter instances per device/sensor: device_id -> sensor_id -> list of filter dicts
        # Each filter dict: { "filter": rs.filter, "name": str, "enabled": bool }
        self.processing_blocks: Dict[str, Dict[str, List[Dict[str, Any]]]] = {}
        self.sensor_metadata_queues: Dict[str, Dict[str, List[Dict]]] = {}
        # Per-sensor rs.frame_queue objects: device_id -> sensor_id -> rs.frame_queue
        self.sensor_rs_queues: Dict[str, Dict[str, Any]] = {}
        # Track sensor stopping state
        self.sensor_stopping: Dict[str, Set[str]] = {}  # device_id -> set of sensor_ids

        # Initialize devices
        self.refresh_devices()

        # Refresh devices when one is plugged in or out.
        self.ctx.set_devices_changed_callback(self._on_devices_changed)

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
        self.color_frames.pop(serial, None)
        self.point_clouds.pop(serial, None)
        # Drop the point-cloud-enabled flag so a re-plug of the same serial
        # doesn't silently resume emitting PC metadata that the UI thinks is
        # off (frontend resets to off on disconnect).
        self.is_pointcloud_enabled.pop(serial, None)
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

        info = DeviceInfo(
            device_id=device_id,
            name=_info(rs.camera_info.name, "Unknown Device"),
            serial_number=device_id,
            firmware_version=_info(rs.camera_info.firmware_version),
            recommended_firmware_version=None,
            firmware_status=FW_STATUS_UNKNOWN,
            firmware_file_available=False,
            physical_port=_info(rs.camera_info.physical_port),
            usb_type=_info(rs.camera_info.usb_type_descriptor),
            product_id=_info(rs.camera_info.product_id),
            sensors=sensors,
            is_streaming=device_id in self.pipelines,
            metadata_enabled=metadata_enabled,
        )
        # Publish atomically at the end — if anything above raises, no partial
        # cache entry is left behind. Keep new work above this block.
        self.devices[device_id] = dev
        self.device_infos[device_id] = info
        self.streaming_mode.setdefault(device_id, "idle")
        return device_id

    def _emit_socket_event(self, event: str, payload: Dict[str, Any]) -> None:
        """Emit a Socket.IO event from sync contexts using the main FastAPI event loop."""
        loop = RealSenseManager._main_loop
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
            # Clear existing devices (that aren't streaming)
            for device_id in list(self.devices.keys()):
                if device_id not in self.pipelines:
                    del self.devices[device_id]
                    self.device_infos.pop(device_id, None)
                    self._supported_md_by_profile.pop(device_id, None)

            for dev in self.ctx.devices:
                self._register_new_device(dev)

            # Update cache timestamp after a successful refresh
            import time
            self._last_refresh_time = time.perf_counter()
            return list(self.device_infos.values())

    def _make_signature(self, configs: List[StreamConfig], align_to: Optional[str]) -> str:
        """Deterministic signature for a stream start request."""
        parts = []
        for cfg in sorted(configs, key=lambda c: (c.stream_type.lower(), c.sensor_id, c.resolution.width, c.resolution.height, c.framerate, c.format.lower())):
            parts.append(
                f"{cfg.stream_type.lower()}|{cfg.format.lower()}|{cfg.resolution.width}x{cfg.resolution.height}@{cfg.framerate}|sensor:{cfg.sensor_id}"
            )
        align_part = align_to.lower() if align_to else "none"
        return ";".join(parts) + f"|align:{align_part}"

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

    def get_firmware_status(self, device_id: str) -> Dict[str, Any]:
        """Return firmware status metadata for a device."""
        device = self.get_device(device_id)
        return {
            "device_id": device_id,
            "current": device.firmware_version,
            "recommended": None,
            "status": device.firmware_status or FW_STATUS_UNKNOWN,
            "file_available": False,
        }

    @staticmethod
    def _is_update_device(dev: rs.device) -> bool:
        """True if the device exposes the DFU update interface.

        Some pyrealsense2 builds return an empty rs.update_device wrapper
        (truthy Python object, null underlying pointer) instead of throwing
        for a non-DFU device. Validate the wrapper via bool() to catch that.
        """
        try:
            up = rs.update_device(dev)
            return bool(up)
        except Exception:
            return False

    def update_firmware_from_bytes(self, device_id: str, fw_bytes: bytes) -> Dict[str, Any]:
        """Run firmware update using a user-supplied image blob.

        Reuses the DFU flow: check_firmware_compatibility -> enter_update_state ->
        wait for DFU device -> update_dev.update(image, on_progress) -> wait for reconnect.
        Emits the same Socket.IO progress / success / failure events as a bundled-image update.
        """
        self._claim_fw_update_slot(device_id)
        try:
            self._ensure_fw_update_allowed(device_id)
            # pyrealsense2 accepts bytes-like objects; bytearray keeps memory
            # bounded (list(bytes) would balloon ~28x by boxing each byte).
            try:
                fw_image = bytearray(fw_bytes)
            except Exception as exc:
                logging.error("Failed to materialize firmware bytes: %s", exc)
                raise RealSenseError(status_code=400, detail="Invalid firmware payload")

            # Re-fetch the device handle directly from the SDK context. The cached
            # `self.devices[device_id]` Python wrapper can outlive its underlying
            # C++ device pointer when refresh_devices() runs concurrently (the
            # 5-second polling loop) or when the device re-enumerates between
            # the GET /devices call and this POST, producing a wrapper that is
            # still truthy but whose C++ pointer is null. Passing such a handle
            # to rs.updatable() raises `null pointer passed for argument "device"`.
            target_dev = self._resolve_live_device(device_id)
            firmware_update_id = self._resolve_firmware_update_id(target_dev, device_id)

            progress_holder, on_progress = self._make_fw_progress_callback(device_id)
            # Always emit a starting progress so the UI doesn't stay at 0% forever
            self._emit_socket_event(
                f"firmware_progress_{device_id}",
                {"device_id": device_id, "progress": 0.0},
            )

            try:
                update_dev = self._enter_dfu_and_get_update_dev(
                    target_dev, fw_image, device_id, firmware_update_id,
                )
                if not update_dev:
                    raise RealSenseError(
                        status_code=500,
                        detail="Could not obtain a valid DFU device handle for the update",
                    )

                logging.info("Starting firmware update on DFU device...")
                update_dev.update(fw_image, on_progress)
                self._post_update_hardware_reset(update_dev)

                logging.info("Firmware download completed, waiting for device to finalize...")
                time.sleep(3)

                # Drop SDK references to the pre-DFU handles; they will be
                # invalidated by the device re-enumeration. The ctx itself is
                # kept — replacing it would silently drop the
                # set_devices_changed_callback registration done in __init__,
                # and the post-DFU device-back event would never reach us.
                update_dev = None
                target_dev = None

                self._wait_for_device_reconnect(device_id, firmware_update_id)
            except RealSenseError:
                raise
            except Exception as exc:
                logging.exception("Firmware update failed for %s", device_id)
                self._emit_socket_event(
                    f"firmware_update_failed_{device_id}",
                    {"device_id": device_id, "error": str(exc)},
                )
                raise RealSenseError(status_code=500, detail=f"Firmware update failed: {exc}")

            updated_info = self._refresh_until_device_returns(device_id)

            self._emit_socket_event(
                f"firmware_progress_{device_id}",
                {"device_id": device_id, "progress": 1.0},
            )
            self._emit_socket_event(
                f"firmware_update_success_{device_id}",
                {
                    "device_id": device_id,
                    "firmware_version": updated_info.firmware_version if updated_info else None,
                },
            )

            return {
                "device_id": device_id,
                "progress": progress_holder["value"],
                "firmware_version": updated_info.firmware_version if updated_info else None,
                "status": "success",
            }
        except RealSenseError as exc:
            self._emit_socket_event(
                f"firmware_update_failed_{device_id}",
                {"device_id": device_id, "error": exc.detail},
            )
            raise
        finally:
            with self.lock:
                self._fw_updates_in_progress.discard(device_id)

    # ---- update_firmware_from_bytes helpers ---------------------------------
    # Split out so the orchestrator above reads as a sequence of named steps
    # rather than a 300-line block of intermixed locking, SDK calls, polling,
    # and socket emission.

    def _claim_fw_update_slot(self, device_id: str) -> None:
        """Reserve the per-device FW-update slot; raise 409 if one is already running."""
        with self.lock:
            if device_id in self._fw_updates_in_progress:
                raise RealSenseError(status_code=409, detail="Firmware update already in progress")
            self._fw_updates_in_progress.add(device_id)

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

    def _ensure_fw_update_allowed(self, device_id: str) -> None:
        """Reject the update if the device is unknown or if anything is streaming."""
        if device_id not in self.devices:
            raise RealSenseError(status_code=404, detail=f"Device {device_id} not found")
        # Only refuse if THIS device is streaming. Since we no longer recreate
        # self.ctx during the update, other devices' handles stay valid and
        # their pipelines are unaffected by the DFU transition of this device.
        with self.lock:
            if device_id in self.pipelines:
                raise RealSenseError(
                    status_code=400,
                    detail="Stop streaming on this device before updating firmware",
                )

    @staticmethod
    def _resolve_firmware_update_id(target_dev: rs.device, device_id: str) -> str:
        """Return FIRMWARE_UPDATE_ID (stable across DFU transitions) or fall back to device_id."""
        firmware_update_id: Optional[str] = None
        try:
            if target_dev.supports(rs.camera_info.firmware_update_id):
                firmware_update_id = target_dev.get_info(rs.camera_info.firmware_update_id)
            else:
                sensors = target_dev.query_sensors()
                if sensors:
                    firmware_update_id = sensors[0].get_info(rs.camera_info.firmware_update_id)
        except RuntimeError:
            pass
        if not firmware_update_id:
            firmware_update_id = device_id
        logging.info("Firmware update id for tracking: %s", firmware_update_id)
        return firmware_update_id

    def _make_fw_progress_callback(
        self, device_id: str,
    ) -> Tuple[Dict[str, float], Callable[[float], None]]:
        """Build the on_progress callback and a holder dict that records the latest value.

        Rate-limits emissions to ~10/sec but always lets the final 100% through.
        """
        progress_holder = {"value": 0.0}
        last_emit_ts = {"value": 0.0}

        def _on_progress(p: float) -> None:
            progress_holder["value"] = p
            now = time.time()
            if now - last_emit_ts["value"] < 0.1 and p < 1.0:
                return
            last_emit_ts["value"] = now
            self._emit_socket_event(
                f"firmware_progress_{device_id}",
                {"device_id": device_id, "progress": float(p)},
            )

        return progress_holder, _on_progress

    def _enter_dfu_and_get_update_dev(
        self,
        target_dev: rs.device,
        fw_image: bytearray,
        device_id: str,
        firmware_update_id: str,
    ):
        """Return a DFU update_device — either the device is already in DFU, or push it there.

        Runs the compatibility check, drops cached refs, calls enter_update_state(), and
        polls the SDK until the matching DFU device shows up (or raises on timeout).
        """
        try:
            update_dev = rs.update_device(target_dev)
            # Some pyrealsense2 builds return an empty wrapper for non-DFU
            # devices instead of throwing. bool() returns False on the empty
            # wrapper, so treat that as "not in DFU yet" and fall through to
            # the enter_update_state path below.
            if update_dev:
                logging.info("Device is already in DFU mode")
                return update_dev
        except Exception:
            pass

        logging.info("Device is in normal mode, checking firmware compatibility...")
        updatable = rs.updatable(target_dev)

        try:
            if not updatable.check_firmware_compatibility(fw_image):
                raise RealSenseError(status_code=400, detail="Firmware is not compatible with this device")
            logging.info("Firmware compatibility check passed")
        except RealSenseError:
            raise
        except Exception as e:
            logging.warning("Firmware compatibility check failed or not supported: %s", e)

        # Cached refs will become invalid after enter_update_state.
        with self.lock:
            self._remove_device(device_id)
        self._emit_socket_event(
            "devices_changed", {"added": [], "removed": [device_id]},
        )

        logging.info("Requesting device to enter DFU mode...")
        updatable.enter_update_state()

        update_dev = self._wait_for_dfu_device(firmware_update_id)
        if not update_dev:
            raise RealSenseError(
                status_code=500,
                detail="Device did not enter DFU mode within timeout. Please reconnect the device and try again.",
            )
        return update_dev

    def _wait_for_dfu_device(self, firmware_update_id: str, timeout_seconds: int = 60):
        """Poll self.ctx until a DFU device matching firmware_update_id appears; return it or None.

        Falls back to "exactly one visible DFU device" when firmware_update_id isn't reported.
        """
        start_time = time.time()
        while time.time() - start_time < timeout_seconds:
            time.sleep(0.5)
            try:
                devs = self.ctx.query_devices()
                for dev in devs:
                    try:
                        candidate = rs.update_device(dev)
                    except Exception:
                        continue
                    # Empty wrapper from a non-DFU device (some pyrealsense2
                    # builds return one rather than throwing). Skip — calling
                    # update() on it later raises 'null pointer for "device"'.
                    if not candidate:
                        continue
                    try:
                        if dev.supports(rs.camera_info.firmware_update_id):
                            dev_fw_id = dev.get_info(rs.camera_info.firmware_update_id)
                            if dev_fw_id == firmware_update_id:
                                return candidate
                    except RuntimeError:
                        pass
                    # Fallback: if exactly one DFU device is visible, assume it's ours.
                    if sum(1 for d in devs if self._is_update_device(d)) == 1:
                        return candidate
            except Exception as e:
                logging.debug("Error querying devices during DFU wait: %s", e)
                continue
        return None

    @staticmethod
    def _post_update_hardware_reset(update_dev) -> None:
        # update_dev.update() is supposed to call hardware_reset() internally
        # (see common/fw-update-helper.cpp), but on some devices/firmware
        # combos the device stays in DFU/recovery mode. Issue an explicit
        # reset as a belt-and-suspenders kick to force USB re-enumeration.
        try:
            update_dev.hardware_reset()
            logging.info("Issued explicit hardware_reset() on DFU device after update")
        except Exception as exc:
            logging.warning("Explicit hardware_reset() on DFU device failed (likely benign): %s", exc)

    def _wait_for_device_reconnect(
        self, device_id: str, firmware_update_id: str, max_wait_seconds: int = 120,
    ) -> None:
        """Wait for the device to re-enumerate in normal mode after DFU.

        Polls ``self.ctx.query_devices()`` once a second. The SDK's
        devices-changed callback (registered once in ``__init__``) also fires
        when the device returns; we don't touch self.ctx here because
        replacing it would silently drop that callback registration.

        Raises RealSenseError if the device sticks in DFU/recovery; logs and returns
        if it simply never reappears within the timeout (update may have succeeded).
        """
        reconnected = False
        stuck_in_dfu = False
        start_time = time.time()
        while time.time() - start_time < max_wait_seconds:
            time.sleep(1)
            try:
                devs = self.ctx.query_devices()
                # First pass: look for the device in NORMAL mode.
                for dev in devs:
                    try:
                        if self._is_update_device(dev):
                            continue
                        sensors = dev.query_sensors()
                        if sensors:
                            try:
                                dev_fw_id = sensors[0].get_info(rs.camera_info.firmware_update_id)
                                if dev_fw_id == firmware_update_id:
                                    reconnected = True
                                    break
                            except RuntimeError:
                                pass
                        try:
                            if dev.supports(rs.camera_info.serial_number):
                                sn = dev.get_info(rs.camera_info.serial_number)
                                if sn == device_id:
                                    reconnected = True
                                    break
                        except RuntimeError:
                            pass
                    except Exception:
                        continue
                if reconnected:
                    break
                # Second pass: is the device still sitting in DFU/recovery?
                stuck_in_dfu = False
                for dev in devs:
                    try:
                        if not self._is_update_device(dev):
                            continue
                        try:
                            if dev.supports(rs.camera_info.firmware_update_id):
                                dev_fw_id = dev.get_info(rs.camera_info.firmware_update_id)
                                if dev_fw_id == firmware_update_id:
                                    stuck_in_dfu = True
                                    break
                        except RuntimeError:
                            pass
                        # Fallback: any DFU device counts if we don't have ID match.
                        stuck_in_dfu = True
                    except Exception:
                        continue
            except Exception as e:
                logging.debug("Error querying devices during reconnect wait: %s", e)
                continue

        if reconnected:
            return
        if stuck_in_dfu:
            logging.error(
                "Device %s is stuck in DFU/recovery mode after firmware update. "
                "The firmware image may be incompatible or the update did not finalize.",
                device_id,
            )
            msg = (
                "Device is stuck in recovery (DFU) mode after the update. "
                "Please physically disconnect and reconnect the device, then try again "
                "with a known-good firmware image."
            )
            self._emit_socket_event(
                f"firmware_update_failed_{device_id}",
                {"device_id": device_id, "error": msg},
            )
            raise RealSenseError(status_code=500, detail=msg)
        logging.warning("Device did not reconnect within timeout, but update may have succeeded")

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

    def get_sensors(self, device_id: str) -> List[SensorInfo]:
        """Get all sensors for a device"""
        if device_id not in self.devices:
            self.refresh_devices()
        if device_id not in self.devices:
            raise RealSenseError(
                status_code=404, detail=f"Device {device_id} not found"
            )

        dev = self.devices[device_id]
        sensors = []

        for i, sensor in enumerate(dev.sensors):
            sensor_id = f"{device_id}-sensor-{i}"
            try:
                name = sensor.get_info(rs.camera_info.name)
            except RuntimeError:
                name = f"Sensor {i}"

            # Determine sensor type
            sensor_type = sensor.name

            # Get supported stream profiles
            profiles = sensor.get_stream_profiles()
            supported_stream_profiles = (
                {}
            )  # Dictionary to temporarily store profiles by stream_type

            for profile in profiles:
                if profile.is_video_stream_profile():
                    video_profile = profile.as_video_stream_profile()
                    fmt = str(profile.format()).split(".")[1]
                    width, height = video_profile.width(), video_profile.height()
                    fps = video_profile.fps()
                else:
                    # Motion stream profiles - get actual fps, use placeholder for format/resolution
                    fmt = "combined_motion"
                    width, height = 320, 120  # Visualization frame size
                    fps = profile.fps()  # Use actual motion sensor fps
                stream_type = profile.stream_type().name
                if profile.stream_type() == rs.stream.infrared:
                    stream_index = profile.stream_index()
                    if stream_index == 0:
                        continue
                    else:
                        stream_type = f"{profile.stream_type().name}-{stream_index}"

                if stream_type not in supported_stream_profiles:
                    supported_stream_profiles[stream_type] = {
                        "stream_type": stream_type,
                        "resolutions": [],
                        "fps": [],
                        "formats": [],
                    }

                # Add resolution if not already in the list
                resolution = (width, height)
                if (
                    resolution
                    not in supported_stream_profiles[stream_type]["resolutions"]
                ):
                    supported_stream_profiles[stream_type]["resolutions"].append(
                        resolution
                    )

                # Add fps if not already in the list
                if fps not in supported_stream_profiles[stream_type]["fps"]:
                    supported_stream_profiles[stream_type]["fps"].append(fps)

                # Add format if not already in the list
                if fmt not in supported_stream_profiles[stream_type]["formats"]:
                    supported_stream_profiles[stream_type]["formats"].append(fmt)

            # Convert dictionary to list of SupportedStreamProfile objects
            stream_profiles_list = []
            for stream_data in supported_stream_profiles.values():
                stream_profile = SupportedStreamProfile(
                    stream_type=stream_data["stream_type"],
                    resolutions=stream_data["resolutions"],
                    fps=stream_data["fps"],
                    formats=stream_data["formats"],
                )
                stream_profiles_list.append(stream_profile)

            # Get options
            options = self.get_sensor_options(device_id, sensor_id)

            sensor_info = SensorInfo(
                sensor_id=sensor_id,
                name=name,
                type=sensor_type,
                supported_stream_profiles=stream_profiles_list,  # Use correct field name
                options=options,
            )

            sensors.append(sensor_info)

        return sensors

    def get_sensor(self, device_id: str, sensor_id: str) -> SensorInfo:
        """Get a specific sensor by ID"""
        sensors = self.get_sensors(device_id)
        for sensor in sensors:
            if sensor.sensor_id == sensor_id:
                return sensor
        raise RealSenseError(status_code=404, detail=f"Sensor {sensor_id} not found")

    def get_sensor_options(self, device_id: str, sensor_id: str) -> List[OptionInfo]:
        """Get all options for a sensor"""
        if device_id not in self.devices:
            self.refresh_devices()
            if device_id not in self.devices:
                raise RealSenseError(
                    status_code=404, detail=f"Device {device_id} not found"
                )

        dev = self.devices[device_id]

        # Parse sensor index from sensor_id
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

        sensor = dev.sensors[sensor_index]
        options = []
        
        # 1. Add native sensor options (Basic Controls)
        for option in sensor.get_supported_options():
            try:
                opt_name = option.name
                current_value = sensor.get_option(option)
                option_range = sensor.get_option_range(option)

                option_info = OptionInfo(
                    option_id=opt_name,
                    name=opt_name.replace("_", " ").title(),
                    description=sensor.get_option_description(option),
                    current_value=current_value,
                    default_value=option_range.default,
                    min_value=option_range.min,
                    max_value=option_range.max,
                    step=option_range.step,
                    read_only=sensor.is_option_read_only(option),
                    category="Basic Controls",
                )
                options.append(option_info)
            except RuntimeError as e:
                # Skip options that can't be read
                pass

        # 2. Add post-processing filter options
        filters = self._get_or_create_processing_blocks(device_id, sensor_id, sensor)
        for filter_info in filters:
            filter_obj = filter_info["filter"]
            filter_name = filter_info["name"]
            # Use URL-safe name for option_id (replace spaces with underscores)
            safe_filter_name = filter_name.replace(" ", "_")
            is_enabled = filter_info["enabled"]
            
            # Add enable/disable toggle for the filter
            options.append(OptionInfo(
                option_id=f"PP_{safe_filter_name}_Enabled",
                name=f"{filter_name}",
                description=f"Enable/Disable {filter_name}",
                current_value=1.0 if is_enabled else 0.0,
                default_value=filter_info["default_enabled"],
                min_value=0.0,
                max_value=1.0,
                step=1.0,
                read_only=False,
                category="Post-Processing",
                filter_name=filter_name,
            ))
            
            # Hidden options that shouldn't be shown to users (same as legacy viewer)
            hidden_options = {
                'frames_queue_size',
                'stream_filter', 
                'stream_format_filter',
                'stream_index_filter',
                'noise_estimation',
                'region_of_interest',
            }
            
            # Add filter-specific options (excluding hidden ones)
            for opt in filter_obj.get_supported_options():
                try:
                    opt_name = opt.name
                    # Skip hidden options
                    if opt_name in hidden_options:
                        continue
                        
                    current_value = filter_obj.get_option(opt)
                    option_range = filter_obj.get_option_range(opt)
                    opt_description = filter_obj.get_option_description(opt)
                    
                    # For holes_fill option, use description as display name (matches legacy viewer)
                    # This is because holes_fill has different meanings per filter:
                    # - Spatial: "Holes filling mode"
                    # - Temporal: "Persistency mode"  
                    # - Hole Filling: "Hole Filling mode"
                    if opt_name == 'holes_fill':
                        display_name = opt_description
                    else:
                        display_name = opt_name.replace('_', ' ').title()
                    
                    # Check for enum-type options (step of 1, integer range)
                    # and collect value descriptions if available
                    value_descs = None
                    if option_range.step == 1.0 and option_range.min == int(option_range.min) and option_range.max == int(option_range.max):
                        # Might be an enum, try to get value descriptions
                        descs = {}
                        for val in range(int(option_range.min), int(option_range.max) + 1):
                            try:
                                desc = filter_obj.get_option_value_description(opt, float(val))
                                if desc:
                                    descs[str(val)] = desc
                            except RuntimeError:
                                pass
                        if descs:
                            value_descs = descs
                    
                    options.append(OptionInfo(
                        option_id=f"PP_{safe_filter_name}_{opt_name}",
                        name=display_name,
                        description=opt_description,
                        current_value=current_value,
                        default_value=option_range.default,
                        min_value=option_range.min,
                        max_value=option_range.max,
                        step=option_range.step,
                        read_only=filter_obj.is_option_read_only(opt),
                        category="Post-Processing",
                        filter_name=filter_name,
                        value_descriptions=value_descs,
                    ))
                except RuntimeError:
                    pass

        return options

    def _get_or_create_processing_blocks(self, device_id: str, sensor_id: str, sensor) -> List[Dict[str, Any]]:
        """Get or create post-processing filter blocks for a sensor.
        
        Uses the SDK's get_recommended_filters() to get sensor-appropriate filters.
        Returns list of dicts: { "filter": rs.filter, "name": str, "enabled": bool, "default_enabled": float }
        """
        if device_id not in self.processing_blocks:
            self.processing_blocks[device_id] = {}
        
        if sensor_id not in self.processing_blocks[device_id]:
            filters = []
            try:
                recommended = sensor.get_recommended_filters()
                for f in recommended:
                    try:
                        filter_name = f.get_info(rs.camera_info.name)
                    except RuntimeError:
                        filter_name = "Unknown Filter"
                    
                    # All filters disabled by default for performance
                    # User can enable specific filters as needed
                    default_enabled = 0.0
                    
                    filters.append({
                        "filter": f,
                        "name": filter_name,
                        "enabled": bool(default_enabled),
                        "default_enabled": default_enabled,
                    })
            except RuntimeError:
                # Sensor doesn't support get_recommended_filters
                pass
            
            self.processing_blocks[device_id][sensor_id] = filters
        
        return self.processing_blocks[device_id][sensor_id]

    def get_sensor_option(
        self, device_id: str, sensor_id: str, option_id: str
    ) -> OptionInfo:
        """Get a specific option for a sensor"""
        options = self.get_sensor_options(device_id, sensor_id)
        for option in options:
            if option.option_id == option_id:
                return option
        raise RealSenseError(status_code=404, detail=f"Option {option_id} not found")

    def set_sensor_option(
        self, device_id: str, sensor_id: str, option_id: str, value: Any
    ) -> bool:
        """Set an option value for a sensor"""
        if device_id not in self.devices:
            self.refresh_devices()
            if device_id not in self.devices:
                raise RealSenseError(
                    status_code=404, detail=f"Device {device_id} not found"
                )

        dev = self.devices[device_id]

        # Parse sensor index from sensor_id
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

        sensor = dev.sensors[sensor_index]

        # Check if this is a post-processing filter option (starts with "PP_")
        if option_id.startswith("PP_"):
            return self._set_filter_option(device_id, sensor_id, sensor, option_id, value)

        # Find the option by name (case-insensitive comparison)
        # Match against both raw option name and display name
        option_value = None
        supported_options = list(sensor.get_supported_options())
        option_id_lower = option_id.lower().replace(" ", "_")  # Normalize spaces to underscores
        
        for option in supported_options:
            opt_name_lower = option.name.lower()
            # Match by raw name or by normalized display name
            if opt_name_lower == option_id_lower or opt_name_lower == option_id.lower():
                option_value = option
                break

        if option_value is None:
            # Provide helpful error with available options
            available_names = [opt.name for opt in supported_options]
            raise RealSenseError(
                status_code=404, 
                detail=f"Option '{option_id}' not found. Available options: {', '.join(available_names)}"
            )

        # Check value range (only for numeric values)
        option_range = sensor.get_option_range(option_value)
        
        # Convert boolean to float (RealSense uses 0/1 for booleans)
        if isinstance(value, bool):
            value = 1.0 if value else 0.0
        
        # Ensure value is numeric for range check
        try:
            numeric_value = float(value)
            if numeric_value < option_range.min or numeric_value > option_range.max:
                raise RealSenseError(
                    status_code=400,
                    detail=f"Value {value} is out of range [{option_range.min}, {option_range.max}] for option {option_id}",
                )
            value = numeric_value
        except (ValueError, TypeError):
            # Non-numeric value, skip range check
            pass

        # Set the option value
        try:
            sensor.set_option(option_value, value)
            return True
        except RuntimeError as e:
            raise RealSenseError(
                status_code=500, detail=f"Failed to set option: {str(e)}"
            )

    def _set_filter_option(self, device_id: str, sensor_id: str, sensor, option_id: str, value: Any) -> bool:
        """Set a post-processing filter option.
        
        option_id format: PP_{SafeFilterName}_Enabled or PP_{SafeFilterName}_{OptionName}
        where SafeFilterName has spaces replaced with underscores.
        """
        filters = self._get_or_create_processing_blocks(device_id, sensor_id, sensor)
        
        # Find the filter by matching URL-safe name
        target_filter = None
        remaining_option = None
        
        for filter_info in filters:
            filter_name = filter_info["name"]
            # Use URL-safe name for matching (spaces replaced with underscores)
            safe_filter_name = filter_name.replace(" ", "_")
            prefix = f"PP_{safe_filter_name}_"
            if option_id.startswith(prefix):
                target_filter = filter_info
                remaining_option = option_id[len(prefix):]
                break
        
        if target_filter is None:
            raise RealSenseError(status_code=404, detail=f"Filter not found for option: {option_id}")
        
        # Handle enable/disable toggle
        if remaining_option == "Enabled":
            target_filter["enabled"] = bool(value) if isinstance(value, bool) else float(value) > 0
            logging.info(f"[PP] Filter '{target_filter['name']}' enabled={target_filter['enabled']}")
            return True
        
        # Handle filter-specific option
        filter_obj = target_filter["filter"]
        for opt in filter_obj.get_supported_options():
            if opt.name == remaining_option:
                try:
                    # Convert boolean to float
                    if isinstance(value, bool):
                        value = 1.0 if value else 0.0
                    filter_obj.set_option(opt, float(value))
                    return True
                except RuntimeError as e:
                    raise RealSenseError(status_code=500, detail=f"Failed to set filter option: {str(e)}")
        
        raise RealSenseError(status_code=404, detail=f"Filter option not found: {remaining_option}")

    def _apply_depth_filters(self, device_id: str, frame: rs.depth_frame) -> rs.depth_frame:
        """Apply enabled post-processing filters to a depth frame.
        
        Filters are applied in the SDK-recommended order.
        """
        if device_id not in self.processing_blocks:
            return frame
        
        # Find depth sensor filters (sensor-0 is typically depth)
        # Only depth sensor has PP filters, skip other sensors
        depth_sensor_id = None
        for sensor_id in self.processing_blocks[device_id]:
            if "sensor-0" in sensor_id:  # Depth sensor is typically sensor-0
                depth_sensor_id = sensor_id
                break
        
        if not depth_sensor_id:
            return frame
        
        filters = self.processing_blocks[device_id].get(depth_sensor_id, [])
        
        # Quick check: if no filters are enabled, return early
        if not any(f["enabled"] for f in filters):
            return frame
        
        # Apply only enabled filters
        for filter_info in filters:
            if not filter_info["enabled"]:
                continue
            try:
                result = filter_info["filter"].process(frame)
                # Some filters return depth_frame, others return frame
                if result.is_depth_frame():
                    frame = result.as_depth_frame()
                else:
                    # Try to extract depth frame from frameset if filter returns a frameset
                    try:
                        fs = result.as_frameset()
                        depth = fs.get_depth_frame()
                        if depth:
                            frame = depth
                    except Exception:
                        pass
            except Exception as e:
                # Log but don't fail streaming if a filter errors
                logging.warning(f"Filter {filter_info['name']} error: {e}")
        
        return frame

    def _apply_color_filters(self, device_id: str, frame: rs.video_frame) -> rs.video_frame:
        """Apply enabled post-processing filters to a color frame.
        
        Currently limited to filters that support color frames.
        """
        # Color frame filtering is minimal - most PP filters are depth-specific
        # Future: could add rotation filter for color here
        return frame

    def start_stream(
        self,
        device_id: str,
        configs: List[StreamConfig],
        align_to: Optional[str] = None,
        reuse_cache: bool = True,
        timing: bool = True,
    ) -> dict:
        """Start streaming from a device, with timing info for diagnostics"""
        import time
        
        # Check mode compatibility - pipeline API cannot be used if sensor API is active
        self._check_streaming_mode(device_id, "pipeline")
        
        timings = {}
        t0 = time.perf_counter()
        refreshed = False
        # Only refresh when the cache is empty or the requested device is unknown
        if not self.devices or device_id not in self.devices:
            self.refresh_devices()
            refreshed = True
        timings['refresh_devices'] = time.perf_counter() - t0 if refreshed else 0.0

        t1 = time.perf_counter()
        if device_id not in self.devices:
            raise RealSenseError(
                status_code=404, detail=f"Device {device_id} not found"
            )
        if device_id in self.stopping:
            raise RealSenseError(status_code=409, detail="Stop in progress; try again shortly")
        timings['device_lookup'] = time.perf_counter() - t1
        signature = self._make_signature(configs, align_to)

        t2 = time.perf_counter()
        # If already streaming with identical signature, short-circuit
        if device_id in self.pipelines and self.pipeline_signatures.get(device_id) == signature:
            return {
                'device_id': device_id,
                'is_streaming': True,
                'active_streams': list(self.active_streams[device_id]),
                'timings': timings,
                'config_reused': True,
                'config_signature': signature,
            }

        # Initialize or reuse pipeline and config
        config_cache_for_device = self.config_cache.setdefault(device_id, {})

        if not reuse_cache:
            config_cache_for_device.pop(signature, None)
            self.pipeline_cache.pop(device_id, None)
        pipeline = self.pipeline_cache.get(device_id) if reuse_cache else None
        pipeline = pipeline or rs.pipeline(self.ctx)

        config_reused = False
        if reuse_cache and signature in config_cache_for_device:
            config = config_cache_for_device[signature]
            config_reused = True
        else:
            config = rs.config()
            config.enable_device(device_id)
        timings['pipeline_config_init'] = 0.0 if config_reused else time.perf_counter() - t2

        t3 = time.perf_counter()
        # Track active stream types
        active_streams = set()
        # Enable streams based on configuration only if not reused
        if not config_reused:
            for stream_config in configs:
                # Parse sensor index from sensor_id
                try:
                    sensor_index = int(stream_config.sensor_id.split("-")[-1])
                    if sensor_index < 0 or sensor_index >= len(
                        self.devices[device_id].sensors
                    ):
                        raise RealSenseError(
                            status_code=404,
                            detail=f"Sensor {stream_config.sensor_id} not found",
                        )
                except (ValueError, IndexError):
                    raise RealSenseError(
                        status_code=404,
                        detail=f"Invalid sensor ID format: {stream_config.sensor_id}",
                    )
                # Get stream type from string
                stream_name_list = stream_config.stream_type.split("-")
                stream_type = None
                for name, val in rs.stream.__members__.items():
                    if name.lower() == stream_name_list[0].lower():
                        stream_type = val
                        break
                if stream_type is None:
                    raise RealSenseError(
                        status_code=400,
                        detail=f"Invalid stream type: {stream_config.stream_type}",
                    )
                format_type = None
                for name, val in rs.format.__members__.items():
                    if name.lower() == stream_config.format.lower():
                        format_type = val
                        break
                if format_type is None:
                    raise RealSenseError(
                        status_code=400, detail=f"Invalid format: {stream_config.format}"
                    )
                if active_streams and stream_config.stream_type in active_streams:
                    continue
                    
                # Try to enable stream - first with exact format, then with any format
                stream_enabled = False
                last_error = None
                
                for try_format in [format_type, rs.format.any]:
                    if stream_enabled:
                        break
                    try:
                        if len(stream_name_list) > 1:
                            stream_index = int(stream_name_list[1])
                            config.enable_stream(
                                stream_type,
                                stream_index,
                                stream_config.resolution.width,
                                stream_config.resolution.height,
                                try_format,
                                stream_config.framerate,
                            )
                        elif format_type == rs.format.combined_motion:
                            config.enable_stream(stream_type)
                        else:
                            config.enable_stream(
                                stream_type,
                                stream_config.resolution.width,
                                stream_config.resolution.height,
                                try_format,
                                stream_config.framerate,
                            )
                        stream_enabled = True
                        if try_format == rs.format.any:
                            logging.info(f"[PIPELINE] Using fallback format for {stream_config.stream_type} "
                                        f"(requested {stream_config.format} not available at "
                                        f"{stream_config.resolution.width}x{stream_config.resolution.height}@{stream_config.framerate}fps)")
                    except RuntimeError as e:
                        last_error = e
                        continue
                        
                if not stream_enabled:
                    raise RealSenseError(
                        status_code=400, detail=f"Failed to enable stream {stream_config.stream_type}: {str(last_error)}"
                    )
                active_streams.add(stream_config.stream_type)
        else:
            # Even when reusing config, rebuild the active_streams set for reporting
            for stream_config in configs:
                active_streams.add(stream_config.stream_type)

        timings['stream_enable'] = 0.0 if config_reused else time.perf_counter() - t3
        t4 = time.perf_counter()
        # Start streaming
        try:
            pipeline_profile = pipeline.start(config)
            timings['pipeline_start'] = time.perf_counter() - t4
            t5 = time.perf_counter()
            # Set up align if requested
            align_processor = None
            if align_to:
                align_stream = None
                for name, val in rs.stream.__members__.items():
                    if name.lower() == align_to.lower():
                        align_stream = val
                        break
                if align_stream:
                    align_processor = rs.align(align_stream)
            # Store pipeline and config
            with self.lock:
                self.pipelines[device_id] = pipeline
                self.configs[device_id] = config
                self.pipeline_cache[device_id] = pipeline
                self.pipeline_signatures[device_id] = signature
                config_cache_for_device[signature] = config
                self.active_streams[device_id] = active_streams
                self.frame_queues[device_id] = {
                    stream_type: [] for stream_type in active_streams
                }
                self.metadata_queues[device_id] = {
                    stream_key: [] for stream_key in active_streams
                }
                # Track that this device is using pipeline API
                self.streaming_mode[device_id] = "pipeline"
            timings['post_start_setup'] = time.perf_counter() - t5
            t6 = time.perf_counter()
            
            # Initialize post-processing filters for enabled sensors if not already done
            dev = self.devices[device_id]
            for stream_config in configs:
                if not stream_config.enable:
                    continue
                try:
                    sensor_index = int(stream_config.sensor_id.split("-")[-1])
                    if 0 <= sensor_index < len(dev.sensors):
                        sensor = dev.sensors[sensor_index]
                        self._get_or_create_processing_blocks(device_id, stream_config.sensor_id, sensor)
                except (ValueError, IndexError):
                    pass
            
            # Start frame collection thread
            threading.Thread(
                target=self._collect_frames,
                args=(device_id, align_processor),
                daemon=True,
            ).start()
            # Update device info
            if device_id in self.device_infos:
                self.device_infos[device_id].is_streaming = True
            self.metadata_socket_server.start_broadcast(device_id)
            timings['thread_start'] = time.perf_counter() - t6
            timings['total'] = time.perf_counter() - t0
            logging.debug("[TIMING] start_stream timings for %s: %s", device_id, timings)
            return {
                'device_id': device_id,
                'is_streaming': True,
                'active_streams': list(active_streams),
                'timings': timings,
                'config_reused': config_reused,
                'config_signature': signature,
            }
        except RuntimeError as e:
            raise RealSenseError(
                status_code=500, detail=f"Failed to start streaming: {str(e)}"
            )

    def stop_stream(self, device_id: str) -> StreamStatus:
        """Stop streaming from a device. Returns immediately and completes stop in background."""
        with self.lock:
            if device_id not in self.devices:
                return StreamStatus(device_id=device_id, is_streaming=False, active_streams=[], stopping=False)

            # If already stopping, report status
            if device_id in self.stopping:
                return StreamStatus(
                    device_id=device_id,
                    is_streaming=device_id in self.pipelines,
                    active_streams=list(self.active_streams.get(device_id, set())),
                    stopping=True,
                )

            is_streaming = device_id in self.pipelines
            active_streams = list(self.active_streams.get(device_id, set()))
            if not is_streaming:
                return StreamStatus(device_id=device_id, is_streaming=False, active_streams=active_streams, stopping=False)

            self.stopping.add(device_id)

        def _do_stop():
            try:
                self.metadata_socket_server.stop_broadcast(device_id)
                self.pipelines[device_id].stop()
            except Exception as e:
                logging.error("Failed to stop streaming for %s: %s", device_id, e)
            finally:
                with self.lock:
                    # Clean up resources
                    self.pipelines.pop(device_id, None)
                    self.configs.pop(device_id, None)
                    active = list(self.active_streams.pop(device_id, set()))
                    self.pipeline_signatures.pop(device_id, None)
                    self.frame_queues.pop(device_id, None)
                    self.metadata_queues.pop(device_id, None)
                    self.color_frames.pop(device_id, None)
                    self.point_clouds.pop(device_id, None)
                    self._supported_md_by_profile.pop(device_id, None)
                    self.stopping.discard(device_id)
                    # Reset streaming mode to idle
                    self.streaming_mode[device_id] = "idle"
                    if device_id in self.device_infos:
                        self.device_infos[device_id].is_streaming = False

        threading.Thread(target=_do_stop, daemon=True).start()

        return StreamStatus(
            device_id=device_id,
            is_streaming=False,
            active_streams=active_streams,
            stopping=True,
        )

    def activate_point_cloud(self, device_id: str, enable: bool) -> bool:
        """Activate or deactivate point cloud processing"""
        if device_id not in self.devices:
            self.refresh_devices()
            if device_id not in self.devices:
                raise RealSenseError(
                    status_code=404, detail=f"Device {device_id} not found"
                )

        if enable:
            self.is_pointcloud_enabled[device_id] = True
        else:
            self.is_pointcloud_enabled[device_id] = False

        return PointCloudStatus(device_id=device_id, is_active=enable)

    def get_point_cloud_status(self, device_id: str) -> bool:
        """Get the point cloud status for a device"""
        if device_id not in self.devices:
            self.refresh_devices()
            if device_id not in self.devices:
                raise RealSenseError(
                    status_code=404, detail=f"Device {device_id} not found"
                )

        return PointCloudStatus(
            device_id=device_id, is_active=self.is_pointcloud_enabled[device_id]
        )

    def get_stream_status(self, device_id: str) -> StreamStatus:
        """Get the streaming status for a device (supports both pipeline and sensor modes)"""
        if device_id not in self.devices:
            self.refresh_devices()
            if device_id not in self.devices:
                raise RealSenseError(
                    status_code=404, detail=f"Device {device_id} not found"
                )

        mode = self.streaming_mode.get(device_id, "idle")
        
        # Check pipeline mode
        is_pipeline_streaming = device_id in self.pipelines
        pipeline_streams = list(self.active_streams.get(device_id, set()))
        
        # Check sensor mode - collect active stream types from sensor_streams
        sensor_streams = []
        if device_id in self.sensor_streams:
            for sensor_id, sensor_info in self.sensor_streams[device_id].items():
                if sensor_info.get("is_streaming", False):
                    # Use stream_types (plural) - it's a list of active stream types
                    stream_types_list = sensor_info.get("stream_types", [])
                    sensor_streams.extend(stream_types_list)
        
        # Combine based on mode
        is_streaming = is_pipeline_streaming or len(sensor_streams) > 0
        active_streams = pipeline_streams if mode == "pipeline" else sensor_streams
        stopping = device_id in self.stopping

        return StreamStatus(
            device_id=device_id,
            is_streaming=is_streaming,
            active_streams=active_streams,
            stopping=stopping,
        )

    def get_latest_frame(
        self, device_id: str, stream_type: str
    ) -> Tuple[np.ndarray, dict]:
        """Get the latest frame from a specific stream (supports both pipeline and sensor modes)"""
        with self.lock:
            mode = self.streaming_mode.get(device_id, "idle")
            
            # Try pipeline mode first
            if mode == "pipeline" or device_id in self.frame_queues:
                if device_id in self.frame_queues:
                    if stream_type in self.frame_queues[device_id]:
                        queue = self.frame_queues[device_id][stream_type]
                        if len(queue) > 0:
                            return queue[-1]
            
            # Try sensor mode - find sensor by stream_type
            if mode == "sensor" or device_id in self.sensor_streams:
                if device_id in self.sensor_streams:
                    for sensor_id, sensor_info in self.sensor_streams[device_id].items():
                        sensor_stream_types = sensor_info.get("stream_types", [])
                        # Check if this stream type is active on this sensor
                        matching_type = None
                        for st in sensor_stream_types:
                            if st.lower() == stream_type.lower():
                                matching_type = st
                                break
                        
                        if sensor_info.get("is_streaming", False) and matching_type:
                            # Found matching sensor, get frame from per-stream-type queue
                            if (device_id in self.sensor_frame_queues and
                                sensor_id in self.sensor_frame_queues[device_id] and
                                matching_type in self.sensor_frame_queues[device_id][sensor_id]):
                                queue = self.sensor_frame_queues[device_id][sensor_id][matching_type]
                                if len(queue) > 0:
                                    return queue[-1]
                                else:
                                    # 503 Service Unavailable: stream is active but no frames yet
                                    # (transient — caller should retry rather than treat as fatal)
                                    raise RealSenseError(
                                        status_code=503,
                                        detail=f"No frames available for stream {stream_type}",
                                    )
                    # Stream type not found in active sensors
                    active_sensor_streams = []
                    for sensor_info in self.sensor_streams[device_id].values():
                        if sensor_info.get("is_streaming", False):
                            active_sensor_streams.extend(sensor_info.get("stream_types", []))
                    raise RealSenseError(
                        status_code=400, 
                        detail=f"Stream type '{stream_type}' is not active. Available: {active_sensor_streams}"
                    )
            
            # Device not streaming
            raise RealSenseError(
                status_code=400, detail=f"Device {device_id} is not streaming"
            )

    def get_latest_metadata(self, device_id: str, stream_type: str) -> Dict:
        """Get the latest METADATA dictionary from a specific stream (supports both pipeline and sensor modes)"""
        stream_key = stream_type.lower()  # Use consistent key format
        with self.lock:
            mode = self.streaming_mode.get(device_id, "idle")
            
            # Try pipeline mode first
            if mode == "pipeline" and device_id in self.pipelines and device_id in self.metadata_queues:
                if stream_key in self.metadata_queues.get(device_id, {}):
                    queue = self.metadata_queues[device_id][stream_key]
                    if len(queue) > 0:
                        return queue[-1]
                    return {}
            
            # Try sensor mode - find the sensor that has this stream type
            if mode == "sensor" and device_id in self.sensor_metadata_queues:
                for sensor_id, sensor_queues in self.sensor_metadata_queues[device_id].items():
                    if stream_key in sensor_queues:
                        queue = sensor_queues[stream_key]
                        if len(queue) > 0:
                            return queue[-1]
                        return {}
            
            # If we get here, the stream is not active or device is not streaming
            if mode == "idle":
                raise RealSenseError(
                    status_code=400, detail=f"Device {device_id} is not streaming."
                )
            else:
                # Streaming but stream type not found
                raise RealSenseError(
                    status_code=400,
                    detail=f"Stream type '{stream_key}' is not active for device {device_id}.",
                )

    # TODO: replace with `list(rs.frame_metadata_value)` once pyrealsense2 ships
    # with pybind11 >= 2.12 (added __iter__ on py::enum_). Current PyPI wheels
    # use older pybind11 where the enum is not iterable.
    _FRAME_METADATA_VALUES = list(rs.frame_metadata_value.__members__.values())

    @staticmethod
    def _build_viewer_info(frame_data) -> Dict[str, Any]:
        """Top metadata block, mirrors C++ realsense-viewer."""
        profile = frame_data.get_profile()
        actual_fps_key = rs.frame_metadata_value.actual_fps
        hardware_fps = profile.fps()
        if frame_data.supports_frame_metadata(actual_fps_key):
            try:
                hardware_fps = frame_data.get_frame_metadata(actual_fps_key) / 1000.0
            except Exception as exc:
                logging.debug("[METADATA] failed to read %s: %s", actual_fps_key.name, exc)
        info: Dict[str, Any] = {
            "timestamp": frame_data.get_timestamp(),
            "frame_number": frame_data.get_frame_number(),
            "clock_domain": frame_data.get_frame_timestamp_domain().name,
            "pixel_format": profile.format().name,
            "hardware_fps": hardware_fps,
        }
        try:  # video frames only; motion frames have no width/height
            info["width"] = frame_data.get_width()
            info["height"] = frame_data.get_height()
            vsp = profile.as_video_stream_profile()
            info["hardware_width"] = vsp.width()
            info["hardware_height"] = vsp.height()
        except Exception:
            pass
        return info

    def _get_frame_metadata(self, frame_data, device_id: str) -> Dict[str, int]:
        """Return all rs2_frame_metadata_value attributes the frame supports.
        Mirrors common/stream-model.cpp:52-59 in the C++ realsense-viewer.
        Caches the supported subset per (device, profile uid) so the steady-state
        per-frame cost is one dict lookup + N get_frame_metadata calls."""
        try:
            profile_uid = frame_data.get_profile().unique_id()
        except Exception:
            profile_uid = None

        device_cache = self._supported_md_by_profile.get(device_id)
        supported = device_cache.get(profile_uid) if (device_cache is not None and profile_uid is not None) else None
        # Build the supported set from the 2nd frame on: delta-computed metadata
        # (e.g. actual_fps) is not yet available on the first frame.
        if supported is None and frame_data.get_frame_number() >= 2:
            supported = [md for md in self._FRAME_METADATA_VALUES
                         if frame_data.supports_frame_metadata(md)]
            if profile_uid is not None:
                if device_cache is None:
                    device_cache = self._supported_md_by_profile[device_id] = {}
                device_cache[profile_uid] = supported

        attrs: Dict[str, int] = {}
        for md in (supported or []):
            try:
                attrs[md.name] = frame_data.get_frame_metadata(md)
            except Exception as e:
                # supports_frame_metadata said yes; getting the value should not throw.
                logging.debug("[METADATA] failed to read %s: %s", md.name, e)
                continue
        return attrs

    # Cap the number of vertices shipped per frame so payload/decode/render cost
    # stays roughly constant regardless of stream resolution.
    POINT_CLOUD_MAX_VERTICES = 60000

    def _build_point_cloud_metadata(self, device_id: str, depth_frame, color_frame=None) -> Optional[Dict]:
        """Compute decimated point-cloud vertices from a depth frame, ready for serialization.

        When ``color_frame`` is supplied and it's an RGB8/BGR8 frame, this also
        samples a per-vertex RGB triplet using the texture coordinates produced
        by ``rs.pointcloud.map_to`` — mirrors the C++ realsense-viewer's textured
        point cloud rendering. The client falls back to a depth colormap when
        ``colors`` is absent.

        Uses a per-device ``rs.pointcloud`` so map_to()/calculate() can't race
        across devices.

        Returns None if calculate() yields nothing. Used by both the pipeline-mode
        frame collector and the sensor-mode frame processor.
        """
        pc = self.point_clouds.get(device_id)
        if pc is None:
            pc = rs.pointcloud()
            self.point_clouds[device_id] = pc
        if color_frame:
            try:
                pc.map_to(color_frame)
            except Exception:
                color_frame = None  # not all profiles support map_to; fall back silently
        points = pc.calculate(depth_frame)
        if not points:
            return None
        verts = np.asanyarray(points.get_vertices()).view(np.float32).reshape(-1, 3)

        # Mask + decimate BEFORE sampling colors — at 1280x720 the full vertex
        # buffer is ~921K entries, and sampling/copying 900K colors per depth
        # frame was bottlenecking the depth thread down to ~1 Hz (the rotation
        # desync the user reported: by the time depth_T was ready, color was
        # already at color_T+1s). Sampling at the final ~60K decimated indices
        # is ~15x less work.
        mask = verts[:, 2] >= 0.03  # drop invalid near-camera points
        verts = verts[mask]
        count = len(verts)
        step = 1
        if count > self.POINT_CLOUD_MAX_VERTICES:
            step = (count // self.POINT_CLOUD_MAX_VERTICES) + 1
            verts = verts[::step]

        colors_rgb: Optional[np.ndarray] = None
        if color_frame:
            tex = np.asanyarray(points.get_texture_coordinates()).view(np.float32).reshape(-1, 2)
            tex = tex[mask]
            if step > 1:
                tex = tex[::step]
            colors_rgb = self._sample_color_at_tex(tex, color_frame)

        # Contiguous so .tobytes() in the socket server is a straight memcpy.
        verts = np.ascontiguousarray(verts, dtype=np.float32)
        result: Dict[str, Any] = {"vertices": verts, "texture_coordinates": []}
        if colors_rgb is not None:
            result["colors"] = np.ascontiguousarray(colors_rgb, dtype=np.uint8)
        return result

    def _pick_color_for_depth(self, device_id: str, depth_frame) -> Optional[Any]:
        """Pick the cached color frame whose timestamp is closest to the depth
        frame's. Both timestamps come from ``frame.get_timestamp()``, which —
        with ``global_time_enabled`` set on the sensors at start time — are
        translated by the SDK into a unified system-time domain regardless of
        which sensor produced them. The short history covers the typical
        depth/color SDK-latency offset (~one capture interval) so the picked
        color reflects the same real-time moment as the depth.
        """
        # Snapshot the deque before iterating — the color sensor thread can
        # append/evict concurrently and `for cf in deque` is not safe against
        # that. tuple() captures the current frame refs atomically under GIL.
        hist = self.color_frames.get(device_id)
        if not hist:
            return None
        snapshot = tuple(hist)
        if not snapshot:
            return None
        try:
            depth_ts = depth_frame.get_timestamp()
        except RuntimeError:
            return snapshot[-1]
        best = snapshot[-1]
        best_dt = float("inf")
        for cf in snapshot:
            try:
                dt = abs(cf.get_timestamp() - depth_ts)
            except RuntimeError:
                continue
            if dt < best_dt:
                best_dt = dt
                best = cf
        return best

    def _is_color_streaming(self, device_id: str) -> bool:
        """Whether 'color' is currently in this device's active stream set.

        Used by the sensor-mode depth thread to decide whether the cached color
        frame in ``self.color_frames`` is still fresh enough to texture-map the
        cloud. Pipeline mode reads color straight from the frameset and doesn't
        need this — the frameset already drops disabled streams.
        """
        mode = self.streaming_mode.get(device_id)
        if mode == "pipeline":
            return any(s.lower() == "color" for s in self.active_streams.get(device_id, set()))
        if device_id in self.sensor_streams:
            for sensor_info in self.sensor_streams[device_id].values():
                if not sensor_info.get("is_streaming", False):
                    continue
                if any(st.lower() == "color" for st in sensor_info.get("stream_types", [])):
                    return True
        return False

    @staticmethod
    def _sample_color_at_tex(tex: np.ndarray, color_frame) -> Optional[np.ndarray]:
        """Return Nx3 uint8 RGB sampled at the supplied texture coordinates.

        ``tex`` is the already-masked, already-decimated tex-coord array (Nx2,
        u/v in [0,1]) — sampling per-vertex on the full pre-decimation buffer
        is too slow at 1280x720. Only handles RGB8 / BGR8 color frames; other
        formats (YUYV, Y16, …) return None and the client falls back to the
        depth colormap.
        """
        try:
            color_format = color_frame.get_profile().format()
        except Exception:
            return None
        if color_format not in (rs.format.rgb8, rs.format.bgr8):
            return None
        cim = np.asanyarray(color_frame.get_data())
        if cim.ndim != 3 or cim.shape[2] < 3:
            return None
        H, W = cim.shape[:2]
        u = tex[:, 0]
        v = tex[:, 1]
        # rs2 emits tex coords outside [0,1] for depth pixels that don't project
        # into the color frame — mark those black instead of clamping (clamping
        # would smear the frame borders across off-screen points).
        valid = (u >= 0.0) & (u <= 1.0) & (v >= 0.0) & (v <= 1.0)
        xi = np.clip((u * W).astype(np.int32), 0, W - 1)
        yi = np.clip((v * H).astype(np.int32), 0, H - 1)
        sampled = cim[yi, xi][:, :3].astype(np.uint8, copy=True)
        sampled[~valid] = 0
        if color_format == rs.format.bgr8:
            sampled = sampled[:, ::-1]  # BGR -> RGB so the client doesn't need to swap
        return sampled

    def _collect_frames(self, device_id: str, align_processor=None):
        """Thread function to collect frames from the pipeline"""
        logging.debug("[INFO] Frame collection thread started for device %s", device_id)
        logging.debug("[INFO] Active streams: %s", self.active_streams.get(device_id, set()))
        
        # Pre-compute stream mappings for performance (avoid lookup on every frame)
        stream_mappings = {}
        for active_stream in self.active_streams.get(device_id, set()):
            stream_name_list = active_stream.split("-")
            stream_type_base = stream_name_list[0]
            rs_stream = None
            for name, val in rs.stream.__members__.items():
                if name.lower() == stream_type_base.lower():
                    rs_stream = val
                    break
            if rs_stream is not None:
                ir_index = int(stream_name_list[1]) if len(stream_name_list) > 1 else 1
                stream_mappings[active_stream] = (rs_stream, ir_index)
        
        # Create a single colorizer instance for depth (reuse for performance)
        colorizer = rs.colorizer()
        
        try:
            while device_id in self.pipelines:
                try:
                    # Wait for a frameset
                    frames = self.pipelines[device_id].wait_for_frames()
                    
                    # Apply alignment if requested
                    if align_processor:
                        frames = align_processor.process(frames)

                    # Process frames outside the lock for better performance
                    processed_frames = {}
                    processed_metadata = {}
                    
                    for active_stream, (rs_stream, ir_index) in stream_mappings.items():

                        try:
                            frame = None
                            frame_data = None

                            # Use the rs_stream enum directly for comparison
                            if rs_stream == rs.stream.depth:
                                frame_data = frames.get_depth_frame()
                                if frame_data:
                                    # Apply post-processing filters
                                    frame_data = self._apply_depth_filters(device_id, frame_data)
                                    # Store raw depth frame for pixel queries
                                    self.depth_frames[device_id] = frame_data
                                    colorized = colorizer.colorize(frame_data)
                                    frame = np.asanyarray(colorized.get_data())
                            elif rs_stream == rs.stream.color:
                                frame_data = frames.get_color_frame()
                                if frame_data:
                                    # Apply post-processing filters for color
                                    frame_data = self._apply_color_filters(device_id, frame_data)
                                    frame = np.asanyarray(frame_data.get_data())
                            elif rs_stream == rs.stream.infrared:
                                frame_data = frames.get_infrared_frame(ir_index)
                                if frame_data:
                                    frame = np.asanyarray(frame_data.get_data())
                            elif rs_stream == rs.stream.gyro or rs_stream == rs.stream.accel:
                                motion_data = None
                                frame_data = None
                                for f in frames:
                                    if f.get_profile().stream_type() == rs_stream:
                                        frame_data = f.as_motion_frame()
                                        motion_data = frame_data.get_motion_data()
                                        break

                                motion_json_data = None
                                if motion_data:
                                    motion_json_data = {
                                        "x": float(motion_data.x),
                                        "y": float(motion_data.y),
                                        "z": float(motion_data.z),
                                    }
                                    # Create simple visualization frame for motion data
                                    frame = np.zeros((120, 320, 3), dtype=np.uint8)
                                    cv2.putText(frame, f"X: {motion_data.x:.3f}", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 100, 100), 1)
                                    cv2.putText(frame, f"Y: {motion_data.y:.3f}", (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (100, 255, 100), 1)
                                    cv2.putText(frame, f"Z: {motion_data.z:.3f}", (10, 90), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (100, 100, 255), 1)
                            else:
                                continue  # Unknown stream type

                            # Skip if no frame data was obtained
                            if frame is None or frame_data is None:
                                continue

                            # Add metadata
                            metadata = {
                                "frame_metadata": self._get_frame_metadata(frame_data, device_id),
                                **self._build_viewer_info(frame_data),
                            }

                            if rs_stream == rs.stream.gyro or rs_stream == rs.stream.accel:
                                if motion_json_data:
                                    metadata["motion_data"] = motion_json_data

                            if rs_stream == rs.stream.depth and self.is_pointcloud_enabled.get(device_id, False):
                                # If color is also active in this frameset, use it
                                # to texture-map the cloud (cpp realsense-viewer
                                # parity). get_color_frame() returns an empty
                                # falsy frame when no color stream is enabled.
                                # Apply the same color filters as the 2D path
                                # uses so the textured cloud and the color tile
                                # never diverge if filters become non-trivial.
                                color_for_texture = frames.get_color_frame() or None
                                if color_for_texture:
                                    color_for_texture = self._apply_color_filters(device_id, color_for_texture)
                                pc_meta = self._build_point_cloud_metadata(device_id, frame_data, color_for_texture)
                                if pc_meta:
                                    metadata["point_cloud"] = pc_meta

                            # Store processed frame and metadata
                            processed_frames[active_stream] = frame
                            processed_metadata[active_stream] = metadata
                            
                        except Exception as e:
                            if not isinstance(e, RuntimeError):
                                print(f"Error processing {active_stream}: {type(e).__name__}: {str(e)}")

                    # Now add to queues with lock held briefly
                    with self.lock:
                        if device_id not in self.frame_queues:
                            break
                            
                        for active_stream, frame in processed_frames.items():
                            frame_queue = self.frame_queues[device_id][active_stream]
                            frame_queue.append(frame)
                            # Keep queue size limited
                            while len(frame_queue) > self.max_queue_size:
                                frame_queue.pop(0)
                                
                        for active_stream, metadata in processed_metadata.items():
                            metadata_queue = self.metadata_queues[device_id][active_stream]
                            metadata_queue.append(metadata)
                            while len(metadata_queue) > self.max_queue_size:
                                metadata_queue.pop(0)

                except RuntimeError as e:
                    # Handle timeout or other error
                    print(f"Error collecting frames: {str(e)}")
                    time.sleep(0.1)

        except Exception as e:
            print(f"Frame collection thread exception: {str(e)}")
            # Stop the pipeline if there's an error
            try:
                with self.lock:
                    if device_id in self.pipelines:
                        self.pipelines[device_id].stop()
                        del self.pipelines[device_id]
                        if device_id in self.configs:
                            del self.configs[device_id]
                        if device_id in self.active_streams:
                            del self.active_streams[device_id]
                        if device_id in self.frame_queues:
                            del self.frame_queues[device_id]
                        if device_id in self.metadata_queues:
                            del self.metadata_queues[device_id]
                        if device_id in self.depth_frames:
                            del self.depth_frames[device_id]
                        self.color_frames.pop(device_id, None)
                        self.point_clouds.pop(device_id, None)
                        self._supported_md_by_profile.pop(device_id, None)
                        if device_id in self.device_infos:
                            self.device_infos[device_id].is_streaming = False
            except Exception:
                pass

    def get_depth_at_pixel(self, device_id: str, x: int, y: int) -> Optional[float]:
        """Get depth value (in meters) at specific pixel coordinates."""
        with self.lock:
            if device_id not in self.depth_frames:
                return None
            depth_frame = self.depth_frames[device_id]
            try:
                # get_distance returns depth in meters
                return depth_frame.get_distance(x, y)
            except Exception as e:
                print(f"Error getting depth at pixel ({x}, {y}): {str(e)}")
                return None

    def get_depth_range(self, device_id: str) -> Dict[str, Any]:
        """
        Calculate dynamic depth range for legend based on current frame.
        Matches legacy viewer algorithm: mean + 1.5*stddev, rounded up to nearest 4m.
        """
        import math
        with self.lock:
            if device_id not in self.depth_frames:
                return {"min_depth": 0, "max_depth": 6, "units": "meters"}
            depth_frame = self.depth_frames[device_id]
            try:
                # Ensure we have a proper depth frame (may be raw frame from sensor mode)
                if hasattr(depth_frame, 'as_depth_frame'):
                    depth_frame = depth_frame.as_depth_frame()
                width = depth_frame.get_width()
                height = depth_frame.get_height()
                # Sample every 30th pixel like legacy viewer
                skip = 30
                distances = []
                for y in range(0, height, skip):
                    for x in range(0, width, skip):
                        d = depth_frame.get_distance(x, y)
                        if d > 0:
                            distances.append(d)
                if not distances:
                    return {"min_depth": 0, "max_depth": 6, "units": "meters"}
                # Calculate mean and standard deviation
                mean = sum(distances) / len(distances)
                variance = sum((d - mean) ** 2 for d in distances) / len(distances)
                stddev = math.sqrt(variance)
                # Round up to nearest 4m
                length_jump = 4.0
                max_depth = math.ceil((mean + 1.5 * stddev) / length_jump) * length_jump
                # Clamp to reasonable range
                max_depth = max(4.0, min(max_depth, 16.0))
                return {"min_depth": 0, "max_depth": max_depth, "units": "meters"}
            except Exception as e:
                print(f"Error calculating depth range: {str(e)}")
                return {"min_depth": 0, "max_depth": 6, "units": "meters"}

    # =========================================================================
    # Per-Sensor Streaming API (using RealSense sensor API)
    # =========================================================================

    def _check_streaming_mode(self, device_id: str, requested_mode: str) -> None:
        """
        Ensure requested mode is compatible with current state.
        
        Args:
            device_id: The device to check
            requested_mode: "pipeline" or "sensor"
            
        Raises:
            RealSenseError: If mode conflict detected
        """
        current_mode = self.streaming_mode.get(device_id, "idle")
        
        if current_mode == "idle":
            return  # OK to start with any mode
        
        if current_mode != requested_mode:
            raise RealSenseError(
                status_code=409,
                detail=f"Device is in '{current_mode}' mode. "
                       f"Stop all streams before switching to '{requested_mode}' mode."
            )

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

    def _find_matching_profile(
        self,
        sensor: rs.sensor,
        config: SensorStreamConfig
    ) -> rs.stream_profile:
        """
        Find a stream profile matching the configuration.
        
        If exact format match isn't found at the requested resolution/fps,
        falls back to finding any available format for that stream/resolution/fps.
        
        Returns:
            Matching rs.stream_profile
            
        Raises:
            RealSenseError: If no matching profile found
        """
        profiles = sensor.get_stream_profiles()
        
        exact_match = None
        fallback_match = None  # Any format match for same stream/res/fps
        
        for profile in profiles:
            # Get stream type name
            stream_name = profile.stream_type().name.lower()
            
            # Handle infrared index
            if profile.stream_type() == rs.stream.infrared:
                stream_name = f"infrared-{profile.stream_index()}"
            
            # Check stream type match
            if stream_name != config.stream_type.lower():
                continue
            
            # Check format match (skip for motion streams if format is "combined_motion")
            format_name = str(profile.format()).split('.')[-1].lower()
            is_motion_stream = stream_name in ('accel', 'gyro')
            
            format_matches = False
            if is_motion_stream:
                # Motion streams: accept if config says "combined_motion" or actual format matches
                format_matches = (config.format.lower() == "combined_motion" or 
                                  format_name == config.format.lower())
            else:
                # Video streams: check exact format match
                format_matches = (format_name == config.format.lower())
            
            # For video streams, check resolution and fps
            res_fps_matches = False
            if profile.is_video_stream_profile():
                video_profile = profile.as_video_stream_profile()
                res_fps_matches = (video_profile.width() == config.resolution.width and
                                   video_profile.height() == config.resolution.height and
                                   video_profile.fps() == config.framerate)
            else:
                # Motion streams - just check fps if applicable
                res_fps_matches = (profile.fps() == config.framerate)
            
            if not res_fps_matches:
                continue
                
            # Found a profile with matching stream/res/fps
            if format_matches:
                exact_match = profile
                break  # Perfect match, use it
            elif fallback_match is None:
                fallback_match = profile  # Keep as fallback
        
        if exact_match:
            return exact_match
        
        if fallback_match:
            # Use fallback with different format
            fallback_format = str(fallback_match.format()).split('.')[-1]
            logging.info(f"[SENSOR] Using fallback format '{fallback_format}' for {config.stream_type} "
                        f"(requested '{config.format}' not available at {config.resolution.width}x{config.resolution.height}@{config.framerate}fps)")
            return fallback_match
        
        raise RealSenseError(
            status_code=400,
            detail=f"No matching profile found for stream_type={config.stream_type}, "
                   f"format={config.format}, resolution={config.resolution.width}x{config.resolution.height}, "
                   f"fps={config.framerate}"
        )

    def _validate_profile_compatibility(self, profiles: List[rs.stream_profile]) -> None:
        """
        Validate that all profiles can be opened together on one sensor.
        
        Args:
            profiles: List of stream profiles to validate
            
        Raises:
            RealSenseError: If profiles are incompatible (different FPS)
        """
        if len(profiles) <= 1:
            return
        
        # Motion streams (gyro/accel) can have different FPS - skip validation
        motion_streams = {rs.stream.gyro, rs.stream.accel}
        all_motion = all(p.stream_type() in motion_streams for p in profiles)
        if all_motion:
            return  # Motion streams don't require FPS sync
        
        # Video streams must have same FPS for hardware sync
        fps_values = set(p.fps() for p in profiles)
        if len(fps_values) > 1:
            profile_details = [f"{p.stream_type().name}@{p.fps()}fps" for p in profiles]
            raise RealSenseError(
                status_code=400,
                detail=f"Incompatible FPS values. All streams on same sensor must use same FPS. "
                       f"Requested: {', '.join(profile_details)}"
            )

    def _process_sensor_frame(
        self,
        frame: Any,
        frame_stream_name: str,
        device_id: str,
        colorizer: Any,
    ) -> Tuple[Optional[Any], dict]:
        """Process a single sensor frame; return (processed_frame, metadata)."""
        processed_frame = None
        info_source = frame  # frame to read info from after post processing is done
        metadata: dict = {
            "frame_metadata": self._get_frame_metadata(frame, device_id),
        }

        if "depth" in frame_stream_name:
            depth_frame = frame.as_depth_frame()
            depth_frame = self._apply_depth_filters(device_id, depth_frame)
            self.depth_frames[device_id] = depth_frame
            colorized = colorizer.colorize(depth_frame)
            processed_frame = np.asanyarray(colorized.get_data())
            info_source = depth_frame

            if self.is_pointcloud_enabled.get(device_id, False):
                # Sensor mode runs depth/color on separate threads so there is
                # no frameset — pick the cached color frame whose timestamp is
                # closest to this depth frame's, otherwise rotations show
                # colors leading the geometry (color has lower SDK latency).
                # Only do this if color is still streaming; without that check
                # the depth thread would keep texturing with a stale frame
                # after the user disables color.
                color_for_texture = (
                    self._pick_color_for_depth(device_id, depth_frame)
                    if self._is_color_streaming(device_id)
                    else None
                )
                pc_meta = self._build_point_cloud_metadata(device_id, depth_frame, color_for_texture)
                if pc_meta:
                    metadata["point_cloud"] = pc_meta

        elif "color" in frame_stream_name:
            color_frame = frame.as_video_frame()
            color_frame = self._apply_color_filters(device_id, color_frame)
            # Append to bounded history so the depth thread can pick the color
            # frame closest in time to the depth frame it's processing.
            hist = self.color_frames.get(device_id)
            if hist is None:
                hist = deque(maxlen=COLOR_FRAME_HISTORY)
                self.color_frames[device_id] = hist
            hist.append(color_frame)
            processed_frame = np.asanyarray(color_frame.get_data())
            info_source = color_frame

        elif "infrared" in frame_stream_name:
            processed_frame = np.asanyarray(frame.get_data())
            info_source = frame.as_video_frame()

        elif "gyro" in frame_stream_name or "accel" in frame_stream_name:
            motion_frame = frame.as_motion_frame()
            motion_data = motion_frame.get_motion_data()
            metadata["motion_data"] = {
                "x": float(motion_data.x),
                "y": float(motion_data.y),
                "z": float(motion_data.z),
            }
            processed_frame = np.zeros((120, 320, 3), dtype=np.uint8)
            cv2.putText(processed_frame, f"X: {motion_data.x:.3f}", (10, 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 100, 100), 1)
            cv2.putText(processed_frame, f"Y: {motion_data.y:.3f}", (10, 60),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (100, 255, 100), 1)
            cv2.putText(processed_frame, f"Z: {motion_data.z:.3f}", (10, 90),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (100, 100, 255), 1)

        metadata.update(self._build_viewer_info(info_source))
        return processed_frame, metadata

    def _collect_sensor_frames(
        self,
        device_id: str,
        sensor_id: str,
        rs_queue: Any,
        stream_types: List[str]
    ) -> None:
        """
        Thread function to collect frames from a single sensor's queue.
        Routes frames to appropriate per-stream-type queues.
        
        Args:
            device_id: Device ID
            sensor_id: Sensor ID
            rs_queue: The rs.frame_queue to poll
            stream_types: List of stream types this sensor is producing
        """
        logging.info(f"[SENSOR] Frame collection thread started for {device_id}/{sensor_id} streams: {stream_types}")
        
        colorizer = rs.colorizer()
        
        try:
            while True:
                # Check if we should stop
                with self.lock:
                    if device_id not in self.sensor_streams:
                        break
                    if sensor_id not in self.sensor_streams[device_id]:
                        break
                    sensor_info = self.sensor_streams[device_id][sensor_id]
                    if not sensor_info.get("is_streaming", False):
                        break
                
                try:
                    # Wait for frame with timeout
                    frame = rs_queue.wait_for_frame(timeout_ms=1000)
                    if not frame:
                        continue
                    
                    # Determine frame's stream type from the frame itself
                    frame_profile = frame.get_profile()
                    frame_stream = frame_profile.stream_type()
                    frame_stream_name = frame_stream.name.lower()
                    
                    # Handle infrared index
                    if frame_stream == rs.stream.infrared:
                        frame_stream_name = f"infrared-{frame_profile.stream_index()}"
                    
                    processed_frame, metadata = self._process_sensor_frame(
                        frame, frame_stream_name, device_id, colorizer
                    )

                    if processed_frame is None:
                        continue
                    
                    # Find matching stream type (case-insensitive)
                    target_stream_type = None
                    for st in stream_types:
                        if st.lower() == frame_stream_name.lower():
                            target_stream_type = st
                            break
                    
                    if target_stream_type is None:
                        continue
                    
                    # Add to per-stream-type queues
                    with self.lock:
                        if (device_id in self.sensor_frame_queues and 
                            sensor_id in self.sensor_frame_queues[device_id] and
                            target_stream_type in self.sensor_frame_queues[device_id][sensor_id]):
                            queue = self.sensor_frame_queues[device_id][sensor_id][target_stream_type]
                            queue.append(processed_frame)
                            while len(queue) > self.max_queue_size:
                                queue.pop(0)
                        
                        if (device_id in self.sensor_metadata_queues and 
                            sensor_id in self.sensor_metadata_queues[device_id] and
                            target_stream_type in self.sensor_metadata_queues[device_id][sensor_id]):
                            mqueue = self.sensor_metadata_queues[device_id][sensor_id][target_stream_type]
                            mqueue.append(metadata)
                            while len(mqueue) > self.max_queue_size:
                                mqueue.pop(0)
                    
                except Exception as e:
                    if "timeout" not in str(e).lower():
                        logging.debug(f"[SENSOR] Frame collection error: {e}")
                    continue
                    
        except Exception as e:
            logging.error(f"[SENSOR] Frame collection thread exception: {e}")
        finally:
            logging.info(f"[SENSOR] Frame collection thread ended for {device_id}/{sensor_id}")

    def start_sensor(
        self,
        device_id: str,
        sensor_id: str,
        configs: List[SensorStreamConfig]
    ) -> SensorStreamStatus:
        """
        Start streaming from a single sensor using the sensor API.
        Supports multiple stream profiles (e.g., depth + IR from same sensor).
        
        Args:
            device_id: The device ID
            sensor_id: The sensor ID (format: "{device_id}-sensor-{index}")
            configs: List of stream configurations
            
        Returns:
            SensorStreamStatus with current state
        """
        if not configs:
            raise RealSenseError(status_code=400, detail="At least one stream config required")
        
        # Check mode compatibility
        self._check_streaming_mode(device_id, "sensor")
        
        # Get sensor
        sensor, sensor_index = self._get_sensor_by_id(device_id, sensor_id)
        
        # Check if already streaming - with recovery mechanism
        with self.lock:
            if (device_id in self.sensor_streams and 
                sensor_id in self.sensor_streams[device_id] and
                self.sensor_streams[device_id][sensor_id].get("is_streaming", False)):
                # State says streaming - try to recover by stopping first
                logging.warning(f"[SENSOR] {sensor_id} has stale streaming state - attempting recovery")
                try:
                    sensor.stop()
                except:
                    pass
                try:
                    sensor.close()
                except:
                    pass
                # Clean up stale state
                self.sensor_streams[device_id].pop(sensor_id, None)
                if not self.sensor_streams[device_id]:
                    del self.sensor_streams[device_id]
                    self.streaming_mode[device_id] = "idle"
                if device_id in self.sensor_frame_queues:
                    self.sensor_frame_queues[device_id].pop(sensor_id, None)
                if device_id in self.sensor_metadata_queues:
                    self.sensor_metadata_queues[device_id].pop(sensor_id, None)
                if device_id in self.sensor_rs_queues:
                    self.sensor_rs_queues[device_id].pop(sensor_id, None)
                logging.info(f"[SENSOR] {sensor_id} stale state cleaned up - proceeding with start")
        
        try:
            # Get sensor name
            try:
                sensor_name = sensor.get_info(rs.camera_info.name)
            except RuntimeError:
                sensor_name = f"Sensor {sensor_index}"
            
            # Find matching profile for EACH config
            profiles = []
            for config in configs:
                profile = self._find_matching_profile(sensor, config)
                profiles.append(profile)
            
            # Validate profile compatibility (same FPS required)
            self._validate_profile_compatibility(profiles)

            # Translate sensor timestamps to a unified system-time domain so
            # depth/color frames from independent sensors on the same device
            # are directly comparable. Without this, sensors can report
            # timestamps in different clock domains with a fixed multi-second
            # offset, defeating cross-sensor matching for the textured point
            # cloud (observed: 1.635s offset on the D585 prototype).
            # WARN — not debug — when set_option fails on a sensor that
            # supports() said yes: a partial failure silently reintroduces
            # the cross-sensor offset and the rotation desync.
            if sensor.supports(rs.option.global_time_enabled):
                try:
                    sensor.set_option(rs.option.global_time_enabled, 1)
                except RuntimeError as e:
                    logging.warning(
                        "[SENSOR] %s: global_time_enabled set failed (%s) — "
                        "depth/color may stay in different clock domains and "
                        "textured point cloud may show rotation desync.",
                        sensor_id, e,
                    )

            # Open sensor with ALL profiles
            sensor.open(profiles)
            
            # Small queue so the collector always sees fresh frames. A larger
            # capacity (the old value was 50) lets a 1.6s FIFO backlog build up
            # at 30 fps and the depth thread ends up always processing
            # 1.6s-stale frames — visible as the textured 3D cloud where the
            # color (no PC math, no backlog) reacts to motion immediately and
            # the depth geometry lags ~1.6s behind.
            rs_queue = rs.frame_queue(2)
            
            # Start sensor
            sensor.start(rs_queue)
            
            # Collect stream types (normalized to lowercase for consistent lookup)
            stream_types = [c.stream_type.lower() for c in configs]
            
            # Update state
            with self.lock:
                self.streaming_mode[device_id] = "sensor"
                
                if device_id not in self.sensor_streams:
                    self.sensor_streams[device_id] = {}
                if device_id not in self.sensor_frame_queues:
                    self.sensor_frame_queues[device_id] = {}
                if device_id not in self.sensor_metadata_queues:
                    self.sensor_metadata_queues[device_id] = {}
                if device_id not in self.sensor_rs_queues:
                    self.sensor_rs_queues[device_id] = {}
                
                self.sensor_streams[device_id][sensor_id] = {
                    "is_streaming": True,
                    "stream_types": stream_types,  # List of stream types
                    "configs": configs,  # All configs
                    "started_at": datetime.now(),
                    "error": None,
                    "sensor": sensor,
                    "name": sensor_name,
                }
                # Create per-stream-type frame queues
                self.sensor_frame_queues[device_id][sensor_id] = {st: [] for st in stream_types}
                self.sensor_metadata_queues[device_id][sensor_id] = {st: [] for st in stream_types}
                self.sensor_rs_queues[device_id][sensor_id] = rs_queue
            
            # Initialize post-processing filters for this sensor if not already done
            self._get_or_create_processing_blocks(device_id, sensor_id, sensor)
            
            # Start frame collection thread
            threading.Thread(
                target=self._collect_sensor_frames,
                args=(device_id, sensor_id, rs_queue, stream_types),
                daemon=True
            ).start()
            
            # Start per-device metadata broadcast (no-op if already running for this device).
            self.metadata_socket_server.start_broadcast(device_id)
            
            logging.info(f"[SENSOR] Started {sensor_id} with streams: {stream_types}")
            
            # Return status with backward compat fields
            first_config = configs[0]
            return SensorStreamStatus(
                sensor_id=sensor_id,
                name=sensor_name,
                is_streaming=True,
                stream_type=first_config.stream_type.lower(),  # Backward compat (lowercase for consistency)
                stream_types=stream_types,
                streams=configs,
                resolution=first_config.resolution,
                framerate=first_config.framerate,
                format=first_config.format,
                started_at=datetime.now(),
            )
            
        except RealSenseError:
            raise
        except Exception as e:
            logging.error(f"[SENSOR] Failed to start {sensor_id}: {e}")
            # Clean up on failure
            try:
                sensor.stop()
            except:
                pass
            try:
                sensor.close()
            except:
                pass
            raise RealSenseError(
                status_code=500,
                detail=f"Failed to start sensor: {str(e)}"
            )

    def stop_sensor(
        self,
        device_id: str,
        sensor_id: str
    ) -> SensorStreamStatus:
        """
        Stop streaming from a single sensor.
        
        Args:
            device_id: The device ID
            sensor_id: The sensor ID
            
        Returns:
            SensorStreamStatus with current state
        """
        sensor, sensor_index = self._get_sensor_by_id(device_id, sensor_id)
        
        # Get sensor name
        try:
            sensor_name = sensor.get_info(rs.camera_info.name)
        except RuntimeError:
            sensor_name = f"Sensor {sensor_index}"
        
        with self.lock:
            if (device_id not in self.sensor_streams or
                sensor_id not in self.sensor_streams[device_id]):
                return SensorStreamStatus(
                    sensor_id=sensor_id,
                    name=sensor_name,
                    is_streaming=False,
                )
            
            sensor_info = self.sensor_streams[device_id][sensor_id]
            if not sensor_info.get("is_streaming", False):
                return SensorStreamStatus(
                    sensor_id=sensor_id,
                    name=sensor_name,
                    is_streaming=False,
                )
            
            # Mark as stopping
            sensor_info["is_streaming"] = False
        
        # Stop and close sensor
        try:
            sensor.stop()
            sensor.close()
        except Exception as e:
            logging.warning(f"[SENSOR] Error stopping {sensor_id}: {e}")
        
        # Clean up state
        last_sensor_stopped = False
        with self.lock:
            # Capture the stopped sensor's stream types BEFORE removing its
            # entry — needed below to know whether to evict cached color frames.
            stopped_stream_types: List[str] = []
            if (device_id in self.sensor_streams
                    and sensor_id in self.sensor_streams[device_id]):
                stopped_stream_types = list(
                    self.sensor_streams[device_id][sensor_id].get("stream_types", [])
                )

            if device_id in self.sensor_streams:
                self.sensor_streams[device_id].pop(sensor_id, None)
                if not self.sensor_streams[device_id]:
                    del self.sensor_streams[device_id]
                    self.streaming_mode[device_id] = "idle"
                    last_sensor_stopped = True

            if device_id in self.sensor_frame_queues:
                self.sensor_frame_queues[device_id].pop(sensor_id, None)
                if not self.sensor_frame_queues[device_id]:
                    del self.sensor_frame_queues[device_id]

            if device_id in self.sensor_metadata_queues:
                self.sensor_metadata_queues[device_id].pop(sensor_id, None)
                if not self.sensor_metadata_queues[device_id]:
                    del self.sensor_metadata_queues[device_id]

            if device_id in self.sensor_rs_queues:
                self.sensor_rs_queues[device_id].pop(sensor_id, None)
                if not self.sensor_rs_queues[device_id]:
                    del self.sensor_rs_queues[device_id]

            # Free the cached color frames if the stopped sensor was producing
            # color — otherwise the 5 cached rs.video_frame refs stay pinned
            # in the SDK pool while depth keeps streaming.
            stopped_color = any(st.lower() == "color" for st in stopped_stream_types)
            if stopped_color or last_sensor_stopped:
                self.color_frames.pop(device_id, None)
            if last_sensor_stopped:
                # Per-device rs.pointcloud is only needed while depth runs.
                self.point_clouds.pop(device_id, None)

        # Stop the per-device metadata broadcaster once the last sensor on this
        # device has stopped.
        if last_sensor_stopped:
            self.metadata_socket_server.stop_broadcast(device_id)

        logging.info(f"[SENSOR] Stopped {sensor_id}")
        
        return SensorStreamStatus(
            sensor_id=sensor_id,
            name=sensor_name,
            is_streaming=False,
        )

    def get_sensor_status(
        self,
        device_id: str,
        sensor_id: str
    ) -> SensorStreamStatus:
        """
        Get streaming status for a specific sensor.
        
        Args:
            device_id: The device ID
            sensor_id: The sensor ID
            
        Returns:
            SensorStreamStatus with current state
        """
        sensor, sensor_index = self._get_sensor_by_id(device_id, sensor_id)
        
        # Get sensor name
        try:
            sensor_name = sensor.get_info(rs.camera_info.name)
        except RuntimeError:
            sensor_name = f"Sensor {sensor_index}"
        
        with self.lock:
            if (device_id not in self.sensor_streams or
                sensor_id not in self.sensor_streams[device_id]):
                return SensorStreamStatus(
                    sensor_id=sensor_id,
                    name=sensor_name,
                    is_streaming=False,
                )
            
            info = self.sensor_streams[device_id][sensor_id]
            resolution = info.get("resolution")
            
            return SensorStreamStatus(
                sensor_id=sensor_id,
                name=info.get("name", sensor_name),
                is_streaming=info.get("is_streaming", False),
                stream_type=info.get("stream_type"),
                resolution=Resolution(width=resolution[0], height=resolution[1]) if resolution else None,
                framerate=info.get("framerate"),
                format=info.get("format"),
                error=info.get("error"),
                started_at=info.get("started_at"),
            )

    def batch_start_sensors(
        self,
        device_id: str,
        sensors: List[SensorStartItem]
    ) -> BatchSensorStatus:
        """
        Start multiple sensors atomically.
        
        If any sensor fails to start, all previously started sensors are stopped.
        
        Args:
            device_id: The device ID
            sensors: List of sensor configurations to start
            
        Returns:
            BatchSensorStatus with status of all sensors
        """
        # Check mode compatibility
        self._check_streaming_mode(device_id, "sensor")
        
        started = []
        errors = []
        
        for item in sensors:
            try:
                status = self.start_sensor(device_id, item.sensor_id, item.config)
                if status.error:
                    raise Exception(status.error)
                started.append(item.sensor_id)
            except Exception as e:
                errors.append(f"Failed to start {item.sensor_id}: {str(e)}")
                # Rollback: stop all successfully started sensors
                for started_sensor_id in started:
                    try:
                        self.stop_sensor(device_id, started_sensor_id)
                    except:
                        pass
                break
        
        return self.get_batch_status(device_id)

    def batch_stop_sensors(
        self,
        device_id: str,
        sensor_ids: Optional[List[str]] = None
    ) -> BatchSensorStatus:
        """
        Stop multiple sensors.
        
        Args:
            device_id: The device ID
            sensor_ids: List of sensor IDs to stop, or None to stop all
            
        Returns:
            BatchSensorStatus with status of all sensors
        """
        with self.lock:
            if device_id not in self.sensor_streams:
                return BatchSensorStatus(
                    device_id=device_id,
                    mode=self.streaming_mode.get(device_id, "idle"),
                    sensors=[],
                    errors=[],
                )
            
            # Get sensor IDs to stop
            if sensor_ids is None:
                sensor_ids = list(self.sensor_streams[device_id].keys())
        
        errors = []
        for sensor_id in sensor_ids:
            try:
                self.stop_sensor(device_id, sensor_id)
            except Exception as e:
                errors.append(f"Failed to stop {sensor_id}: {str(e)}")
        
        status = self.get_batch_status(device_id)
        status.errors = errors
        return status

    def get_batch_status(
        self,
        device_id: str
    ) -> BatchSensorStatus:
        """
        Get streaming status for all sensors on a device.
        
        Args:
            device_id: The device ID
            
        Returns:
            BatchSensorStatus with status of all sensors
        """
        if device_id not in self.devices:
            self.refresh_devices()
            if device_id not in self.devices:
                raise RealSenseError(
                    status_code=404, detail=f"Device {device_id} not found"
                )
        
        # Get all sensors for the device
        sensors_info = self.get_sensors(device_id)
        
        sensor_statuses = []
        for sensor_info in sensors_info:
            status = self.get_sensor_status(device_id, sensor_info.sensor_id)
            sensor_statuses.append(status)
        
        return BatchSensorStatus(
            device_id=device_id,
            mode=self.streaming_mode.get(device_id, "idle"),
            sensors=sensor_statuses,
            errors=[],
        )

    def get_sensor_frame(
        self,
        device_id: str,
        sensor_id: str
    ) -> Tuple[np.ndarray, dict]:
        """
        Get the latest frame from a specific sensor.
        
        Args:
            device_id: The device ID
            sensor_id: The sensor ID
            
        Returns:
            Tuple of (frame_data, metadata)
        """
        with self.lock:
            if (device_id not in self.sensor_frame_queues or
                sensor_id not in self.sensor_frame_queues[device_id]):
                raise RealSenseError(
                    status_code=400,
                    detail=f"Sensor {sensor_id} is not streaming"
                )
            
            queue = self.sensor_frame_queues[device_id][sensor_id]
            if len(queue) == 0:
                # 503 Service Unavailable: sensor is streaming but no frames yet
                # (transient — caller should retry rather than treat as fatal)
                raise RealSenseError(
                    status_code=503,
                    detail=f"No frames available for sensor {sensor_id}"
                )
            
            return queue[-1]

    def send_hwm_command(
        self,
        device_id: str,
        opcode: int,
        param1: int = 0,
        param2: int = 0,
        param3: int = 0,
        param4: int = 0,
        data: Optional[List[int]] = None,
    ) -> List[int]:
        """Send a hardware monitor (HWM) command and return the raw firmware response.

        Uses the SDK debug_protocol extension to build and transmit the command.

        Args:
            device_id: Serial number of the target device.
            opcode: HWM opcode (e.g. 0x10 for GVD).
            param1..param4: Optional command parameters (default 0).
            data: Optional payload bytes appended after the header.

        Returns:
            Raw firmware response as a list of int byte values.

        Raises:
            RealSenseError 404: Device not found.
            RealSenseError 400: Device does not support the debug_protocol extension.
            RealSenseError 500: Firmware rejected or failed to execute the command.
        """
        if device_id not in self.devices:
            self.refresh_devices()
        with self.lock:
            if device_id not in self.devices:
                raise RealSenseError(
                    status_code=404, detail=f"Device {device_id} not found"
                )
            dev = self.devices[device_id]

        # is_debug_protocol() is the correct way to check extension support before casting.
        # as_debug_protocol() does not raise when unsupported — it returns an empty handle
        # whose methods would fail later with a harder-to-diagnose error.
        if not dev.is_debug_protocol():
            raise RealSenseError(
                status_code=400,
                detail=f"Device {device_id} does not support hardware monitor commands",
            )

        debug = dev.as_debug_protocol()
        payload = list(data) if data else []

        try:
            cmd = debug.build_command(opcode, param1, param2, param3, param4, payload)
            raw_response = debug.send_and_receive_raw_data(cmd)
            response_bytes = list(raw_response)

            # The raw response starts with a 4-byte little-endian uint32 that echoes
            # the sent opcode on success or contains a firmware error code on failure.
            # send_and_receive_raw_data does not raise on firmware-level errors, so we
            # must inspect the opcode ourselves.
            if len(response_bytes) < 4:
                raise RealSenseError(
                    status_code=500, detail="HWM command failed: response too short"
                )
            response_opcode, = struct.unpack_from('<I', bytes(response_bytes[:4]))
            if response_opcode != opcode:
                raise RealSenseError(
                    status_code=500,
                    detail=f"HWM command failed: firmware returned error code 0x{response_opcode:08X} (expected opcode echo 0x{opcode:08X})",
                )
            return response_bytes
        except RealSenseError:
            raise
        except Exception as e:
            raise RealSenseError(
                status_code=500, detail=f"HWM command failed: {e}"
            )

    def get_sensor_metadata(
        self,
        device_id: str,
        sensor_id: str
    ) -> Dict:
        """
        Get the latest metadata from a specific sensor.
        
        Args:
            device_id: The device ID
            sensor_id: The sensor ID
            
        Returns:
            Metadata dictionary
        """
        with self.lock:
            if (device_id not in self.sensor_metadata_queues or
                sensor_id not in self.sensor_metadata_queues[device_id]):
                raise RealSenseError(
                    status_code=400,
                    detail=f"Sensor {sensor_id} is not streaming"
                )
            
            queue = self.sensor_metadata_queues[device_id][sensor_id]
            if len(queue) == 0:
                return {}
            
            return queue[-1]