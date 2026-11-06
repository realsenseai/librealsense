# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Passive Depth selects which exposure classes produce depth on D585 dual-RGB SKUs, over the depth XU.
# Full Passive hands the laser and the AE policy to the firmware, so the host locks both.

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D585"),
    pytest.mark.device_exclude("D585S"),  # safety owns the projector: no passive depth control there
    pytest.mark.skip(reason="No D585 dual-RGB camera in CI"),  # verified locally against a D585 Proto Dual RGB (PID 0x0C07)
]

PASSIVE_DEPTH = rs.option.passive_depth_mode
MODES = rs.passive_depth_mode
AE_MODE = rs.option.auto_exposure_mode
POLICIES = rs.colored_ir_auto_exposure_mode
LASER_OPTIONS = [rs.option.emitter_enabled, rs.option.laser_power, rs.option.emitter_always_on, rs.option.emitter_on_off]


@pytest.fixture
def depth_sensor(test_device):
    """The depth sensor, with Passive Depth restored to disabled when the test ends."""
    dev, _ = test_device
    sensor = dev.first_depth_sensor()
    if not sensor.supports(PASSIVE_DEPTH):
        pytest.skip("passive depth is not supported by this firmware")
    sensor.set_option(PASSIVE_DEPTH, MODES.disabled)
    yield sensor
    try:
        sensor.set_option(PASSIVE_DEPTH, MODES.disabled)
    except Exception as e:
        log.warning("could not restore Passive Depth: %s", e)


def depth_profile(sensor):
    return next(p for p in sensor.profiles
                if p.stream_type() == rs.stream.depth and p.format() == rs.format.z16)


def test_passive_depth_range(depth_sensor):
    r = depth_sensor.get_option_range(PASSIVE_DEPTH)
    assert r.min == MODES.disabled
    assert r.max == MODES.full
    assert r.step == 1
    assert r.default == MODES.disabled


def test_passive_depth_set_get(depth_sensor):
    for mode in (MODES.alternating, MODES.full, MODES.disabled):
        depth_sensor.set_option(PASSIVE_DEPTH, mode)
        assert depth_sensor.get_option(PASSIVE_DEPTH) == mode


def test_passive_depth_rejects_out_of_range(depth_sensor):
    for bad in (int(MODES.full) + 1, 10, 255):
        with pytest.raises(Exception):
            depth_sensor.set_option(PASSIVE_DEPTH, bad)


def test_passive_depth_rejected_while_streaming(depth_sensor):
    # The mode is applied at stream start; the shared imagers make any streaming activity block the set.
    assert not depth_sensor.is_option_read_only(PASSIVE_DEPTH)
    depth_sensor.open(depth_profile(depth_sensor))
    depth_sensor.start(lambda frame: None)
    try:
        assert depth_sensor.is_option_read_only(PASSIVE_DEPTH)  # so the viewer greys it out
        if depth_sensor.supports(AE_MODE):
            assert depth_sensor.is_option_read_only(AE_MODE)
        # the emitter belongs to the exposure schedule the firmware fixes at stream start
        assert depth_sensor.is_option_read_only(rs.option.emitter_enabled)
        with pytest.raises(Exception):
            depth_sensor.set_option(rs.option.emitter_enabled, 0)
        with pytest.raises(Exception):
            depth_sensor.set_option(PASSIVE_DEPTH, MODES.full)
        assert depth_sensor.get_option(PASSIVE_DEPTH) == MODES.disabled
    finally:
        depth_sensor.stop()
        depth_sensor.close()
    assert not depth_sensor.is_option_read_only(PASSIVE_DEPTH)
    assert not depth_sensor.is_option_read_only(rs.option.emitter_enabled)


def test_full_passive_locks_the_laser(depth_sensor):
    supported = [opt for opt in LASER_OPTIONS if depth_sensor.supports(opt)]
    assert supported, "no laser control to lock"

    for opt in supported:
        assert not depth_sensor.is_option_read_only(opt)

    depth_sensor.set_option(PASSIVE_DEPTH, MODES.full)
    for opt in supported:
        assert depth_sensor.is_option_read_only(opt)
        with pytest.raises(Exception):
            depth_sensor.set_option(opt, depth_sensor.get_option_range(opt).max)

    depth_sensor.set_option(PASSIVE_DEPTH, MODES.disabled)
    for opt in supported:
        assert not depth_sensor.is_option_read_only(opt)


def test_full_passive_switches_ae_to_color_priority(depth_sensor):
    # Firmware runs Full Passive on Color Priority but keeps reporting the previous policy, so the host moves it.
    if not depth_sensor.supports(AE_MODE):
        pytest.skip("auto exposure policy not exposed on this device")

    for policy in (POLICIES.auto, POLICIES.depth_priority, POLICIES.hybrid):
        depth_sensor.set_option(PASSIVE_DEPTH, MODES.disabled)
        depth_sensor.set_option(AE_MODE, policy)
        depth_sensor.set_option(PASSIVE_DEPTH, MODES.full)
        assert depth_sensor.get_option(AE_MODE) == POLICIES.color_priority


def test_full_passive_hides_hybrid_ae(depth_sensor):
    if not depth_sensor.supports(AE_MODE):
        pytest.skip("auto exposure policy not exposed on this device")

    assert depth_sensor.get_option_range(AE_MODE).max == POLICIES.hybrid

    depth_sensor.set_option(PASSIVE_DEPTH, MODES.full)
    assert depth_sensor.get_option_range(AE_MODE).max == POLICIES.color_priority
    with pytest.raises(Exception):
        depth_sensor.set_option(AE_MODE, POLICIES.hybrid)
    depth_sensor.set_option(AE_MODE, POLICIES.color_priority)  # still settable within the reduced range

    depth_sensor.set_option(PASSIVE_DEPTH, MODES.disabled)
    assert depth_sensor.get_option_range(AE_MODE).max == POLICIES.hybrid


def test_alternating_passive_delivers_both_classes(depth_sensor):
    # Mode 1 alternates the laser, so one stream carries both exposure classes, told apart by metadata.
    emitter_mode = rs.frame_metadata_value.frame_emitter_mode
    depth_sensor.set_option(PASSIVE_DEPTH, MODES.alternating)

    queue = rs.frame_queue(50)
    depth_sensor.open(depth_profile(depth_sensor))
    depth_sensor.start(queue)
    try:
        seen = set()
        for _ in range(50):
            frame = queue.wait_for_frame(5000)
            if frame.supports_frame_metadata(emitter_mode):
                seen.add(frame.get_frame_metadata(emitter_mode))
    finally:
        depth_sensor.stop()
        depth_sensor.close()

    assert seen == {0, 1}, f"expected both active and passive frames, got emitter modes {seen}"
