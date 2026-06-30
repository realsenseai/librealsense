# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import base64
import time
import threading
from typing import Optional, Dict
import asyncio


class MetadataSocketServer:
    """
    Handles fetching metadata from RealSenseManager and broadcasting it
    via a provided Socket.IO server instance.
    """

    def __init__(
        self,
        sio,  # Can be either socketio.Server or socketio.AsyncServer
        rs_manager,
        update_interval: float = 1.0/30.0,  # Default to 30 FPS
        point_cloud_throttle: int = 3,  # Include point_cloud every Nth broadcast (30/3 = 10 FPS)
    ):
        self._sio = sio
        self._rs_manager = rs_manager
        self._update_interval = update_interval
        self._threads: Dict[str, threading.Thread] = {}
        self._stop_events: Dict[str, threading.Event] = {}
        self._lock = threading.Lock()
        self._point_cloud_throttle = max(1, point_cloud_throttle)

    def _emit_event(self, event_name, data):
        """Helper method to handle emit for both sync and async server types"""
        loop = self._rs_manager._main_loop
        if not loop or loop.is_closed():
            return
        future = asyncio.run_coroutine_threadsafe(self._sio.emit(event_name, data), loop)
        future.add_done_callback(
            lambda f: f.exception() and print(
                f"[MetadataBroadcaster] emit {event_name} failed: {f.exception()}"
            )
        )

    def _broadcast_metadata_loop(self, device_id, stop_event):
        """The core loop that fetches and broadcasts metadata."""
        print(f"[MetadataBroadcaster] Starting broadcast loop for {device_id}...")

        # Per-loop counter so each device's throttle is independent — a shared
        # instance counter would race across per-device broadcaster threads and
        # make the point-cloud "every Nth tick" pattern non-deterministic.
        broadcast_count = 0

        while not stop_event.is_set():
            start_time = time.monotonic()

            # --- Fetch status and metadata ---
            active_streams = []
            is_streaming = False
            try:
                status = self._rs_manager.get_stream_status(device_id)
                is_streaming = status.is_streaming
                if is_streaming:
                    active_streams = status.active_streams
            except Exception as e:
                print(
                    f"[MetadataBroadcaster] Error getting stream status for {device_id}: {e}"
                )
                active_streams = []

            broadcast_count += 1
            send_point_cloud = (broadcast_count % self._point_cloud_throttle) == 0

            all_metadata: Dict[str, Optional[Dict]] = {}
            if is_streaming and active_streams:
                for stream_type in active_streams:
                    try:
                        metadata = self._rs_manager.get_latest_metadata(
                            device_id, stream_type
                        )
                        if (
                            stream_type == "depth"
                            and "point_cloud" in metadata
                            and "vertices" in metadata["point_cloud"]
                        ):
                            # get_latest_metadata returns the cached dict by reference;
                            # copy before encoding/dropping to avoid corrupting it for the next read.
                            metadata = {**metadata}
                            if send_point_cloud:
                                pc_src = metadata["point_cloud"]
                                pc_encoded = {
                                    **pc_src,
                                    "vertices": base64.b64encode(
                                        pc_src["vertices"].tobytes()
                                    ).decode("utf-8"),
                                }
                                if "colors" in pc_src and pc_src["colors"] is not None:
                                    pc_encoded["colors"] = base64.b64encode(
                                        pc_src["colors"].tobytes()
                                    ).decode("utf-8")
                                metadata["point_cloud"] = pc_encoded
                            else:
                                del metadata["point_cloud"]
                        all_metadata[stream_type] = metadata
                    except Exception as e:
                        if hasattr(e, "status_code"):
                            if e.status_code == 503 or e.status_code == 400:
                                all_metadata[stream_type] = None
                            else:
                                all_metadata[stream_type] = {"error": str(e)}
                        else:
                            all_metadata[stream_type] = {
                                "error": f"Unexpected: {str(e)}"
                            }

            # --- Emit via the provided sio instance ---
            payload = {
                "device_id": device_id,
                "is_streaming": is_streaming,
                "timestamp_server": time.time(),
                "metadata_streams": all_metadata,
            }
            try:
                # Use helper method to handle emit appropriately
                self._emit_event("metadata_update", payload)
            except Exception as e:
                print(
                    f"[MetadataBroadcaster] Error emitting 'metadata_update' event: {e}"
                )

            # --- Sleep ---
            elapsed_time = time.monotonic() - start_time
            sleep_duration = max(0, self._update_interval - elapsed_time)
            time.sleep(sleep_duration)

        print(f"[MetadataBroadcaster] Broadcast loop stopped for {device_id}.")

    def start_broadcast(self, device_id: str):
        """Starts the metadata broadcast loop as a background thread."""
        if not device_id:
            raise ValueError("A target device_id must be provided.")

        with self._lock:
            t = self._threads.get(device_id)
            if t is not None and t.is_alive():
                return

            stop_event = threading.Event()
            self._stop_events[device_id] = stop_event
            self._threads[device_id] = threading.Thread(
                target=self._broadcast_metadata_loop,
                args=(device_id, stop_event),
                daemon=True,
                name=f"MetadataBroadcaster-{device_id}",
            )
            self._threads[device_id].start()

        print(
            f"[MetadataBroadcaster] Broadcast loop started for device: {device_id}"
        )

    def stop_broadcast(self, device_id: Optional[str] = None):
        """Stops the metadata broadcast loop gracefully. If device_id is None, stops all."""
        # Join under _lock to block concurrent start_broadcast — safe because broadcaster uses a different lock.
        with self._lock:
            if device_id is None:
                targets = list(self._threads.keys())
            else:
                targets = [device_id] if device_id in self._threads else []
            if not targets:
                return

            threads_to_join = []
            for d in targets:
                ev = self._stop_events.pop(d, None)
                if ev is not None:
                    ev.set()
                t = self._threads.pop(d, None)
                if t is not None:
                    threads_to_join.append(t)

            print("[MetadataBroadcaster] Stopping broadcast loop...")
            for t in threads_to_join:
                if t.is_alive():
                    t.join(timeout=2.0)  # Wait for thread to terminate with timeout
            print("[MetadataBroadcaster] Broadcast loop stopped.")
