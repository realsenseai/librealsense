# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import numpy as np
import pyrealsense2 as rs
from pytest_check import check


def prepare_video_stream(width, height, bpp):
    depth_intrinsics = rs.intrinsics()
    depth_intrinsics.width = width
    depth_intrinsics.height = height
    depth_intrinsics.ppx = width / 2
    depth_intrinsics.ppy = height / 2
    depth_intrinsics.fx = width
    depth_intrinsics.fy = height
    depth_intrinsics.model = rs.distortion.brown_conrady
    depth_intrinsics.coeffs = [0, 0, 0, 0, 0]

    vs = rs.video_stream()
    vs.type = rs.stream.depth
    vs.index = 0
    vs.uid = 0
    vs.width = width
    vs.height = height
    vs.fps = 60
    vs.bpp = bpp
    vs.fmt = rs.format.z16
    vs.intrinsics = depth_intrinsics
    return vs


def prepare_motion_stream():
    motion_intrinsics = rs.motion_device_intrinsic()
    motion_intrinsics.data = [[1.0] * 4] * 3
    motion_intrinsics.noise_variances = [2, 2, 2]
    motion_intrinsics.bias_variances = [3, 3, 3]

    motion_stream = rs.motion_stream()
    motion_stream.type = rs.stream.accel
    motion_stream.index = 0
    motion_stream.uid = 1
    motion_stream.fps = 200
    motion_stream.fmt = rs.format.motion_raw
    motion_stream.intrinsics = motion_intrinsics

    return motion_stream


def prepare_depth_frame(video_frame, pixels, width, bpp, depth_stream_profile):
    video_frame.pixels = pixels
    video_frame.bpp = bpp
    video_frame.stride = width*bpp
    video_frame.timestamp = 10000
    video_frame.domain = rs.timestamp_domain.hardware_clock
    video_frame.frame_number = 0
    video_frame.profile = depth_stream_profile.as_video_stream_profile()
    return video_frame


def prepare_motion_frame(motion_frame, motion_frame_data, motion_stream_profile):
    motion_frame.data = motion_frame_data
    motion_frame.timestamp = 20000
    motion_frame.domain = rs.timestamp_domain.hardware_clock
    motion_frame.frame_number = 0
    motion_frame.profile = motion_stream_profile.as_motion_stream_profile()
    return motion_frame


def record_frames(filename, sd, sync, video_frame, motion_frame, sensor, stream_profiles):
    recorder = rs.recorder(filename, sd)
    sensor.open(stream_profiles)
    sensor.start(sync)

    sensor.on_video_frame(video_frame)
    sensor.on_motion_frame(motion_frame)

    sensor.stop()
    sensor.close()

    recorder.pause()
    recorder = None


def compare_frames(recorded_depth, recorded_accel, pixels, video_frame, motion_frame, motion_frame_data):
    recorded_depth_data = np.hstack(np.asarray(recorded_depth.as_depth_frame().get_data())).view(dtype=np.uint8)
    for (i, pixel) in enumerate(pixels):
        check.equal(pixel, recorded_depth_data[i])

    check.equal(video_frame.frame_number, recorded_depth.get_frame_number())
    check.equal(video_frame.domain, recorded_depth.get_frame_timestamp_domain())
    check.equal(video_frame.timestamp, recorded_depth.get_timestamp())


    recorded_accel_data = recorded_accel.as_motion_frame().get_motion_data()
    check.equal(motion_frame_data.x, recorded_accel_data.x)
    check.equal(motion_frame_data.y, recorded_accel_data.y)
    check.equal(motion_frame_data.z, recorded_accel_data.z)
    check.equal(motion_frame.frame_number, recorded_accel.get_frame_number())
    check.equal(motion_frame.domain, recorded_accel.get_frame_timestamp_domain())
    check.equal(motion_frame.timestamp, recorded_accel.get_timestamp())


def play_frames(filename, pixels, video_frame, motion_frame, motion_frame_data):
    ctx = rs.context()
    player_dev = ctx.load_device(filename)
    player_dev.set_real_time(False)
    player_sync = rs.syncer()
    s = player_dev.query_sensors()[0]
    s.open(s.get_stream_profiles())
    s.start(player_sync)

    fset = rs.frame().as_frameset()
    recorded_depth = rs.frame()
    recorded_accel = rs.frame()

    success, fset = player_sync.try_wait_for_frames()
    while success:
        if fset.first_or_default(rs.stream.depth):
            recorded_depth = fset.first_or_default(rs.stream.depth)
        if fset.first_or_default(rs.stream.accel):
            recorded_accel = fset.first_or_default(rs.stream.accel)
        success, fset = player_sync.try_wait_for_frames()

    compare_frames(recorded_depth, recorded_accel, pixels, video_frame, motion_frame, motion_frame_data)

    s.stop()
    s.close()


################################################################################################
def test_record_software_device(tmp_path):
    W = 640
    H = 480
    BPP = 2

    filename = str(tmp_path / "recording.db3")

    video_frame = rs.software_video_frame()
    motion_frame = rs.software_motion_frame()

    pixels = np.array([100 for i in range(W*H*BPP)], dtype=np.uint8)
    motion_frame_data = rs.vector()
    motion_frame_data.x = 1.0
    motion_frame_data.y = 2.0
    motion_frame_data.z = 3.0

    sd = rs.software_device()
    sensor = sd.add_sensor("Synthetic")

    vs = prepare_video_stream(W, H, BPP)
    depth_stream_profile = sensor.add_video_stream(vs).as_video_stream_profile()

    motion_stream = prepare_motion_stream()
    motion_stream_profile = sensor.add_motion_stream(motion_stream)

    sync = rs.syncer()
    stream_profiles = [depth_stream_profile, motion_stream_profile]

    video_frame = prepare_depth_frame(video_frame, pixels, W, BPP, depth_stream_profile)
    motion_frame = prepare_motion_frame(motion_frame, motion_frame_data, motion_stream_profile)

    record_frames(filename, sd, sync, video_frame, motion_frame, sensor, stream_profiles)
    play_frames(filename, pixels, video_frame, motion_frame, motion_frame_data)
################################################################################################
