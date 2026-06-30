# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Run the full rest-api test suite: pytest backend tests + react-viewer Vitest tests.

Usage:
    python run_tests.py            # run both suites
    python run_tests.py --backend  # backend pytest only
    python run_tests.py --frontend # react-viewer Vitest only
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

REST_API_DIR = Path(__file__).resolve().parent
REACT_VIEWER_DIR = REST_API_DIR / "tools" / "react-viewer"


def run_backend() -> int:
    print("=" * 60)
    print("Running backend pytest tests")
    print("=" * 60)
    return subprocess.call([sys.executable, "-m", "pytest", "tests/", "-v"], cwd=REST_API_DIR)


def run_frontend() -> int:
    print("=" * 60)
    print("Running react-viewer Vitest tests")
    print("=" * 60)
    if not (REACT_VIEWER_DIR / "node_modules").exists():
        print(f"node_modules missing in {REACT_VIEWER_DIR} - run 'npm install' first.")
        return 1
    npm = shutil.which("npm") or shutil.which("npm.cmd")
    if not npm:
        print("npm not found on PATH.")
        return 1
    return subprocess.call([npm, "test"], cwd=REACT_VIEWER_DIR, shell=False)


def main() -> int:
    parser = argparse.ArgumentParser(description="Run rest-api and react-viewer tests.")
    parser.add_argument("--backend", action="store_true", help="Run only backend pytest tests")
    parser.add_argument("--frontend", action="store_true", help="Run only react-viewer Vitest tests")
    args = parser.parse_args()

    run_both = not (args.backend or args.frontend)
    rc = 0
    if args.backend or run_both:
        rc |= run_backend()
    if args.frontend or run_both:
        rc |= run_frontend()
    return rc


if __name__ == "__main__":
    sys.exit(main())
