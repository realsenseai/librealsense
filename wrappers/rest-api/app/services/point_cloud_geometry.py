# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Camera geometry for the client-side point cloud: the depth stream's intrinsics and depth
units, and the texture stream's intrinsics plus the depth->texture extrinsics, so the browser
can unproject depth and map the texture on the GPU (viewer.cpp / pointcloud-gl.cpp)."""

from typing import Any, Dict, Iterable, Optional

import pyrealsense2 as rs


def _name(profile) -> str:
    """'depth', 'color', 'infrared-1'... as the stream configs name them."""
    name = str(profile.stream_type()).rsplit(".", 1)[-1].lower()
    if profile.stream_type() == rs.stream.infrared:
        name = f"infrared-{profile.stream_index()}"
    return name


def intrinsics_dict(profile) -> Dict[str, Any]:
    video = profile.as_video_stream_profile()
    i = video.get_intrinsics()
    return {
        "width": i.width, "height": i.height,
        "fx": i.fx, "fy": i.fy, "ppx": i.ppx, "ppy": i.ppy,
        "model": str(i.model).rsplit(".", 1)[-1],
        "coeffs": [float(c) for c in i.coeffs],
    }


def extrinsics_dict(from_profile, to_profile) -> Dict[str, Any]:
    e = from_profile.get_extrinsics_to(to_profile)
    return {"rotation": [float(r) for r in e.rotation], "translation": [float(t) for t in e.translation]}


def active_profile(sensor, config) -> Optional[Any]:
    """The sensor profile a running stream config was opened with (exact type/format/size/fps)."""
    for profile in sensor.get_stream_profiles():
        if _name(profile) != config.stream_type.lower():
            continue
        if str(profile.format()).rsplit(".", 1)[-1].lower() != config.format.lower():
            continue
        try:
            video = profile.as_video_stream_profile()
        except RuntimeError:
            continue
        if video.width() == config.resolution.width and video.height() == config.resolution.height and profile.fps() == config.framerate:
            return profile
    return None


def geometry(dev, streaming: Iterable[Any], texture_stream: Optional[str]) -> Dict[str, Any]:
    """``streaming`` yields (sensor, config) for every running stream of the device.

    Returns {"depth": {...} | None, "texture": {...} | None}. The texture entry carries the
    extrinsics from the depth stream, or None when the requested stream is not running."""
    depth_profile = depth_sensor = None
    texture_profile = None
    for sensor, config in streaming:
        profile = active_profile(sensor, config)
        if profile is None:
            continue
        if config.stream_type.lower() == "depth" and depth_profile is None:
            depth_profile, depth_sensor = profile, sensor
        if texture_stream and config.stream_type.lower() == texture_stream.lower():
            texture_profile = profile

    result: Dict[str, Any] = {"depth": None, "texture": None}
    if depth_profile is None:
        return result
    units = depth_sensor.get_depth_scale() if hasattr(depth_sensor, "get_depth_scale") else 0.001
    result["depth"] = {"stream": "depth", "units": float(units), **intrinsics_dict(depth_profile)}
    if texture_profile is not None:
        result["texture"] = {
            "stream": texture_stream.lower(),
            **intrinsics_dict(texture_profile),
            "extrinsics": extrinsics_dict(depth_profile, texture_profile),
        }
    return result
