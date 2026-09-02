# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from pytest_check import check
import numpy as np
import pyrealsense2 as rs
from rspy import repo
import os.path

# a pixel whose 4-neighbors are missing, or further away than this, sits on a depth discontinuity -
# there the reverse (color -> depth) beam search is inherently ambiguous
MAX_NEIGHBOR_DIFF_METERS = 0.05


def interior_mask(depth_image, depth_scale):
    d = depth_image.astype(np.float32) * depth_scale
    height, width = d.shape
    center = d[1:-1, 1:-1]
    ok = center > 0
    for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
        neighbor = d[1 + dy : height - 1 + dy, 1 + dx : width - 1 + dx]
        ok &= (neighbor > 0) & (np.abs(center - neighbor) <= MAX_NEIGHBOR_DIFF_METERS)
    mask = np.zeros(d.shape, dtype=bool)    # the image border has no 4-neighborhood
    mask[1:-1, 1:-1] = ok
    return mask


################################################################################################
def test_projection_from_recording():
    filename = os.path.join(repo.build, 'unit-tests', 'recordings', 'single_depth_color_640x480.bag')
    ctx = rs.context()
    dev = ctx.load_device(filename)
    dev.set_real_time(False)

    depth_sensor = dev.first_depth_sensor()
    depth_profile = depth_sensor.get_stream_profiles()[0]
    color_profile = dev.first_color_sensor().get_stream_profiles()[0]

    # the recording holds more than one depth frame; queue them all so we always measure the first
    queue = rs.frame_queue(2)
    depth_sensor.open(depth_profile)
    depth_sensor.start(queue)
    try:
        depth = queue.wait_for_frame().as_depth_frame()
    finally:
        depth_sensor.stop()
        depth_sensor.close()

    depth_intrin = depth_profile.as_video_stream_profile().get_intrinsics()
    color_intrin = color_profile.as_video_stream_profile().get_intrinsics()
    depth_extrin_to_color = depth_profile.as_video_stream_profile().get_extrinsics_to(color_profile)
    color_extrin_to_depth = color_profile.as_video_stream_profile().get_extrinsics_to(depth_profile)

    depth_scale = depth_sensor.get_depth_scale()
    for s in dev.query_sensors():
        check.equal(s.get_info(rs.camera_info.name) == "Stereo Module", s.is_depth_sensor())
        check.equal(s.get_info(rs.camera_info.name) == "RGB Camera", s.is_color_sensor())

    depth_data = depth.get_data()
    mask = interior_mask(np.asanyarray(depth_data), depth_scale)

    count = 0
    for i in range(depth_intrin.width):
        for j in range(depth_intrin.height):
            depth_pixel = [i,j]
            if not mask[j,i]:
                continue
            udist = depth.get_distance(int(depth_pixel[0]+0.5),int(depth_pixel[1]+0.5))

            point = rs.rs2_deproject_pixel_to_point(depth_intrin, depth_pixel, udist)
            other_point = rs.rs2_transform_point_to_point(depth_extrin_to_color, point)
            from_pixel = rs.rs2_project_point_to_pixel(color_intrin, other_point)

            #Search along a projected beam from 0.1m to 10 meter
            to_pixel = rs.rs2_project_color_pixel_to_depth_pixel(depth_data, depth_scale, 0.1,10,depth_intrin,  color_intrin,color_extrin_to_depth, depth_extrin_to_color, from_pixel)

            dist = ((depth_pixel[1] - to_pixel[1]) ** 2 + (depth_pixel[0] - to_pixel[0]) ** 2) ** 0.5
            if dist > 1:
                count += 1

    MAX_ERROR_PERCENTAGE = 0.1
    check.is_true(count * 100 / mask.sum() < MAX_ERROR_PERCENTAGE)
