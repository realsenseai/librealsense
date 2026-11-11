# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Tell every client of the events WebSocket, /api/v1/events, what changed on the server.

Each message is one JSON text message, {"event": name, ...its fields}:

    devices_changed          added, removed: device ids
    streaming_changed        device_id, sensors: {sensor_id: [stream profile]}, what each sensor streams now
    firmware_progress        device_id, progress: 0 to 1, phase: "downloading" or "installing"
    firmware_update_success  device_id, firmware_version
    firmware_update_failed   device_id, error

Nothing is replayed: a client reads the current state through the REST API once connected.
"""

import asyncio
from typing import Any, Dict, Optional, Set


class Events:
    def __init__(self) -> None:
        self._queues: Set[asyncio.Queue] = set()  # one per client, in the order the events happened
        self._loop: Optional[asyncio.AbstractEventLoop] = None

    async def serve(self, websocket: Any) -> None:
        """Send the events to an accepted WebSocket until the client disconnects."""
        self._loop = asyncio.get_running_loop()
        queue: asyncio.Queue = asyncio.Queue()
        self._queues.add(queue)

        async def send() -> None:
            while True:
                await websocket.send_json(await queue.get())

        sending = asyncio.ensure_future(send())
        sending.add_done_callback(lambda t: t.cancelled() or t.exception())  # a send to a gone client is expected
        try:
            while (await websocket.receive())["type"] != "websocket.disconnect":
                pass  # the client has nothing to say
        finally:
            sending.cancel()
            self._queues.discard(queue)

    def emit(self, event: str, fields: Dict[str, Any]) -> None:
        """From any thread."""
        if self._loop:  # no client was ever served otherwise
            self._loop.call_soon_threadsafe(self._put, {"event": event, **fields})

    def _put(self, message: Dict[str, Any]) -> None:
        for queue in self._queues:
            queue.put_nowait(message)
