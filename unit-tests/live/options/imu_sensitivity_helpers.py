# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Shared by the IMU sensitivity tests: the SDK leaves the last written level in FW (seen by the next application)."""

import pyrealsense2 as rs


def stream_imu_once(dev, ctx):
    pipe = rs.pipeline(ctx)
    pipe.set_device(dev)
    cfg = rs.config()
    cfg.enable_stream(rs.stream.accel)
    cfg.enable_stream(rs.stream.gyro)
    pipe.start(cfg)
    pipe.stop()


def restore_sensitivity(dev, ctx, original):
    """Write back the {option: value} levels read before a test, and stream once so they reach FW."""
    ms = dev.first_motion_sensor()
    for opt, value in original.items():
        ms.set_option(opt, value)
    if original:
        stream_imu_once(dev, ctx)
