## License: Apache 2.0. See LICENSE file in root directory.
## Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Build pyrealsense2 wheels for every installed Python 3 interpreter.

This is the multi-interpreter companion to build_wheel.py. It configures and
builds the SDK once per interpreter, then packages each extension into its own
wheel, so one checkout can produce wheels for e.g. Python 3.10 through 3.15.

Usage:
    python build_wheels.py
    python build_wheels.py --python /usr/bin/python3.14 --python /usr/bin/python3.15
    python build_wheels.py --rsusb --clean

The script works on Linux (including NVIDIA Jetson/aarch64), macOS and Windows.
It only uses the Python standard library; each target interpreter must have its
development headers (Python.h) installed.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent       # wrappers/python/
LIBREALSENSE_ROOT = (SCRIPT_DIR / ".." / "..").resolve()

# uv installs each interpreter as <prefix>/bin/python3, python3.14, python3.14t
# (free-threaded). Matching the exact interpreter names keeps helper scripts
# such as python3.14-config out of the candidate list.
UV_INTERPRETER = re.compile(r"^python3(\.\d+)?t?$")

PROBE = (
    "import json, sys, sysconfig\n"
    "print(json.dumps({\n"
    "    'version': list(sys.version_info[:3]),\n"
    "    'executable': sys.executable,\n"
    "    'include': sysconfig.get_paths().get('include', ''),\n"
    "    'platform': sysconfig.get_platform(),\n"
    "}))\n"
)


class PythonInfo(object):
    def __init__(self, executable, version, include, platform):
        self.executable = executable
        self.version = version
        self.include = include
        self.platform = platform

    @property
    def short_version(self):
        return "{}.{}".format(self.version[0], self.version[1])

    @property
    def tag(self):
        return "cp{}{}".format(self.version[0], self.version[1])

    def __str__(self):
        return "Python {}.{}.{} ({})".format(
            self.version[0], self.version[1], self.version[2], self.executable
        )


def run(cmd, **kwargs):
    print("+ {}".format(" ".join(str(c) for c in cmd)))
    return subprocess.check_call([str(c) for c in cmd], **kwargs)


def capture(cmd):
    return subprocess.check_output([str(c) for c in cmd], stderr=subprocess.STDOUT)


def probe_python(executable):
    try:
        raw = capture([executable, "-c", PROBE])
    except (OSError, subprocess.CalledProcessError) as exc:
        print("  skipping {}: cannot run interpreter ({})".format(executable, exc))
        return None

    try:
        data = json.loads(raw.decode("utf-8", "replace").strip().splitlines()[-1])
    except (ValueError, IndexError):
        print("  skipping {}: unexpected interpreter output".format(executable))
        return None

    version = tuple(data["version"])
    if version < (3, 9):
        print("  skipping {}: Python {}.{} is older than 3.9".format(
            executable, version[0], version[1]))
        return None

    include = data.get("include") or ""
    if not include or not (Path(include) / "Python.h").is_file():
        print("  skipping {}: development headers not found (Python.h)".format(executable))
        return None

    return PythonInfo(str(Path(executable).resolve()), version, include, data.get("platform", ""))


def uv_candidates():
    """Interpreters installed by uv are not necessarily on PATH."""
    root = Path.home() / ".local" / "share" / "uv" / "python"
    if not root.is_dir():
        return []
    found = []
    for path in sorted(root.glob("cpython-3.*-*/bin/python3*")):
        if not UV_INTERPRETER.match(path.name):
            continue
        if path.is_file() and os.access(str(path), os.X_OK):
            found.append(str(path))
    return found


def discover_pythons():
    candidates = []
    seen = set()

    def add(path):
        if not path:
            return
        resolved = str(Path(path).resolve())
        if resolved in seen:
            return
        seen.add(resolved)
        candidates.append(resolved)

    add(sys.executable)
    for minor in range(20, 8, -1):
        add(shutil.which("python3.{}".format(minor)))
        add(shutil.which("python3.{}t".format(minor)))
    add(shutil.which("python3"))
    for path in uv_candidates():
        add(path)

    result = []
    for path in candidates:
        info = probe_python(path)
        if info is not None:
            result.append(info)
    return result


