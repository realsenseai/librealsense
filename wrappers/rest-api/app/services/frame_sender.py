# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Send the SDK's frames to WebSocket subscribers as they are.

One WebSocket per (device, stream). Each binary message is one frame:

    [uint32 little-endian header length][header JSON, UTF-8][frame bytes, unchanged]
    then, for a Z16 depth frame: [color table: RGB of every depth value, "color_table_size" bytes]

The header holds what the SDK reports about the frame: number, timestamp, its domain,
the frame's metadata, its profile (stream, format, fps, size), intrinsics, and depth units. A subscriber
that falls behind gets only the newest frame; older ones are dropped, never queued.

Only the client closes the socket: it gets the stream's frames whenever the stream runs, by any client, and
nothing while it is stopped or the device is gone. The events socket tells it which.
"""

import asyncio
import json
import struct
from typing import Any, Dict, Optional, Set, Tuple

import pyrealsense2 as rs
from fastapi import WebSocket

from app.services.events import serve

_METADATA = list(rs.frame_metadata_value.__members__.items())


def encode_frame(frame: rs.frame, color_table: bytes = b"") -> bytes:
    """One message for one SDK frame. Copies the frame's data once, so the frame can be released."""
    profile = frame.get_profile()
    header: Dict[str, Any] = {
        "frame_number": frame.get_frame_number(),
        "timestamp": frame.get_timestamp(),
        "timestamp_domain": frame.get_frame_timestamp_domain().name,
        # The frame's profile: after a filter such as decimation, not the one the stream was started with
        "stream_type": profile.stream_type().name,
        "stream_index": profile.stream_index(),
        "format": profile.format().name,
        "fps": profile.fps(),
        "metadata": {name: frame.get_frame_metadata(md) for name, md in _METADATA
                     if frame.supports_frame_metadata(md)},
    }
    if frame.is_video_frame():
        video = frame.as_video_frame()
        header.update(width=video.get_width(), height=video.get_height(),
                      stride=video.get_stride_in_bytes(), bpp=video.get_bytes_per_pixel())
        # Per frame: a filter such as decimation changes the size, and with it the intrinsics
        try:
            i = profile.as_video_stream_profile().get_intrinsics()
            header["intrinsics"] = {"width": i.width, "height": i.height, "ppx": i.ppx, "ppy": i.ppy,
                                    "fx": i.fx, "fy": i.fy, "model": i.model.name, "coeffs": list(i.coeffs)}
        except RuntimeError:  # uncalibrated streams have none
            pass
    if frame.is_depth_frame():
        header["units"] = frame.as_depth_frame().get_units()  # meters per depth value; can change while streaming
    if color_table:
        header["color_table_size"] = len(color_table)
    head = json.dumps(header).encode()
    return struct.pack("<I", len(head)) + head + bytes(frame.get_data()) + color_table


def _key(device_id: str, frame: rs.frame) -> Tuple[str, rs.stream, int]:
    profile = frame.get_profile()
    return device_id, profile.stream_type(), profile.stream_index()


class _Subscriber:
    """One WebSocket. Holds at most one unsent frame: a newer frame replaces it."""

    def __init__(self, websocket: WebSocket):
        self.websocket = websocket
        self._pending: Optional[bytes] = None
        self._ready = asyncio.Event()

    def offer(self, message: bytes) -> None:  # on the event loop
        self._pending = message
        self._ready.set()

    async def run(self) -> None:
        while True:
            await self._ready.wait()
            self._ready.clear()
            await self.websocket.send_bytes(self._pending)


class FrameSender:
    """Routes frames from the SDK threads to the subscribers of each stream."""

    def __init__(self) -> None:
        self._subscribers: Dict[Tuple[str, rs.stream, int], Set[_Subscriber]] = {}  # (device, stream, index)
        self._loop: Optional[asyncio.AbstractEventLoop] = None

    async def serve(self, websocket: WebSocket, device_id: str, stream_type: rs.stream, stream_index: int) -> None:
        """Stream one device stream's frames to an accepted WebSocket until the client disconnects."""
        self._loop = asyncio.get_running_loop()
        key = (device_id, stream_type, stream_index)
        subscriber = _Subscriber(websocket)
        self._subscribers.setdefault(key, set()).add(subscriber)
        try:
            await serve(websocket, subscriber.run())
        finally:
            self._subscribers[key].discard(subscriber)
            if not self._subscribers[key]:
                del self._subscribers[key]

    def watched(self, device_id: str, frame: rs.frame) -> bool:
        """Whether any socket shows the frame's stream"""
        return bool(self._subscribers.get(_key(device_id, frame)))

    def send(self, device_id: str, frame: rs.frame, colorizer: rs.colorizer) -> None:
        """Called from an SDK thread with a frame of the device; returns once its data is copied."""
        z16 = frame.get_profile().format() == rs.format.z16
        table = colorizer.get_color_table(frame.as_depth_frame()) if z16 else b""
        self._loop.call_soon_threadsafe(self._offer, _key(device_id, frame), encode_frame(frame, table))

    def _offer(self, key: Tuple[str, rs.stream, int], message: bytes) -> None:
        for subscriber in self._subscribers.get(key, ()):
            subscriber.offer(message)
