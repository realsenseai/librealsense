# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Smallest-possible RUM ingest stub for local development.

Accepts HTTPS/HTTP POST /v1/rum, writes each received body to a timestamped JSON
file under ./received/ so the payload can be inspected, and replies 200 OK. This is
NOT the production server (no validation, auth, or storage) -- it exists only to
confirm the viewer's uploader sends well-formed reports end-to-end.

Usage:
    python rum_dev_server.py [--port 8080] [--dir received]

Point the viewer's uploader at it with the dev-only override:
    set RS2_RUM_ENDPOINT=http://127.0.0.1:8080/v1/rum
"""

import argparse
import json
import os
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


class _Handler(BaseHTTPRequestHandler):
    out_dir = "received"
    _counter = 0

    def do_POST(self):
        if self.path != "/v1/rum":
            self.send_response(404)
            self.end_headers()
            return

        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length) if length else b""

        os.makedirs(self.out_dir, exist_ok=True)
        _Handler._counter += 1
        stamp = time.strftime("%Y%m%d-%H%M%S")
        path = os.path.join(self.out_dir, "rum-{}-{:03d}.json".format(stamp, _Handler._counter))

        # Pretty-print if it parses as JSON; otherwise store raw bytes.
        try:
            parsed = json.loads(body.decode("utf-8"))
            with open(path, "w", encoding="utf-8") as f:
                json.dump(parsed, f, indent=2)
            self._print_summary(parsed, len(body), path)
        except Exception:
            with open(path, "wb") as f:
                f.write(body)
            print("\n[{}] received {} bytes (non-JSON) -> {}".format(
                time.strftime("%H:%M:%S"), len(body), path))

        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(b'{"status":"ok"}')

    @staticmethod
    def _print_summary(r, nbytes, path):
        sdk = r.get("sdk", {})
        sys_ = r.get("system", {})
        flags = sdk.get("cmake_flags", {})
        flags_str = ", ".join("{}={}".format(k, v) for k, v in flags.items()) or "-"

        def join(arr, fmt):
            return "; ".join(fmt(x) for x in r.get(arr, [])) or "-"

        print("\n" + "=" * 70)
        print("[{}] RUM report  ({} bytes)".format(time.strftime("%H:%M:%S"), nbytes))
        print("  source_id : {}".format(r.get("source_id", "?")))
        print("  sdk       : {} ({}, backend={})".format(
            sdk.get("version"), sdk.get("build_type"), sdk.get("backend")))
        print("  cmake     : {}".format(flags_str))
        print("  system    : {} / {}".format(sys_.get("os"), sys_.get("arch")))
        print("  devices   : {}".format(join("devices",
            lambda d: "{} fw={} {} mipi={} (x{})".format(d.get("type"), d.get("fw_version"), d.get("connection"), d.get("mipi_driver_version") or "-", d.get("count")))))
        print("  streams   : {}".format(join("streams",
            lambda s: "{} {} {}@{} (x{}, {:.1f}s)".format(s.get("type"), s.get("format"), s.get("resolution"), s.get("fps"), s.get("count"), s.get("duration_seconds", 0)))))
        print("  options   : {}".format(join("options_changed",
            lambda o: "{}={} (x{})".format(o.get("option"), o.get("last_value"), o.get("set_count")))))
        print("  filters   : {}".format(join("filters",
            lambda f: "{} (x{})".format(f.get("name"), f.get("count")))))
        print("  notifs    : {}".format(join("notifications",
            lambda n: "{} (x{})".format(n.get("category"), n.get("count")))))
        print("  saved -> {}".format(path))
        print("=" * 70)

    def log_message(self, *args):
        pass  # quiet default access logging


def main():
    parser = argparse.ArgumentParser(description="RUM local dev ingest stub")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--dir", default="received")
    args = parser.parse_args()

    _Handler.out_dir = args.dir
    server = ThreadingHTTPServer(("127.0.0.1", args.port), _Handler)
    print("RUM dev server listening on http://127.0.0.1:{}/v1/rum (writing to '{}/')".format(args.port, args.dir))
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    server.server_close()


if __name__ == "__main__":
    main()
