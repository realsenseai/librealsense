# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import asyncio

from app.services.events import QUEUE_SIZE, TOO_SLOW, Events


class _StuckClient:
    """A client that stays connected but never reads: every send waits forever"""

    def __init__(self):
        self.closed = asyncio.Event()
        self.close_code = None

    async def send_json(self, message):
        await asyncio.Event().wait()

    async def close(self, code, reason):
        self.close_code = code
        self.closed.set()

    async def receive(self):
        await self.closed.wait()
        return {"type": "websocket.disconnect"}


def test_a_client_too_far_behind_is_closed():
    async def scenario():
        events, client = Events(), _StuckClient()
        serving = asyncio.ensure_future(events.serve(client))
        await asyncio.sleep(0)
        for i in range(QUEUE_SIZE + 2):  # one is taken off by the stuck send, the rest fill the queue and overflow
            events.emit("firmware_progress", {"progress": i})
        await asyncio.wait_for(serving, 1)
        return client.close_code, events._queues

    assert asyncio.run(scenario()) == (TOO_SLOW, {})
