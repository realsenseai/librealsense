# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# On a dual-color (2C) device color shares the imagers with depth and IR, and the firmware refuses Emitter On/Off
# while they stream together. The SDK must report the control read-only then, and refuse it with a clear message.

import pytest
import pyrealsense2 as rs
import threading


pytestmark = [
    pytest.mark.device_each( "Dual RGB" ),
    pytest.mark.context( "nightly" ),
]

W, H, FPS = 1280, 720, 30
OPT = rs.option.emitter_on_off


@pytest.fixture
def depth_sensor(test_device):
    # On a dual-color device depth, IR and both color streams all live on the depth sensor
    dev, _ = test_device
    sensor = dev.first_depth_sensor()
    if not sensor.supports( OPT ):
        pytest.skip( "Emitter On/Off not supported" )
    always_on = sensor.get_option( rs.option.emitter_always_on )
    sensor.set_option( rs.option.emitter_always_on, 0 )   # Always On silently blocks Emitter On/Off
    yield sensor
    try:
        sensor.close()   # the sensor outlives the test - never hand it to the next one still open
    except RuntimeError:
        pass             # not open, which is the normal path
    sensor.set_option( rs.option.emitter_always_on, always_on )


def profile(sensor, stream_type, index, fmt):
    for p in sensor.get_stream_profiles():
        vp = p.as_video_stream_profile()
        if vp and p.stream_type() == stream_type and p.stream_index() == index and p.format() == fmt \
                and vp.width() == W and vp.height() == H and p.fps() == FPS:
            return p
    pytest.skip( f"Device publishes no {stream_type} {index} {W}x{H}@{FPS} {fmt}" )


STREAMS = {
    "depth": ( rs.stream.depth, 0, rs.format.z16 ),
    "ir1": ( rs.stream.infrared, 1, rs.format.y8 ),
    "color1": ( rs.stream.color, 1, rs.format.rgb8 ),
    "color2": ( rs.stream.color, 2, rs.format.rgb8 ),
}


def open_and_start(sensor, names):
    sensor.open( [profile( sensor, *STREAMS[n] ) for n in names] )
    streaming = threading.Event()
    sensor.start( lambda f: streaming.set() )
    # Stopping before streaming really starts leaves the firmware unable to answer the next command
    assert streaming.wait( 5 ), "no frames"


def stop_and_close(sensor):
    sensor.stop()
    sensor.close()


@pytest.mark.parametrize( "names", [["depth", "color1"], ["ir1", "color1"], ["depth", "ir1", "color1", "color2"]] )
def test_locked_while_color_shares_imagers(depth_sensor, names):
    open_and_start( depth_sensor, names )
    try:
        assert depth_sensor.is_option_read_only( OPT )
        with pytest.raises( RuntimeError, match="cannot be changed while color streams together with depth or infrared" ):
            depth_sensor.set_option( OPT, 1 )
    finally:
        stop_and_close( depth_sensor )
    assert not depth_sensor.is_option_read_only( OPT )
    depth_sensor.set_option( OPT, 0 )   # settable again once stopped


@pytest.mark.parametrize( "names", [["depth"], ["ir1"], ["color1"], ["color1", "color2"]] )
def test_settable_without_shared_imagers(depth_sensor, names):
    open_and_start( depth_sensor, names )
    try:
        assert not depth_sensor.is_option_read_only( OPT )
        for value in ( 1, 0 ):
            depth_sensor.set_option( OPT, value )
            assert depth_sensor.get_option( OPT ) == value
    finally:
        stop_and_close( depth_sensor )
