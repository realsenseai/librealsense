# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import platform
import struct
import threading
import time
import logging
from collections import defaultdict
from typing import Callable, Dict, List, Optional, Any, Tuple, Set
import pyrealsense2 as rs
from app.core.errors import RealSenseError
from app.services import advanced_mode, options
from app.models.device import Device, DeviceInfo
from app.models.sensor import SensorInfo, StreamProfile
from app.models.option import Option, OptionInfo
from app.models.stream import Extrinsics, StreamConfig, StreamExtrinsics

from app.services.events import Events
from app.services.frame_sender import DISCONNECTED, STOPPED, FrameSender
from app.services import firmware as fw


_IS_WINDOWS = platform.system() == "Windows"



def _active_rows(sensor) -> List[StreamProfile]:
    """What the sensor streams now, started by any client through either API."""
    return [_profile_row(p) for p in sensor.get_active_streams()]


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
    def __init__(self):
        self.ctx = rs.context()
        self.devices: Dict[str, rs.device] = {}
        self.device_infos: Dict[str, DeviceInfo] = {}
        self.pipelines: Dict[str, rs.pipeline] = {}
        self.lock = threading.Lock()

        # Firmware update tracking (one update at a time per device)
        self._fw_updates_in_progress: Set[str] = set()

        self.events = Events()

        # --- Post-Processing Filters ---
        # Stores filter instances per device/sensor: device_id -> sensor_id -> list of filter dicts
        # Each filter dict: { "filter": rs.filter, "name": str, "enabled": bool }
        self.processing_blocks: Dict[str, Dict[str, List[Dict[str, Any]]]] = {}
        # One colorizer per device so depth-visualization options (color scheme,
        # min/max distance, histogram eq) set on it affect the streamed depth image.
        self.colorizers: Dict[str, "rs.colorizer"] = defaultdict(rs.colorizer)
        # Per-sensor rs.frame_queue objects: device_id -> sensor_id -> rs.frame_queue
        self.sensor_rs_queues: Dict[str, Dict[str, Any]] = {}
        # Sends each stream's frames, unchanged, to its WebSocket subscribers
        self.frame_sender = FrameSender()

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
            gone_pipelines = [self._remove_device(serial) for serial in removed]
            for dev in new_devs:
                serial = self._register_new_device(dev)
                if serial is not None:
                    added.append(serial)
        del gone_pipelines

        if added or removed:
            logging.info("devices_changed: +%s -%s", added, removed)
            self.events.emit("devices_changed", {"added": added, "removed": removed})

    def _remove_device(self, serial: str) -> Optional[rs.pipeline]:
        """Returns the device's pipeline for the caller to drop after releasing the lock:
        dropping it stops a running pipeline, and the stop waits for frame callbacks, which take the lock."""
        assert self.lock.locked(), "_remove_device called without self.lock held"
        self.colorizers.pop(serial, None)
        self.devices.pop(serial, None)
        self.device_infos.pop(serial, None)
        self.sensor_rs_queues.pop(serial, None)
        self.frame_sender.close(serial, DISCONNECTED)
        return self.pipelines.pop(serial, None)

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
            metadata_enabled=metadata_enabled,
        )
        pipeline = rs.pipeline(self.ctx)
        pipeline.set_device(dev)  # skips re-creating the device on start
        # Publish atomically at the end — if anything above raises, no partial
        # cache entry is left behind. Keep new work above this block.
        self.devices[device_id] = dev
        self.device_infos[device_id] = info
        self.pipelines[device_id] = pipeline
        return device_id

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
            # Known devices are kept; unplugged ones go in _on_devices_changed
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
        memory, and reuses the standard DFU flash flow — so the
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
            self.events.emit(
                "firmware_update_failed",
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
        Emits the same progress / success / failure events as a bundled-image update.
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
                self.events.emit(
                    "firmware_update_failed",
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

            self.events.emit(
                "firmware_update_success",
                {"device_id": device_id, "firmware_version": updated_info.firmware_version},
            )

            return {
                "device_id": device_id,
                "progress": progress_holder["value"],
                "firmware_version": updated_info.firmware_version,
                "status": "success",
            }
        except RealSenseError as exc:
            self.events.emit(
                "firmware_update_failed",
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
        """Reject the update if the device is unknown."""
        if device_id not in self.devices:
            raise RealSenseError(status_code=404, detail=f"Device {device_id} not found")

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
            self.events.emit(
                "firmware_progress",
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
            pipeline = self._remove_device(device_id)
        del pipeline
        self.events.emit(
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
            self.events.emit(
                "firmware_update_failed",
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
            pipeline = self._remove_device(device_id)
        del pipeline

        self.events.emit("devices_changed", {"added": [], "removed": [device_id]})

        try:
            dev.hardware_reset()
            return True
        except Exception as e:
            # Reset failed: handle still valid, device still plugged in.
            # Restore cache + announce device back so FE stops showing it as gone.
            with self.lock:
                self._register_new_device(dev)
            self.events.emit(
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
                active_profiles=_active_rows(sensor),
                options=sensor_options,
            )

            sensors.append(sensor_info)

        return sensors

    def is_streaming(self, device_id: str, stream_type: Any, stream_index: int) -> bool:
        """Whether a sensor of the device streams the stream, through either the sensor API or the pipeline."""
        dev = self.devices.get(device_id)
        return dev is not None and any((p.stream_type(), p.stream_index()) == (stream_type, stream_index)
                                       for sensor in dev.sensors for p in sensor.get_active_streams())

    def get_extrinsics(self, device_id: str) -> List[StreamExtrinsics]:
        """From each stream of the device to every other; calibration, so they hold whatever streams."""
        streams = {(p.stream_type(), p.stream_index()): p  # per stream, not per resolution
                   for sensor in self._require_device(device_id).sensors for p in sensor.get_stream_profiles()}
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

    def _streaming_changed(self, device_id: str) -> None:
        """Tell every client what each sensor of the device streams now, as a start or stop by any client changed it."""
        dev = self.devices.get(device_id)
        if dev is not None:
            self.events.emit("streaming_changed", {"device_id": device_id, "sensors": {
                f"{device_id}-sensor-{i}": [r.model_dump() for r in _active_rows(s)] for i, s in enumerate(dev.sensors)}})

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
        Returns list of dicts: { "filter": rs.filter, "name": str, "enabled": bool }; every filter starts off
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

                    filters.append({"filter": f, "name": filter_name, "enabled": False})
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

    def start_stream(
        self,
        device_id: str,
        configs: List[StreamConfig],
    ) -> None:
        """Start streaming from a device"""
        # Only refresh when the cache is empty or the requested device is unknown
        if device_id not in self.devices:
            self.refresh_devices()

        if device_id not in self.devices:
            raise RealSenseError(
                status_code=404, detail=f"Device {device_id} not found"
            )

        config = rs.config()
        for c in configs:
            config.enable_stream(c.stream, c.index, c.width, c.height, c.format, c.fps)

        self.pipelines[device_id].start(config, lambda frames: self._on_frames(device_id, frames))
        self._streaming_changed(device_id)

    def stop_stream(self, device_id: str) -> None:
        """Stop streaming from a device"""
        self._require_device(device_id)
        pipeline = self.pipelines[device_id]
        profiles = pipeline.get_active_profile().get_streams()
        pipeline.stop()
        self._drop_streams(device_id, profiles)

    def _drop_streams(self, device_id: str, profiles) -> None:
        """Close the given streams' sockets"""
        self.frame_sender.close(device_id, STOPPED, [(p.stream_type(), p.stream_index()) for p in profiles])
        self._streaming_changed(device_id)

    def _on_frames(self, device_id: str, frameset):
        """Pipeline callback: send each frame"""
        # A synced set arrives as one frameset; unsynced streams (motion) as single frames
        for f in list(frameset.as_frameset()) if frameset.is_frameset() else [frameset]:
            self._send_frame(device_id, f)

    def _send_frame(self, device_id: str, frame: Any) -> None:
        """Send one frame to its stream's sockets, depth after the enabled filters"""
        if frame.get_profile().stream_type() == rs.stream.depth:
            frame = self._apply_depth_filters(device_id, frame.as_depth_frame())
        self.frame_sender.send(device_id, frame, self.colorizers[device_id])

    # =========================================================================
    # Per-Sensor Streaming API (using RealSense sensor API)
    # =========================================================================

    def _collect_sensor_frames(
        self,
        device_id: str,
        sensor_id: str,
        rs_queue: Any,
    ) -> None:
        """Thread: send each frame of the sensor's queue, until the sensor is stopped"""
        logging.info(f"[SENSOR] Frame collection thread started for {device_id}/{sensor_id}")

        try:
            while True:
                # Stop once this start's queue is gone (stopped, or replaced by a restart)
                with self.lock:
                    if self.sensor_rs_queues.get(device_id, {}).get(sensor_id) is not rs_queue:
                        break
                
                try:
                    # Wait for frame with timeout
                    frame = rs_queue.wait_for_frame(timeout_ms=1000)
                    if frame:
                        self._send_frame(device_id, frame)

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
        sensor = self._find_sensor(device_id, sensor_id)
        opened = False  # a sensor another client holds refuses open(), and is not ours to stop

        try:
            profiles = [next(p for p in sensor.get_stream_profiles() if _profile_row(p) == r) for r in rows]

            # Open sensor with ALL profiles
            sensor.open(profiles)
            opened = True
            
            # Small queue so the collector always sees fresh frames. A larger
            # capacity (the old value was 50) lets a 1.6s FIFO backlog build up
            # at 30 fps, and a slow stream then always sends 1.6s-stale frames.
            rs_queue = rs.frame_queue(2)
            
            # Start sensor
            sensor.start(rs_queue)
            

            with self.lock:
                self.sensor_rs_queues.setdefault(device_id, {})[sensor_id] = rs_queue
            
            # Initialize post-processing filters for this sensor if not already done
            self._get_or_create_processing_blocks(device_id, sensor_id, sensor)
            
            # Start frame collection thread
            threading.Thread(
                target=self._collect_sensor_frames,
                args=(device_id, sensor_id, rs_queue),
                daemon=True
            ).start()
            
            logging.info(f"[SENSOR] Started {sensor_id} with streams: {rows}")
            self._streaming_changed(device_id)

        except RealSenseError:
            raise
        except Exception as e:
            logging.error(f"[SENSOR] Failed to start {sensor_id}: {e}")
            # Clean up on failure
            if opened:
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
                if sensor.get_active_streams():
                    raise RealSenseError(status_code=409,
                                         detail="The sensor streams through the pipeline; stop it with /stream/stop")
                return
        
        # The sensor reports its open streams until close()
        stopped = sensor.get_active_streams()

        # Stop and close sensor
        try:
            sensor.stop()
            sensor.close()
        except Exception as e:
            logging.warning(f"[SENSOR] Error stopping {sensor_id}: {e}")

        with self.lock:
            if not self.sensor_rs_queues.get(device_id, True):
                del self.sensor_rs_queues[device_id]
        self._drop_streams(device_id, stopped)

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
