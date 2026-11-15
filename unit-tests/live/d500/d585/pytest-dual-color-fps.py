# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Measured frame rates of the two color streams of a dual-color (2C) device: each color alone at every
# advertised rate, both together, and alongside depth and infrared. Each stream must run within 15% of its
# profile rate (fps_helper tolerance), whatever carries it - separate pins (USB) or one multiplexed pin (GMSL).

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

import sys, os
sys.path.append( os.path.join( os.path.dirname( os.path.abspath( __file__ ) ), "..", "..", "frames" ) )
import fps_helper


pytestmark = [
    pytest.mark.device_each( "Dual RGB" ),
    pytest.mark.context( "nightly" ),
]

RES = (1280, 720)
LOW_RES = (640, 360)


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


def profile(sensor, stream, index, fmt, fps, res=RES):
    for p in sensor.get_stream_profiles():
        vp = p.as_video_stream_profile()
        if vp and p.stream_type() == stream and p.stream_index() == index and p.format() == fmt \
                and (vp.width(), vp.height()) == res and p.fps() == fps:
            return p
    pytest.skip( f"Device publishes no {stream} {index} {res[0]}x{res[1]}@{fps} {fmt}" )


def color(sensor, index, fps, res=RES):
    return profile( sensor, rs.stream.color, index, rs.format.nv12, fps, res )


def measure(sensor, profiles):
    """Stream the profiles together and assert each runs within tolerance of its rate."""
    names = [p.stream_name() for p in profiles]
    fps_helper.perform_fps_test( [(sensor, p) for p in profiles], [names] )


@pytest.mark.parametrize( "index", [1, 2] )
@pytest.mark.parametrize( "fps", [5, 15, 30, 60, 90] )
def test_single_color_fps(depth_sensor, index, fps):
    # 60 and 90 are enumerated but not every transport can start them; a refused open is a skip, not a
    # failure. No open/close probe beforehand: on USB an open without a start leaves the pin silent.
    try:
        measure( depth_sensor, [color( depth_sensor, index, fps )] )
    except RuntimeError as e:
        if fps < 60:
            raise
        pytest.skip( f"Color {index} not openable at {fps} FPS: {e}" )


@pytest.mark.parametrize( "fps", [5, 15, 30] )
def test_both_colors_fps(depth_sensor, fps):
    measure( depth_sensor, [color( depth_sensor, 1, fps ), color( depth_sensor, 2, fps )] )


@pytest.mark.parametrize( "fps", [15, 30] )
def test_all_streams_fps(depth_sensor, fps):
    measure( depth_sensor, [profile( depth_sensor, rs.stream.depth, 0, rs.format.z16, fps ),
                            profile( depth_sensor, rs.stream.infrared, 1, rs.format.y8, fps ),
                            profile( depth_sensor, rs.stream.infrared, 2, rs.format.y8, fps ),
                            color( depth_sensor, 1, fps ), color( depth_sensor, 2, fps )] )


def test_mixed_resolutions_fps(depth_sensor):
    """Resolutions may differ between depth/IR and color as long as the rate is shared."""
    measure( depth_sensor, [profile( depth_sensor, rs.stream.depth, 0, rs.format.z16, 30, LOW_RES ),
                            profile( depth_sensor, rs.stream.infrared, 1, rs.format.y8, 30, LOW_RES ),
                            color( depth_sensor, 1, 30 ), color( depth_sensor, 2, 30 )] )
