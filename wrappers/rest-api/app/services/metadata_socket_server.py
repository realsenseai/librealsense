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
    ):
        self._sio = sio
        self._rs_manager = rs_manager
        self._update_interval = update_interval
        self._broadcast_thread: Optional[threading.Thread] = None
        self._target_device_id: Optional[str] = None
        self._is_broadcasting = False
        self._thread_stop_event = threading.Event()
        self._async_loop = None

    def _emit_event(self, event_name, data):
        """Helper method to handle emit for both sync and async server types"""
        if self._async_loop is None:
            self._async_loop = asyncio.new_event_loop()
            asyncio.set_event_loop(self._async_loop)

        async def async_emit():
            await self._sio.emit(event_name, data)

        # Run the coroutine in the event loop
        self._async_loop.run_until_complete(async_emit())

    def _broadcast_metadata_loop(self):
        """The core loop that fetches and broadcasts metadata."""
        print("[MetadataBroadcaster] Starting broadcast loop...")

        if not self._async_loop:
            self._async_loop = asyncio.new_event_loop()
            asyncio.set_event_loop(self._async_loop)

        while self._is_broadcasting and not self._thread_stop_event.is_set():
            start_time = time.monotonic()

            if not self._target_device_id:
                time.sleep(self._update_interval)
                continue

            # --- Fetch status and metadata ---
            active_streams = []
            is_streaming = False
            try:
                status = self._rs_manager.get_stream_status(self._target_device_id)
                is_streaming = status.is_streaming
                if is_streaming:
                    active_streams = status.active_streams
            except Exception as e:
                print(
                    f"[MetadataBroadcaster] Error getting stream status for {self._target_device_id}: {e}"
                )
                active_streams = []
            except Exception as e:
                print(f"[MetadataBroadcaster] Unexpected error getting status: {e}")
                active_streams = []

            all_metadata: Dict[str, Optional[Dict]] = {}
            if is_streaming and active_streams:
                for stream_type in active_streams:
                    try:
                        metadata = self._rs_manager.get_latest_metadata(
                            self._target_device_id, stream_type
                        )
                        if (
                            stream_type == "depth"
                            and "point_cloud" in metadata
                            and "vertices" in metadata["point_cloud"]
                        ):
                            metadata["point_cloud"]["vertices"] = base64.b64encode(
                                metadata["point_cloud"]["vertices"].tobytes()
                            ).decode("utf-8")
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
                "device_id": self._target_device_id,
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

        print("[MetadataBroadcaster] Broadcast loop stopped.")

    def start_broadcast(self, device_id: str):
        """Starts the metadata broadcast loop as a background thread."""
        if self._is_broadcasting:
            return

        if not device_id:
            raise ValueError("A target device_id must be provided.")

        self._target_device_id = device_id
        self._is_broadcasting = True
        self._thread_stop_event.clear()

        self._broadcast_thread = threading.Thread(
            target=self._broadcast_metadata_loop, daemon=True
        )
        self._broadcast_thread.start()

        print(
            f"[MetadataBroadcaster] Broadcast loop started for device: {self._target_device_id}"
        )

    def stop_broadcast(self):
        """Stops the metadata broadcast loop gracefully."""
        if not self._is_broadcasting or not self._broadcast_thread:
            return

        print("[MetadataBroadcaster] Stopping broadcast loop...")
        self._is_broadcasting = False
        self._thread_stop_event.set()

        # Clean up threading resources
        if self._broadcast_thread and self._broadcast_thread.is_alive():
            self._broadcast_thread.join(
                timeout=2.0
            )  # Wait for thread to terminate with timeout

        # Clean up the async loop if it exists
        if self._async_loop:
            self._async_loop.close()
            self._async_loop = None

        self._broadcast_thread = None
        self._target_device_id = None
        print("[MetadataBroadcaster] Broadcast loop stopped.")