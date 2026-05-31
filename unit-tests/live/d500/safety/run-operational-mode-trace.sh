#!/usr/bin/env bash
# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#
# Diagnostic wrapper for test-operational-mode.py on Linux.
# Captures kernel ring buffer (dmesg), optionally strace of host syscalls,
# and the test's stdout/stderr into a timestamped output directory, so the
# failing second pipe.start() (safety+depth+color) can be traced through
# the kernel/v4l2/usbfs layer.
#
# Usage:
#     ./run-operational-mode-trace.sh
#     STRACE=1 ./run-operational-mode-trace.sh        # include syscall trace
#     PYREALSENSE2_PATH=/path/to/Release ./run-...    # if pyrealsense2 isn't on PYTHONPATH
#     TEST_PY=/abs/path/to/test-operational-mode.py ./run-...  # override test path
#
# Notes:
#   - strace adds significant syscall overhead. The 30-fps streaming pipeline
#     produces ~150 ioctl/s per stream; tracing them can slow the host enough
#     that the test fails *differently* than without trace. Run both ways.
#   - dmesg follower needs read access to /dev/kmsg. On most Ubuntu boxes this
#     requires sudo; we fall back to journalctl -k if sudo isn't available.
#   - The CDC + SMCU captures already produced by the test (via rspy.d500_log)
#     land in CWD as test-operational-mode-{cdc,smcu}_*.txt -- they're moved
#     into the output directory at the end.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_PY="${TEST_PY:-$SCRIPT_DIR/test-operational-mode.py}"

if [[ ! -f "$TEST_PY" ]]; then
    echo "test file not found: $TEST_PY" >&2
    exit 2
fi

STAMP=$(date +%Y%m%d_%H%M%S)
OUT_DIR="trace_operational_mode_$STAMP"
mkdir -p "$OUT_DIR"
echo "writing diagnostics to: $(realpath "$OUT_DIR")"

# Snapshot device topology before the run -------------------------------------
ls -la /dev/video* /dev/ttyACM* /dev/ttyUSB* 2>/dev/null > "$OUT_DIR/devices_before.txt"
lsusb -t > "$OUT_DIR/lsusb_before.txt" 2>&1 || true

# Kernel ring buffer follower -------------------------------------------------
DMESG_PID=""
if command -v dmesg >/dev/null 2>&1 && sudo -n true 2>/dev/null; then
    # Marker line so we can find our run in the kernel log
    sudo dmesg -T --console-on >/dev/null 2>&1 || true
    sudo bash -c "echo '--- run-operational-mode-trace $STAMP ---' > /dev/kmsg" 2>/dev/null || true
    sudo dmesg -wT > "$OUT_DIR/dmesg.txt" 2>&1 &
    DMESG_PID=$!
elif command -v journalctl >/dev/null 2>&1; then
    journalctl -kf -o short-precise --since "now" > "$OUT_DIR/journalctl_kernel.txt" 2>&1 &
    DMESG_PID=$!
else
    echo "warning: no dmesg (no passwordless sudo) and no journalctl -- skipping kernel log"
fi

cleanup() {
    if [[ -n "$DMESG_PID" ]]; then
        kill "$DMESG_PID" 2>/dev/null || true
        wait "$DMESG_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

# Build Python invocation -----------------------------------------------------
PY_PREFIX=()
if [[ -n "${PYREALSENSE2_PATH:-}" ]]; then
    PY_PREFIX+=(env "PYTHONPATH=$PYREALSENSE2_PATH:${PYTHONPATH:-}")
fi

PY_CMD=(python3 "$TEST_PY")

if [[ "${STRACE:-0}" == "1" ]]; then
    if ! command -v strace >/dev/null 2>&1; then
        echo "STRACE=1 requested but strace not installed" >&2
        exit 3
    fi
    PY_CMD=(strace -f -tt -o "$OUT_DIR/strace.log"
            -e trace=openat,close,ioctl,mmap,munmap,write,clone3,futex
            "${PY_CMD[@]}")
fi

# Run -------------------------------------------------------------------------
echo "running: ${PY_PREFIX[*]} ${PY_CMD[*]}"
set +e
"${PY_PREFIX[@]}" "${PY_CMD[@]}" > "$OUT_DIR/test_stdout.log" 2>&1
TEST_RC=$?
set -e

# Post-snapshot ---------------------------------------------------------------
ls -la /dev/video* /dev/ttyACM* /dev/ttyUSB* 2>/dev/null > "$OUT_DIR/devices_after.txt"
lsusb -t > "$OUT_DIR/lsusb_after.txt" 2>&1 || true

# Move CDC/SMCU captures into the output directory ----------------------------
shopt -s nullglob
for f in ./test-operational-mode-cdc_*.txt ./test-operational-mode-smcu_*.txt; do
    mv "$f" "$OUT_DIR/" 2>/dev/null || true
done
shopt -u nullglob

echo
echo "test exit code: $TEST_RC"
echo "outputs in: $(realpath "$OUT_DIR")"
ls -la "$OUT_DIR/"
exit $TEST_RC
