# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
rspy.pytest — Modular pytest infrastructure for RealSense unit tests.

Sub-modules:
- logging_setup: Per-test log files, build dir detection, rspy.log bridging
- cli: Legacy CLI flag translation (e.g. -r/--regex → -k)
- device_helpers: Device resolution from markers and CLI filters
- collection: Test filtering and sorting
"""
