# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import numpy as np
import pyrealsense2 as rs

WIDTH, HEIGHT = 640, 480
COLOR_WIDTH, COLOR_HEIGHT = 320, 240


def make_intrinsics(width, height):
    i = rs.intrinsics()
    i.width, i.height = width, height
    i.ppx, i.ppy = width / 2.0, height / 2.0
    i.fx = i.fy = float(width)
    i.model = rs.distortion.brown_conrady
    i.coeffs = [0, 0, 0, 0, 0]
    return i


def add_stream(sensor, stream_type, uid, fmt, bpp, width, height):
    vs = rs.video_stream()
    vs.type, vs.index, vs.uid, vs.fmt, vs.bpp = stream_type, 0, uid, fmt, bpp
    vs.width, vs.height, vs.fps = width, height, 30
    vs.intrinsics = make_intrinsics(width, height)
    return sensor.add_video_stream(vs)


def make_frame(profile, pixels, bpp, width, index):
    f = rs.software_video_frame()
    f.pixels = pixels.tobytes()
    f.bpp, f.stride = bpp, width * bpp
    f.timestamp, f.frame_number = index * 33, index
    f.domain = rs.timestamp_domain.system_time
    f.profile = profile.as_video_stream_profile()
    f.depth_units = 0.001
    return f


def test_align_unchanged_after_decimation_toggle():
    sw_dev = rs.software_device()
    depth_sensor = sw_dev.add_sensor("Depth")
    color_sensor = sw_dev.add_sensor("Color")
    depth_profile = add_stream(depth_sensor, rs.stream.depth, 0, rs.format.z16, 2, WIDTH, HEIGHT)
    color_profile = add_stream(color_sensor, rs.stream.color, 1, rs.format.rgb8, 3, COLOR_WIDTH, COLOR_HEIGHT)
    identity = rs.extrinsics()
    identity.rotation = [1, 0, 0, 0, 1, 0, 0, 0, 1]
    identity.translation = [0, 0, 0]
    depth_profile.register_extrinsics_to(color_profile, identity)

    sync = rs.syncer()
    depth_sensor.open(depth_profile)
    color_sensor.open(color_profile)
    depth_sensor.start(sync)
    color_sensor.start(sync)

    depth = (500 + np.add.outer(np.arange(HEIGHT), np.arange(WIDTH)) % 1000).astype(np.uint16)
    color = np.full((COLOR_HEIGHT, COLOR_WIDTH, 3), 128, dtype=np.uint8)
    frames = None
    for index in range(10):
        depth_sensor.on_video_frame(make_frame(depth_profile, depth, 2, WIDTH, index))
        color_sensor.on_video_frame(make_frame(color_profile, color, 3, COLOR_WIDTH, index))
        fs = sync.wait_for_frames()
        if fs.size() == 2:
            frames = fs
    assert frames is not None, "syncer never produced a depth+color frameset"

    align = rs.align(rs.stream.color)
    decimation = rs.decimation_filter()

    def aligned_depth(fs):
        return np.asanyarray(align.process(fs).get_depth_frame().get_data()).copy()

    before = aligned_depth(frames)
    assert before.any(), "aligned depth is empty"
    # filter.process returns a plain frame; wrap it as a composite so align.process accepts it
    aligned_depth(rs.composite_frame(decimation.process(frames)))
    after = aligned_depth(frames)

    assert np.array_equal(before, after), "aligned depth changed after decimation was toggled back off"
