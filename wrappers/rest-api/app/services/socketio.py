# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import socketio

# Create Socket.IO Server
# max_http_buffer_size raised to 10 MB to fit point cloud frames (default 1 MB is too small).
sio = socketio.AsyncServer(async_mode="asgi", cors_allowed_origins='*', max_http_buffer_size=10_000_000)

# Setup basic event handlers
@sio.event
async def connect(sid, environ):
    print(f"Socket.IO client connected: {sid}")
    await sio.emit('welcome', {'message': 'Connected to RealSense Metadata Server'}, to=sid)

@sio.event
async def disconnect(sid):
    print(f"Socket.IO client disconnected: {sid}")