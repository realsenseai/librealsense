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
import socketio

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
                    # Provide default values for non-video stream profiles
                    fmt = "combined_motion"
                    width, height = 640, 480
                    fps = 30
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
                try:
                    if len(stream_name_list) > 1:
                        stream_index = int(stream_name_list[1])
                        config.enable_stream(
                            stream_type,
                            stream_index,
                            stream_config.resolution.width,
                            stream_config.resolution.height,
                            format_type,
                            stream_config.framerate,
                        )
                    elif format_type == rs.format.combined_motion:
                        config.enable_stream(stream_type)
                    else:
                        config.enable_stream(
                            stream_type,
                            stream_config.resolution.width,
                            stream_config.resolution.height,
                            format_type,
                            stream_config.framerate,
                        )
                    active_streams.add(stream_config.stream_type)
                except RuntimeError as e:
                    raise RealSenseError(
                        status_code=400, detail=f"Failed to enable stream: {str(e)}"
                    )
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
        """Get the streaming status for a device"""
        if device_id not in self.devices:
            self.refresh_devices()
            if device_id not in self.devices:
                raise RealSenseError(
                    status_code=404, detail=f"Device {device_id} not found"
                )

        is_streaming = device_id in self.pipelines
        active_streams = list(self.active_streams.get(device_id, set()))
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
        """Get the latest frame from a specific stream"""
        with self.lock:
            if device_id not in self.frame_queues:
                raise RealSenseError(
                    status_code=400, detail=f"Device {device_id} is not streaming"
                )

            if stream_type not in self.frame_queues[device_id]:
                available_streams = list(self.frame_queues[device_id].keys())
                raise RealSenseError(
                    status_code=400, detail=f"Stream type '{stream_type}' is not active. Available: {available_streams}"
                )

            queue = self.frame_queues[device_id][stream_type]
            if len(queue) == 0:
                raise RealSenseError(
                    status_code=503,
                    detail=f"No frames available for stream {stream_type}",
                )

            # Return the most recent frame
            return queue[-1]

    def get_latest_metadata(self, device_id: str, stream_type: str) -> Dict:
        """Get the latest METADATA dictionary from a specific stream"""
        stream_key = stream_type.lower()  # Use consistent key format
        with self.lock:
            # Check if device is supposed to be streaming
            if device_id not in self.pipelines or device_id not in self.metadata_queues:
                if (
                    device_id not in self.pipelines
                    and self.get_stream_status(device_id).is_streaming == False
                ):
                    raise RealSenseError(
                        status_code=400, detail=f"Device {device_id} is not streaming."
                    )
                else:
                    raise RealSenseError(
                        status_code=500,
                        detail=f"Inconsistent state for device {device_id}. Assumed not streaming.",
                    )

            # Check if the specific stream is active and has a queue
            if stream_key not in self.metadata_queues.get(device_id, {}):
                active_keys = list(self.active_streams.get(device_id, []))
                raise RealSenseError(
                    status_code=400,
                    detail=f"Stream type '{stream_key}' is not active for device {device_id}. Active streams: {active_keys}",
                )

            queue = self.metadata_queues[device_id][stream_key]
            if queue.__len__() == 0:
                return {}
            if not queue:
                # Stream is active, but no metadata arrived yet or queue was cleared
                raise RealSenseError(
                    status_code=503,
                    detail=f"No metadata available yet for stream '{stream_key}' on device {device_id}. Please wait.",
                )

            # Return the most recent metadata dictionary (last element)
            return queue[-1]

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

