# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Ratchet check for the pyrealsense2 type stubs. Run with the interpreter that has the
pyrealsense2 wheel, pybind11-stubgen and pyright installed:

    python check_stubs.py

Fails when pybind11-stubgen reports an error missing from known_errors.txt, when a
listed error no longer occurs (remove it from the list), or when pyright finds an
error in api_usage.py against the stubs shipped in the installed wheel.
"""
import re
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent


def stubgen_errors():
    with tempfile.TemporaryDirectory() as tmp:
        result = subprocess.run([sys.executable, '-m', 'pybind11_stubgen', 'pyrealsense2.pyrealsense2', '--dry-run',
                                 '--numpy-array-wrap-with-annotated'], cwd=tmp, capture_output=True, text=True)
    errors = set()
    for line in (result.stdout + result.stderr).splitlines():
        match = re.search(r'\[\s*ERROR\] (.*)', line)
        if match:
            errors.add(re.sub(r'0x[0-9a-f]+', '0x...', match.group(1).strip()))
    if result.returncode:
        sys.exit(result.stderr)
    return errors


def known_errors():
    lines = (HERE / 'known_errors.txt').read_text().splitlines()
    return {line.strip() for line in lines if line.strip()}


def main():
    errors, known = stubgen_errors(), known_errors()
    new, fixed = sorted(errors - known), sorted(known - errors)
    for error in new:
        print('NEW stubgen error: ' + error)
    for error in fixed:
        print('FIXED, remove from known_errors.txt: ' + error)
    print('stubgen: {} errors, {} new, {} fixed'.format(len(errors), len(new), len(fixed)))

    with tempfile.TemporaryDirectory() as tmp:
        pyright = subprocess.call([sys.executable, '-m', 'pyright', '--pythonpath', sys.executable,
                                   str(HERE / 'api_usage.py')], cwd=tmp)

    sys.exit(1 if new or fixed or pyright else 0)


if __name__ == '__main__':
    main()
