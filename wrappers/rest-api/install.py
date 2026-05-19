# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Install rest-api Python dependencies.

Installs pyrealsense2 from PyPI only if it is not already importable - this
avoids clobbering a locally-built or apt-installed pyrealsense2. Then runs the
regular pip install of requirements.txt.

Usage:
    python3 install.py
"""

import subprocess
import sys
from pathlib import Path


def main() -> int:
    here = Path(__file__).resolve().parent
    try:
        import pyrealsense2  # noqa: F401
        print("pyrealsense2 already available - skipping PyPI install")
    except ImportError:
        print("pyrealsense2 not found - installing from PyPI")
        subprocess.check_call([sys.executable, "-m", "pip", "install", "pyrealsense2"])

    print("Installing rest-api requirements")
    subprocess.check_call([sys.executable, "-m", "pip", "install", "-r", str(here / "requirements.txt")])
    return 0


if __name__ == "__main__":
    sys.exit(main())
