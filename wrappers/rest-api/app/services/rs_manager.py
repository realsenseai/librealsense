import threading
import time
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


class RealSenseManager:
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

        self.metadata_socket_server = MetadataSocketServer(sio, self)

        # Initialize devices
        self.refresh_devices()

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
                device_id = dev.get_info(rs.camera_info.serial_number)

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
                    physical_port=physical_port,
                    usb_type=usb_type,
                    product_id=product_id,
                    sensors=sensors,
                    is_streaming=device_id in self.pipelines,
                )

                self.device_infos[device_id] = device_info

            return list(self.device_infos.values())

    def get_devices(self) -> List[DeviceInfo]:
        """Get all connected devices"""
        return self.refresh_devices()

    def get_device(self, device_id: str) -> DeviceInfo:
        """Get a specific device by ID"""
        devices = self.get_devices()
        for device in devices:
            if device.device_id == device_id:
                return device
        raise RealSenseError(status_code=404, detail=f"Device {device_id} not found")

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
        option_value = None
        for option in sensor.get_supported_options():
            if option.name.lower() == option_id.lower():
                option_value = option
                break

        if option_value is None:
            raise RealSenseError(
                status_code=404, detail=f"Option {option_id} not found"
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
    ) -> StreamStatus:
        """Start streaming from a device"""
        self.refresh_devices()
        if device_id not in self.devices:
            if device_id not in self.devices:
                raise RealSenseError(
                    status_code=404, detail=f"Device {device_id} not found"
                )

        # Stop existing stream if running
        if device_id in self.pipelines:
            return StreamStatus(
                device_id=device_id,
                is_streaming=True,
                active_streams=list(self.active_streams[device_id]),
            )

        # Initialize pipeline and config
        pipeline = rs.pipeline(self.ctx)
        config = rs.config()
        config.enable_device(device_id)

        # Track active stream types
        active_streams = set()

        # Enable streams based on configuration
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

            # Get format from string
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

            # Enable stream
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

        # Start streaming
        try:
            pipeline_profile = pipeline.start(config)

            # Set up align if requested
            align_processor = None
            if align_to:
                # Get stream type from string
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
                self.active_streams[device_id] = active_streams
                self.frame_queues[device_id] = {
                    stream_type: [] for stream_type in active_streams
                }
                self.metadata_queues[device_id] = {
                    stream_key: [] for stream_key in active_streams
                }

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
            if device_id not in self.pipelines:
                return StreamStatus(
                    device_id=device_id, is_streaming=False, active_streams=[]
                )

            try:
                self.metadata_socket_server.stop_broadcast()
                self.pipelines[device_id].stop()

                # Clean up resources
                del self.pipelines[device_id]
                if device_id in self.configs:
                    del self.configs[device_id]
                if device_id in self.active_streams:
                    active_streams = list(self.active_streams[device_id])
                    del self.active_streams[device_id]
                else:
                    active_streams = []
                if device_id in self.frame_queues:
                    del self.frame_queues[device_id]
                if device_id in self.metadata_queues:
                    del self.metadata_queues[device_id]

                # Update device info
                if device_id in self.device_infos:
                    self.device_infos[device_id].is_streaming = False

                return StreamStatus(
                    device_id=device_id,
                    is_streaming=False,
                    active_streams=active_streams,
                )
            except RuntimeError as e:
                raise RealSenseError(
                    status_code=500, detail=f"Failed to stop streaming: {str(e)}"
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

        return StreamStatus(
            device_id=device_id,
            is_streaming=is_streaming,
            active_streams=active_streams,
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
                        if device_id in self.device_infos:
                            self.device_infos[device_id].is_streaming = False
            except Exception:
                pass
