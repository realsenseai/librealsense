# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Frame-rate limits when depth/IR and color stream together on a dual-color (2C) device.
#
# Both run off the same imagers, so together they cap at 45 FPS and must share one frame rate:
# the enumerated 60/90 FPS profiles, and any rate mismatch, stream no frames at all. The SDK
# rejects those combinations at sensor open with a user-friendly error, surfaced as a
# RuntimeError in Python. Depth/IR and color resolutions may differ, but the two color streams
# must share one resolution.
# Full Passive Depth raises the cap to 60 FPS.
#
# The test adapts to the device: each case skips when the device does not publish the profiles
# it needs, rather than gating on firmware versions.

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)


pytestmark = [
    # "Dual RGB" matches the 2C SKUs only (D535/D585 Dual RGB, incl. the prototype).
    pytest.mark.device_each( "Dual RGB" ),
    pytest.mark.context( "nightly" ),
]

COLOR_INDEX = 1
IR_INDEX = 1


@pytest.fixture
def depth_sensor(test_device):
    # On a dual-color device depth, IR and both color streams all live on the depth sensor.
    dev, _ = test_device
    sensor = dev.first_depth_sensor()
    yield sensor
    try:
        sensor.close()   # the sensor outlives the test - never hand it to the next one still open
    except RuntimeError:
        pass             # not open, which is the normal path


def video_profiles(sensor, stream_type, index):
    out = []
    for p in sensor.get_stream_profiles():
        vp = p.as_video_stream_profile()
        if vp and p.stream_type() == stream_type and p.stream_index() == index:
            out.append( vp )
    return out


def pick(sensor, stream_type, index, fps, res=None):
    """First profile of the given stream at `fps`, optionally at resolution `res` = (w, h)."""
    for vp in video_profiles( sensor, stream_type, index ):
        if vp.fps() == fps and (res is None or (vp.width(), vp.height()) == res):
            return vp
    return None


def stereo_and_color(sensor, stream_type, stereo_fps, color_fps=None, shared_res=True):
    """A (depth-or-IR, color) profile pair at the requested rates, sharing a resolution when asked.
    Returns None when the device does not publish such a pair."""
    color_fps = stereo_fps if color_fps is None else color_fps
    stereo_index = 0 if stream_type == rs.stream.depth else IR_INDEX
    colors = [c for c in video_profiles( sensor, rs.stream.color, COLOR_INDEX ) if c.fps() == color_fps]
    for stereo in video_profiles( sensor, stream_type, stereo_index ):
        if stereo.fps() != stereo_fps:
            continue
        for color in colors:
            if ((color.width(), color.height()) == (stereo.width(), stereo.height())) == shared_res:
                return stereo, color
    return None


def get_or_skip(pair, what):
    if pair is None:
        pytest.skip( f"Device publishes no {what}" )
    return pair


def open_and_close(sensor, profiles):
    sensor.open( profiles )
    sensor.close()


def expect_rejected(sensor, profiles):
    """Assert the combination is refused at open. A refusal carrying 'already opened' means an
    earlier test leaked the sensor, which would otherwise look like a pass here."""
    try:
        sensor.open( profiles )
    except RuntimeError as e:
        assert "already opened" not in str( e ), f"sensor was left open by an earlier test: {e}"
        log.debug( "rejected as expected: %s", e )
        return
    sensor.close()   # accepted - undo it before failing so the next test starts clean
    pytest.fail( "open() accepted an unsupported stream combination" )


def set_passive_depth_mode_or_skip(sensor, mode):
    if not sensor.supports( rs.option.passive_depth_mode ):
        pytest.skip( "Device has no Passive Depth mode" )
    sensor.set_option( rs.option.passive_depth_mode, int( mode ) )


@pytest.fixture
def passive_depth_restored(depth_sensor):
    """Put the Passive Depth mode back the way the test found it."""
    has_mode = depth_sensor.supports( rs.option.passive_depth_mode )
    before = depth_sensor.get_option( rs.option.passive_depth_mode ) if has_mode else None
    yield
    if has_mode:
        depth_sensor.set_option( rs.option.passive_depth_mode, before )


@pytest.mark.parametrize( "stream_type", [rs.stream.depth, rs.stream.infrared] )
@pytest.mark.parametrize( "fps", [60, 90] )
def test_60_and_90_fps_rejected(depth_sensor, passive_depth_restored, stream_type, fps):
    """60/90 FPS is enumerated on each stream but unusable once depth/IR and color run together."""
    set_passive_depth_mode_or_skip( depth_sensor, rs.passive_depth_mode.disabled )
    stereo, color = get_or_skip( stereo_and_color( depth_sensor, stream_type, fps ),
                                 f"{stream_type} + color pair at {fps} FPS" )

    # The rejection only means something if each stream opens standalone at this rate.
    for profile in (stereo, color):
        try:
            open_and_close( depth_sensor, profile )
        except RuntimeError as e:
            pytest.skip( f"{profile.stream_type()} not openable standalone at {fps} FPS: {e}" )

    expect_rejected( depth_sensor, [stereo, color] )


