# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Legacy rs.pipeline streaming path (/stream endpoints)."""

import threading
import time
import logging
from typing import Callable, Deque, Dict, List, Optional, Any, Tuple, Set
import pyrealsense2 as rs
import numpy as np
import cv2
from app.core.errors import RealSenseError
from app.models.stream import PointCloudStatus, StreamConfig, StreamStatus, Resolution


class PipelineStreamingMixin:
    """Mixed into RealSenseManager; see rs_manager.py."""

    def _make_signature(self, configs: List[StreamConfig], align_to: Optional[str]) -> str:
        """Deterministic signature for a stream start request."""
        parts = []
        for cfg in sorted(configs, key=lambda c: (c.stream_type.lower(), c.sensor_id, c.resolution.width, c.resolution.height, c.framerate, c.format.lower())):
            parts.append(
                f"{cfg.stream_type.lower()}|{cfg.format.lower()}|{cfg.resolution.width}x{cfg.resolution.height}@{cfg.framerate}|sensor:{cfg.sensor_id}"
            )
        align_part = align_to.lower() if align_to else "none"
        return ";".join(parts) + f"|align:{align_part}"

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
                            self.last_frames.setdefault(device_id, {})[active_stream.lower()] = {
                                "frame": frame_data,
                                "shown": frame if rs_stream == rs.stream.depth else None,
                                "motion": metadata.get("motion_data"),
                            }
                            
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
