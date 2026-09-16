# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Socket.IO request/reply handlers: small, latency-sensitive queries the UI makes at mouse
rate, answered over the already-open socket instead of one HTTP round trip each."""

from typing import Any, Callable, Dict

from starlette.concurrency import run_in_threadpool


def register(sio, get_manager: Callable[[], Any]) -> None:
    @sio.event
    async def depth_at_pixel(sid, data: Dict[str, Any]):
        """Depth in meters under a pixel of the newest depth frame, or None."""
        try:
            depth = await run_in_threadpool(
                get_manager().get_depth_at_pixel, data["device_id"], int(data["x"]), int(data["y"]))
        except Exception as exc:  # a bad request must answer, not hang the ack
            return {"depth": None, "error": str(exc)}
        return {"depth": depth, "x": data["x"], "y": data["y"], "units": "meters"}
