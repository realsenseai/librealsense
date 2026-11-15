# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import asyncio
import platform
import struct
import threading
import time
import logging
from collections import defaultdict, deque
from typing import Callable, Deque, Dict, List, Optional, Any, Tuple, Set
import pyrealsense2 as rs
import numpy as np
from app.core.errors import RealSenseError
from app.services import advanced_mode, options
from app.models.device import Device, DeviceInfo
from app.models.sensor import SensorInfo, StreamProfile
from app.models.option import Option, OptionInfo
from app.models.stream import PointCloudStatus, StreamConfig, StreamStatus
import socketio

from app.services.metadata_socket_server import MetadataSocketServer
from app.services import firmware as fw


_IS_WINDOWS = platform.system() == "Windows"

# Recent color frames retained per device for texturing the 3D point cloud.
# Five frames at 30 fps covers the typical depth/color SDK-latency offset
# (~one capture interval); the picker selects the entry whose timestamp is
# closest to the depth frame being processed.
COLOR_FRAME_HISTORY = 5


def _stream_name(profile) -> str:
    if profile.stream_type() == rs.stream.infrared:
        return f"infrared-{profile.stream_index()}"
    return profile.stream_type().name.lower()


def _profile_row(profile) -> StreamProfile:
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

        # Frame-arrival signalling for async consumers (WebRTC). ``_frame_seq``
        # counts frames pushed per "<device_id>:<stream_type>"; collection
        # threads bump it and set the matching asyncio.Event on the main loop,
        # so waiters block without occupying an executor thread.
        self._frame_seq: Dict[str, int] = {}
        self._frame_events: Dict[str, asyncio.Event] = {}
        self.is_pointcloud_enabled: Dict[str, bool] = {}
        # One rs.pointcloud() per device: pc.map_to(color) mutates internal
        # state, and depth/color threads from different devices would race on
        # a shared instance (texturing device A's cloud with device B's color).
        self.point_clouds: Dict[str, "rs.pointcloud"] = {}

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

        # --- Per-Sensor Streaming State (Sensor API) ---
        # Tracks which mode each device is using: "pipeline", "sensor", or "idle"
        self.streaming_mode: Dict[str, str] = {}  # device_id -> mode

        # --- Post-Processing Filters ---
        # Stores filter instances per device/sensor: device_id -> sensor_id -> list of filter dicts
        # Each filter dict: { "filter": rs.filter, "name": str, "enabled": bool }
        self.processing_blocks: Dict[str, Dict[str, List[Dict[str, Any]]]] = {}
        # One colorizer per device so depth-visualization options (color scheme,
        # min/max distance, histogram eq) set on it affect the streamed depth image.
        self.colorizers: Dict[str, "rs.colorizer"] = defaultdict(rs.colorizer)
        # Per-sensor rs.frame_queue objects: device_id -> sensor_id -> rs.frame_queue
        self.sensor_rs_queues: Dict[str, Dict[str, Any]] = {}

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
            # Clear existing devices (that aren't streaming, in either mode)
            for device_id in list(self.devices.keys()):
                if device_id not in self.pipelines and not self.sensor_rs_queues.get(device_id):
                    del self.devices[device_id]
                    self.device_infos.pop(device_id, None)
                    self._supported_md_by_profile.pop(device_id, None)

            for dev in self.ctx.devices:
                self._register_new_device(dev)

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

    def get_recommended_firmware(self, device_id: str) -> Dict[str, Any]:
        """Return the firmware version the online DB recommends for a device, if any."""
        device = self.get_device(device_id)
        recommended, _link = fw.recommended_firmware(device.name)
        return {"recommended": recommended}

    def update_firmware_from_recommended(self, device_id: str) -> Dict[str, Any]:
        """Download the device's recommended firmware image and flash it.

        Looks up the recommended .bin from the online versions DB, downloads it into
        memory, and reuses the standard DFU flash flow — so Socket.IO
        progress/success/failure events are emitted exactly as for a user-supplied file.
        """
        device = self.get_device(device_id)
        recommended, link = fw.recommended_firmware(device.name)
        if not link:
            raise RealSenseError(status_code=404, detail="No recommended firmware available for this device")
        # Refuse before downloading ~2 MiB to flash a version the device already has or beats.
        if fw.is_newer_or_same(device.firmware_version, recommended):
            raise RealSenseError(
                status_code=409,
                detail=f"Device already runs firmware {device.firmware_version}; recommended is {recommended}",
            )
        # Claim before emitting: a duplicate request must not put events on this device's
        # channel, where they would rewrite the modal of the update already running.
        self._claim_fw_update_slot(device_id)
        # Emit download progress under the "downloading" phase so the UI can show it
        # before the (separate) install phase begins.
        _holder, on_download = self._make_fw_progress_callback(device_id, phase="downloading")
        try:
            fw_bytes = fw.download_firmware(link, on_progress=on_download)
        except Exception as exc:
            # Surface download failures on the same channel as flash failures so the
            # progress modal shows the error instead of hanging.
            self._emit_socket_event(
                f"firmware_update_failed_{device_id}",
                {"device_id": device_id, "error": str(exc)},
            )
            with self.lock:
                self._fw_updates_in_progress.discard(device_id)
            raise
        return self.update_firmware_from_bytes(device_id, fw_bytes, slot_held=True)

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

    def update_firmware_from_bytes(self, device_id: str, fw_bytes: bytes, slot_held: bool = False) -> Dict[str, Any]:
        """Run firmware update using a user-supplied image blob.

        Reuses the DFU flow: check_firmware_compatibility -> enter_update_state ->
        wait for DFU device -> update_dev.update(image, on_progress) -> wait for reconnect.
        Emits the same Socket.IO progress / success / failure events as a bundled-image update.
        """
        if not slot_held:
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
            # Mark the install phase up front: the one-click path has been showing download
            # progress, and the SDK's first flash tick only arrives after DFU re-enumeration.
            on_progress(0.0)

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
            if not updated_info:
                # Written, but a device that never came back is not a success we can report.
                raise RealSenseError(
                    status_code=504,
                    detail="Firmware written, but the device did not reconnect. "
                           "Power-cycle it and check its version.",
                )

            self._emit_socket_event(
                f"firmware_update_success_{device_id}",
                {"device_id": device_id, "firmware_version": updated_info.firmware_version},
            )

            return {
                "device_id": device_id,
                "progress": progress_holder["value"],
                "firmware_version": updated_info.firmware_version,
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
        self, device_id: str, phase: str = "installing",
    ) -> Tuple[Dict[str, float], Callable[[float], None]]:
        """Build the on_progress callback and a holder dict that records the latest value.

        Rate-limits emissions to ~10/sec but always lets the final 100% through.
        `phase` ("downloading" | "installing") lets the UI label what's happening.
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
                {"device_id": device_id, "progress": float(p), "phase": phase},
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

            stream_profiles_list = [_profile_row(profile) for profile in sensor.get_stream_profiles()]

            # Get options
            sensor_options = self.get_sensor_options(device_id, sensor_id)

            sensor_info = SensorInfo(
                sensor_id=sensor_id,
                name=name,
                type=sensor_type,
                supported_stream_profiles=stream_profiles_list,  # Use correct field name
                options=sensor_options,
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

    def get_sensor_options(self, device_id: str, sensor_id: str) -> List[OptionInfo]:
        """Get all options for a sensor"""
        return options.all_options(self._find_sensor(device_id, sensor_id))

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

    def get_sensor_filters(self, device_id: str, sensor_id: str) -> Dict[str, Any]:
        """The sensor's post-processing filters, keyed by filter name.

        Keyed rather than flat because filters share option names - holes_fill exists on
        Spatial, Temporal and Hole Filling with a different meaning on each.
        """
        sensor = self._find_sensor(device_id, sensor_id)
        filters = {}
        for filter_info in self._get_or_create_processing_blocks(device_id, sensor_id, sensor):
            filters[filter_info["name"]] = {
                "enabled": filter_info["enabled"],
                "default_enabled": bool(filter_info["default_enabled"]),
                "options": options.all_options(filter_info["filter"]),
            }
        return filters

    def _filters_by_name(self, device_id: str, sensor_id: str) -> Dict[str, Dict[str, Any]]:
        sensor = self._find_sensor(device_id, sensor_id)
        return {f["name"]: f for f in self._get_or_create_processing_blocks(device_id, sensor_id, sensor)}

    def set_filter_option(
        self, device_id: str, sensor_id: str, filter_name: str, field: str, value: float
    ) -> OptionInfo:
        """Set one option on one filter of the sensor's chain."""
        filters = self._filters_by_name(device_id, sensor_id)
        return options.set_option(filters[filter_name]["filter"], field, value)

    def set_filter_enabled(
        self, device_id: str, sensor_id: str, filter_name: str, enabled: float
    ) -> None:
        """Bypass or apply one filter of the sensor."""
        self._filters_by_name(device_id, sensor_id)[filter_name]["enabled"] = bool(enabled)

    def get_colorizer_options(self, device_id: str) -> List[OptionInfo]:
        """The device colorizer's controls."""
        self._require_device(device_id)
        return options.all_options(self.colorizers[device_id])

    def set_colorizer_option(self, device_id: str, field: str, value: float) -> OptionInfo:
        """Set one colorizer option."""
        self._require_device(device_id)
        return options.set_option(self.colorizers[device_id], field, value)

    def _require_device(self, device_id: str):
        if device_id not in self.devices:
            self.refresh_devices()
        dev = self.devices.get(device_id)
        if dev is None:
            raise RealSenseError(status_code=404, detail=f"Device {device_id} not found")
        return dev

    def get_advanced_mode_status(self, device_id: str) -> Dict[str, bool]:
        """Whether the device supports RS400 advanced mode, and whether it is on."""
        return advanced_mode.status(self._require_device(device_id))

    def set_advanced_mode(self, device_id: str, enable: bool) -> Dict[str, bool]:
        """Enable/disable advanced mode. This RESTARTS the device; wait for it to return."""
        advanced_mode.toggle(self._require_device(device_id), enable)
        # Re-resolve the re-enumerated device, then report what it says rather than what
        # was asked for.
        self._refresh_until_device_returns(device_id)
        return self.get_advanced_mode_status(device_id)

    def get_advanced_controls(self, device_id: str) -> Dict[str, List[OptionInfo]]:
        return advanced_mode.controls(self._require_device(device_id))

    def set_advanced_control(self, device_id: str, group: str, field: str, value: float) -> OptionInfo:
        return advanced_mode.set_control(self._require_device(device_id), group, field, value)

    def get_sensor_option(
        self, device_id: str, sensor_id: str, option_id: str
    ) -> OptionInfo:
        """Get a specific option for a sensor"""
        for option in self.get_sensor_options(device_id, sensor_id):
            if option.option_id == option_id:
                return option
        raise RealSenseError(status_code=404, detail=f"Option {option_id} not found")

    def set_sensor_option(
        self, device_id: str, sensor_id: str, option_id: str, value: Any
    ) -> OptionInfo:
        """Set one option of a sensor.

        The option is matched by its SDK name or by a display label of it, since the
        chatbot proposes settings by the name the user sees ("Laser Power").
        """
        sensor = self._find_sensor(device_id, sensor_id)
        wanted = {option_id.lower(), option_id.lower().replace(" ", "_")}
        name = next(o.name for o in sensor.get_supported_options() if o.name.lower() in wanted)
        return options.set_option(sensor, name, value)

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
    ) -> StreamStatus:
        """Start streaming from a device"""
        # Check mode compatibility - pipeline API cannot be used if sensor API is active
        self._check_streaming_mode(device_id, "pipeline")

        # Only refresh when the cache is empty or the requested device is unknown
        if device_id not in self.devices:
            self.refresh_devices()

        if device_id not in self.devices:
            raise RealSenseError(
                status_code=404, detail=f"Device {device_id} not found"
            )

        pipeline = rs.pipeline(self.ctx)
        pipeline.set_device(self.devices[device_id])  # skips re-creating the device on start
        config = rs.config()

        # Track active stream types
        active_streams = set()
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

        # Start streaming
        try:
            pipeline_profile = pipeline.start(config)
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
                self.active_streams[device_id] = active_streams
                self.frame_queues[device_id] = {
                    stream_type: [] for stream_type in active_streams
                }
                self.metadata_queues[device_id] = {
                    stream_key: [] for stream_key in active_streams
                }
                # Track that this device is using pipeline API
                self.streaming_mode[device_id] = "pipeline"

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
            return StreamStatus(
                device_id=device_id,
                is_streaming=True,
                active_streams=list(active_streams),
            )
        except RuntimeError as e:
            raise RealSenseError(
                status_code=500, detail=f"Failed to start streaming: {str(e)}"
            )

    def stop_stream(self, device_id: str) -> StreamStatus:
        """Stop streaming from a device"""
        with self.lock:
            if device_id not in self.devices:
                return StreamStatus(device_id=device_id, is_streaming=False, active_streams=[])

            is_streaming = device_id in self.pipelines
            active_streams = list(self.active_streams.get(device_id, set()))
            if not is_streaming:
                return StreamStatus(device_id=device_id, is_streaming=False, active_streams=active_streams)

        try:
            self.metadata_socket_server.stop_broadcast(device_id)
            self.pipelines[device_id].stop()
        except Exception as e:
            logging.error("Failed to stop streaming for %s: %s", device_id, e)
        finally:
            with self.lock:
                # Clean up resources
                self.pipelines.pop(device_id, None)
                self.active_streams.pop(device_id, None)
                self.frame_queues.pop(device_id, None)
                self.metadata_queues.pop(device_id, None)
                self.color_frames.pop(device_id, None)
                self.point_clouds.pop(device_id, None)
                self._supported_md_by_profile.pop(device_id, None)
                # Reset streaming mode to idle
                self.streaming_mode[device_id] = "idle"
                if device_id in self.device_infos:
                    self.device_infos[device_id].is_streaming = False

        return StreamStatus(
            device_id=device_id,
            is_streaming=False,
            active_streams=active_streams,
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
        
        # Check sensor mode - its active streams are the device's frame queues
        sensor_streams = list(self.frame_queues.get(device_id, {})) if mode == "sensor" else []
        
        # Combine based on mode
        is_streaming = is_pipeline_streaming or len(sensor_streams) > 0
        active_streams = pipeline_streams if mode == "pipeline" else sensor_streams

        return StreamStatus(
            device_id=device_id,
            is_streaming=is_streaming,
            active_streams=active_streams,
        )

    def _publish_frames(self, device_id: str, stream_types) -> None:
        """Bump the arrival counter for each stream and wake async waiters.

        Runs on collection threads; the events are set via the main loop so
        waiters need no executor thread.
        """
        loop = RealSenseManager._main_loop
        for stream_type in stream_types:
            key = f"{device_id}:{stream_type.lower()}"
            self._frame_seq[key] = self._frame_seq.get(key, 0) + 1
            event = self._frame_events.get(key)
            if event is not None and loop is not None and not loop.is_closed():
                loop.call_soon_threadsafe(event.set)

    async def wait_for_frame_after(
        self, device_id: str, stream_type: str, last_seq: int, timeout: float = 1.0
    ) -> Tuple[Optional[np.ndarray], int]:
        """Wait until a frame newer than ``last_seq`` arrives, then return it.

        Returns ``(frame, seq)``; ``frame`` is None on timeout. The seq is read
        after the frame is fetched, so a frame landing in between skips ahead
        rather than being re-delivered.
        """
        key = f"{device_id}:{stream_type.lower()}"
        event = self._frame_events.setdefault(key, asyncio.Event())
        deadline = time.monotonic() + timeout
        while self._frame_seq.get(key, 0) <= last_seq:
            event.clear()
            if self._frame_seq.get(key, 0) > last_seq:
                break
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return None, last_seq
            try:
                await asyncio.wait_for(event.wait(), remaining)
            except asyncio.TimeoutError:
                return None, last_seq
        frame = self.get_latest_frame(device_id, stream_type)
        return frame, self._frame_seq.get(key, 0)

    def get_latest_frame(
        self, device_id: str, stream_type: str
    ) -> np.ndarray:
        """Get the latest frame from a specific stream (supports both pipeline and sensor modes)"""
        with self.lock:
            queues = self.frame_queues.get(device_id)
            if queues is None:
                raise RealSenseError(
                    status_code=400, detail=f"Device {device_id} is not streaming"
                )
            if stream_type not in queues:
                raise RealSenseError(
                    status_code=400,
                    detail=f"Stream type '{stream_type}' is not active. Available: {list(queues)}"
                )
            if not queues[stream_type]:
                # 503 Service Unavailable: stream is active but no frames yet
                # (transient — caller should retry rather than treat as fatal)
                raise RealSenseError(
                    status_code=503,
                    detail=f"No frames available for stream {stream_type}",
                )
            return queues[stream_type][-1]

    def get_latest_metadata(self, device_id: str, stream_type: str) -> Dict:
        """Get the latest METADATA dictionary from a specific stream (supports both pipeline and sensor modes)"""
        stream_key = stream_type.lower()  # Use consistent key format
        with self.lock:
            queue = self.metadata_queues.get(device_id, {}).get(stream_key)
            if queue is not None:
                return queue[-1] if queue else {}

            # If we get here, the stream is not active or device is not streaming
            if self.streaming_mode.get(device_id, "idle") == "idle":
                raise RealSenseError(
                    status_code=400, detail=f"Device {device_id} is not streaming."
                )
            else:
                # Streaming but stream type not found
                raise RealSenseError(
                    status_code=400,
                    detail=f"Stream type '{stream_key}' is not active for device {device_id}.",
                )

    _FRAME_METADATA_ITEMS = list(rs.frame_metadata_value.__members__.items())

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
            supported = [(name, md) for name, md in self._FRAME_METADATA_ITEMS
                         if frame_data.supports_frame_metadata(md)]
            if profile_uid is not None:
                if device_cache is None:
                    device_cache = self._supported_md_by_profile[device_id] = {}
                device_cache[profile_uid] = supported

        attrs: Dict[str, int] = {}
        for name, md in (supported or []):
            try:
                attrs[name] = frame_data.get_frame_metadata(md)
            except Exception as e:
                # supports_frame_metadata said yes; getting the value should not throw.
                logging.debug("[METADATA] failed to read %s: %s", name, e)
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
        return any(st.lower() == "color" for st in self.frame_queues.get(device_id, {}))

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
        
        # Cached per-device colorizer so Depth Visualization options apply live
        colorizer = self.colorizers[device_id]

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
                                    # Placeholder frame; viewers draw motion from metadata["motion_data"]
                                    frame = np.zeros((120, 320, 3), dtype=np.uint8)
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

                    self._publish_frames(device_id, processed_frames.keys())

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

    def _process_sensor_frame(
        self,
        frame: Any,
        device_id: str,
        colorizer: Any,
    ) -> Tuple[Optional[Any], dict]:
        """Process a single sensor frame; return (processed_frame, metadata)."""
        processed_frame = None
        info_source = frame  # frame to read info from after post processing is done
        metadata: dict = {
            "frame_metadata": self._get_frame_metadata(frame, device_id),
        }
        stream = frame.get_profile().stream_type()

        if stream == rs.stream.depth:
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

        elif stream == rs.stream.color:
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

        elif stream == rs.stream.infrared:
            processed_frame = np.asanyarray(frame.get_data())
            info_source = frame.as_video_frame()

        elif stream in (rs.stream.gyro, rs.stream.accel):
            motion_frame = frame.as_motion_frame()
            motion_data = motion_frame.get_motion_data()
            metadata["motion_data"] = {
                "x": float(motion_data.x),
                "y": float(motion_data.y),
                "z": float(motion_data.z),
            }
            processed_frame = np.zeros((120, 320, 3), dtype=np.uint8)

        metadata.update(self._build_viewer_info(info_source))
        return processed_frame, metadata

    def _collect_sensor_frames(
        self,
        device_id: str,
        sensor_id: str,
        rs_queue: Any,
        queue_keys: Dict[Tuple[Any, int], str]
    ) -> None:
        """
        Thread function to collect frames from a single sensor's queue.
        Routes frames to appropriate per-stream-type queues.

        Args:
            device_id: Device ID
            sensor_id: Sensor ID
            rs_queue: The rs.frame_queue to poll
            queue_keys: (stream type, stream index) of each opened stream -> its queue key
        """
        logging.info(f"[SENSOR] Frame collection thread started for {device_id}/{sensor_id} streams: {list(queue_keys.values())}")

        colorizer = self.colorizers[device_id]

        try:
            while True:
                # Stop once this start's queue is gone (stopped, or replaced by a restart)
                with self.lock:
                    if self.sensor_rs_queues.get(device_id, {}).get(sensor_id) is not rs_queue:
                        break
                
                try:
                    # Wait for frame with timeout
                    frame = rs_queue.wait_for_frame(timeout_ms=1000)
                    if not frame:
                        continue
                    
                    profile = frame.get_profile()
                    target_stream_type = queue_keys.get((profile.stream_type(), profile.stream_index()))
                    if target_stream_type is None:
                        continue

                    processed_frame, metadata = self._process_sensor_frame(frame, device_id, colorizer)

                    if processed_frame is None:
                        continue

                    # Add to per-stream-type queues
                    with self.lock:
                        queue = self.frame_queues.get(device_id, {}).get(target_stream_type)
                        if queue is not None:
                            queue.append(processed_frame)
                            while len(queue) > self.max_queue_size:
                                queue.pop(0)

                        mqueue = self.metadata_queues.get(device_id, {}).get(target_stream_type)
                        if mqueue is not None:
                            mqueue.append(metadata)
                            while len(mqueue) > self.max_queue_size:
                                mqueue.pop(0)

                    self._publish_frames(device_id, (target_stream_type,))

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
        rows: List[StreamProfile]
    ) -> None:
        """
        Start streaming from a single sensor using the sensor API.
        Supports multiple stream profiles (e.g., depth + IR from same sensor).
        
        Args:
            device_id: The device ID
            sensor_id: The sensor ID (format: "{device_id}-sensor-{index}")
            rows: profiles from the sensor's supported_stream_profiles
        """
        # Check mode compatibility
        self._check_streaming_mode(device_id, "sensor")

        sensor = self._find_sensor(device_id, sensor_id)

        try:
            profiles = [next(p for p in sensor.get_stream_profiles() if _profile_row(p) == r) for r in rows]

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
            
            queue_keys = {(p.stream_type(), p.stream_index()): _stream_name(p) for p in profiles}
            stream_types = list(queue_keys.values())

            # Update state
            with self.lock:
                self.streaming_mode[device_id] = "sensor"
                
                # Create per-stream-type frame queues
                for st in stream_types:
                    self.frame_queues.setdefault(device_id, {})[st] = []
                    self.metadata_queues.setdefault(device_id, {})[st] = []
                self.sensor_rs_queues.setdefault(device_id, {})[sensor_id] = rs_queue
            
            # Initialize post-processing filters for this sensor if not already done
            self._get_or_create_processing_blocks(device_id, sensor_id, sensor)
            
            # Start frame collection thread
            threading.Thread(
                target=self._collect_sensor_frames,
                args=(device_id, sensor_id, rs_queue, queue_keys),
                daemon=True
            ).start()
            
            # Start per-device metadata broadcast (no-op if already running for this device).
            self.metadata_socket_server.start_broadcast(device_id)
            
            logging.info(f"[SENSOR] Started {sensor_id} with streams: {stream_types}")
            
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
    ) -> None:
        """
        Stop streaming from a single sensor.
        
        Args:
            device_id: The device ID
            sensor_id: The sensor ID
        """
        sensor = self._find_sensor(device_id, sensor_id)
        
        # Dropping the rs queue claims the stop and ends the collection thread
        with self.lock:
            if self.sensor_rs_queues.get(device_id, {}).pop(sensor_id, None) is None:
                return
        
        # The sensor reports its open streams until close()
        stopped_stream_types = [_stream_name(p) for p in sensor.get_active_streams()]

        # Stop and close sensor
        try:
            sensor.stop()
            sensor.close()
        except Exception as e:
            logging.warning(f"[SENSOR] Error stopping {sensor_id}: {e}")

        # Clean up state
        last_sensor_stopped = False
        with self.lock:
            for st in stopped_stream_types:
                self.frame_queues.get(device_id, {}).pop(st, None)
                self.metadata_queues.get(device_id, {}).pop(st, None)
            if not self.frame_queues.get(device_id):
                self.frame_queues.pop(device_id, None)
                self.metadata_queues.pop(device_id, None)
                self.streaming_mode[device_id] = "idle"
                last_sensor_stopped = True

            if not self.sensor_rs_queues.get(device_id, True):
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
