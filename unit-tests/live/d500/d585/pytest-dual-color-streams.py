# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Both color streams of a dual-color (2C) device deliver frames, alone and together, whatever carries them:
# separate pins (USB today) or one multiplexed pin whose frames are attributed by metadata (GMSL).
#
# Every frame must arrive on the stream it was requested for, at the requested rate, and the two color
# streams must run at the same rate when streamed together.

import pytest
import pyrealsense2 as rs
import threading
import time
import logging
log = logging.getLogger(__name__)


pytestmark = [
    pytest.mark.device_each( "Dual RGB" ),
    pytest.mark.context( "nightly" ),
]

W, H, FPS = 1280, 720, 30
DURATION_S = 4
MIN_RATIO = 0.75   # frames received / frames expected


@pytest.fixture
def depth_sensor(test_device):
    # On a dual-color device depth, IR and both color streams all live on the depth sensor
    dev, _ = test_device
    sensor = dev.first_depth_sensor()
    yield sensor
    try:
        sensor.close()   # the sensor outlives the test - never hand it to the next one still open
    except RuntimeError:
        pass             # not open, which is the normal path


def profile(sensor, stream_type, index, fmt):
    for p in sensor.get_stream_profiles():
        vp = p.as_video_stream_profile()
        if vp and p.stream_type() == stream_type and p.stream_index() == index and p.format() == fmt \
                and vp.width() == W and vp.height() == H and p.fps() == FPS:
            return p
    pytest.skip( f"Device publishes no {stream_type} {index} {W}x{H}@{FPS} {fmt}" )


def color(sensor, index, fmt=rs.format.rgb8):
    return profile( sensor, rs.stream.color, index, fmt )


def stream(sensor, profiles):
    """Stream the profiles for DURATION_S; return {stream name: [frame numbers]} in arrival order."""
    frames = {}
    def on_frame(f):
        frames.setdefault( f.get_profile().stream_name(), [] ).append( f.get_frame_number() )
    sensor.open( profiles )
    sensor.start( on_frame )
    time.sleep( DURATION_S )
    sensor.stop()
    sensor.close()
    return frames


def check_stream(frames, name):
    numbers = frames.get( name, [] )
    log.debug( "%s: %d frames", name, len( numbers ) )
    assert len( numbers ) >= FPS * DURATION_S * MIN_RATIO, f"{name} delivered {len( numbers )} frames"
    assert all( b >= a for a, b in zip( numbers, numbers[1:] ) ), f"{name} frame numbers went backwards"
    return len( numbers )


def test_both_colors_enumerated(depth_sensor):
    """Color 1 and Color 2 publish the same profile set."""
    def key(p):
        vp = p.as_video_stream_profile()
        return vp.width(), vp.height(), p.fps(), p.format()
    colors = {1: set(), 2: set()}
    for p in depth_sensor.get_stream_profiles():
        if p.stream_type() == rs.stream.color and p.stream_index() in colors and p.as_video_stream_profile():
            colors[p.stream_index()].add( key( p ) )
    assert colors[1], "no Color 1 profiles"
    assert colors[1] == colors[2], "Color 1 and Color 2 publish different profiles"


@pytest.mark.parametrize( "fmt", [rs.format.rgb8, rs.format.nv12] )
def test_both_colors_stream(depth_sensor, fmt):
    frames = stream( depth_sensor, [color( depth_sensor, 1, fmt ), color( depth_sensor, 2, fmt )] )
    assert set( frames ) == {"Color 1", "Color 2"}, f"unexpected streams delivered: {sorted( frames )}"
    n1 = check_stream( frames, "Color 1" )
    n2 = check_stream( frames, "Color 2" )
    assert abs( n1 - n2 ) <= FPS * DURATION_S * 0.1, f"Color 1 ({n1}) and Color 2 ({n2}) rates differ"


@pytest.mark.parametrize( "index", [1, 2] )
def test_one_color_streams_alone(depth_sensor, index):
    """Requesting one color stream delivers that stream only, at its full rate."""
    name = f"Color {index}"
    frames = stream( depth_sensor, [color( depth_sensor, index )] )
    assert set( frames ) == {name}, f"unexpected streams delivered: {sorted( frames )}"
    check_stream( frames, name )


def test_colors_held_by_slow_consumer(depth_sensor):
    """Each color stream has its own frame pool: holding frames of one does not starve the other (RSDEV-15260)."""
    HOLD = 12   # frames kept per stream, as a slow viewer does; both colors together exceed one pool (16)
    held = {}
    frames = {}
    lock = threading.Lock()
    done = False
    def on_frame(f):
        name = f.get_profile().stream_name()
        with lock:
            frames.setdefault( name, [] ).append( f.get_frame_number() )
            if done:
                return
            q = held.setdefault( name, [] )
            q.append( f )
            del q[:-HOLD]
    depth_sensor.open( [profile( depth_sensor, rs.stream.depth, 0, rs.format.z16 ),
                        color( depth_sensor, 1 ), color( depth_sensor, 2 )] )
    depth_sensor.start( on_frame )
    time.sleep( DURATION_S )
    depth_sensor.stop()
    # stop() does not wait for a callback already running (the capture thread joins in close()), so stop holding
    # frames under the lock before releasing them
    with lock:
        done = True
        held.clear()
    depth_sensor.close()
    for name in ("Depth", "Color 1", "Color 2"):
        check_stream( frames, name )


def test_colors_with_depth_and_infrared(depth_sensor):
    profiles = [profile( depth_sensor, rs.stream.depth, 0, rs.format.z16 ),
                profile( depth_sensor, rs.stream.infrared, 1, rs.format.y8 ),
                profile( depth_sensor, rs.stream.infrared, 2, rs.format.y8 ),
                color( depth_sensor, 1 ), color( depth_sensor, 2 )]
    frames = stream( depth_sensor, profiles )
    for name in ("Depth", "Infrared 1", "Infrared 2", "Color 1", "Color 2"):
        check_stream( frames, name )
