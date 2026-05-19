#!/usr/bin/env bash
# Complete RealSense Viewer Build Script (Linux / macOS)
# Builds: FastAPI executable + React UI + Tauri bundles
# Output: artifacts under <repo-root>/build/

set -euo pipefail

CLEAN=false
HELP=false

for arg in "$@"; do
    case $arg in
        --clean) CLEAN=true ;;
        --help|-h) HELP=true ;;
    esac
done

if $HELP; then
    cat <<'EOF'
RealSense Viewer - Complete Build Script
========================================

Usage:
  ./build-all.sh             # Build everything
  ./build-all.sh --clean     # Clean and rebuild
  ./build-all.sh --help      # Show this help

Output Locations (relative to the repository root):
  FastAPI executable:  build/rest-api-dist/realsense_api/
  React build:         wrappers/rest-api/tools/react-viewer/dist/
  Tauri bundles:       build/tauri-target/release/bundle/
                       (.deb, .AppImage on Linux; .dmg on macOS)

Requirements:
  - Node.js 18+
  - Python 3.8+
  - Rust 1.56+ (install from https://rustup.rs/)
  - PyInstaller (pip install pyinstaller)
EOF
    exit 0
fi

GREEN=$'\033[0;32m'
RED=$'\033[0;31m'
YELLOW=$'\033[0;33m'
CYAN=$'\033[0;36m'
RESET=$'\033[0m'

ok()   { printf '%b\n' "${GREEN}[OK] $*${RESET}"; }
err()  { printf '%b\n' "${RED}[ERROR] $*${RESET}" >&2; }
warn() { printf '%b\n' "${YELLOW}[WARN] $*${RESET}"; }
info() { printf '%b\n' "${CYAN}[INFO] $*${RESET}"; }

START_TIME=$(date +%s)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REST_API_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

REST_API_OUTPUT="$PROJECT_ROOT/build/rest-api-dist"
REST_API_WORK="$PROJECT_ROOT/build/rest-api-work"
TAURI_RESOURCES="$PROJECT_ROOT/build/tauri-resources"
CARGO_TARGET="$PROJECT_ROOT/build/tauri-target"

echo "============================================================"
echo "  RealSense Viewer - Complete Build"
echo "============================================================"
echo

# Step 1: Build FastAPI executable
info "Step 1/3: Building FastAPI executable with PyInstaller..."

if $CLEAN; then
    warn "Cleaning FastAPI build artifacts..."
    rm -rf "$REST_API_OUTPUT" "$REST_API_WORK"
    rm -rf "$REST_API_DIR/build" "$REST_API_DIR/dist"
    find "$REST_API_DIR" -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
fi

PYTHON_BIN="${PYTHON:-python3}"
if ! "$PYTHON_BIN" -c "import PyInstaller" >/dev/null 2>&1; then
    err "PyInstaller not found for $PYTHON_BIN. Install with: $PYTHON_BIN -m pip install pyinstaller"
    exit 1
fi

mkdir -p "$REST_API_OUTPUT" "$REST_API_WORK"

step1_start=$(date +%s)
(
    cd "$REST_API_DIR"
    "$PYTHON_BIN" -m PyInstaller main.py --name realsense_api \
        --distpath "$REST_API_OUTPUT" \
        --workpath "$REST_API_WORK" -y
)
step1_end=$(date +%s)

if [ ! -f "$REST_API_OUTPUT/realsense_api/realsense_api" ]; then
    err "PyInstaller build failed: executable not found at $REST_API_OUTPUT/realsense_api/realsense_api"
    exit 1
fi
ok "FastAPI executable built in $((step1_end - step1_start))s"

info "Copying FastAPI bundle to Tauri staging resources..."
mkdir -p "$TAURI_RESOURCES"
rm -rf "$TAURI_RESOURCES/realsense_api"
cp -r "$REST_API_OUTPUT/realsense_api" "$TAURI_RESOURCES/"

# Clean any legacy in-source copy to keep the source tree tidy
rm -rf "$SCRIPT_DIR/src-tauri/resources/realsense_api"
ok "FastAPI bundle staged"

# Step 2: Build React UI
info "Step 2/3: Building React UI..."

if $CLEAN; then
    warn "Cleaning React build artifacts..."
    rm -rf "$SCRIPT_DIR/dist"
fi

step2_start=$(date +%s)
(
    cd "$SCRIPT_DIR"
    npm run build
)
step2_end=$(date +%s)
ok "React UI built in $((step2_end - step2_start))s"

# Step 3: Build Tauri bundles
info "Step 3/3: Building Tauri production bundles..."

# Make sure cargo is on PATH. If Rust was installed via rustup, it lives in
# ~/.cargo/bin which interactive shells get from ~/.cargo/env. Non-interactive
# shells (e.g. when this script runs from a fresh terminal where ~/.bashrc
# wasn't sourced) often miss it - source it here if available.
if ! command -v cargo >/dev/null 2>&1; then
    if [ -f "$HOME/.cargo/env" ]; then
        warn "cargo not on PATH; sourcing $HOME/.cargo/env"
        # shellcheck disable=SC1091
        source "$HOME/.cargo/env"
    fi
fi
if ! command -v cargo >/dev/null 2>&1; then
    err "cargo not found. Install Rust from https://rustup.rs/ then run:"
    err "  source \$HOME/.cargo/env"
    err "and retry this script."
    exit 1
fi

if [ -d "$SCRIPT_DIR/src-tauri/target" ]; then
    warn "Removing legacy src-tauri/target directory..."
    rm -rf "$SCRIPT_DIR/src-tauri/target"
fi

mkdir -p "$CARGO_TARGET"
export CARGO_TARGET_DIR="$CARGO_TARGET"
info "Setting CARGO_TARGET_DIR to: $CARGO_TARGET"

step3_start=$(date +%s)
(
    cd "$SCRIPT_DIR"
    npm run tauri:build
)
step3_end=$(date +%s)
ok "Tauri bundles created in $((step3_end - step3_start))s"

END_TIME=$(date +%s)
echo
echo "============================================================"
echo "  BUILD COMPLETE!"
echo "============================================================"
ok "Total build time: $((END_TIME - START_TIME))s"
echo
echo "Output Artifacts (relative to repository root):"
echo "  .deb:      build/tauri-target/release/bundle/deb/"
echo "  AppImage:  build/tauri-target/release/bundle/appimage/"
echo "  Binary:    build/tauri-target/release/realsense-viewer"
echo
echo "Next Steps:"
echo "  1. Install the .deb (sudo dpkg -i <pkg>.deb) or run the AppImage"
echo "  2. Launch 'RealSense Viewer'"
echo "  3. Verify connected cameras appear in the Device Panel"
