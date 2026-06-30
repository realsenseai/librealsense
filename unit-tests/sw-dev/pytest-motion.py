# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pyrealsense2 as rs

################################################################################################
def test_software_device_motion_stream():
    sd = rs.software_device()
    sensor = sd.add_sensor("Motion")

    intrinsics = rs.motion_device_intrinsic()
    intrinsics.data = [[1.0] * 4] * 3
    intrinsics.noise_variances = [2, 2, 2]
    intrinsics.bias_variances = [3, 3, 3]

    stream = rs.motion_stream()
    stream.type = rs.stream.accel
    stream.index = 0
    stream.uid = 0
    stream.fps = 200
    stream.fmt = rs.format.motion_raw
    stream.intrinsics = intrinsics

    stream_profile = sensor.add_motion_stream(stream).as_motion_stream_profile()

    sync = rs.syncer()

    sensor.open(stream_profile)
    sensor.start(sync)

    frame = rs.software_motion_frame()
    motion_frame_data = rs.vector()
    motion_frame_data.x = 0.111
    motion_frame_data.y = 0.222
    motion_frame_data.z = 0.333
    frame.data = motion_frame_data
    frame.timestamp = 20000
    frame.domain = rs.timestamp_domain.hardware_clock
    frame.frame_number = 0
    frame.profile = stream_profile
    sensor.on_motion_frame(frame)

    fset = sync.wait_for_frames()
    motion = fset.first_or_default(rs.stream.accel)
    motion_data = motion.as_motion_frame().get_motion_data()

    assert frame.data.x == motion_data.x
    assert frame.data.y == motion_data.y
    assert frame.data.z == motion_data.z
    assert frame.frame_number == motion.get_frame_number()
    assert frame.domain == motion.get_frame_timestamp_domain()
    assert frame.timestamp == motion.get_timestamp()
