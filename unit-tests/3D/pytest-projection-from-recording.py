# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
from pytest_check import check133
import pyrealsense2 as rs
from rspy import repo11
import os.path

# disabled until LRS-986 ticket is resolved due to stability issues
pytestmark = [pytest.mark.skip(reason="LRS-986: disabled due to stability issues")]


################################################################################################
def test_projection_from_recording():
    filename = os.path.join(repo.build, 'unit-tests', 'recordings', 'single_depth_color_640x480.bag')
    ctx = rs.context()
    dev = ctx.load_device(filename)
    dev.set_real_time(False)

    syncer = rs.syncer()
    sensors = dev.query_sensors()
    for s in sensors:
        s.open(s.get_stream_profiles()[0])
        s.start(syncer)

    depth = rs.frame()
    depth_profile = rs.stream_profile()
    color_profile = rs.stream_profile()
    while not depth_profile or not color_profile:
        frames = syncer.wait_for_frames()
        check.is_true(frames.size() > 0)
        if frames.size() == 1:
            if frames.get_profile().stream_type() == rs.stream.depth:
                depth = frames.get_depth_frame()
                depth_profile = depth.get_profile()
            else:
                color_profile = frames.get_color_frame().get_profile()
        else:
            depth = frames.get_depth_frame()
            depth_profile = depth.get_profile()
            color_profile = frames.get_color_frame().get_profile()

    depth_intrin = depth_profile.as_video_stream_profile().get_intrinsics()
    color_intrin = color_profile.as_video_stream_profile().get_intrinsics()
    depth_extrin_to_color = depth_profile.as_video_stream_profile().get_extrinsics_to(color_profile)
    color_extrin_to_depth = color_profile.as_video_stream_profile().get_extrinsics_to(depth_profile)

    depth_scale = 0
    for s in sensors:
        depth_sensor = s.is_depth_sensor()
        if depth_sensor:
            depth_scale = s.as_depth_sensor().get_depth_scale()

        check.equal(s.get_info(rs.camera_info.name) == "Stereo Module", s.is_depth_sensor())
        check.equal(s.get_info(rs.camera_info.name) == "RGB Camera", s.is_color_sensor())

    count = 0
    for i in range(depth_intrin.width):
        for j in range(depth_intrin.height):
            depth_pixel = [i,j]
            udist = depth.as_depth_frame().get_distance(int(depth_pixel[0]+0.5),int(depth_pixel[1]+0.5))
            if udist == 0:
                continue

            point = rs.rs2_deproject_pixel_to_point(depth_intrin, depth_pixel, udist)
            other_point = rs.rs2_transform_point_to_point(depth_extrin_to_color, point)
            from_pixel = rs.rs2_project_point_to_pixel(color_intrin, other_point)

            #Search along a projected beam from 0.1m to 10 meter
            to_pixel = rs.rs2_project_color_pixel_to_depth_pixel(depth.get_data(), depth_scale, 0.1,10,depth_intrin,  color_intrin,color_extrin_to_depth, depth_extrin_to_color, from_pixel)

            dist = ((depth_pixel[1] - to_pixel[1]) ** 2 + (depth_pixel[0] - to_pixel[0]) ** 2) ** 0.5
            if dist > 1:
                count += 1

    MAX_ERROR_PERCENTAGE = 0.1
    check.is_true(count * 100 / (depth_intrin.width * depth_intrin.height) < MAX_ERROR_PERCENTAGE)

    for s in sensors:
        s.stop()
        s.close()
