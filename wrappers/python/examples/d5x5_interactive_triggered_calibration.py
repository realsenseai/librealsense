## License: Apache 2.0. See LICENSE file in root directory.
## Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Interactive Triggered Calibration example for D5x5 family cameras (D535, D585
non-safety / non-legacy variants) over USB.

Unlike the legacy D585S Triggered Calibration flow (see
`d500_triggered_calibration.py`), the D5x5 interactive flow is a two-phase
protocol:

    RUN   -> HEALTH_CHECK   (candidate cached in RAM, host inspects health)
              |
              +--> TRY NEW / TRY OLD  (optional: live-preview swap in RAM)
              |
              +--> COMMIT  -> COMPLETE  (candidate flashed to EEPROM)
              +--> CANCEL             (candidate discarded, back to IDLE)

The single SDK entry point is `auto_calibrated_device.run_on_chip_calibration`
with the following JSON tokens:

    "calib run"       - start the algorithm; blocks until HEALTH_CHECK / failure
    "calib try new"   - live-swap the candidate table into RAM (preview)
    "calib try old"   - restore the currently-committed table into RAM
    "calib commit"    - persist the HEALTH_CHECK candidate to flash
    "calib cancel"    - discard the candidate, return to IDLE

The health value returned alongside the RUN result is `rect_health` in pixels;
the firmware's pass gate is `rect_health < 0.4 px`.
"""

import struct
import sys
import time
import zlib

import pyrealsense2 as rs
import numpy as np


# Firmware pass threshold for `rect_health` in pixels.
# Mirrors `rect_health_pass_threshold_px` in src/calibration-engine-interface.h.
RECT_HEALTH_PASS_THRESHOLD_PX = 0.4

# Raw HW-monitor opcode for GET_CALIB_STATUS (ds::GET_CALIB_STATUS in
# src/ds/d500/d500-private.h). Used to read the metrics the SDK does not expose.
GET_CALIB_STATUS = 0xB9

# Field order of `librealsense::calibration_health_metrics`
# (src/calibration-engine-interface.h). The struct is `#pragma pack(1)`,
# five little-endian float32 at offset 3 of the GET_CALIB_STATUS payload.
HEALTH_FIELDS = (
    "coverage_safe_for_depth",
    "rect_health",
    "rect_improvement",
    "scale_health",
    "scale_improvement",
)

# Payload size once the 4-byte opcode echo is stripped: 3-byte header
# (state, progress, result) + 20-byte health block + 512-byte calibration table.
HEALTH_PAYLOAD_SIZE = 535
HEALTH_OFFSET = 3

# Device names of the D5x5 family that expose the interactive flow.
# D585S and the legacy D585 stay on the older triggered-calibration path
# (covered by d500_triggered_calibration.py); D555 uses the D400 OCC path.
D5X5_INTERACTIVE_NAME_HINTS = ("D535", "D585")
D5X5_INTERACTIVE_NAME_EXCLUDES = ("D585S", "D555")


def find_d5x5_device():
    ctx = rs.context()
    for dev in ctx.query_devices():
        if not dev.supports(rs.camera_info.name):
            continue
        name = dev.get_info(rs.camera_info.name)
        if any(x in name for x in D5X5_INTERACTIVE_NAME_EXCLUDES):
            continue
        if any(x in name for x in D5X5_INTERACTIVE_NAME_HINTS):
            print("Found D5x5 device:", name)
            return dev
    return None


def depth_fill_rate(pipe, num_frames=5):
    """Average non-zero-pixel ratio of the depth frame."""
    total = 0.0
    counted = 0
    for _ in range(num_frames):
        frames = pipe.wait_for_frames()
        depth = frames.get_depth_frame()
        if not depth:
            continue
        arr = np.asarray(depth.get_data(), dtype=np.uint16).ravel()
        total += float(np.count_nonzero(arr)) / arr.size
        counted += 1
    return (total / counted) * 100.0 if counted else 0.0


def stream_and_report_fill_rate(dev, label):
    cfg = rs.config()
    cfg.enable_device(dev.get_info(rs.camera_info.serial_number))
    cfg.enable_stream(rs.stream.depth, 0, 1280, 720, rs.format.z16, 30)
    pipe = rs.pipeline()
    profile = pipe.start(cfg)
    try:
        # Skip the auto-exposure warm-up.
        for _ in range(10):
            pipe.wait_for_frames()
        rate = depth_fill_rate(pipe)
    finally:
        pipe.stop()
    print("[{}] depth fill rate = {:.2f}%".format(label, rate))
    return rate


def read_full_health(dev):
    """Read the full `calibration_health_metrics` struct over the raw HW monitor.

    `run_on_chip_calibration` reports only `rect_health` through its `health`
    out-param; firmware returns all five metrics in the GET_CALIB_STATUS reply.

    Only valid while firmware is parked at HEALTH_CHECK (or COMPLETE) — that is,
    after a RUN that reported success and before COMMIT/CANCEL. At IDLE/PROCESS
    the reply carries only the 3-byte header and this returns None. Note the SDK
    cancels a *failed* RUN back to IDLE before returning, so there is no health
    payload left to read on that path.

    The offsets below mirror `interactive_calibration_answer`; there is no
    version field on the wire, so a firmware layout change cannot be detected
    here beyond the total size check.
    """
    dbg = dev.as_debug_protocol()
    reply = bytes(dbg.send_and_receive_raw_data(dbg.build_command(GET_CALIB_STATUS)))
    payload = reply[4:]  # strip the opcode echo the HW monitor prepends
    if len(payload) != HEALTH_PAYLOAD_SIZE:
        return None
    values = struct.unpack_from("<5f", payload, HEALTH_OFFSET)
    return dict(zip(HEALTH_FIELDS, values))


def calibration_table_crc(ac_dev):
    """Return CRC32 of the currently flashed depth calibration table."""
    table = bytes(ac_dev.get_calibration_table())
    return zlib.crc32(table)


def progress_cb(progress):
    # `progress` is a float in [0, 100] — firmware only populates it during
    # calibration_state::PROCESS; other states report 0.
    sys.stdout.write("\r  progress: {:>3.0f}%".format(progress))
    sys.stdout.flush()


def prompt_yes(question, default_yes=True):
    suffix = " [Y/n] " if default_yes else " [y/N] "
    try:
        ans = input(question + suffix).strip().lower()
    except EOFError:
        return default_yes
    if not ans:
        return default_yes
    return ans in ("y", "yes")


def run_stage(ac_dev, json_token, timeout_ms):
    """Invoke run_on_chip_calibration with a callback and unpack the health."""
    table, (health, _reserved) = ac_dev.run_on_chip_calibration(
        json_token, progress_cb, timeout_ms
    )
    # Clear the progress line.
    sys.stdout.write("\r" + " " * 24 + "\r")
    sys.stdout.flush()
    return table, health


def main():
    dev = find_d5x5_device()
    if dev is None:
        print("No D5x5 (D535 / D585 non-safety / non-legacy) device found.")
        print("If you have a D585S or the legacy D585, use d500_triggered_calibration.py instead.")
        sys.exit(1)

    ac_dev = dev.as_auto_calibrated_device()
    if not ac_dev:
        print("Device does not implement auto_calibrated_device extension.")
        sys.exit(1)

    crc_before = calibration_table_crc(ac_dev)
    print("Flashed depth calibration CRC32 (before) = 0x{:08x}".format(crc_before))

    fill_before = stream_and_report_fill_rate(dev, "before RUN")

    # ------------------------------------------------------------------
    # Phase 1: RUN -> HEALTH_CHECK
    # ------------------------------------------------------------------
    print("\nRunning interactive triggered calibration (this may take ~30-90 s)...")
    try:
        _, health = run_stage(ac_dev, "calib run", timeout_ms=180000)
    except RuntimeError as e:
        print("RUN failed:", e)
        sys.exit(2)

    if health < 0:
        # -1 sentinel from the SDK: FW never populated a HEALTH_CHECK payload
        # (FAILED_TO_RUN / FAILED_TO_CONVERGE / cancelled). No candidate cached.
        print("Calibration did not produce a candidate (rect_health = -1).")
        print("Nothing to commit; leaving flashed calibration untouched.")
        sys.exit(3)

    passed = health < RECT_HEALTH_PASS_THRESHOLD_PX
    print("Candidate cached in RAM. rect_health = {:.4f} px  ({} threshold = {} px)".format(
        health,
        "PASS <" if passed else "FAIL >=",
        RECT_HEALTH_PASS_THRESHOLD_PX,
    ))

    # Everything except `rect_health` is informational today, but firmware
    # reports it and the pass gate for coverage/scale is still open (spec 5.5).
    metrics = read_full_health(dev)
    if metrics is None:
        print("  (full health metrics unavailable - firmware not at HEALTH_CHECK)")
    else:
        for name in HEALTH_FIELDS:
            print("  {:<24} = {:.6f}".format(name, metrics[name]))

    # ------------------------------------------------------------------
    # Phase 2 (optional): TRY NEW / TRY OLD live preview
    # ------------------------------------------------------------------
    if prompt_yes("Preview the candidate live (TRY NEW)?"):
        try:
            run_stage(ac_dev, "calib try new", timeout_ms=5000)
        except RuntimeError as e:
            print("TRY NEW failed:", e)
        else:
            print("Candidate table is now active in RAM.")
            # Note: the running pipeline keeps its baked intrinsics until
            # restart. Restarting the stream here shows the candidate in action.
            fill_new = stream_and_report_fill_rate(dev, "TRY NEW")

            if not prompt_yes("Keep candidate active (say N to revert to OLD)?"):
                try:
                    run_stage(ac_dev, "calib try old", timeout_ms=5000)
                    print("Reverted to previously committed table (RAM).")
                    stream_and_report_fill_rate(dev, "TRY OLD")
                except RuntimeError as e:
                    print("TRY OLD failed:", e)

    # ------------------------------------------------------------------
    # Phase 3: COMMIT or CANCEL
    # ------------------------------------------------------------------
    do_commit = passed and prompt_yes(
        "Commit candidate to flash?", default_yes=passed
    )
    if not do_commit:
        try:
            run_stage(ac_dev, "calib cancel", timeout_ms=5000)
            print("Candidate discarded. Flashed calibration unchanged.")
        except RuntimeError as e:
            print("CANCEL reported:", e)
        return

    print("Committing candidate to flash (do not disconnect)...")
    try:
        run_stage(ac_dev, "calib commit", timeout_ms=60000)
    except RuntimeError as e:
        print("COMMIT failed:", e)
        sys.exit(4)

    crc_after = calibration_table_crc(ac_dev)
    print("Flashed depth calibration CRC32 (after)  = 0x{:08x}".format(crc_after))
    if crc_after == crc_before:
        print("Warning: CRC did not change - firmware may have skipped the flash write.")
    else:
        print("New calibration committed successfully.")

    fill_after = stream_and_report_fill_rate(dev, "after COMMIT")
    print("Depth fill rate change: {:+.2f} pp".format(fill_after - fill_before))


if __name__ == "__main__":
    main()
