# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""PLY export of the newest depth frame through the SDK's save_to_ply block, with the legacy
export dialog's choices (viewer.cpp export_to_ply): mesh or points, normals, binary or ASCII."""

import os
import tempfile

import pyrealsense2 as rs

from app.core.errors import RealSenseError


def export_depth_to_ply(depth_frame, mesh: bool = True, normals: bool = False, binary: bool = True) -> bytes:
    """Write ``depth_frame`` as PLY and return the file's bytes."""
    if depth_frame is None:
        raise RealSenseError(status_code=409, detail="No depth frame to export; start the depth stream first")
    fd, path = tempfile.mkstemp(suffix=".ply")
    os.close(fd)
    try:
        exporter = rs.save_to_ply(path)
        exporter.set_option(rs.save_to_ply.option_ply_mesh, 1.0 if mesh else 0.0)
        exporter.set_option(rs.save_to_ply.option_ply_normals, 1.0 if normals else 0.0)
        exporter.set_option(rs.save_to_ply.option_ply_binary, 1.0 if binary else 0.0)
        exporter.process(depth_frame)  # synchronous: the file is complete when this returns
        with open(path, "rb") as f:
            data = f.read()
    except RuntimeError as exc:
        raise RealSenseError(status_code=500, detail=f"PLY export failed: {exc}")
    finally:
        try:
            os.remove(path)
        except OSError:
            pass
    if not data:
        raise RealSenseError(status_code=500, detail="PLY export produced an empty file")
    return data
