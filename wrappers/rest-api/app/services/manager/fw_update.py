# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Firmware update over DFU: entering recovery, flashing, waiting for the device back."""

import time
import logging
from typing import Callable, Deque, Dict, List, Optional, Any, Tuple, Set
import pyrealsense2 as rs
from app.core.errors import RealSenseError
from app.services import firmware as fw


class FirmwareUpdateMixin:
    """Mixed into RealSenseManager; see rs_manager.py."""

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
            self._fail_fw_update(device_id, str(exc))
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
                self._fail_fw_update(device_id, str(exc))
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
            job = self._fw_jobs.get(device_id)
            if job:
                job.done({"firmware_version": updated_info.firmware_version})

            return {
                "device_id": device_id,
                "job_id": job.id if job else None,
                "progress": progress_holder["value"],
                "firmware_version": updated_info.firmware_version,
                "status": "success",
            }
        except RealSenseError as exc:
            self._fail_fw_update(device_id, str(exc.detail))
            raise
        finally:
            with self.lock:
                self._fw_updates_in_progress.discard(device_id)
                self._fw_jobs.pop(device_id, None)

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
            self._fw_jobs[device_id] = self.jobs.create("firmware_update", device_id)

    def _fail_fw_update(self, device_id: str, error: str) -> None:
        """Report a failed update on the legacy per-device channel and on its job."""
        self._emit_socket_event(f"firmware_update_failed_{device_id}", {"device_id": device_id, "error": error})
        job = self._fw_jobs.get(device_id)
        if job:
            job.fail(error)

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
            job = self._fw_jobs.get(device_id)
            if job:
                job.progress(p, phase)
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
            self._fail_fw_update(device_id, msg)
            raise RealSenseError(status_code=500, detail=msg)
        logging.warning("Device did not reconnect within timeout, but update may have succeeded")
