# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from pytest_check import check
import time

pytestmark = [
    pytest.mark.device_each("D455"),
]

FPS = 30
DURATION = 10.0

def test_actual_fps_within_tolerance_from_startup(module_device_setup):
    cfg = rs.config()
    cfg.enable_stream(rs.stream.depth, 848, 480, rs.format.z16, FPS)
    pipe = rs.pipeline()
    pipe.start(cfg)  # global time on by default - no warmup sleep on purpose

    md = rs.frame_metadata_value.actual_fps
    frames = 0
    out_of_band = []
    start = time.time()
    try:
        while time.time() - start < DURATION:
            f = pipe.wait_for_frames().get_depth_frame()
            if not f or not f.supports_frame_metadata(md):  # first frame has no actual_fps
                continue
            frames += 1
            fps = f.get_frame_metadata(md) / 1000.0
            if not 0.9 * FPS <= fps <= 1.1 * FPS:
                out_of_band.append((f.get_frame_number(), round(fps, 2)))
    finally:
        pipe.stop()

    check.greater(frames, 0, "no frames with actual_fps metadata were collected")
    check.equal(out_of_band, [], f"actual_fps outside +/-10% of {FPS}: {out_of_band[:20]}")
