# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Frame/metadata queues shared by both streaming paths, point cloud, depth queries."""

import asyncio
import time
import logging
from typing import Callable, Deque, Dict, List, Optional, Any, Tuple, Set
import pyrealsense2 as rs
import numpy as np
from app.core.errors import RealSenseError
from app.services import snapshot
from app.models.stream import PointCloudStatus, StreamConfig, StreamStatus, Resolution


class FramesMixin:
    """Mixed into RealSenseManager; see rs_manager.py."""

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

    def get_point_cloud_geometry(self, device_id: str, texture: Optional[str]) -> Dict[str, Any]:
        """Camera geometry of the running streams for the client-side point cloud."""
        from app.services import point_cloud_geometry
        dev = self._require_device(device_id)
        streaming = []
        with self.lock:
            entries = [(sid, list(info.get("configs", []))) for sid, info in self.sensor_streams.get(device_id, {}).items()
                       if info.get("is_streaming")]
        for sensor_id, configs in entries:
            sensor, _ = self._get_sensor_by_id(device_id, sensor_id)
            streaming.extend((sensor, config) for config in configs)
        return point_cloud_geometry.geometry(dev, streaming, texture)

    def export_point_cloud(self, device_id: str, mesh: bool, normals: bool, binary: bool) -> bytes:
        """The newest depth frame as PLY (viewer.cpp export_to_ply through rs.save_to_ply)."""
        from app.services import ply_export
        self._require_device(device_id)
        return ply_export.export_depth_to_ply(self.depth_frames.get(device_id), mesh, normals, binary)

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

    def _publish_frames(self, device_id: str, stream_types) -> None:
        """Bump the arrival counter for each stream and wake async waiters.

        Runs on collection threads; the events are set via the main loop so
        waiters need no executor thread.
        """
        loop = type(self)._main_loop
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
            "received_at": time.time(),
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

    def snapshot(self, device_id: str, stream_type: str) -> Tuple[str, bytes]:
        """Zip of the newest frame of a stream: PNG as shown, raw pixels, attributes CSV."""
        entry = self.last_frames.get(device_id, {}).get(stream_type.lower())
        if entry is None:
            raise RealSenseError(status_code=404, detail=f"No frame captured yet for stream {stream_type}")
        frame = entry["frame"]
        info = self._build_viewer_info(frame)
        info.pop("received_at", None)
        metadata = {**info, **self._get_frame_metadata(frame, device_id)}
        raw = None if entry["motion"] else np.asanyarray(frame.get_data())
        base, data = snapshot.build_snapshot(
            stream_type.lower(), int(info["frame_number"]), raw, str(info["pixel_format"]),
            entry["shown"], metadata, entry["motion"],
        )
        return f"{device_id}_{base}.zip", data

    def get_max_usable_range(self, device_id: str) -> Dict[str, Any]:
        """The depth sensor's max-usable-range estimate, when the option is on (D400).

        Legacy readout (stream-model.cpp): shown clamped to 1.5-9 m in 1.5 m steps.
        """
        dev = self._require_device(device_id)
        depth = next((s for s in dev.sensors if s.is_depth_sensor()), None)
        if depth is None or not depth.supports(rs.option.enable_max_usable_range):
            return {"supported": False, "enabled": False, "range_m": None}
        enabled = depth.get_option(rs.option.enable_max_usable_range) == 1.0
        range_m = None
        if enabled and depth.is_max_usable_range_sensor():
            raw = depth.as_max_usable_range_sensor().get_max_usable_depth_range()
            range_m = int(min(max(raw, 1.5), 9.0) / 1.5) * 1.5
        return {"supported": True, "enabled": enabled, "range_m": range_m}

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
