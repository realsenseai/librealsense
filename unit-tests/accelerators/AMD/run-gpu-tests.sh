#!/usr/bin/env bash
# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

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
# Requires a static build (-DBUILD_SHARED_LIBS=OFF). The three tests carry
# `//#cmake: static!` and #include internal src/proc/cuda/*.cuh headers whose
# symbols are visibility=hidden in the shared-library ABI, so
# unit-tests/CMakeLists.txt gates them behind `if(NOT BUILD_SHARED_LIBS)` and
# they are simply absent from a shared build.
#
# Usage:
#   ./unit-tests/accelerators/AMD/run-gpu-tests.sh                  # auto-detect build dir
#   ./unit-tests/accelerators/AMD/run-gpu-tests.sh build_rocm       # explicit build dir
#   ./unit-tests/accelerators/AMD/run-gpu-tests.sh --filter "small" # pass extra args to Catch2
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
# Script lives at unit-tests/accelerators/AMD/; the repo root is three levels up.
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

# Sentinel used for both auto-detection and the missing-binary hint below.
SENTINEL_BIN="Release/test-algo-projection-distortion"

if [[ -z "$BUILD_DIR" ]]; then
    # Prefer the AMD HIP build dir if it exists, then a generic build/.
    for candidate in build_rocm build_hip build; do
        if [[ -x "$REPO_ROOT/$candidate/$SENTINEL_BIN" ]]; then
            BUILD_DIR="$REPO_ROOT/$candidate"
            break
        fi
    done
fi

if [[ -z "$BUILD_DIR" ]]; then
    cat >&2 <<EOF
ERROR: could not auto-detect a build directory containing the GPU test binaries.
Looked for $SENTINEL_BIN under build_rocm/, build_hip/, build/ in $REPO_ROOT.
Pass the build directory explicitly:
    $0 path/to/build_dir
Note: these tests only exist in a static build. Configure with
    -DBUILD_UNIT_TESTS=ON -DBUILD_SHARED_LIBS=OFF
and rebuild -- see the file header for details.
EOF
    exit 2
fi

# Make BUILD_DIR absolute regardless of how the user supplied it.
BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"

# librealsense outputs every executable target to <build>/Release/ by default
# (see CMake/global_config.cmake: OUTPUT_DIR = ${CMAKE_BINARY_DIR}/Release).
# The per-test tree under unit-tests/build/algo/projection/*/CMakeFiles/ only
# holds intermediate object files, not the linked binaries.
TEST_BIN_DIR="$BUILD_DIR/Release"
if [[ ! -d "$TEST_BIN_DIR" ]]; then
    echo "ERROR: $TEST_BIN_DIR does not exist." >&2
    echo "Did you configure with -DBUILD_UNIT_TESTS=ON and build the project?" >&2
    exit 2
fi

# Fail fast with a specific hint when the binaries are absent: the most common
# cause is a shared build, in which case the three tests are gated out entirely
# by unit-tests/CMakeLists.txt and never produced -- reporting three MISSING
# lines below would hide the real reason.
if [[ ! -x "$TEST_BIN_DIR/test-algo-projection-distortion" ]]; then
    cat >&2 <<EOF
ERROR: expected test binaries not found under $TEST_BIN_DIR.
These tests are gated behind 'if(NOT BUILD_SHARED_LIBS)' in unit-tests/CMakeLists.txt
(they carry //#cmake: static! and reference internal src/proc/cuda/*.cuh symbols
whose visibility is hidden in the shared-library ABI). Reconfigure with
    -DBUILD_UNIT_TESTS=ON -DBUILD_SHARED_LIBS=OFF
and rebuild.
EOF
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
