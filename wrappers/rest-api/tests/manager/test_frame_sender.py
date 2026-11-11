# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import asyncio
import json
import struct

import pyrealsense2 as rs

from app.services.frame_sender import DISCONNECTED, NOT_STREAMING, STOPPED, FrameSender, encode_frame


class _Intrinsics:
    width, height, ppx, ppy, fx, fy = 2, 2, 1.0, 1.0, 2.5, 2.5
    model, coeffs = rs.distortion.brown_conrady, [0.0] * 5


class _Profile:
    def stream_type(self): return rs.stream.depth
    def stream_index(self): return 0
    def format(self): return rs.format.z16
    def fps(self): return 30
    def as_video_stream_profile(self): return self
    def get_intrinsics(self): return _Intrinsics()


class _Frame:
    """A 2x2 Z16 depth frame reporting one metadata value."""

    def __init__(self, number, data=b"\x01\x00\x02\x00\x03\x00\x04\x00"):
        self.number, self.data = number, data

    def get_profile(self): return _Profile()
    def get_frame_number(self): return self.number
    def get_timestamp(self): return 1000.5
    def get_frame_timestamp_domain(self): return rs.timestamp_domain.hardware_clock
    def supports_frame_metadata(self, md): return md == rs.frame_metadata_value.actual_exposure
    def get_frame_metadata(self, md): return 33
    def is_video_frame(self): return True
    def as_video_frame(self): return self
    def is_depth_frame(self): return True
    def as_depth_frame(self): return self
    def get_units(self): return 0.001
    def get_width(self): return 2
    def get_height(self): return 2
    def get_stride_in_bytes(self): return 4
    def get_bytes_per_pixel(self): return 2
    def get_data(self): return self.data


def _decode(message):
    (length,) = struct.unpack_from("<I", message)
    return json.loads(message[4:4 + length]), message[4 + length:]


class _WebSocket:
    def __init__(self):
        self.sent = []
        self.closed = asyncio.Event()
        self.close_code = None

    async def send_bytes(self, message):
        self.sent.append(message)

    async def close(self, code, reason):
        self.close_code = code
        self.closed.set()  # the client answers the close

    async def receive(self):
        await self.closed.wait()
        return {"type": "websocket.disconnect"}


def test_message_is_header_then_unchanged_bytes():
    header, data = _decode(encode_frame(_Frame(7)))
    assert data == _Frame(7).data
    assert header == {
        "frame_number": 7, "timestamp": 1000.5, "timestamp_domain": "hardware_clock",
        "stream_type": "depth", "stream_index": 0, "format": "z16", "fps": 30,
        "metadata": {"actual_exposure": 33}, "width": 2, "height": 2, "stride": 4, "bpp": 2,
        "intrinsics": {"width": 2, "height": 2, "ppx": 1.0, "ppy": 1.0, "fx": 2.5, "fy": 2.5,
                       "model": "brown_conrady", "coeffs": [0.0] * 5},
        "units": 0.001,
    }


def test_color_table_follows_the_frame_bytes():
    header, data = _decode(encode_frame(_Frame(7), b"\x05" * 6))
    assert header["color_table_size"] == 6
    assert data == _Frame(7).data + b"\x05" * 6


def test_only_subscribers_of_the_stream_get_frames_and_a_slow_one_gets_the_newest():
    async def scenario():
        sender = FrameSender()
        depth, color = _WebSocket(), _WebSocket()
        serving = [asyncio.ensure_future(sender.serve(depth, "dev", rs.stream.depth, 0, lambda: True)),
                   asyncio.ensure_future(sender.serve(color, "dev", rs.stream.color, 0, lambda: True))]
        await asyncio.sleep(0)

        for number in (1, 2, 3):  # arrive before the sender gets to run: only the newest is sent
            sender.send("dev", _Frame(number))
        await asyncio.sleep(0.01)
        sender.send("dev", _Frame(4))
        await asyncio.sleep(0.01)

        depth.closed.set()
        color.closed.set()
        await asyncio.gather(*serving)
        sender.send("dev", _Frame(5))  # nobody subscribed any more
        return depth.sent, color.sent, sender

    depth_sent, color_sent, sender = asyncio.run(scenario())
    assert [_decode(m)[0]["frame_number"] for m in depth_sent] == [3, 4]
    assert color_sent == []
    assert sender._subscribers == {}


def test_a_stream_that_is_not_running_closes_at_once():
    async def scenario():
        sender, socket = FrameSender(), _WebSocket()
        await sender.serve(socket, "dev", rs.stream.depth, 0, lambda: False)
        return socket.close_code, sender._subscribers

    assert asyncio.run(scenario()) == (NOT_STREAMING, {})


def test_close_ends_the_sockets_of_the_given_streams_or_of_the_whole_device():
    async def scenario():
        sender = FrameSender()
        depth, color, other = _WebSocket(), _WebSocket(), _WebSocket()
        serving = [asyncio.ensure_future(sender.serve(socket, device, stream, 0, lambda: True))
                   for socket, device, stream in ((depth, "dev", rs.stream.depth), (color, "dev", rs.stream.color),
                                                  (other, "dev2", rs.stream.depth))]
        await asyncio.sleep(0)
        sender.close("dev", STOPPED, [(rs.stream.depth, 0)])
        await asyncio.sleep(0.01)
        stopped = (depth.close_code, color.close_code)
        sender.close("dev", DISCONNECTED)
        await asyncio.sleep(0.01)
        other.closed.set()
        await asyncio.gather(*serving)
        return stopped, color.close_code, other.close_code

    assert asyncio.run(scenario()) == ((STOPPED, None), DISCONNECTED, None)
