# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import asyncio
import threading
from collections import defaultdict, deque
from typing import Callable, Deque, Dict, List, Optional, Any, Tuple, Set
import pyrealsense2 as rs
from app.core.errors import RealSenseError  # noqa: F401  re-exported for the endpoints
from app.models.device import DeviceInfo
import socketio
from app.services.metadata_socket_server import MetadataSocketServer
from app.services.jobs import JobRegistry
from app.services.settings import SettingsStore
from app.services.manager import (
    devices,
    fw_update,
    controls,
    pipeline,
    frames,
    sensor_streaming,
)


class RealSenseManager(
    devices.DeviceRegistryMixin,
    fw_update.FirmwareUpdateMixin,
    controls.ControlsMixin,
    pipeline.PipelineStreamingMixin,
    frames.FramesMixin,
    sensor_streaming.SensorStreamingMixin,
):
    # Class-level event loop reference for async operations from sync contexts
    _main_loop: Optional[asyncio.AbstractEventLoop] = None
    
    @classmethod
    def set_event_loop(cls, loop: asyncio.AbstractEventLoop):
        """Store reference to main event loop for use in sync callbacks."""
        cls._main_loop = loop
    
    def __init__(self, sio: socketio.AsyncServer, settings: Optional[SettingsStore] = None):
        self.ctx = rs.context()
        self.settings = settings or SettingsStore(None)
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

        # Caches for pipeline/config reuse to reduce startup cost
        self.config_cache: Dict[str, Dict[str, rs.config]] = {}  # device -> signature -> config
        self.pipeline_cache: Dict[str, rs.pipeline] = {}  # device -> last pipeline object
        self.pipeline_signatures: Dict[str, str] = {}  # device -> active signature

        # Stop coordination
        self.stopping: Set[str] = set()

        # Store latest raw depth frames for pixel depth queries
        self.depth_frames: Dict[str, Any] = {}  # device_id -> rs.depth_frame
        # Newest frame per stream for snapshots: device_id -> stream -> {frame, shown, motion}
        self.last_frames: Dict[str, Dict[str, Dict[str, Any]]] = {}

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
        self._fw_jobs: Dict[str, Any] = {}  # device_id -> the update's Job

        self.sio = sio
        self.metadata_socket_server = MetadataSocketServer(sio, self)
        # Long operations (firmware, calibration, export) report through here.
        self.jobs = JobRegistry(self._emit_socket_event)

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

        # --- Post-Processing Filters ---
        # Stores filter instances per device/sensor: device_id -> sensor_id -> list of filter dicts
        # Each filter dict: { "filter": rs.filter, "name": str, "enabled": bool }
        self.processing_blocks: Dict[str, Dict[str, List[Dict[str, Any]]]] = {}
        # One colorizer per device so depth-visualization options (color scheme,
        # min/max distance, histogram eq) set on it affect the streamed depth image.
        self.colorizers: Dict[str, "rs.colorizer"] = defaultdict(rs.colorizer)
        self.sensor_metadata_queues: Dict[str, Dict[str, List[Dict]]] = {}
        # Per-sensor rs.frame_queue objects: device_id -> sensor_id -> rs.frame_queue
        self.sensor_rs_queues: Dict[str, Dict[str, Any]] = {}
        # Track sensor stopping state
        self.sensor_stopping: Dict[str, Set[str]] = {}  # device_id -> set of sensor_ids

        # Initialize devices
        self.refresh_devices()

        # Refresh devices when one is plugged in or out.
        self.ctx.set_devices_changed_callback(self._on_devices_changed)
