## License: Apache 2.0. See LICENSE file in root directory.
## Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Build a pyrealsense2 wheel locally from a CMake build directory.

Usage:
    python build_wheel.py --build-dir <path-to-cmake-build>

Examples (run from wrappers/python/):
    # Windows multi-config build
    python build_wheel.py --build-dir ../../build/Release

    # Linux/macOS single-config build
    python build_wheel.py --build-dir ../../build

The script:
  1. Generates pyrealsense2/_version.py (via find_librs_version.py)
  2. Copies the compiled extension (.pyd/.so) and the librealsense2 shared
     library out of the build dir into pyrealsense2/
  3. Runs `python -m build --wheel`, producing dist/pyrealsense2-*.whl

Use --extra-lib to bundle additional runtime dependencies (e.g. DDS libs).
"""
import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent       # wrappers/python/
PACKAGE_DIR = SCRIPT_DIR / "pyrealsense2"
LIBREALSENSE_ROOT = (SCRIPT_DIR / ".." / "..").resolve()

# Files we manage in the package dir — safe to remove between runs.
# __init__.py is committed and must NOT be touched.
STAGED_SUFFIXES = (".pyd", ".dll", ".so", ".dylib")
STAGED_NAMES = ("_version.py",)


def platform_patterns():
    system = platform.system()
    if system == "Windows":
        return "pyrealsense2*.pyd", ["realsense2.dll"]
    if system == "Darwin":
        return "pyrealsense2*.so", ["librealsense2*.dylib"]
    return "pyrealsense2*.so", ["librealsense2.so*"]


def find_artifacts(build_dir, extra_patterns):
    ext_pattern, runtime_patterns = platform_patterns()
    runtime_patterns = list(runtime_patterns) + list(extra_patterns)

    ext_matches = list(build_dir.rglob(ext_pattern))
    if not ext_matches:
        raise FileNotFoundError(
            "No '{}' found under {}. Did the build complete?".format(ext_pattern, build_dir)
        )
    if len(ext_matches) > 1:
        print("Warning: multiple {} found, using first:".format(ext_pattern))
        for m in ext_matches:
            print("  {}".format(m))
    extension = ext_matches[0]

    runtime_libs = []
    seen = set()
    for pattern in runtime_patterns:
        for lib in build_dir.rglob(pattern):
            if lib.name in seen:
                continue
            seen.add(lib.name)
            runtime_libs.append(lib)
    return extension, runtime_libs


def clean_staged():
    for f in PACKAGE_DIR.iterdir():
        if f.name == "__init__.py":
            continue
        if f.name in STAGED_NAMES or f.suffix in STAGED_SUFFIXES or ".so." in f.name:
            print("  removing stale: {}".format(f.name))
            if f.is_symlink() or f.exists():
                f.unlink()


def stage(src):
    # Resolve symlinks so the wheel ends up with real files. Wheels are zips
    # and don't preserve symlinks reliably; a versioned `librealsense2.so.2`
    # symlink would otherwise land in the wheel as a broken pointer back into
    # the build tree.
    dest = PACKAGE_DIR / src.name
    if dest.exists() or dest.is_symlink():
        dest.unlink()
    shutil.copy2(src.resolve(), dest, follow_symlinks=True)
    print("  staged: {}".format(src.name))


def generate_version_file():
    subprocess.check_call([
        sys.executable,
        str(SCRIPT_DIR / "find_librs_version.py"),
        str(LIBREALSENSE_ROOT),
        str(PACKAGE_DIR),
    ])


def ensure_build_tool():
    try:
        subprocess.check_call(
            [sys.executable, "-m", "build", "--version"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("Installing 'build' and 'hatchling'...")
        subprocess.check_call([sys.executable, "-m", "pip", "install", "build", "hatchling"])


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--build-dir",
        required=True,
        type=lambda p: Path(p).expanduser().resolve(),
        help="CMake build directory containing the compiled pyrealsense2 extension",
    )
    parser.add_argument(
        "--extra-lib",
        action="append",
        default=[],
        metavar="GLOB",
        help="Additional runtime library glob to bundle (repeatable), e.g. fastdds*.dll",
    )
    args = parser.parse_args()

    if not args.build_dir.exists():
        sys.exit("Error: build dir not found: {}".format(args.build_dir))

    print("Source root:  {}".format(LIBREALSENSE_ROOT))
    print("Build dir:    {}".format(args.build_dir))
    print("Package dir:  {}".format(PACKAGE_DIR))
    print()

    print("Locating build artifacts...")
    extension, runtime_libs = find_artifacts(args.build_dir, args.extra_lib)
    print("  extension: {}".format(extension))
    for lib in runtime_libs:
        print("  runtime:   {}".format(lib))
    if not runtime_libs:
        print("  (no runtime libs matched — extension may fail to load on the target machine)")
    print()

    print("Cleaning previously-staged files...")
    clean_staged()
    print()

    print("Generating _version.py...")
    generate_version_file()
    print()

    print("Staging into package...")
    stage(extension)
    for lib in runtime_libs:
        stage(lib)
    print()

    print("Ensuring 'build' is available...")
    ensure_build_tool()
    print()

    print("Running `python -m build --wheel`...")
    subprocess.check_call([sys.executable, "-m", "build", "--wheel"], cwd=str(SCRIPT_DIR))
    print()

    dist = SCRIPT_DIR / "dist"
    wheels = sorted(dist.glob("pyrealsense2-*.whl"), key=lambda p: p.stat().st_mtime, reverse=True)
    if not wheels:
        sys.exit("Build completed but no wheel found in {}".format(dist))

    print("Success! Wheel: {}".format(wheels[0]))
    print()
    print("Install on the target machine with:")
    print("  pip install {}".format(wheels[0].name))


if __name__ == "__main__":
    main()
