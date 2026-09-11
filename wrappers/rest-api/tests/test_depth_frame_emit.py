# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import numpy as np

from app.services.manager.sensor_streaming import SensorStreamingMixin


class _Profile:
    def as_video_stream_profile(self):
        return self

    def width(self):
        return 4

    def height(self):
        return 2


class _DepthFrame:
    def __init__(self, number):
        self._number = number
        self._data = np.arange(8, dtype=np.uint16) * number

    def get_profile(self):
        return _Profile()

    def get_frame_number(self):
        return self._number

    def get_units(self):
        return 0.001

    def get_data(self):
        return self._data


class _Host(SensorStreamingMixin):
    def __init__(self):
        self.emitted = []

    def _emit_socket_event(self, event, payload):
        self.emitted.append((event, payload))


def test_depth_frames_go_out_as_z16_bytes_at_most_15_hz(monkeypatch):
    host = _Host()
    clock = [100.0]
    monkeypatch.setattr("app.services.manager.sensor_streaming.time.monotonic", lambda: clock[0])

    host._emit_depth_frame("d1", _DepthFrame(1))
    clock[0] += 0.01
    host._emit_depth_frame("d1", _DepthFrame(2))  # too soon: dropped
    clock[0] += 0.1
    host._emit_depth_frame("d1", _DepthFrame(3))

    assert [p["frame_number"] for _, p in host.emitted] == [1, 3]
    event, payload = host.emitted[0]
    assert event == "depth_frame"
    assert (payload["device_id"], payload["width"], payload["height"], payload["units"], payload["format"]) == ("d1", 4, 2, 0.001, "z16")
    assert payload["data"] == (np.arange(8, dtype=np.uint16)).tobytes()
    assert len(payload["data"]) == 4 * 2 * 2