def cmake_command(cmake, source, build_dir, python, args):
    cmd = [
        cmake,
        "-S", str(source),
        "-B", str(build_dir),
        "-DCMAKE_BUILD_TYPE=Release",
        "-DBUILD_PYTHON_BINDINGS=ON",
        "-DPython_EXECUTABLE={}".format(python.executable),
        "-DBUILD_SHARED_LIBS=ON",
        "-DBUILD_EXAMPLES=OFF",
        "-DBUILD_GRAPHICAL_EXAMPLES=OFF",
        "-DBUILD_TOOLS=OFF",
        "-DBUILD_WITH_DDS=OFF",
        "-DBUILD_PC_STITCHING=OFF",
        "-DBUILD_UNIT_TESTS=OFF",
        "-DCHECK_FOR_UPDATES=OFF",
        "-DENABLE_AI_ASSISTANT=OFF",
        "-DENABLE_STATS=OFF",
    ]
    if args.rsusb:
        cmd.append("-DFORCE_RSUSB_BACKEND=ON")
    if args.generator:
        cmd.extend(["-G", args.generator])
    for extra in args.extra_cmake_arg:
        cmd.append(extra)
    return cmd


def build_one(python, args):
    build_dir = args.build_root / "py{}".format(python.short_version)
    print()
    print("== {} ==".format(python))
    print("   build dir: {}".format(build_dir))

    if args.clean and build_dir.exists():
        print("   removing {}".format(build_dir))
        shutil.rmtree(str(build_dir))
    build_dir.mkdir(parents=True, exist_ok=True)

    run(cmake_command(args.cmake, args.source, build_dir, python, args))

    build_cmd = [
        args.cmake,
        "--build", str(build_dir),
        "--config", "Release",
        "--target", "pyrealsense2",
        "--parallel", str(args.jobs),
    ]
    run(build_cmd)

    if args.skip_wheel:
        return None

    wheel_script = args.source / "wrappers" / "python" / "build_wheel.py"
    run([python.executable, str(wheel_script), "--build-dir", str(build_dir)],
        cwd=str(args.source / "wrappers" / "python"))
    return build_dir


def find_wheels():
    dist = SCRIPT_DIR / "dist"
    if not dist.is_dir():
        return []
    return sorted(dist.glob("pyrealsense2-*.whl"), key=lambda p: p.stat().st_mtime)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--python", action="append", default=[], metavar="PATH",
                        help="Python interpreter to build for (repeatable). "
                             "Default: every Python 3.9+ found on PATH and in uv.")
    parser.add_argument("--source", type=lambda p: Path(p).expanduser().resolve(),
                        default=LIBREALSENSE_ROOT, help="librealsense checkout")
    parser.add_argument("--build-root", type=lambda p: Path(p).expanduser().resolve(),
                        default=LIBREALSENSE_ROOT / "build" / "python-wheels",
                        help="Directory for per-version CMake build trees")
    parser.add_argument("--cmake", default=os.environ.get("CMAKE", "cmake"),
                        help="CMake executable (default: cmake)")
    parser.add_argument("--generator", default=None,
                        help="CMake generator, e.g. Ninja (default: let CMake choose)")
    parser.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 1,
                        help="Parallel build jobs")
    parser.add_argument("--rsusb", action="store_true",
                        help="Force the RSUSB (libusb) backend; recommended on Jetson "
                             "unless the native kernel patches are installed")
    parser.add_argument("--clean", action="store_true",
                        help="Remove each per-version build tree before configuring")
    parser.add_argument("--skip-wheel", action="store_true",
                        help="Build the extension modules but do not package wheels")
    parser.add_argument("--list", action="store_true",
                        help="List usable interpreters and exit")
    parser.add_argument("--extra-cmake-arg", action="append", default=[],
                        metavar="ARG", help="Additional CMake argument (repeatable)")
    args = parser.parse_args()

    if not (args.source / "CMakeLists.txt").is_file():
        parser.error("--source does not look like a librealsense checkout: {}".format(args.source))

    if args.python:
        pythons = []
        for executable in args.python:
            info = probe_python(executable)
            if info is None:
                parser.error("cannot use {}".format(executable))
            pythons.append(info)
    else:
        pythons = discover_pythons()

    if not pythons:
        parser.error("no usable Python 3.9+ interpreter found; pass --python")

    if args.list:
        for python in pythons:
            print(python)
        return 0

    print("Source: {}".format(args.source))
    print("Build root: {}".format(args.build_root))
    print("Interpreters:")
    for python in pythons:
        print("  - {}".format(python))

    failures = []
    for python in pythons:
        try:
            build_one(python, args)
        except subprocess.CalledProcessError as exc:
            failures.append((python, exc.returncode))
            print("FAILED: {} (exit {})".format(python, exc.returncode))

    print()
    if not args.skip_wheel:
        wheels = find_wheels()
        if wheels:
            print("Wheels:")
            for wheel in wheels:
                print("  {}".format(wheel))

    if failures:
        print()
        print("Failures:")
        for python, code in failures:
            print("  {} (exit {})".format(python, code))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
