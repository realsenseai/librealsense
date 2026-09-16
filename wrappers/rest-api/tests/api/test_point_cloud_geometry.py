# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from app.models.stream import Resolution, StreamConfig
from app.services import point_cloud_geometry
from .conftest import client

URL = "/api/v1/devices/device1/point_cloud/geometry"


def _cfg(sensor_id, stream, fmt, w, h, fps=30):
    return StreamConfig(sensor_id=sensor_id, stream_type=stream, format=fmt, resolution=Resolution(width=w, height=h), framerate=fps)


def _stream(rs_manager, sensor_id, configs):
    rs_manager.sensor_streams.setdefault("device1", {})[sensor_id] = {"is_streaming": True, "configs": configs, "stream_types": [c.stream_type for c in configs]}


def test_geometry_needs_a_running_depth_stream(setup_mock_managers):
    body = client.get(URL).json()
    assert body == {"depth": None, "texture": None}


def test_geometry_carries_depth_intrinsics_units_and_texture_extrinsics(setup_mock_managers):
    rs_manager = setup_mock_managers["rs_manager"]
    _stream(rs_manager, "device1-sensor-0", [_cfg("device1-sensor-0", "depth", "z16", 640, 480)])
    _stream(rs_manager, "device1-sensor-1", [_cfg("device1-sensor-1", "color", "rgb8", 1280, 720)])

    body = client.get(URL + "?texture=color").json()
    assert body["depth"]["units"] == 0.001
    assert (body["depth"]["width"], body["depth"]["height"], body["depth"]["fx"], body["depth"]["ppx"]) == (640, 480, 640.0, 320.0)
    assert body["depth"]["model"] == "brown_conrady" and body["depth"]["coeffs"] == [0.0] * 5
    assert body["texture"]["stream"] == "color" and body["texture"]["width"] == 1280
    assert body["texture"]["extrinsics"] == {"rotation": [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0], "translation": [0.015, 0.0, 0.0]}

    # No texture when the requested stream is not running
    assert client.get(URL + "?texture=infrared-1").json()["texture"] is None


def test_active_profile_matches_the_exact_mode(setup_mock_managers):
    rs_manager = setup_mock_managers["rs_manager"]
    sensor = rs_manager.devices["device1"].sensors[0]
    assert point_cloud_geometry.active_profile(sensor, _cfg("s", "depth", "z16", 640, 480)) is not None
    assert point_cloud_geometry.active_profile(sensor, _cfg("s", "depth", "z16", 123, 45)) is None
