# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Per-sensor streaming path (rs.sensor open/start with a frame queue per sensor)."""

import threading
import logging
from collections import defaultdict, deque
from typing import Callable, Deque, Dict, List, Optional, Any, Tuple, Set
import pyrealsense2 as rs
import time
import numpy as np
import cv2
from app.core.errors import RealSenseError
from app.models.stream import PointCloudStatus, StreamConfig, StreamStatus, Resolution
from app.models.sensor_streaming import SensorStreamConfig, SensorStreamStatus
from datetime import datetime

# Recent color frames retained per device for texturing the 3D point cloud.
# Five frames at 30 fps covers the typical depth/color SDK-latency offset
# (~one capture interval); the picker selects the entry whose timestamp is
# closest to the depth frame being processed.
COLOR_FRAME_HISTORY = 5


class SensorStreamingMixin:
    """Mixed into RealSenseManager; see rs_manager.py."""

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

    DEPTH_FRAME_MIN_INTERVAL_S = 1.0 / 15  # the 3D view does not need more than this
    DEAD_STREAM_TIMEOUTS = 10  # consecutive 1 s waits with no frame ever -> the handle is dead

    def _emit_depth_frame(self, device_id: str, depth_frame) -> None:
        """Binary ``depth_frame`` socket event with the z16 pixels of a (filtered) depth frame."""
        now = time.monotonic()
        last = getattr(self, "_last_depth_emit", {})
        if now - last.get(device_id, 0.0) < self.DEPTH_FRAME_MIN_INTERVAL_S:
            return
        last[device_id] = now
        self._last_depth_emit = last
        profile = depth_frame.get_profile().as_video_stream_profile()
        self._emit_socket_event("depth_frame", {
            "device_id": device_id,
            "width": profile.width(),
            "height": profile.height(),
            "frame_number": depth_frame.get_frame_number(),
            "units": float(depth_frame.get_units()) if hasattr(depth_frame, "get_units") else 0.001,
            "format": "z16",
            "data": np.ascontiguousarray(np.asanyarray(depth_frame.get_data()), dtype=np.uint16).tobytes(),
        })

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
                # The 3D view unprojects on the GPU from the raw depth image (like the
                # legacy viewer's pointcloud-gl); ship the z16 frame itself, throttled.
                self._emit_depth_frame(device_id, depth_frame)

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
        # "Hardware Size" is the sensor's, even when a filter (decimation) shrank the frame
        try:
            raw_profile = frame.get_profile().as_video_stream_profile()
            metadata["hardware_width"], metadata["hardware_height"] = raw_profile.width(), raw_profile.height()
        except Exception:
            pass
        # Legacy "Frame Drops per Second" dashboard figures, per stream
        stats = getattr(self, "_frame_stats", None)
        if stats is None:
            from app.services.frame_stats import FrameStats
            stats = self._frame_stats = FrameStats()
        try:
            metadata["stats"] = stats.observe(f"{device_id}:{frame_stream_name}", float(metadata["timestamp"]),
                                              float(metadata.get("hardware_fps") or info_source.get_profile().fps()))
        except Exception as exc:
            logging.debug("frame stats skipped: %s", exc)
        self.last_frames.setdefault(device_id, {})[frame_stream_name] = {
            "frame": info_source,
            "shown": processed_frame if "depth" in frame_stream_name else None,
            "motion": metadata.get("motion_data"),
        }
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

        colorizer = self.colorizers[device_id]
        timeouts_in_a_row = 0
        got_any_frame = False

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
                    timeouts_in_a_row = 0
                    got_any_frame = True
                    if not frame or sensor_info.get("paused"):
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

                    self._publish_frames(device_id, (target_stream_type,))

                except Exception as e:
                    if self.is_device_lost_error(e):
                        self.device_lost(device_id, str(e)[:120])
                        break
                    if "did not arrive" in str(e).lower() or "timeout" in str(e).lower():
                        timeouts_in_a_row += 1
                        # A sensor the SDK says is streaming but that never produced a frame is
                        # a dead handle (seen after the machine slept); ten seconds is plenty.
                        if not got_any_frame and timeouts_in_a_row >= self.DEAD_STREAM_TIMEOUTS:
                            self.device_lost(device_id, f"{sensor_id} started but no frame arrived in {timeouts_in_a_row}s")
                            break
                    if "timeout" not in str(e).lower():
                        # First occurrence per sensor and message at WARNING, repeats at DEBUG
                        seen = getattr(self, "_frame_errors_seen", None)
                        if seen is None:
                            seen = self._frame_errors_seen = set()
                        key = (sensor_id, str(e)[:120])
                        if key not in seen:
                            seen.add(key)
                            logging.warning(f"[SENSOR] Frame collection error on {sensor_id}: {e!r}")
                        else:
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
        
        # Already streaming? The same configuration is a no-op; a different one restarts the
        # sensor through stop_sensor so its collector thread and queues wind down first.
        # (Stopping and closing the SDK sensor from here, under the manager lock and with the
        # collector still waiting on its queue, left the SDK convinced the sensor was still
        # open: "UVC device is already opened!" on the next open().)
        with self.lock:
            info = self.sensor_streams.get(device_id, {}).get(sensor_id)
            running = list(info.get("configs", [])) if info and info.get("is_streaming") else None
        if running is not None:
            wanted = [c.model_dump() for c in configs]
            current = [c.model_dump() if hasattr(c, "model_dump") else c for c in running]
            if current == wanted:
                logging.info(f"[SENSOR] {sensor_id} already streams the requested configuration")
                return self.get_sensor_status(device_id, sensor_id)
            logging.info(f"[SENSOR] {sensor_id} streams a different configuration - restarting")
            self.stop_sensor(device_id, sensor_id)

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
                    "paused": False,
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
            collector = threading.Thread(
                target=self._collect_sensor_frames,
                args=(device_id, sensor_id, rs_queue, stream_types),
                daemon=True
            )
            collector.start()
            self._collector_threads[sensor_id] = collector
            
            # Start per-device metadata broadcast (no-op if already running for this device).
            self.metadata_socket_server.start_broadcast(device_id)
            
            logging.info(f"[SENSOR] Started {sensor_id} with streams: {stream_types}")
            
            # Return status with backward compat fields
            first_config = configs[0]
            return self._announce_sensor_status(device_id, SensorStreamStatus(
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
            ))
            
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
            for st in stopped_stream_types:
                self.last_frames.get(device_id, {}).pop(st.lower(), None)

        # Stop the per-device metadata broadcaster once the last sensor on this
        # device has stopped.
        if last_sensor_stopped:
            self.metadata_socket_server.stop_broadcast(device_id)

        logging.info(f"[SENSOR] Stopped {sensor_id}")
        
        return self._announce_sensor_status(device_id, SensorStreamStatus(
            sensor_id=sensor_id,
            name=sensor_name,
            is_streaming=False,
        ))

    def _announce_sensor_status(self, device_id: str, status: SensorStreamStatus) -> SensorStreamStatus:
        """Tell every client a sensor started, stopped or paused - whoever caused it (another
        client, a calibration job restoring the stream, a lost device), so no UI keeps showing
        a Stop button for a sensor the server no longer streams."""
        self._emit_socket_event("sensor_status", {"device_id": device_id, "sensor_id": status.sensor_id,
                                                  "status": status.model_dump(mode="json")})
        return status

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
            # A client that arrives mid-stream (a reload, a second tab) learns from here what
            # the sensor is sending; without the stream list it cannot show a single tile.
            configs = info.get("configs") or []
            stream_types = info.get("stream_types") or []
            first = configs[0] if configs else None

            return SensorStreamStatus(
                sensor_id=sensor_id,
                name=info.get("name", sensor_name),
                is_streaming=info.get("is_streaming", False),
                paused=info.get("paused", False),
                stream_type=info.get("stream_type") or (stream_types[0] if stream_types else None),
                stream_types=list(stream_types),
                streams=list(configs),
                resolution=Resolution(width=resolution[0], height=resolution[1]) if resolution
                    else (first.resolution if first else None),
                framerate=info.get("framerate") or (first.framerate if first else None),
                format=info.get("format") or (first.format if first else None),
                error=info.get("error"),
                started_at=info.get("started_at"),
            )

    def set_sensor_paused(self, device_id: str, sensor_id: str, paused: bool) -> SensorStreamStatus:
        """Hold back (or release) a streaming sensor's frames; the sensor itself keeps running."""
        with self.lock:
            info = self.sensor_streams.get(device_id, {}).get(sensor_id)
            if not info or not info.get("is_streaming"):
                raise RealSenseError(status_code=409, detail=f"Sensor {sensor_id} is not streaming")
            info["paused"] = paused
        return self._announce_sensor_status(device_id, self.get_sensor_status(device_id, sensor_id))

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