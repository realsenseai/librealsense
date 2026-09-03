# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import time
import pytest
import pyrealsense2 as rs
from pytest_check import check
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D500*"),
]


MAX_TIME_TO_WAIT_FOR_FRAMES = 10  # [sec]
MODE_SETTLE_TIME = 1  # [sec] the device rebuilds its depth pipeline on a mode change


@pytest.fixture
def depth_sensor(test_device):
    """The depth sensor, with aligned depth restored to off when the test ends."""
    dev, _ = test_device
    sensor = dev.first_depth_sensor()
    if not sensor.supports(rs.option.align_depth):
        pytest.skip("device-side aligned depth is not supported by this firmware")
    set_align_depth(sensor, 0)  # each test starts from raw depth, whatever the previous one left behind
    yield sensor
    try:
        set_align_depth(sensor, 0)
    except Exception as e:
        log.warning("could not restore Align Depth: %s", e)


def set_align_depth(sensor, value):
    sensor.set_option(rs.option.align_depth, value)
    time.sleep(MODE_SETTLE_TIME)


def depth_profile(sensor):
    """The raw depth profile. A device may expose a device-aligned depth stream too, whose topic only
    exists while the mode is on, so 'the first depth profile' is not good enough here."""
    depth = [p for p in sensor.profiles if p.stream_type() == rs.stream.depth]
    return next((p for p in depth if p.stream_name() == "Depth"), depth[0]).as_video_stream_profile()


def color_profiles(dev):
    """Profiles of the dedicated color sensor - the imager the alignment engine calibrates against. Dual-RGB
    devices carry their color streams on the depth sensor and have no such sensor, so they yield nothing."""
    return [p.as_video_stream_profile() for s in dev.sensors if s.is_color_sensor()
            for p in s.profiles if p.stream_type() == rs.stream.color]


def grab_frame(sensor, profile):
    queue = rs.frame_queue(10)
    sensor.open(profile)
    sensor.start(queue)
    try:
        return queue.wait_for_frame(MAX_TIME_TO_WAIT_FOR_FRAMES * 1000)
    finally:
        sensor.stop()
        sensor.close()
        time.sleep(MODE_SETTLE_TIME)  # the device rejects enabling the mode right after a stream closes


def test_option_defaults(depth_sensor):
    """Aligned depth is off by default and settable while the sensor is closed."""
    option_range = depth_sensor.get_option_range(rs.option.align_depth)
    check.equal(option_range.min, 0)
    check.equal(option_range.default, 0)
    check.is_false(depth_sensor.is_option_read_only(rs.option.align_depth))
    check.equal(depth_sensor.get_option(rs.option.align_depth), 0)

    set_align_depth(depth_sensor, 1)
    check.equal(depth_sensor.get_option(rs.option.align_depth), 1)


def test_profile_invariance(depth_sensor):
    """Enabling aligned depth keeps the depth profile - and the frames - unchanged in size and format."""
    profile = depth_profile(depth_sensor)
    raw = (profile.width(), profile.height(), profile.fps(), profile.format())
    raw_frame = grab_frame(depth_sensor, profile)
    assert raw_frame

    set_align_depth(depth_sensor, 1)

    profile = depth_profile(depth_sensor)
    check.equal((profile.width(), profile.height(), profile.fps(), profile.format()), raw)
    aligned_frame = grab_frame(depth_sensor, profile)
    assert aligned_frame
    check.equal(aligned_frame.as_video_frame().get_data_size(), raw_frame.as_video_frame().get_data_size())


