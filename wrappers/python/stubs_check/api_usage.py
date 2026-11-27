# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from typing_extensions import assert_type

import pyrealsense2 as rs

assert_type(rs.__version__, str)

devices = rs.context().query_devices()
assert_type(devices, rs.device_list)
dev = devices[0]
assert_type(dev, rs.device)
assert_type(dev.get_info(rs.camera_info.serial_number), str)
depth_sensor = dev.first_depth_sensor()
assert_type(depth_sensor, rs.depth_sensor)
assert_type(depth_sensor.get_depth_scale(), float)
assert_type(depth_sensor.get_option(rs.option.exposure), float)

cfg = rs.config()
cfg.enable_stream(rs.stream.depth, 640, 480, rs.format.z16, 30)
pipe = rs.pipeline()
assert_type(pipe.start(cfg), rs.pipeline_profile)

frames = pipe.wait_for_frames()
assert_type(frames, rs.composite_frame)
assert_type(frames[0], rs.frame)
assert_type(frames.first(rs.stream.color), rs.frame)
depth = frames.get_depth_frame()
assert_type(depth, rs.depth_frame)
assert_type(depth.get_distance(320, 240), float)
assert_type(depth.get_width(), int)

intrinsics = rs.video_stream_profile(depth.get_profile()).get_intrinsics()
assert_type(intrinsics, rs.intrinsics)
assert_type(intrinsics.fx, float)
assert_type(rs.rs2_deproject_pixel_to_point(intrinsics, [320.0, 240.0], 1.0), list[float])

assert_type(rs.align(rs.stream.color).process(frames), rs.composite_frame)
assert_type(rs.pointcloud().calculate(depth), rs.points)
assert_type(rs.colorizer().colorize(depth), rs.video_frame)

pipe.stop()
