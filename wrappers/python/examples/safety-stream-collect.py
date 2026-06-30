## License: Apache 2.0. See LICENSE file in root directory.
## Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#####################################################
##              Safety Stream Collect              ##
#####################################################

"""
Stream the Safety stream from a connected device and dump every frame's
metadata - mirroring how rs-save-to-disk and realsense-viewer iterate
metadata: walk every rs2_frame_metadata_value, keep the ones the frame
supports, write the raw integer.

The script intentionally has no hardcoded knowledge of safety field
semantics (bit meanings, hex formatting, enum names). When a new
RS2_FRAME_METADATA_SAFETY_* is added to the C API, it shows up in the
output automatically as soon as pyrealsense2 is rebuilt - no script
change required. For human-readable decoding of bitmasks, use the
realsense-viewer.

Output modes:
  console (default) - one block per frame on stdout / file
  csv               - one row per frame, columns = all supported metadata keys
  jsonl             - one JSON object per line per frame

Usage examples:
  python safety-stream-collect.py
  python safety-stream-collect.py --duration 10
  python safety-stream-collect.py --format csv --output safety.csv
  python safety-stream-collect.py --serial 123456789012 --frames 100
"""

import argparse
import csv
import json
import signal
import sys
import time
from datetime import datetime

import pyrealsense2 as rs


METADATA_ENUMS = list(rs.frame_metadata_value.__members__.values())


def parse_frame(frame):
    """Mirror stream-model.cpp / rs-save-to-disk metadata_to_csv:
    iterate every metadata value and keep the ones the frame supports."""
    parsed = []
    for md_val in METADATA_ENUMS:
        if frame.supports_frame_metadata(md_val):
            parsed.append((md_val.name, int(frame.get_frame_metadata(md_val))))
    return parsed


# --- Output backends ---------------------------------------------------------

class ConsoleWriter:
    def __init__(self, stream):
        self.stream = stream

    def write(self, frame, parsed):
        ts = datetime.fromtimestamp(time.time()).strftime("%H:%M:%S.%f")[:-3]
        self.stream.write(
            "\n=== Safety frame #{}  hw_ts={:.3f}  wall={} ===\n".format(
                frame.get_frame_number(), frame.get_timestamp(), ts
            )
        )
        width = max((len(n) for n, _ in parsed), default=0)
        for name, value in parsed:
            self.stream.write("  {} : {}\n".format(name.ljust(width), value))
        self.stream.flush()

    def close(self):
        pass


class CsvWriter:
    def __init__(self, path):
        self.fh = open(path, "w", newline="", encoding="utf-8")
        self.writer = csv.writer(self.fh)
        self.headers = None

    def write(self, frame, parsed):
        if self.headers is None:
            self.headers = ["wall_time", "frame_number", "hw_timestamp"] + [n for n, _ in parsed]
            self.writer.writerow(self.headers)
        row = {n: "" for n in self.headers}
        row["wall_time"] = datetime.utcnow().isoformat()
        row["frame_number"] = frame.get_frame_number()
        row["hw_timestamp"] = frame.get_timestamp()
        for name, value in parsed:
            row[name] = value
        self.writer.writerow([row[h] for h in self.headers])
        self.fh.flush()

    def close(self):
        self.fh.close()


class JsonlWriter:
    def __init__(self, path):
        self.fh = open(path, "w", encoding="utf-8")

    def write(self, frame, parsed):
        record = {
            "wall_time": datetime.utcnow().isoformat(),
            "frame_number": frame.get_frame_number(),
            "hw_timestamp": frame.get_timestamp(),
            "metadata": {name: value for name, value in parsed},
        }
        self.fh.write(json.dumps(record) + "\n")
        self.fh.flush()

    def close(self):
        self.fh.close()


# --- Streaming core ----------------------------------------------------------

def build_pipeline(serial, fps):
    ctx = rs.context()
    cfg = rs.config()
    if serial:
        cfg.enable_device(serial)
    cfg.enable_stream(rs.stream.safety, rs.format.y8, fps)
    pipe = rs.pipeline(ctx)
    return pipe, cfg


def make_writer(args):
    if args.format == "csv":
        if not args.output:
            print("--output is required for --format csv", file=sys.stderr)
            sys.exit(2)
        return CsvWriter(args.output)
    if args.format == "jsonl":
        if not args.output:
            print("--output is required for --format jsonl", file=sys.stderr)
            sys.exit(2)
        return JsonlWriter(args.output)
    stream = open(args.output, "w", encoding="utf-8") if args.output else sys.stdout
    return ConsoleWriter(stream)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--serial", help="Device serial number (defaults to first device)")
    parser.add_argument("--fps", type=int, default=30, help="Safety stream FPS (default: 30)")
    parser.add_argument("--format", choices=["console", "csv", "jsonl"], default="console")
    parser.add_argument("--output", help="Output file path (stdout if omitted, in console mode)")
    parser.add_argument("--duration", type=float, default=0.0, help="Seconds to stream (0 = until Ctrl-C)")
    parser.add_argument("--frames", type=int, default=0, help="Max frames to capture (0 = unlimited)")
    parser.add_argument("--timeout-ms", type=int, default=5000, help="wait_for_frames timeout (default: 5000ms)")
    args = parser.parse_args()

    writer = make_writer(args)
    pipe, cfg = build_pipeline(args.serial, args.fps)

    stop = {"flag": False}
    def _sigint(_signum, _frame):
        stop["flag"] = True
    signal.signal(signal.SIGINT, _sigint)

    try:
        profile = pipe.start(cfg)
    except RuntimeError as e:
        print("Failed to start pipeline: {}".format(e), file=sys.stderr)
        sys.exit(1)

    dev = profile.get_device()
    name = dev.get_info(rs.camera_info.name) if dev.supports(rs.camera_info.name) else "?"
    sn = dev.get_info(rs.camera_info.serial_number) if dev.supports(rs.camera_info.serial_number) else "?"
    print("Streaming safety from {} (SN {}) at {} fps. Ctrl-C to stop.".format(name, sn, args.fps), file=sys.stderr)

    start = time.time()
    n = 0
    try:
        while not stop["flag"]:
            if args.duration > 0 and (time.time() - start) >= args.duration:
                break
            if args.frames > 0 and n >= args.frames:
                break
            try:
                frames = pipe.wait_for_frames(args.timeout_ms)
            except RuntimeError as e:
                print("wait_for_frames timed out / failed: {}".format(e), file=sys.stderr)
                continue
            safety = frames.first_or_default(rs.stream.safety)
            if not safety:
                continue
            parsed = parse_frame(safety)
            writer.write(safety, parsed)
            n += 1
    finally:
        try:
            pipe.stop()
        except Exception:
            pass
        writer.close()
        print("\nCaptured {} safety frames in {:.2f}s".format(n, time.time() - start), file=sys.stderr)


if __name__ == "__main__":
    main()
