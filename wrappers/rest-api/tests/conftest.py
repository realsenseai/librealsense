# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Select the repo-built pyrealsense2 before any test imports the module: main.py prefers
build/Release over a wheel in the venv, and a module imported first wins for the process."""

import main  # noqa: F401