def full_passive_stereo_and_colors(sensor, stream_type, fps):
    """Depth-or-IR + Color 1 + Color 2 at `fps` with Full Passive Depth set; skips when not published."""
    set_passive_depth_mode_or_skip( sensor, rs.passive_depth_mode.full )
    stereo, color1 = get_or_skip( stereo_and_color( sensor, stream_type, fps ),
                                  f"{stream_type} + color pair at {fps} FPS" )
    color2 = pick( sensor, rs.stream.color, 2, fps, (color1.width(), color1.height()) )
    if color2 is None:
        pytest.skip( f"Device publishes no Color 2 profile at {fps} FPS on Color 1's resolution" )
    return [stereo, color1, color2]


@pytest.mark.parametrize( "stream_type", [rs.stream.depth, rs.stream.infrared] )
def test_full_passive_60_fps_accepted(depth_sensor, passive_depth_restored, stream_type):
    """Full Passive Depth lifts depth/IR + both colors to 60 FPS."""
    open_and_close( depth_sensor, full_passive_stereo_and_colors( depth_sensor, stream_type, 60 ) )


@pytest.mark.parametrize( "stream_type", [rs.stream.depth, rs.stream.infrared] )
def test_full_passive_90_fps_rejected(depth_sensor, passive_depth_restored, stream_type):
    expect_rejected( depth_sensor, full_passive_stereo_and_colors( depth_sensor, stream_type, 90 ) )


@pytest.mark.parametrize( "stream_type", [rs.stream.depth, rs.stream.infrared] )
def test_mismatched_fps_rejected(depth_sensor, stream_type):
    """Both rates are within the combined limit, but they must be equal."""
    stereo, color = get_or_skip( stereo_and_color( depth_sensor, stream_type, 45, color_fps=30 ),
                                 f"{stream_type} at 45 FPS with color at 30 FPS" )

    expect_rejected( depth_sensor, [stereo, color] )


@pytest.mark.parametrize( "stream_type", [rs.stream.depth, rs.stream.infrared] )
def test_matched_fps_accepted(depth_sensor, stream_type):
    stereo, color = get_or_skip( stereo_and_color( depth_sensor, stream_type, 30 ),
                                 f"{stream_type} + color pair at 30 FPS" )

    open_and_close( depth_sensor, [stereo, color] )


@pytest.mark.parametrize( "stream_type", [rs.stream.depth, rs.stream.infrared] )
def test_different_resolutions_accepted(depth_sensor, stream_type):
    """Only the frame rate is constrained - depth/IR and color may run at different resolutions."""
    stereo, color = get_or_skip( stereo_and_color( depth_sensor, stream_type, 30, shared_res=False ),
                                 f"{stream_type} + color pair at 30 FPS on different resolutions" )

    open_and_close( depth_sensor, [stereo, color] )


def test_color_streams_mismatched_fps_rejected(depth_sensor):
    """The two color pins must share a rate too - this one holds without depth/IR in the request."""
    color1 = pick( depth_sensor, rs.stream.color, 1, 30 )
    if color1 is None:
        pytest.skip( "Device publishes no Color 1 profile at 30 FPS" )
    color2 = pick( depth_sensor, rs.stream.color, 2, 60, (color1.width(), color1.height()) )
    if color2 is None:
        pytest.skip( "Device publishes no Color 2 profile at 60 FPS on Color 1's resolution" )

    expect_rejected( depth_sensor, [color1, color2] )


def test_color_streams_mismatched_resolution_rejected(depth_sensor):
    """The color pins must share one resolution - a mismatch would starve Color 1, so the SDK rejects it."""
    color1 = pick( depth_sensor, rs.stream.color, 1, 30 )
    if color1 is None:
        pytest.skip( "Device publishes no Color 1 profile at 30 FPS" )
    color2 = next( (c for c in video_profiles( depth_sensor, rs.stream.color, 2 )
                    if c.fps() == 30 and (c.width(), c.height()) != (color1.width(), color1.height())), None )
    if color2 is None:
        pytest.skip( "Device publishes no Color 2 profile at 30 FPS on another resolution" )

    expect_rejected( depth_sensor, [color1, color2] )


@pytest.mark.parametrize( "fps", [60, 90] )
def test_color_only_high_fps_accepted(depth_sensor, fps):
    """Without depth/IR the color streams reach 60/90 FPS, so the guard must not fire."""
    color1 = pick( depth_sensor, rs.stream.color, 1, fps )
    if color1 is None:
        pytest.skip( f"Device publishes no color profile at {fps} FPS" )
    color2 = pick( depth_sensor, rs.stream.color, 2, fps, (color1.width(), color1.height()) )
    if color2 is None:
        # A lone color stream cannot trip the same-rate rule, so it would prove nothing here.
        pytest.skip( f"Device publishes no second color profile at {fps} FPS on Color 1's resolution" )

    open_and_close( depth_sensor, [color1, color2] )
