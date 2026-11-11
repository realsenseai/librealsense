# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Labeled point cloud (LPCL) together with Depth, in both start orders.
# The camera feeds the LPCL from Depth and refuses the stream that starts second when the frame rates differ,
# so the SDK must raise a clear error instead of passing the request on (RSDEV-16285).
# Depth sizes other than 640x360 / 1280x720 depend on the FW (relaxed from FW with RSDEV-16286),
# so for them the test only checks that the SDK does not raise its own rate error.

import pytest
import time
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each( "D585" ),
    pytest.mark.device_exclude( "D585S" ),   # D585S has its own mapping stream rules
]

SETTLE_TIME = 2    # [sec] the camera refuses a mapping start that follows a stop too closely
FRAMES_TIMEOUT = 10   # [sec] to get a frame from each stream
RATE_ERROR = "fps"


def get_sensors( dev ):
    sensors = { s.get_info( rs.camera_info.name ): s for s in dev.query_sensors() }
    return sensors["Depth Mapping Camera"], sensors["Stereo Module"]


def lpcl_profile( mapping ):
    return next( p for p in mapping.profiles if p.stream_type() == rs.stream.labeled_point_cloud )


def depth_profile( depth, width, height, fps ):
    for p in depth.profiles:
        if p.stream_type() != rs.stream.depth:
            continue
        v = p.as_video_stream_profile()
        if ( v.width(), v.height(), p.fps() ) == ( width, height, fps ):
            return p
    pytest.skip( f"Depth {width}x{height}@{fps} is not exposed by this FW" )


def run_pair( first, second ):
    """Open and start both (sensor, profile) pairs, in order. Returns the frame counts. Always closes everything."""
    counts = [0, 0]
    started = []
    try:
        for i, ( sensor, profile ) in enumerate( ( first, second ) ):
            sensor.open( profile )
            started.append( sensor )
            sensor.start( lambda f, i=i: counts.__setitem__( i, counts[i] + 1 ) )
        deadline = time.time() + FRAMES_TIMEOUT
        while min( counts ) == 0 and time.time() < deadline:
            time.sleep( 0.1 )
        return counts
    finally:
        for sensor in reversed( started ):
            sensor.stop()
            sensor.close()
        time.sleep( SETTLE_TIME )


def ordered( order, mapping_pair, depth_pair ):
    return ( mapping_pair, depth_pair ) if order == "lpcl_first" else ( depth_pair, mapping_pair )


ORDERS = ["lpcl_first", "depth_first"]


@pytest.mark.parametrize( "order", ORDERS )
@pytest.mark.parametrize( "size", [( 640, 360 ), ( 1280, 720 )] )
def test_depth_at_lpcl_rate_streams( test_device, order, size ):
    dev, _ = test_device
    mapping, depth = get_sensors( dev )
    lpcl = lpcl_profile( mapping )
    dprofile = depth_profile( depth, *size, lpcl.fps() )
    counts = run_pair( *ordered( order, ( mapping, lpcl ), ( depth, dprofile ) ) )
    assert all( c > 0 for c in counts ), f"missing frames, {counts}"


@pytest.mark.parametrize( "order", ORDERS )
def test_depth_at_other_rate_raises( test_device, order ):
    dev, _ = test_device
    mapping, depth = get_sensors( dev )
    lpcl = lpcl_profile( mapping )
    other_fps = next( p.fps() for p in depth.profiles
                      if p.stream_type() == rs.stream.depth and p.fps() != lpcl.fps() )
    dprofile = depth_profile( depth, 640, 360, other_fps )
    with pytest.raises( RuntimeError, match=RATE_ERROR ):
        run_pair( *ordered( order, ( mapping, lpcl ), ( depth, dprofile ) ) )


@pytest.mark.parametrize( "order", ORDERS )
@pytest.mark.parametrize( "size", [( 640, 480 ), ( 848, 480 )] )
def test_other_depth_size_is_left_to_the_camera( test_device, order, size ):
    dev, _ = test_device
    mapping, depth = get_sensors( dev )
    lpcl = lpcl_profile( mapping )
    dprofile = depth_profile( depth, *size, lpcl.fps() )
    try:
        run_pair( *ordered( order, ( mapping, lpcl ), ( depth, dprofile ) ) )
    except RuntimeError as e:
        # FW before RSDEV-16286 refuses these sizes in the backend; the SDK must not claim a rate problem
        assert RATE_ERROR not in str( e ), str( e )
        log.info( "camera refused %s %s: %s", size, order, e )
