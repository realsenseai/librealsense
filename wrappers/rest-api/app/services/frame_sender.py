# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Send the SDK's frames to WebSocket subscribers as they are.

One WebSocket per (device, stream). Each binary message is one frame:

    [uint32 little-endian header length][header JSON, UTF-8][frame bytes, unchanged]
    then, for a Z16 depth frame: [color table: RGB of every depth value, "color_table_size" bytes]

The header holds what the SDK reports about the frame: number, timestamp, its domain,
the frame's metadata, its profile (stream, format, fps, size), intrinsics, and depth units. A subscriber
that falls behind gets only the newest frame; older ones are dropped, never queued.

The socket closes when its stream ends, with the code below that says why.
"""

import asyncio
import json
import struct
from typing import Any, Callable, Dict, List, Optional, Set, Tuple

import pyrealsense2 as rs

STOPPED = 4000  # the stream was stopped, by any client
DISCONNECTED = 4001  # the device is gone
NOT_STREAMING = 4002  # the stream was not running when the socket connected
REASONS = {STOPPED: "stream stopped", DISCONNECTED: "device disconnected", NOT_STREAMING: "not streaming"}

_METADATA = list(rs.frame_metadata_value.__members__.items())


def encode_frame(frame: Any, color_table: bytes = b"") -> bytes:
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


class _Subscriber:
    """One WebSocket. Holds at most one unsent frame: a newer frame replaces it, and a close ends it."""

    def __init__(self, websocket: Any):
        self.websocket = websocket
        self._pending: Optional[bytes] = None
        self._close_code: Optional[int] = None
        self._ready = asyncio.Event()

    def offer(self, message: bytes) -> None:  # on the event loop
        self._pending = message
        self._ready.set()

    def close(self, code: int) -> None:  # on the event loop; only the first close is sent
        self._close_code = self._close_code or code
        self._ready.set()

    async def run(self) -> None:
        while True:
            await self._ready.wait()
            self._ready.clear()
            if self._close_code:
                await self.websocket.close(self._close_code, REASONS[self._close_code])
                return
            await self.websocket.send_bytes(self._pending)


class FrameSender:
    """Routes frames from the SDK threads to the subscribers of each stream."""

    def __init__(self) -> None:
        self._subscribers: Dict[Tuple[str, Any, int], Set[_Subscriber]] = {}  # (device, rs.stream, index)
        self._loop: Optional[asyncio.AbstractEventLoop] = None

    async def serve(self, websocket: Any, device_id: str, stream_type: Any, stream_index: int,
                    streaming: Callable[[], bool]) -> None:
        """Stream one device stream's frames to an accepted WebSocket until the client disconnects or the stream ends."""
        self._loop = asyncio.get_running_loop()
        key = (device_id, stream_type, stream_index)
        subscriber = _Subscriber(websocket)
        self._subscribers.setdefault(key, set()).add(subscriber)
        sending = asyncio.ensure_future(subscriber.run())
        sending.add_done_callback(lambda t: t.cancelled() or t.exception())  # a send to a gone client is expected
        try:
            if not streaming():  # asked once subscribed: a stop from then on closes the socket through close()
                subscriber.close(NOT_STREAMING)
            while (await websocket.receive())["type"] != "websocket.disconnect":
                pass  # the client has nothing to say; a failed send also ends in a disconnect
        finally:
            sending.cancel()
            self._subscribers[key].discard(subscriber)
            if not self._subscribers[key]:
                del self._subscribers[key]

    def send(self, device_id: str, frame: Any, colorizer: Any = None) -> None:
        """Called from an SDK thread with a frame of the device; returns once its data is copied."""
        profile = frame.get_profile()
        key = (device_id, profile.stream_type(), profile.stream_index())
        if self._subscribers.get(key):
            z16 = hasattr(colorizer, "get_color_table") and profile.format() == rs.format.z16  # older SDKs lack it
            table = colorizer.get_color_table(frame.as_depth_frame()) if z16 else b""
            self._loop.call_soon_threadsafe(self._offer, key, encode_frame(frame, table))

    def _offer(self, key: Tuple[str, Any, int], message: bytes) -> None:
        for subscriber in self._subscribers.get(key, ()):
            subscriber.offer(message)

    def close(self, device_id: str, code: int, streams: Optional[List[Tuple[Any, int]]] = None) -> None:
        """Close the sockets of the device's streams, given as (rs.stream, index), or all of them; from any thread."""
        if self._loop:  # no socket was ever served otherwise
            self._loop.call_soon_threadsafe(self._close, device_id, code, streams)

    def _close(self, device_id: str, code: int, streams: Optional[List[Tuple[Any, int]]]) -> None:
        for (device, stream_type, stream_index), subscribers in self._subscribers.items():
            if device == device_id and (streams is None or (stream_type, stream_index) in streams):
                for subscriber in subscribers:
                    subscriber.close(code)
