# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Sensor enumeration, options, post-processing filters, colorizer, advanced mode, HWM."""

import struct
import logging
from typing import Callable, Deque, Dict, List, Optional, Any, Tuple, Set
import pyrealsense2 as rs
from app.core.errors import RealSenseError
from app.services import advanced_mode, options
from app.models.sensor import Sensor, SensorInfo, SupportedStreamProfile
from app.models.option import Option, OptionInfo


class ControlsMixin:
    """Mixed into RealSenseManager; see rs_manager.py."""

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
                        "default": None,
                    }
                if profile.is_default():
                    supported_stream_profiles[stream_type]["default"] = {
                        "resolution": (width, height), "fps": fps, "format": fmt,
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
                    default=stream_data["default"],
                )
                stream_profiles_list.append(stream_profile)

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

    def get_sensor_options(self, device_id: str, sensor_id: str) -> List[OptionInfo]:
        """Get all options for a sensor"""
        sensor = self._find_sensor(device_id, sensor_id)
        with self.option_lock(device_id):
            return options.all_options(sensor)

    def _get_or_create_processing_blocks(self, device_id: str, sensor_id: str, sensor) -> List[Dict[str, Any]]:
        """Get or create post-processing filter blocks for a sensor.
        
        Uses the SDK's get_recommended_filters() to get sensor-appropriate filters.
        Returns list of dicts: { "filter": rs.filter, "name": str, "enabled": bool, "default_enabled": float }
        """
        if device_id not in self.processing_blocks:
            self.processing_blocks[device_id] = {}
        
        if sensor_id not in self.processing_blocks[device_id]:
            filters = []
            remembered = self.settings.get().post_processing.filter_state.get(self._filter_state_key(device_id, sensor_id), {})
            try:
                recommended = sensor.get_recommended_filters()
            except RuntimeError:
                recommended = []  # sensor doesn't support get_recommended_filters
            for f in recommended:
                try:
                    filter_name = f.get_info(rs.camera_info.name)
                except RuntimeError:
                    filter_name = "Unknown Filter"
                if filter_name == "HDR Merge" and not sensor.supports(rs.option.sequence_id):
                    continue  # the legacy viewer skips it too: nothing to merge without sequence ids
                self._apply_filter_presets(device_id, filter_name, f)
                default_enabled = self._default_filter_enabled(sensor, filter_name)
                filters.append({
                    "filter": f,
                    "name": filter_name,
                    "enabled": remembered.get(filter_name, default_enabled),
                    "default_enabled": float(default_enabled),
                })

            self.processing_blocks[device_id][sensor_id] = filters
        
        return self.processing_blocks[device_id][sensor_id]

    @staticmethod
    def _filter_state_key(device_id: str, sensor_id: str) -> str:
        return f"{device_id}/{sensor_id.split('-')[-1]}"

    def _default_filter_enabled(self, sensor, filter_name: str) -> bool:
        """The legacy viewer's defaults (subdevice-model.cpp): hole filling, sequence id, rotation
        and threshold start off, decimation only on depth; performance mode turns everything off."""
        if self.settings.get().post_processing.performance_mode:
            return False
        off = {"Hole Filling Filter", "Filter By Sequence id", "Rotation Filter", "Threshold Filter"}
        if filter_name in off:
            return False
        if filter_name == "Decimation Filter" and sensor.is_color_sensor():
            return False
        return True

    def _apply_filter_presets(self, device_id: str, filter_name: str, f) -> None:
        """D405 is a short-range camera: its threshold filter starts at 5 cm - 4 m."""
        if filter_name != "Threshold Filter":
            return
        dev = self.devices.get(device_id)
        try:
            if dev is not None and dev.get_info(rs.camera_info.product_id) == "0B5B":
                f.set_option(rs.option.min_distance, 0.05)
                f.set_option(rs.option.max_distance, 4.0)
        except RuntimeError:
            pass

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
        """Bypass or apply one filter of the sensor, and remember the choice across runs."""
        self._filters_by_name(device_id, sensor_id)[filter_name]["enabled"] = bool(enabled)
        key = self._filter_state_key(device_id, sensor_id)
        self.settings.update({"post_processing": {"filter_state": {key: {filter_name: bool(enabled)}}}})

    def get_colorizer_options(self, device_id: str) -> List[OptionInfo]:
        """The device colorizer's controls."""
        self._require_device(device_id)
        return options.all_options(self.colorizers[device_id])

    def set_colorizer_option(self, device_id: str, field: str, value: float) -> OptionInfo:
        """Set one colorizer option."""
        self._require_device(device_id)
        return options.set_option(self.colorizers[device_id], field, value)

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
        with self.option_lock(device_id):
            return advanced_mode.controls(self._require_device(device_id))

    def set_advanced_control(self, device_id: str, group: str, field: str, value: float) -> OptionInfo:
        with self.option_lock(device_id):
            return advanced_mode.set_control(self._require_device(device_id), group, field, value)

    def get_roi(self, device_id: str, sensor_id: str) -> Dict[str, Any]:
        """The auto-exposure region of interest of a sensor that has one."""
        sensor = self._find_sensor(device_id, sensor_id)
        if not sensor.is_roi_sensor():
            return {"supported": False}
        with self.option_lock(device_id):
            roi = sensor.as_roi_sensor().get_region_of_interest()
        return {"supported": True, "min_x": roi.min_x, "min_y": roi.min_y, "max_x": roi.max_x, "max_y": roi.max_y}

    def set_roi(self, device_id: str, sensor_id: str, min_x: int, min_y: int, max_x: int, max_y: int) -> Dict[str, Any]:
        sensor = self._find_sensor(device_id, sensor_id)
        if not sensor.is_roi_sensor():
            raise RealSenseError(status_code=400, detail=f"Sensor {sensor_id} has no region of interest")
        roi = rs.region_of_interest()
        roi.min_x, roi.min_y, roi.max_x, roi.max_y = min(min_x, max_x), min(min_y, max_y), max(min_x, max_x), max(min_y, max_y)
        with self.option_lock(device_id):
            sensor.as_roi_sensor().set_region_of_interest(roi)
        return self.get_roi(device_id, sensor_id)

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
        with self.option_lock(device_id):
            name = next(o.name for o in sensor.get_supported_options() if o.name.lower() in wanted)
            applied = options.set_option(sensor, name, value)
        self.options_poller.note_written(device_id, sensor_id, name, applied.current_value)
        return applied

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
