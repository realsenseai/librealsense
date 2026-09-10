# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import asyncio

import socketio

from app.services import socket_handlers


class _Manager:
    def get_depth_at_pixel(self, device_id, x, y):
        if device_id != "dev":
            raise KeyError(device_id)
        return 1.25 if (x, y) == (10, 20) else None


def _handler(sio, name):
    return sio.handlers["/"][name]


def test_depth_at_pixel_answers_over_the_socket():
    sio = socketio.AsyncServer(async_mode="asgi")
    socket_handlers.register(sio, lambda: _Manager())
    reply = asyncio.run(_handler(sio, "depth_at_pixel")("sid", {"device_id": "dev", "x": 10, "y": 20}))
    assert reply == {"depth": 1.25, "x": 10, "y": 20, "units": "meters"}


def test_depth_at_pixel_reports_errors_instead_of_hanging():
    sio = socketio.AsyncServer(async_mode="asgi")
    socket_handlers.register(sio, lambda: _Manager())
    reply = asyncio.run(_handler(sio, "depth_at_pixel")("sid", {"device_id": "nope", "x": 0, "y": 0}))
    assert reply["depth"] is None and "nope" in reply["error"]
