#!/usr/bin/env bash
# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 Intel Corporation. All Rights Reserved.

# Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

#
# Runs the three GPU unit tests added for PR #15074:
#   - test-algo-projection-distortion          (rscuda::deproject_depth_cuda)
#   - test-algo-projection-yuy2-conversion     (rscuda::unpack_yuy2_cuda_helper)
#   - test-algo-projection-cuda-align          (align_cuda_helper::align_other_to_depth)
#
# Works for both BUILD_WITH_CUDA and BUILD_WITH_HIP builds -- the test
# binaries are the same and each test SKIPs when no GPU is visible to the
# runtime probe (rsutils::rs2_is_gpu_available).
#
# Usage:
#   ./unit-tests/run-gpu-tests.sh                  # auto-detect build dir
#   ./unit-tests/run-gpu-tests.sh build_rocm       # explicit build dir
#   ./unit-tests/run-gpu-tests.sh --filter "small" # pass extra args to Catch2
#
# Exit code: 0 if every selected test passes (or is skipped), non-zero on
# the first failure.

set -u   # don't `set -e`: we capture per-test rc explicitly to keep going

# -----------------------------------------------------------------------------
# Argument parsing
# -----------------------------------------------------------------------------
BUILD_DIR=""
CATCH_ARGS=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --filter)
            shift
            CATCH_ARGS+=("$1")
            shift
            ;;
        --reporter|--success|--verbosity)
            CATCH_ARGS+=("$1")
            shift
            if [[ $# -gt 0 && "$1" != -* ]]; then
                CATCH_ARGS+=("$1")
                shift
            fi
            ;;
        --help|-h)
            sed -n '1,/^# *Exit code/p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        --*)
            # Unknown flag: forward to Catch2 verbatim.
            CATCH_ARGS+=("$1")
            shift
            ;;
        *)
            if [[ -z "$BUILD_DIR" ]]; then
                BUILD_DIR="$1"
            else
                echo "Unexpected positional argument: $1" >&2
                exit 2
            fi
            shift
            ;;
    esac
done

# -----------------------------------------------------------------------------
# Locate the repo root and the build directory
# -----------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

if [[ -z "$BUILD_DIR" ]]; then
    # Prefer the AMD HIP build dir if it exists, then a generic build/.
    for candidate in build_rocm build_hip build; do
        if [[ -d "$REPO_ROOT/$candidate/unit-tests/build/algo/projection" ]]; then
            BUILD_DIR="$REPO_ROOT/$candidate"
            break
        fi
    done
fi

if [[ -z "$BUILD_DIR" ]]; then
    cat >&2 <<EOF
ERROR: could not auto-detect a build directory.
Looked for build_rocm/, build_hip/, build/ under $REPO_ROOT.
Pass the build directory explicitly:
    $0 path/to/build_dir
EOF
    exit 2
fi

# Make BUILD_DIR absolute regardless of how the user supplied it.
BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"

TEST_BIN_DIR="$BUILD_DIR/unit-tests/build/algo/projection"
if [[ ! -d "$TEST_BIN_DIR" ]]; then
    echo "ERROR: $TEST_BIN_DIR does not exist." >&2
    echo "Did you configure with -DBUILD_UNIT_TESTS=ON and build the project?" >&2
    exit 2
fi

# -----------------------------------------------------------------------------
# Compose runtime library path so HIP / CUDA libs resolve when run directly
# (ctest does not propagate LD_LIBRARY_PATH on this project).
# -----------------------------------------------------------------------------
RUN_LD_PATH="$BUILD_DIR/lib:${LD_LIBRARY_PATH:-}"
if [[ -n "${ROCM_PATH:-}" && -d "$ROCM_PATH/lib" ]]; then
    RUN_LD_PATH="$ROCM_PATH/lib:$RUN_LD_PATH"
elif [[ -d "/opt/rocm-7.2.0/lib" ]]; then
    RUN_LD_PATH="/opt/rocm-7.2.0/lib:$RUN_LD_PATH"
elif [[ -d "/opt/rocm/lib" ]]; then
    RUN_LD_PATH="/opt/rocm/lib:$RUN_LD_PATH"
fi
export LD_LIBRARY_PATH="$RUN_LD_PATH"

# -----------------------------------------------------------------------------
# The three tests, in stable order
# -----------------------------------------------------------------------------
TESTS=(
    "test-algo-projection-distortion"
    "test-algo-projection-yuy2-conversion"
    "test-algo-projection-cuda-align"
)

echo "=========================================================================="
echo "librealsense GPU unit tests (PR #15074)"
echo "Build directory : $BUILD_DIR"
echo "Test directory  : $TEST_BIN_DIR"
echo "LD_LIBRARY_PATH : $LD_LIBRARY_PATH"
if [[ ${#CATCH_ARGS[@]} -gt 0 ]]; then
    echo "Catch2 args     : ${CATCH_ARGS[*]}"
fi
echo "=========================================================================="

# -----------------------------------------------------------------------------
# Run them, accumulate exit status
# -----------------------------------------------------------------------------
overall_rc=0
declare -a SUMMARY

for t in "${TESTS[@]}"; do
    bin="$TEST_BIN_DIR/$t"
    echo
    echo "------ $t ------"
    if [[ ! -x "$bin" ]]; then
        echo "  MISSING: $bin not found or not executable." >&2
        SUMMARY+=("MISSING  $t")
        overall_rc=2
        continue
    fi

    # Default to the compact reporter unless the caller already picked one.
    local_args=("${CATCH_ARGS[@]}")
    has_reporter=0
    for a in "${local_args[@]}"; do
        [[ "$a" == "--reporter" ]] && has_reporter=1 && break
    done
    if [[ $has_reporter -eq 0 ]]; then
        local_args+=("--reporter" "compact")
    fi

    "$bin" "${local_args[@]}"
    rc=$?
    if [[ $rc -eq 0 ]]; then
        SUMMARY+=("PASS     $t")
    else
        SUMMARY+=("FAIL($rc) $t")
        overall_rc=1
    fi
done

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
echo
echo "=========================================================================="
echo "Summary"
echo "--------------------------------------------------------------------------"
for line in "${SUMMARY[@]}"; do
    echo "  $line"
done
echo "=========================================================================="

if [[ $overall_rc -eq 0 ]]; then
    echo "All GPU tests PASSED (or SKIPPED when no GPU is visible)."
else
    echo "GPU tests FAILED.  See per-test output above."
fi
exit $overall_rc
