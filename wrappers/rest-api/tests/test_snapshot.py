# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import io
import zipfile

import cv2
import numpy as np

from app.services.snapshot import build_snapshot


def _entries(data):
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        return {name: z.read(name) for name in z.namelist()}


def test_depth_snapshot_has_colorized_png_raw_z16_and_metadata_csv():
    raw = np.arange(6, dtype=np.uint16).reshape(2, 3)
    shown = np.zeros((2, 3, 3), dtype=np.uint8)
    shown[..., 0] = 255  # red in RGB
    base, data = build_snapshot("depth", 42, raw, "z16", shown, {"timestamp": 1.5, "actual_fps": 30})

    assert base == "depth_42"
    files = _entries(data)
    assert set(files) == {"depth_42.png", "depth_42.raw", "depth_42_metadata.csv"}
    assert files["depth_42.raw"] == raw.tobytes()
    png = cv2.imdecode(np.frombuffer(files["depth_42.png"], np.uint8), cv2.IMREAD_COLOR)
    assert png.shape == (2, 3, 3) and tuple(png[0, 0]) == (0, 0, 255)  # BGR on disk, red pixel
    assert files["depth_42_metadata.csv"].decode() == "Stream,depth\ntimestamp,1.5\nactual_fps,30\n"


def test_infrared_y16_is_written_as_16_bit_png():
    raw = (np.arange(4, dtype=np.uint16) * 1000).reshape(2, 2)
    _, data = build_snapshot("infrared-1", 7, raw, "y16", None, {})
    png = cv2.imdecode(np.frombuffer(_entries(data)["infrared-1_7.png"], np.uint8), cv2.IMREAD_UNCHANGED)
    assert png.dtype == np.uint16 and int(png[1, 1]) == 3000


def test_motion_snapshot_is_a_csv_with_the_sample():
    _, data = build_snapshot("gyro", 3, None, "motion_xyz32f", None, {"timestamp": 9}, motion={"x": 0.1, "y": -0.2, "z": 9.8})
    files = _entries(data)
    assert set(files) == {"gyro_3.csv"}
    assert files["gyro_3.csv"].decode() == "x,y,z\n0.1,-0.2,9.8\ntimestamp,9\n"
