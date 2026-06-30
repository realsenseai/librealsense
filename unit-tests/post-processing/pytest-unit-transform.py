# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pyrealsense2 as rs
import numpy as np

################################################################################################
def test_unit_transform():
    W = 640
    H = 480
    BPP = 2
    expected_frames = 1
    fps = 60
    depth_unit = 1.5
    ut = rs.units_transform()
    intrinsics = rs.intrinsics()
    intrinsics.width = W
    intrinsics.height = H
    intrinsics.ppx = 0
    intrinsics.ppy = 0
    intrinsics.fx = 0
    intrinsics.fy = 0
    intrinsics.model = rs.distortion.none
    intrinsics.coeffs = [0, 0, 0, 0, 0]

    sd = rs.software_device()
    software_sensor = sd.add_sensor("software_sensor")
    software_sensor.add_read_only_option(rs.option.depth_units, depth_unit)

    vs = rs.video_stream()
    vs.type = rs.stream.depth
    vs.index = 0
    vs.uid = 0
    vs.width = W
    vs.height = H
    vs.fps = fps
    vs.bpp = BPP
    vs.fmt = rs.format.z16
    vs.intrinsics = intrinsics
    software_sensor.add_video_stream(vs)

    profiles = software_sensor.get_stream_profiles()
    depth = profiles[0].as_video_stream_profile()

    sync = rs.syncer()
    software_sensor.open(profiles)
    software_sensor.start(sync)

    pixels = np.array([(i % 10) for i in range(W*H)], dtype=np.uint16)

    for i in range(expected_frames):
        frame = rs.software_video_frame()
        frame.pixels = pixels
        frame.bpp = 2
        frame.stride = frame.bpp * W
        frame.timestamp = float((i + 1) * 100)
        frame.domain = rs.timestamp_domain.hardware_clock
        frame.frame_number = i + 1
        frame.profile = depth
        software_sensor.on_video_frame(frame)

        synced_f = sync.wait_for_frames()
        f = synced_f.get_depth_frame()

        f_format = f.get_profile().format()
        assert rs.format.z16 == f_format

        depth_distance = ut.process(f)

        origin_frame = np.hstack(np.asarray(f.get_data(), dtype=np.uint16))
        ut_frame = np.hstack(np.asarray(depth_distance.get_data())).view(dtype=np.float32)

        depth_distance_format = depth_distance.get_profile().format()
        assert rs.format.distance == depth_distance_format

        expected_units_frame = (origin_frame * depth_unit).astype(np.float32)
        assert np.array_equal(ut_frame, expected_units_frame)
