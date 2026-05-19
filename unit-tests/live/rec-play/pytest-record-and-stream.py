# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#The test flow is a result of a fixed bug - viewer crashed when starting stream after finishing record session

import pytest
import pyrealsense2 as rs, os, time, tempfile
from pytest_check import check
import logging

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
]


def find_default_profile():
    default_profile = next(p for p in depth_sensor.profiles if p.is_default() and p.stream_type() == rs.stream.depth)
    return default_profile


def restart_profile(default_profile):
    """
    You can't use the same profile twice, but we need the same profile several times. So this function resets the
    profiles with the given parameters to allow quick profile creation
    """
    depth_profile = next( p for p in depth_sensor.profiles if p.fps() == default_profile.fps()
               and p.stream_type() == rs.stream.depth
               and p.format() == default_profile.format()
               and p.as_video_stream_profile().width() == default_profile.as_video_stream_profile().width()
               and p.as_video_stream_profile().height() == default_profile.as_video_stream_profile().height())
    return depth_profile

def record(file_name, default_profile):
    global depth_sensor

    frame_queue = rs.frame_queue(10)
    depth_profile = restart_profile(default_profile)
    depth_sensor.open(depth_profile)
    depth_sensor.start(frame_queue)

    recorder = rs.recorder(file_name, dev)
    time.sleep(3)
    recorder.pause()
    recorder = None

    depth_sensor.stop()
    depth_sensor.close()


def try_streaming(default_profile):
    global depth_sensor

    frame_queue = rs.frame_queue(10)
    depth_profile = restart_profile(default_profile)
    depth_sensor.open(depth_profile)
    depth_sensor.start(frame_queue)
    time.sleep(3)
    depth_sensor.stop()
    depth_sensor.close()

    return frame_queue


def play_recording(file_name, default_profile):
    global depth_sensor

    playback = ctx.load_device(file_name)
    depth_sensor = playback.first_depth_sensor()
    frame_queue = try_streaming(default_profile)

    check.is_true(frame_queue.poll_for_frame())
################################################################################################
def test_record_and_stream(test_device):
    global dev, ctx, depth_sensor
    log.info("Record, stream and playback using sensor interface with frame queue")
    temp_dir = tempfile.mkdtemp()
    file_name = os.path.join(temp_dir, "recording.db3")

    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()
    default_profile = find_default_profile()
    record(file_name, default_profile)

    # after we finish recording we close the sensor and then open it again and try streaming
    try_streaming(default_profile)

    play_recording(file_name, default_profile)
################################################################################################