# On DDS aligned depth arrives on its own stream, carrying the calibration the device published for it.
# Only UVC/GMSL replace the depth payload in place, so only there does the depth stream change its model.
@pytest.mark.device_type_exclude("DDS")
def test_intrinsics_follow_the_color_model(test_device, depth_sensor):
    """Aligned depth is projected into the color viewport, so it must report the color intrinsics."""
    dev, _ = test_device
    profile = depth_profile(depth_sensor)
    raw_intrinsics = profile.get_intrinsics()

    set_align_depth(depth_sensor, 1)
    aligned_intrinsics = depth_profile(depth_sensor).get_intrinsics()

    check.equal((aligned_intrinsics.width, aligned_intrinsics.height), (raw_intrinsics.width, raw_intrinsics.height))
    check.is_true(aligned_intrinsics.fx != raw_intrinsics.fx or aligned_intrinsics.ppx != raw_intrinsics.ppx,
                  "aligned depth still reports the native depth intrinsics")

    color = next((p for p in color_profiles(dev)
                  if p.width() == raw_intrinsics.width and p.height() == raw_intrinsics.height), None)
    if color:
        color_intrinsics = color.get_intrinsics()
        log.info("%s %s: %s vs aligned depth: %s", color.stream_name(), color.format(), color_intrinsics, aligned_intrinsics)
        check.almost_equal(aligned_intrinsics.fx, color_intrinsics.fx, abs=0.01)
        check.almost_equal(aligned_intrinsics.ppx, color_intrinsics.ppx, abs=0.01)

    set_align_depth(depth_sensor, 0)
    restored = depth_profile(depth_sensor).get_intrinsics()
    check.almost_equal(restored.fx, raw_intrinsics.fx, abs=0.01)
    check.almost_equal(restored.ppx, raw_intrinsics.ppx, abs=0.01)


@pytest.mark.device_type_exclude("DDS")
def test_extrinsics_to_color_are_identity(test_device, depth_sensor):
    """Aligned depth is already expressed in the color optical frame."""
    dev, _ = test_device
    profile = depth_profile(depth_sensor)
    color = next(iter(color_profiles(dev)), None)
    if color is None:
        pytest.skip("device has no dedicated color sensor")

    set_align_depth(depth_sensor, 1)
    extrinsics = profile.get_extrinsics_to(color)
    check.equal([round(t, 6) for t in extrinsics.translation], [0, 0, 0])
    check.equal([round(r, 6) for r in extrinsics.rotation], [1, 0, 0, 0, 1, 0, 0, 0, 1])


def test_rejected_while_streaming(depth_sensor):
    """The mode changes the meaning of the depth payload, so it is immutable while streaming."""
    profile = depth_profile(depth_sensor)
    queue = rs.frame_queue(10)
    depth_sensor.open(profile)
    depth_sensor.start(queue)
    try:
        queue.wait_for_frame(MAX_TIME_TO_WAIT_FOR_FRAMES * 1000)
        with pytest.raises(RuntimeError):
            depth_sensor.set_option(rs.option.align_depth, 1)
    finally:
        depth_sensor.stop()
        depth_sensor.close()


def test_both_depth_streams_are_synchronized(test_device, depth_sensor):
    """Aligned depth carries the frame number of the depth frame it came from, so the syncer must
    match it together with raw depth and the two IRs rather than leave it unmatched."""
    dev, _ = test_device
    profiles = { (p.stream_type(), p.stream_index()): p for p in depth_sensor.profiles
                 if p.as_video_stream_profile().width() == 640
                 and p.as_video_stream_profile().height() == 360
                 and p.fps() == 30 }
    wanted = [profiles.get(k) for k in [(rs.stream.depth, 0), (rs.stream.depth, 1),
                                        (rs.stream.infrared, 1), (rs.stream.infrared, 2)]]
    if any(p is None for p in wanted):
        pytest.skip("device does not expose two depth streams and both IRs at 640x360@30")

    set_align_depth(depth_sensor, 1)  # the aligned topic only exists while the mode is on
    sync = rs.syncer(20)
    depth_sensor.open(wanted)
    depth_sensor.start(sync)
    complete = 0
    try:
        deadline = time.time() + MAX_TIME_TO_WAIT_FOR_FRAMES
        while time.time() < deadline:
            frames = sync.wait_for_frames(MAX_TIME_TO_WAIT_FOR_FRAMES * 1000)
            if frames.size() == len(wanted):
                complete += 1
    except RuntimeError:
        pass  # the wait times out once the window closes
    finally:
        depth_sensor.stop()
        depth_sensor.close()

    log.info("%d complete framesets", complete)
    assert complete > 0, "the syncer never matched both depth streams with the IRs"
