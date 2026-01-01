import asyncio
import threading
import time
import logging
import re
import hashlib
import urllib.request
from pathlib import Path
from typing import Dict, List, Optional, Any, Tuple, Set
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


FW_STATUS_UP_TO_DATE = "up_to_date"
FW_STATUS_OUTDATED = "outdated"
FW_STATUS_MISSING_FILE = "missing_file"
FW_STATUS_UNKNOWN = "unknown"


def _compare_versions(version_a: Optional[str], version_b: Optional[str]) -> int:
    """Return -1/0/1 comparing dotted numeric firmware versions; unknowns sort last."""
    if not version_a or not version_b:
        return 0
    try:
        parts_a = [int(p) for p in version_a.split(".")]
        parts_b = [int(p) for p in version_b.split(".")]
        # Normalize lengths
        max_len = max(len(parts_a), len(parts_b))
        parts_a += [0] * (max_len - len(parts_a))
        parts_b += [0] * (max_len - len(parts_b))
        if parts_a == parts_b:
            return 0
        return -1 if parts_a < parts_b else 1
    except Exception:
        return 0


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
        self.pc = rs.pointcloud()

        # Caches for pipeline/config reuse to reduce startup cost
        self.config_cache: Dict[str, Dict[str, rs.config]] = {}  # device -> signature -> config
        self.pipeline_cache: Dict[str, rs.pipeline] = {}  # device -> last pipeline object
        self.pipeline_signatures: Dict[str, str] = {}  # device -> active signature

        # Stop coordination
        self.stopping: Set[str] = set()

        # Store latest raw depth frames for pixel depth queries
        self.depth_frames: Dict[str, Any] = {}  # device_id -> rs.depth_frame

        # Firmware update tracking
        self._fw_updates_in_progress: Set[str] = set()

        # Firmware bundle resolution
        # Source directory paths
        self._fw_header_path = Path(__file__).resolve().parents[4] / "common" / "fw" / "firmware-version.h"
        self._fw_dir = self._fw_header_path.parent
        # Build directory path (CMake downloads firmware here)
        self._fw_build_dir = Path(__file__).resolve().parents[4] / "build" / "common" / "fw"
        self._fw_bundle_cache: Dict[str, Optional[Path]] = {}
        self._fw_cmake_path = self._fw_dir / "CMakeLists.txt"
        self._fw_base_url = "https://librealsense.realsenseai.com/Releases/RS4xx/FW"

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
        self.sensor_metadata_queues: Dict[str, Dict[str, List[Dict]]] = {}
        # Per-sensor rs.frame_queue objects: device_id -> sensor_id -> rs.frame_queue
        self.sensor_rs_queues: Dict[str, Dict[str, Any]] = {}
        # Track sensor stopping state
        self.sensor_stopping: Dict[str, Set[str]] = {}  # device_id -> set of sensor_ids

        # Initialize devices
        self.refresh_devices()

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

    def _load_header_version(self) -> Optional[str]:
        """Parse the bundled firmware version from common/fw/firmware-version.h (D4XX only)."""
        try:
            if not self._fw_header_path.exists():
                logging.error("Firmware version header not found at %s", self._fw_header_path)
                return None
            text = self._fw_header_path.read_text(encoding="utf-8", errors="ignore")
            match = re.search(r"D4XX_RECOMMENDED_FIRMWARE_VERSION\s+\"([0-9.]+)\"", text)
            return match.group(1) if match else None
        except Exception as exc:
            logging.error("Failed to parse firmware-version.h: %s", exc)
            return None

    def _get_recommended_firmware_version(self, dev: rs.device) -> Optional[str]:
        """
        Return recommended firmware version following the C++ viewer logic:
        1. Get the bundled/available firmware version from firmware-version.h
        2. Get the device's recommended version
        3. If current FW is upgradeable to bundled version, use bundled version
        4. Otherwise use device's recommended version or bundled version
        """
        bundled_version = self._load_header_version()
        
        device_recommended = None
        try:
            if dev.supports(rs.camera_info.recommended_firmware_version):
                device_recommended = dev.get_info(rs.camera_info.recommended_firmware_version)
        except RuntimeError:
            pass
        
        current_fw = None
        try:
            if dev.supports(rs.camera_info.firmware_version):
                current_fw = dev.get_info(rs.camera_info.firmware_version)
        except RuntimeError:
            pass
        
        # Follow C++ logic: if current FW is upgradeable to bundled version, use bundled
        if current_fw and bundled_version:
            if _compare_versions(current_fw, bundled_version) < 0:
                # Current FW is older than bundled, recommend bundled version
                return bundled_version
        
        # If bundled version exists, prefer it (matches C++ behavior)
        if bundled_version:
            return bundled_version
        
        # Fallback to device's recommended version
        return device_recommended

    def _is_update_device(self, dev: rs.device) -> bool:
        """Check if a device is in DFU/update mode."""
        try:
            rs.update_device(dev)
            return True
        except Exception:
            return False

    def _load_cmake_fw_sha1(self) -> Optional[str]:
        """Parse SHA1 from common/fw/CMakeLists.txt (D4XX_FW_SHA1)."""
        try:
            if not self._fw_cmake_path.exists():
                return None
            text = self._fw_cmake_path.read_text(encoding="utf-8", errors="ignore")
            m = re.search(r"set\(D4XX_FW_SHA1\s+([0-9a-fA-F]{40})\)", text)
            return m.group(1) if m else None
        except Exception:
            return None

    def _download_firmware(self, version: str, dest: Path) -> bool:
        """Download firmware file to dest and verify SHA1 only when we know it for this version."""
        try:
            url = f"{self._fw_base_url}/D4XX_FW_Image-{version}.bin"
            logging.info("Downloading firmware %s -> %s", url, dest)
            dest.parent.mkdir(parents=True, exist_ok=True)
            with urllib.request.urlopen(url) as resp, open(dest, "wb") as out:
                out.write(resp.read())
            # Only enforce SHA when the requested version matches the header's version
            header_version = self._load_header_version()
            sha_expected = self._load_cmake_fw_sha1() if header_version and header_version == version else None
            if sha_expected:
                sha = hashlib.sha1()
                with open(dest, "rb") as f:
                    for chunk in iter(lambda: f.read(8192), b""):
                        sha.update(chunk)
                if sha.hexdigest().lower() != sha_expected.lower():
                    logging.error("Firmware SHA1 mismatch for %s", dest)
                    try:
                        dest.unlink(missing_ok=True)
                    except Exception:
                        pass
                    return False
            return True
        except Exception as exc:
            logging.error("Firmware download failed: %s", exc)
            try:
                if dest.exists():
                    dest.unlink(missing_ok=True)
            except Exception:
                pass
            return False

    def _resolve_firmware_bundle(self, dev: rs.device, recommended_version: Optional[str]) -> Optional[Path]:
        """
        Map device to bundled firmware image path. Currently supports D4XX by default.
        Checks build directory first (CMake downloads firmware there), then source directory.
        Returns the path only if it exists.
        """
        if not recommended_version:
            return None

        try:
            product_line = dev.get_info(rs.camera_info.product_line) if dev.supports(rs.camera_info.product_line) else ""
        except RuntimeError:
            product_line = ""

        filename = None

        # Default to D4XX bundle naming convention used by common/fw target
        if "D4" in product_line or "D4" in (dev.get_info(rs.camera_info.name) if dev.supports(rs.camera_info.name) else ""):
            filename = f"D4XX_FW_Image-{recommended_version}.bin"

        if not filename:
            return None

        # Check build directory first (CMake downloads the recommended firmware here)
        build_path = self._fw_build_dir / filename
        if build_path.exists():
            logging.debug("Found firmware in build directory: %s", build_path)
            return build_path

        # Check source directory
        source_path = self._fw_dir / filename
        if source_path.exists():
            logging.debug("Found firmware in source directory: %s", source_path)
            return source_path

        # Try to auto-download the bundle once if version is known
        if recommended_version:
            # Download to source directory (for persistence across builds)
            ok = self._download_firmware(recommended_version, source_path)
            if ok and source_path.exists():
                return source_path

        logging.error("Firmware bundle missing for device %s (product_line=%s): looked in %s and %s", 
                      dev, product_line, build_path, source_path)
        return None

    def refresh_devices(self) -> List[DeviceInfo]:
        """Refresh the list of connected devices"""
        with self.lock:
            # Clear existing devices (that aren't streaming)
            for device_id in list(self.devices.keys()):
                if device_id not in self.pipelines:
                    del self.devices[device_id]
                    if device_id in self.device_infos:
                        del self.device_infos[device_id]

            # Discover connected devices
            for dev in self.ctx.devices:
                # Try to get device serial number - skip devices that don't support it
                # (e.g., devices in DFU/update mode)
                try:
                    if not dev.supports(rs.camera_info.serial_number):
                        logging.debug("Skipping device without serial number support (likely in DFU mode)")
                        continue
                    device_id = dev.get_info(rs.camera_info.serial_number)
                except RuntimeError as e:
                    logging.debug("Skipping device that doesn't support serial number: %s", e)
                    continue

                # Skip already known devices
                if device_id in self.devices:
                    continue

                self.devices[device_id] = dev

                # Extract device information
                try:
                    name = dev.get_info(rs.camera_info.name)
                except RuntimeError:
                    name = "Unknown Device"

                try:
                    firmware_version = dev.get_info(rs.camera_info.firmware_version)
                except RuntimeError:
                    firmware_version = None

                recommended_version = self._get_recommended_firmware_version(dev)
                fw_bundle_path = self._resolve_firmware_bundle(dev, recommended_version)
                fw_file_exists = fw_bundle_path is not None and fw_bundle_path.exists()

                firmware_status = FW_STATUS_UNKNOWN
                if firmware_version and recommended_version:
                    cmp = _compare_versions(firmware_version, recommended_version)
                    if cmp < 0:
                        firmware_status = FW_STATUS_OUTDATED if fw_file_exists else FW_STATUS_MISSING_FILE
                    else:
                        firmware_status = FW_STATUS_UP_TO_DATE
                elif recommended_version and not fw_file_exists:
                    firmware_status = FW_STATUS_MISSING_FILE

                try:
                    physical_port = dev.get_info(rs.camera_info.physical_port)
                except RuntimeError:
                    physical_port = None

                try:
                    usb_type = dev.get_info(rs.camera_info.usb_type_descriptor)
                except RuntimeError:
                    usb_type = None

                try:
                    product_id = dev.get_info(rs.camera_info.product_id)
                except RuntimeError:
                    product_id = None

                # Get sensors
                sensors = []
                for sensor in dev.sensors:
                    try:
                        sensor_name = sensor.get_info(rs.camera_info.name)
                        sensors.append(sensor_name)
                    except RuntimeError:
                        pass

                # Create device info object
                device_info = DeviceInfo(
                    device_id=device_id,
                    name=name,
                    serial_number=device_id,
                    firmware_version=firmware_version,
                    recommended_firmware_version=recommended_version,
                    firmware_status=firmware_status,
                    firmware_file_available=fw_file_exists,
                    physical_port=physical_port,
                    usb_type=usb_type,
                    product_id=product_id,
                    sensors=sensors,
                    is_streaming=device_id in self.pipelines,
                )

                self.device_infos[device_id] = device_info

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
            "recommended": device.recommended_firmware_version,
            "status": device.firmware_status or FW_STATUS_UNKNOWN,
            "file_available": device.firmware_file_available,
        }

    def update_firmware(self, device_id: str) -> Dict[str, Any]:
        """Run firmware update using bundled image; disallow when streaming or file missing."""
        # Prevent concurrent updates per-device
        with self.lock:
            if device_id in self._fw_updates_in_progress:
                raise RealSenseError(status_code=409, detail="Firmware update already in progress")
            self._fw_updates_in_progress.add(device_id)

        # Don't call refresh_devices here as it can invalidate device handles
        if device_id not in self.devices:
            with self.lock:
                self._fw_updates_in_progress.discard(device_id)
            raise RealSenseError(status_code=404, detail=f"Device {device_id} not found")

        # Clean up any stale pipeline entries first
        with self.lock:
            # Now check if target device is streaming
            if device_id in self.pipelines:
                self._fw_updates_in_progress.discard(device_id)
                raise RealSenseError(status_code=400, detail="Stop streaming before updating firmware")

        # Get device info for metadata
        current_info = self.device_infos.get(device_id)
        recommended = None
        if current_info:
            recommended = current_info.recommended_firmware_version
        
        # Need to use cached device reference to get recommended version
        cached_dev = self.devices.get(device_id)
        if cached_dev and not recommended:
            recommended = self._get_recommended_firmware_version(cached_dev)
        
        if not recommended:
            raise RealSenseError(status_code=400, detail="Cannot determine recommended firmware version")

        fw_path = self._resolve_firmware_bundle(cached_dev if cached_dev else None, recommended)
        if not fw_path or not fw_path.exists():
            logging.error("Firmware update requested but bundle not found for device %s", device_id)
            raise RealSenseError(status_code=404, detail="Firmware bundle not found for this device")

        # Read firmware image
        try:
            fw_bytes = fw_path.read_bytes()
        except Exception as exc:
            logging.error("Failed reading firmware bundle %s: %s", fw_path, exc)
            raise RealSenseError(status_code=500, detail="Unable to read firmware bundle")

        progress_holder = {"value": 0.0}
        last_emit_ts = {"value": 0.0}

        # Always emit a starting progress so the UI doesn't stay at 0% forever
        logging.info("Emitting firmware progress start for %s", device_id)
        self._emit_socket_event(
            f"firmware_progress_{device_id}",
            {"device_id": device_id, "progress": 0.0},
        )

        def _on_progress(p: float):
            progress_holder["value"] = p

            # Rate-limit progress events to avoid overwhelming the client (max ~10/sec)
            now = time.time()
            if now - last_emit_ts["value"] < 0.1 and p < 1.0:
                return
            last_emit_ts["value"] = now

            self._emit_socket_event(
                f"firmware_progress_{device_id}",
                {"device_id": device_id, "progress": float(p)},
            )

        try:
            # Convert bytes to list[int] as required by pyrealsense2 API
            fw_image = list(fw_bytes)
            
            # Get the cached device - this is the most reliable reference
            target_dev = self.devices.get(device_id)
            
            if not target_dev:
                raise RealSenseError(status_code=404, detail="Device not found for firmware update")
            
            # Get FIRMWARE_UPDATE_ID - this is the key identifier that persists across DFU transitions
            # (unlike serial_number which may not be available in DFU mode)
            firmware_update_id = None
            try:
                if target_dev.supports(rs.camera_info.firmware_update_id):
                    firmware_update_id = target_dev.get_info(rs.camera_info.firmware_update_id)
                else:
                    # Try getting from first sensor
                    sensors = target_dev.query_sensors()
                    if sensors:
                        firmware_update_id = sensors[0].get_info(rs.camera_info.firmware_update_id)
            except RuntimeError:
                pass
            
            if not firmware_update_id:
                logging.warning("Could not get firmware_update_id, will use serial_number for matching")
                firmware_update_id = device_id
            
            logging.info("Firmware Update ID for tracking: %s", firmware_update_id)
            
            # Check if device is already in update mode (is an update_device)
            update_dev = None
            try:
                update_dev = rs.update_device(target_dev)
                logging.info("Device is already in update/DFU mode")
            except Exception:
                # Device is not in update mode, need to cast to updatable and enter update state
                pass
            
            if not update_dev:
                # Device is in normal mode, need to transition to DFU mode
                logging.info("Device is in normal mode, checking firmware compatibility...")
                
                # Cast to updatable
                updatable = rs.updatable(target_dev)
                
                # Check firmware compatibility before proceeding
                try:
                    if not updatable.check_firmware_compatibility(fw_image):
                        raise RealSenseError(status_code=400, detail="Firmware is not compatible with this device")
                    logging.info("Firmware compatibility check passed")
                except Exception as e:
                    logging.warning("Firmware compatibility check failed or not supported: %s", e)
                
                # Clear the cached device reference since it will become invalid
                with self.lock:
                    if device_id in self.devices:
                        del self.devices[device_id]
                    if device_id in self.device_infos:
                        del self.device_infos[device_id]
                
                logging.info("Requesting device to enter update/DFU mode...")
                updatable.enter_update_state()
                
                # Wait for device to reconnect in DFU/update mode
                # Use firmware_update_id to match the device (as in the C++ viewer)
                max_wait_seconds = 60  # Same as C++ viewer timeout
                
                logging.info("Waiting for device to reconnect in DFU mode (timeout: %ds)...", max_wait_seconds)
                start_time = time.time()
                
                while time.time() - start_time < max_wait_seconds:
                    time.sleep(0.5)
                    try:
                        # Query for update devices
                        devs = self.ctx.query_devices()
                        for dev in devs:
                            try:
                                # Check if this is an update_device
                                candidate = rs.update_device(dev)
                                
                                # Try to match by firmware_update_id
                                try:
                                    if dev.supports(rs.camera_info.firmware_update_id):
                                        dev_fw_id = dev.get_info(rs.camera_info.firmware_update_id)
                                        if dev_fw_id == firmware_update_id:
                                            logging.info("Found DFU device with matching firmware_update_id: %s", dev_fw_id)
                                            update_dev = candidate
                                            break
                                except RuntimeError:
                                    pass
                                
                                # If only one update_device exists, assume it's ours
                                if not update_dev:
                                    update_device_count = sum(1 for d in devs if self._is_update_device(d))
                                    if update_device_count == 1:
                                        logging.info("Found single DFU device, assuming it's the target")
                                        update_dev = candidate
                                        break
                                        
                            except Exception:
                                # Not an update_device, skip
                                continue
                        
                        if update_dev:
                            break
                    except Exception as e:
                        logging.debug("Error querying devices during DFU wait: %s", e)
                        continue
                
                if not update_dev:
                    raise RealSenseError(status_code=500, detail="Device did not enter DFU mode within timeout. Please reconnect the device and try again.")
            
            # Perform the firmware update on the DFU device
            logging.info("Starting firmware update on DFU device...")
            update_dev.update(fw_image, _on_progress)
            
            logging.info("Firmware download completed, waiting for device to finalize...")
            time.sleep(3)  # Wait for DFU transition as per C++ code
            
            # Wait for original device to reconnect with new firmware
            logging.info("Waiting for device to reconnect with new firmware...")
            max_reconnect_seconds = 60
            reconnected = False
            start_time = time.time()
            
            while time.time() - start_time < max_reconnect_seconds:
                time.sleep(1)
                try:
                    devs = self.ctx.query_devices()
                    for dev in devs:
                        try:
                            # Skip update_devices (still in DFU mode)
                            if self._is_update_device(dev):
                                continue
                            
                            # Check if sensors have the matching firmware_update_id
                            sensors = dev.query_sensors()
                            if sensors:
                                try:
                                    dev_fw_id = sensors[0].get_info(rs.camera_info.firmware_update_id)
                                    if dev_fw_id == firmware_update_id:
                                        logging.info("Original device reconnected successfully (FW Update ID: %s)", dev_fw_id)
                                        reconnected = True
                                        break
                                except RuntimeError:
                                    pass
                            
                            # Also try matching by serial number
                            try:
                                if dev.supports(rs.camera_info.serial_number):
                                    sn = dev.get_info(rs.camera_info.serial_number)
                                    if sn == device_id:
                                        logging.info("Original device reconnected successfully (Serial: %s)", sn)
                                        reconnected = True
                                        break
                            except RuntimeError:
                                pass
                        except Exception:
                            continue
                    
                    if reconnected:
                        break
                except Exception as e:
                    logging.debug("Error querying devices during reconnect wait: %s", e)
                    continue
            
            if not reconnected:
                logging.warning("Device did not reconnect within timeout, but update may have succeeded")
                # Don't fail here - the update itself completed successfully
        except Exception as exc:
            logging.error("Firmware update failed for %s: %s", device_id, exc)
            # Emit failure event
            self._emit_socket_event(
                f"firmware_update_failed_{device_id}",
                {"device_id": device_id, "error": str(exc)},
            )
            raise RealSenseError(status_code=500, detail=f"Firmware update failed: {str(exc)}")
        finally:
            with self.lock:
                self._fw_updates_in_progress.discard(device_id)

        # Refresh device list to pick up new FW version
        self.refresh_devices()
        updated_info = self.device_infos.get(device_id)

        # Ensure the UI receives a completion progress update even if the device callback didn't
        logging.info("Emitting firmware progress completion for %s", device_id)
        self._emit_socket_event(
            f"firmware_progress_{device_id}",
            {"device_id": device_id, "progress": 1.0},
        )

        # Emit success event
        logging.info("Emitting firmware update success for %s", device_id)
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

    def reset_device(self, device_id: str) -> bool:
        """Reset a specific device by ID"""
        if device_id not in self.devices:
            self.refresh_devices()
            if device_id not in self.devices:
                raise RealSenseError(
                    status_code=404, detail=f"Device {device_id} not found"
                )

        dev = self.devices[device_id]
        try:
            dev.hardware_reset()
            return True
        except RuntimeError as e:
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
                )
                options.append(option_info)
            except RuntimeError as e:
                # Skip options that can't be read
                pass

        return options

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
            # Start frame collection thread
            threading.Thread(
                target=self._collect_frames,
                args=(device_id, align_processor),
                daemon=True,
            ).start()
            # Update device info
            if device_id in self.device_infos:
                self.device_infos[device_id].is_streaming = True
            threading.Thread(
                target=self.metadata_socket_server.start_broadcast,
                args=(device_id,),
                daemon=True,
            ).start()
            timings['thread_start'] = time.perf_counter() - t6
            timings['total'] = time.perf_counter() - t0
            print(f"[TIMING] start_stream timings for {device_id}: {timings}")
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
                self.metadata_socket_server.stop_broadcast()
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

    def _collect_frames(self, device_id: str, align_processor=None):
        """Thread function to collect frames from the pipeline"""
        print(f"[INFO] Frame collection thread started for device {device_id}")
        print(f"[INFO] Active streams: {self.active_streams.get(device_id, set())}")
        
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
                            points = None
                            
                            # Use the rs_stream enum directly for comparison
                            if rs_stream == rs.stream.depth:
                                frame_data = frames.get_depth_frame()
                                if frame_data:
                                    # Store raw depth frame for pixel queries
                                    self.depth_frames[device_id] = frame_data
                                    colorized = colorizer.colorize(frame_data)
                                    frame = np.asanyarray(colorized.get_data())
                                    if self.is_pointcloud_enabled.get(device_id, False):
                                        points = self.pc.calculate(frame_data)
                            elif rs_stream == rs.stream.color:
                                frame_data = frames.get_color_frame()
                                if frame_data:
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
                                "timestamp": frame_data.get_timestamp(),
                                "frame_number": frame_data.get_frame_number(),
                                "width": getattr(frame_data, "get_width", lambda: 640)() or 640,
                                "height": getattr(frame_data, "get_height", lambda: 480)() or 480,
                            }

                            if rs_stream == rs.stream.gyro or rs_stream == rs.stream.accel:
                                if motion_json_data:
                                    metadata["motion_data"] = motion_json_data

                            if points:
                                v, t = points.get_vertices(), points.get_texture_coordinates()
                                verts = np.asanyarray(v).view(np.float32).reshape(-1, 3)
                                verts = verts[verts[:, 2] >= 0.03]  # Filter out z < 0.03
                                metadata["point_cloud"] = {"vertices": verts, "texture_coordinates": []}

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
        
        # All profiles must have same FPS for hardware sync
        fps_values = set(p.fps() for p in profiles)
        if len(fps_values) > 1:
            profile_details = [f"{p.stream_type().name}@{p.fps()}fps" for p in profiles]
            raise RealSenseError(
                status_code=400,
                detail=f"Incompatible FPS values. All streams on same sensor must use same FPS. "
                       f"Requested: {', '.join(profile_details)}"
            )

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
                    
                    # Process frame based on stream type
                    processed_frame = None
                    metadata = {
                        "timestamp": frame.get_timestamp(),
                        "frame_number": frame.get_frame_number(),
                    }
                    
                    if "depth" in frame_stream_name:
                        # Store depth frame (cast from raw) for pixel queries and get_depth_range
                        depth_frame = frame.as_depth_frame()
                        self.depth_frames[device_id] = depth_frame
                        colorized = colorizer.colorize(depth_frame)
                        processed_frame = np.asanyarray(colorized.get_data())
                        metadata["width"] = depth_frame.get_width()
                        metadata["height"] = depth_frame.get_height()
                        
                    elif "color" in frame_stream_name:
                        processed_frame = np.asanyarray(frame.get_data())
                        metadata["width"] = frame.as_video_frame().get_width()
                        metadata["height"] = frame.as_video_frame().get_height()
                        
                    elif "infrared" in frame_stream_name:
                        processed_frame = np.asanyarray(frame.get_data())
                        metadata["width"] = frame.as_video_frame().get_width()
                        metadata["height"] = frame.as_video_frame().get_height()
                        
                    elif "gyro" in frame_stream_name or "accel" in frame_stream_name:
                        motion_frame = frame.as_motion_frame()
                        motion_data = motion_frame.get_motion_data()
                        metadata["motion_data"] = {
                            "x": float(motion_data.x),
                            "y": float(motion_data.y),
                            "z": float(motion_data.z),
                        }
                        # Create visualization frame
                        processed_frame = np.zeros((120, 320, 3), dtype=np.uint8)
                        cv2.putText(processed_frame, f"X: {motion_data.x:.3f}", (10, 30), 
                                   cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 100, 100), 1)
                        cv2.putText(processed_frame, f"Y: {motion_data.y:.3f}", (10, 60), 
                                   cv2.FONT_HERSHEY_SIMPLEX, 0.6, (100, 255, 100), 1)
                        cv2.putText(processed_frame, f"Z: {motion_data.z:.3f}", (10, 90), 
                                   cv2.FONT_HERSHEY_SIMPLEX, 0.6, (100, 100, 255), 1)
                        metadata["width"] = 320
                        metadata["height"] = 120
                    
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
            
            # Open sensor with ALL profiles
            sensor.open(profiles)
            
            # Create frame queue
            rs_queue = rs.frame_queue(50, keep_frames=True)
            
            # Start sensor
            sensor.start(rs_queue)
            
            # Collect stream types
            stream_types = [c.stream_type for c in configs]
            
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
            
            # Start frame collection thread
            threading.Thread(
                target=self._collect_sensor_frames,
                args=(device_id, sensor_id, rs_queue, stream_types),
                daemon=True
            ).start()
            
            # Start metadata broadcast if not already running
            if not self.metadata_socket_server._is_broadcasting:
                threading.Thread(
                    target=self.metadata_socket_server.start_broadcast,
                    args=(device_id,),
                    daemon=True,
                ).start()
            
            logging.info(f"[SENSOR] Started {sensor_id} with streams: {stream_types}")
            
            # Return status with backward compat fields
            first_config = configs[0]
            return SensorStreamStatus(
                sensor_id=sensor_id,
                name=sensor_name,
                is_streaming=True,
                stream_type=first_config.stream_type,  # Backward compat
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
        with self.lock:
            if device_id in self.sensor_streams:
                self.sensor_streams[device_id].pop(sensor_id, None)
                if not self.sensor_streams[device_id]:
                    del self.sensor_streams[device_id]
                    self.streaming_mode[device_id] = "idle"
            
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
                raise RealSenseError(
                    status_code=503,
                    detail=f"No frames available for sensor {sensor_id}"
                )
            
            return queue[-1]

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


