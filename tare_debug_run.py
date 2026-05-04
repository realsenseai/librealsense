## License: Apache 2.0. See LICENSE file in root directory.
## Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Non-interactive host-assisted tare calibration runner for the TARE_DEBUG_DUMP
instrumentation branch. Triggers the host-assisted flow so the C++ debug hook
emits tare_debug_<ts>_*.json/.bin artifacts in the current working directory.

Usage:
    python tare_debug_run.py [--gt-mm 300] [--accuracy medium] [--exposure auto]

The new calibration table is NOT written to the device.
"""

import argparse
import glob
import json
import os
import sys
import time

# Pick up the freshly-built pyrealsense2 from build/Release ahead of any
# user-site copy.
_HERE = os.path.dirname(os.path.abspath(__file__))
_BUILD_RELEASE = os.path.join(_HERE, 'build', 'Release')
if os.path.isdir(_BUILD_RELEASE):
    sys.path.insert(0, _BUILD_RELEASE)

import pyrealsense2 as rs
print(f"pyrealsense2 from: {rs.__file__}")

TARE_ACCURACY = {'very_high': 0, 'high': 1, 'medium': 2, 'low': 3}
SCAN = {'intrinsic': 0, 'extrinsic': 1}

DEPTH_W, DEPTH_H, DEPTH_FPS = 256, 144, 90
FRAME_TIMEOUT_MS = 5000
INITIAL_FW_CALL_TIMEOUT_MS = 30000
OVERALL_TIMEOUT_S = 300


def progress_cb(p):
    print(f"\rprogress {p}%   ", end="")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--gt-mm', type=float, default=300.0, help='Ground truth distance in mm (default 300)')
    ap.add_argument('--accuracy', choices=TARE_ACCURACY.keys(), default='medium')
    ap.add_argument('--scan', choices=SCAN.keys(), default='intrinsic')
    ap.add_argument('--exposure', default='auto', help="Exposure value or 'auto'")
    ap.add_argument('--step-count', type=int, default=20)
    ap.add_argument('--out-dir', default='.', help='Where tare_debug_* files are emitted (cwd-relative for the C++ hook)')
    args = ap.parse_args()

    out_dir = os.path.abspath(args.out_dir)
    os.makedirs(out_dir, exist_ok=True)
    os.chdir(out_dir)
    print(f"writing tare_debug_* artifacts to: {out_dir}")

    ctx = rs.context()
    devs = ctx.query_devices()
    if len(devs) == 0:
        print("no device connected")
        sys.exit(1)
    dev = devs[0]
    name = dev.get_info(rs.camera_info.name) if dev.supports(rs.camera_info.name) else "?"
    sn = dev.get_info(rs.camera_info.serial_number) if dev.supports(rs.camera_info.serial_number) else "?"
    print(f"device: {name} (SN {sn})")

    if dev.supports(rs.camera_info.product_line):
        if str(dev.get_info(rs.camera_info.product_line)) != 'D400':
            print("this script targets D400-series cameras")
            sys.exit(1)

    am = rs.rs400_advanced_mode(dev)
    if not am or not am.is_enabled():
        print('advanced mode must be enabled (run rs-enable-advanced-mode or set via Viewer)')
        sys.exit(1)

    depth_sensor = dev.first_depth_sensor()
    depth_sensor.set_option(rs.option.emitter_enabled, 1)
    if depth_sensor.supports(rs.option.thermal_compensation):
        depth_sensor.set_option(rs.option.thermal_compensation, 0)
    if args.exposure == 'auto':
        depth_sensor.set_option(rs.option.enable_auto_exposure, 1)
    else:
        depth_sensor.set_option(rs.option.enable_auto_exposure, 0)
        depth_sensor.set_option(rs.option.exposure, int(args.exposure))

    cfg = rs.config()
    cfg.enable_stream(rs.stream.depth, DEPTH_W, DEPTH_H, rs.format.z16, DEPTH_FPS)
    pipe = rs.pipeline(ctx)
    pp = pipe.start(cfg)
    # Warm up so AE/emitter stabilize before FW takes a snapshot.
    print("warming up frames...")
    for _ in range(30):
        pipe.wait_for_frames(FRAME_TIMEOUT_MS)
    adev = pp.get_device().as_auto_calibrated_device()

    tare_json = json.dumps({
        'host assistance': 1,
        'speed': 3,
        'scan parameter': SCAN[args.scan],
        'step count': args.step_count,
        'apply preset': 1,
        'accuracy': TARE_ACCURACY[args.accuracy],
        'depth': 0,
        'resize factor': 1,
    })
    print(f"tare json: {tare_json}")
    print(f"ground truth: {args.gt_mm} mm")

    listing_before = set(glob.glob('tare_debug_*'))

    try:
        print("initial FW call...")
        new_calib, health = adev.run_tare_calibration(args.gt_mm, tare_json, progress_cb, INITIAL_FW_CALL_TIMEOUT_MS)
        print()
        deadline = time.time() + OVERALL_TIMEOUT_S
        iters = 0
        while len(new_calib) == 0:
            if time.time() > deadline:
                raise RuntimeError(f"timeout after {OVERALL_TIMEOUT_S}s waiting for calibration to converge")
            frames = pipe.wait_for_frames(FRAME_TIMEOUT_MS)
            depth = frames.get_depth_frame()
            if not depth:
                continue
            new_calib, health = adev.process_calibration_frame(depth, progress_cb, FRAME_TIMEOUT_MS)
            iters += 1
        print()
        print(f"tare converged after {iters} processed frames")
        print(f"health: before={health[0]:+.4f} after={health[1]:+.4f}")
        print(f"calibration table: {len(new_calib)} bytes (NOT written to device)")
    finally:
        pipe.stop()

    listing_after = set(glob.glob('tare_debug_*'))
    new_files = sorted(listing_after - listing_before)
    print(f"\nnew debug artifacts ({len(new_files)}):")
    for f in new_files:
        print(f"  {f}  ({os.path.getsize(f)} B)")


if __name__ == '__main__':
    main()
