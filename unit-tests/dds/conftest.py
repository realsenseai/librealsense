# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

try:
    import pyrealdds  # noqa: F401
except ImportError:
    collect_ignore_glob = ['pytest-*.py', 'adapter/pytest-*.py']
