# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import threading
import logging
from typing import Dict, List
import pyrealsense2 as rs
from app.services.camera import Camera
from app.services.events import Events
from app.services.frame_sender import FrameSender


class RealSenseManager:
    """The connected cameras, by ID, as the SDK reports them plugged in and out"""

    def __init__(self):
        self.ctx = rs.context()
        self.cameras: Dict[str, Camera] = {}
        self.lock = threading.Lock()
        self.events = Events()
        # Sends each stream's frames, unchanged, to its WebSocket subscribers
        self.frame_sender = FrameSender()

        # The devices are listed once; after that the SDK reports each one plugged in or out, as in the
        # legacy viewer. The callback goes first so a device plugged in meanwhile is not missed.
        self.ctx.set_devices_changed_callback(self._on_devices_changed)
        with self.lock:
            self._add_cameras(self.ctx.devices)

    def _on_devices_changed(self, info) -> None:
        """The SDK's devices-changed callback, on a thread of its own"""
        with self.lock:
            gone = {i: c for i, c in self.cameras.items() if info.was_removed(c.dev)}
            for device_id in gone:
                del self.cameras[device_id]
            added = self._add_cameras(info.get_new_devices())
        if added or gone:
            logging.info("devices_changed: +%s -%s", added, list(gone))
            self.events.emit("devices_changed", {"added": added, "removed": list(gone)})

    def _add_cameras(self, devices) -> List[str]:
        """Each device of an SDK device list not known yet, as a Camera; the IDs added. Caller holds self.lock"""
        added = []
        for dev in devices:
            try:
                camera = Camera(dev, self.ctx, self.events, self.frame_sender)
            except RuntimeError as e:  # one the SDK can't open now, such as a camera another process flashes
                logging.info("Skipping a device: %s", e)
                continue
            if camera.id not in self.cameras:
                self.cameras[camera.id] = camera
                added.append(camera.id)
        return added
