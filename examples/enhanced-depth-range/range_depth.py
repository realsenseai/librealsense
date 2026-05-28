#!/usr/bin/env python3

# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Live MinZ (close-range depth) demo using rs_depth + pyrealsense2.

Streams from a RealSense D4xx, runs DepthRangeImprover on each frame, and
prints a single live-updating status line showing how much of the close-range
band MinZ recovered that the camera couldn't see on its own.

Run:
    python3 range_depth.py        # Ctrl-C to stop
"""

import sys

import numpy as np
import pyrealsense2 as rs

from rs_depth import Calibration, DepthRangeImprover


# ── 1. Open the camera ──────────────────────────────────────────────────
pipeline = rs.pipeline()
cfg = rs.config()
cfg.enable_stream(rs.stream.infrared, 1, 1280, 720, rs.format.y8,  30)
cfg.enable_stream(rs.stream.infrared, 2, 1280, 720, rs.format.y8,  30)
cfg.enable_stream(rs.stream.depth,       1280, 720, rs.format.z16, 30)
profile = pipeline.start(cfg)

# ── 2. Build calibration from the camera's own intrinsics/extrinsics ────
ir1 = profile.get_stream(rs.stream.infrared, 1).as_video_stream_profile()
ir2 = profile.get_stream(rs.stream.infrared, 2).as_video_stream_profile()
calib = Calibration.from_sdk(ir1.get_intrinsics(), ir1.get_extrinsics_to(ir2))

# Meters per Z16 unit. Typical D4xx = 0.001 (raw Z16 == mm), but high-accuracy
# presets and SR300 use other values — scale raw values to mm before comparing
# against MinZ threshold (which is in mm).
try:
    depth_scale = profile.get_device().first_depth_sensor().get_depth_scale()
except Exception:
    depth_scale = 0.001

# ── 3. Construct the improver (auto threshold = 1.2 * focal × baseline / 126) ─
improver = DepthRangeImprover(calib)
T = improver.min_z_threshold_mm
N = 1280 * 720
print(f"MinZ threshold: {T} mm  (pixels closer than this are improved)")
print("Press Ctrl-C to stop\n")

# ── 4. Stream + improve + live status line ──────────────────────────────
hw_total = imp_total = rescued_total = frames_seen = 0

try:
    while True:
        f = pipeline.wait_for_frames()
        ir_left  = np.asanyarray(f.get_infrared_frame(1).get_data())
        ir_right = np.asanyarray(f.get_infrared_frame(2).get_data())
        depth_hw = (np.asanyarray(f.get_depth_frame().get_data())
                    * depth_scale * 1000.0).astype(np.uint16)

        depth_imp = improver.process(ir_left, ir_right, depth_hw)

        n_hw    = int(((depth_hw  > 0) & (depth_hw  < T)).sum())
        n_imp   = int(((depth_imp > 0) & (depth_imp < T)).sum())
        rescued = int(((depth_hw == 0) & (depth_imp > 0) & (depth_imp < T)).sum())

        hw_pct       = 100.0 * n_hw / N
        imp_pct      = 100.0 * n_imp / N
        recovery_pct = 100.0 * (n_imp - n_hw) / n_imp if n_imp > 0 else 0.0

        sys.stdout.write(
            f"\rframe {f.get_frame_number():>5} │ close-range: "
            f"HW {hw_pct:5.2f}% → improved {imp_pct:5.2f}% "
            f"│ MinZ recovered {recovery_pct:5.1f}% │ +{rescued:>6} px"
        )
        sys.stdout.flush()

        hw_total      += n_hw
        imp_total     += n_imp
        rescued_total += rescued
        frames_seen   += 1
except KeyboardInterrupt:
    pass
finally:
    print()
    pipeline.stop()