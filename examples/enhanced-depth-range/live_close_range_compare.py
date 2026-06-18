#!/usr/bin/env python3

# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
"""Live side-by-side comparison: raw HW depth vs improved close-range depth.

Streams from a RealSense D4xx and shows two live OpenCV windows:

    ┌─────────────────────┐    ┌─────────────────────┐
    │ Raw HW depth        │    │ Improved depth      │
    │ (camera output)     │    │ (close-range filled)│
    └─────────────────────┘    └─────────────────────┘

Pixels closer than the camera's min-Z threshold (~520 mm by default) are
typically dropped by the hardware (shown black on the left). The Improved
Close Range Depth library recovers them (right window).

Run:
    python3 live_close_range_compare.py # press 'q' or ESC to stop
"""

import sys

import cv2
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

# Meters per Z16 unit (RS2_OPTION_DEPTH_UNITS). Typical D4xx is 0.001 (raw
# Z16 == mm), but high-accuracy presets and SR300 use other values, so we
# must scale raw values by this factor to get true millimetres.
try:
    depth_scale = profile.get_device().first_depth_sensor().get_depth_scale()
except Exception:
    depth_scale = 0.001
print(f"Depth scale: {depth_scale} m/unit ({depth_scale * 1000:.4f} mm/unit)")

# ── 3. Construct the improver ───────────────────────────────────────────
improver = DepthRangeImprover(calib)
print(f"Min-Z threshold: {improver.min_z_threshold_mm} mm")
print("Press 'q' or ESC to stop\n")

# ── 4. Helpers ──────────────────────────────────────────────────────────
# Visualisation depth range — invalid (depth==0) and beyond MAX_MM both
# render as black so the eye notices them clearly.
MIN_MM, MAX_MM = 100, 3000


def colorize_depth_mm(depth_mm: np.ndarray) -> np.ndarray:
    """uint16 mm → BGR uint8 with TURBO colormap, invalid pixels black."""
    valid = (depth_mm > 0) & (depth_mm <= MAX_MM)
    norm = np.zeros(depth_mm.shape, dtype=np.uint8)
    if valid.any():
        d = np.clip(depth_mm, MIN_MM, MAX_MM).astype(np.float32)
        norm[valid] = ((d[valid] - MIN_MM) / (MAX_MM - MIN_MM) * 255.0).astype(np.uint8)
    bgr = cv2.applyColorMap(norm, cv2.COLORMAP_TURBO)
    bgr[~valid] = (0, 0, 0)
    return bgr


WIN_HW, WIN_IMP = "Raw HW depth", "Improved close-range depth"

# ── 5. Stream + improve + display loop ──────────────────────────────────
# Windows are NOT pre-created — letting cv2.imshow create them on the
# first frame avoids empty black windows popping up while we wait for
# the camera to stream. Print a "waiting" line so the terminal isn't
# silent during the warm-up.
sys.stdout.write("Waiting for first frame… ")
sys.stdout.flush()

first_frame_drawn = False

try:
    while True:
        f = pipeline.wait_for_frames()
        ir_left  = np.asanyarray(f.get_infrared_frame(1).get_data())
        ir_right = np.asanyarray(f.get_infrared_frame(2).get_data())
        depth_hw = (np.asanyarray(f.get_depth_frame().get_data())
                    * depth_scale * 1000.0).astype(np.uint16)

        depth_imp = improver.process(ir_left, ir_right, depth_hw)

        if not first_frame_drawn:
            sys.stdout.write("got it. Streaming. Press 'q' or ESC to stop.\n")
            sys.stdout.flush()
            first_frame_drawn = True

        cv2.imshow(WIN_HW,  colorize_depth_mm(depth_hw))
        cv2.imshow(WIN_IMP, colorize_depth_mm(depth_imp))

        # Status line below the live windows.
        n_hw  = int((depth_hw  > 0).sum())
        n_imp = int((depth_imp > 0).sum())
        sys.stdout.write(
            f"\rframe {f.get_frame_number():>5} │ "
            f"valid pixels: HW {n_hw:>7,} → improved {n_imp:>7,} "
            f"(+{n_imp - n_hw:>6,})"
        )
        sys.stdout.flush()

        key = cv2.waitKey(1) & 0xFF
        if key in (ord('q'), 27):  # 27 = ESC
            break
except KeyboardInterrupt:
    pass
finally:
    print()
    pipeline.stop()
    cv2.destroyAllWindows()
