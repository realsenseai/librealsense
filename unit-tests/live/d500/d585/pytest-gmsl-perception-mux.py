# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# GMSL Perception MUX: Object Detection (PD) and Occupancy (OCC) time-share the GMSL IR channel and are told apart
# by their metadata. Each stream is still its own sensor and opens and closes independently; the camera starts the
# shared capture with the first and stops it with the last.
# This file covers what is specific to the MUX; the stream lifecycles and the Occupancy input rules on every
# transport are in pytest-perception-streams.py.

import pytest
import pyrealsense2 as rs
import struct
import time
import logging
log = logging.getLogger(__name__)


pytestmark = [
    pytest.mark.device_each( "D585" ),
    pytest.mark.device_type( "GMSL" ),   # the MUX exists only over GMSL
]

MIN_PD_FRAMES = 8                     # lifecycle minimum
MIN_OCC_FRAMES = 15                   # OCC runs at ~30 fps
FRAME_TIMEOUT_S = 5                   # first frames of a fresh stream
SWITCH_S = 0.6                        # frames already in flight around an enable or disable
QUIET_S = 1.0                         # how long a disabled stream must stay silent
PD_ID, OCC_ID = 6, 7                  # stream ids of the camera's MUX control (HWMC 0xC0)
MUX_CONTROL, MUX_SET_ENABLE, MUX_GET_STATE = 0xC0, 1, 2


@pytest.fixture
def device(test_device):
    dev, _ = test_device
    if sensor( dev, 'Perception' ) is None:
        pytest.skip( "No Perception sensor: driver/FW without the RSVL MUX" )
    yield dev
    for s in dev.query_sensors():        # never hand the next test an open sensor
        for close in ( s.stop, s.close ):
            try:
                close()
            except RuntimeError:
                pass                     # not started/open, the normal path


def sensor(dev, name):
    for s in dev.query_sensors():
        if s.get_info( rs.camera_info.name ) == name:
            return s
    return None


def is_3c(dev):
    return sensor( dev, 'RGB Camera' ) is not None   # 2C carries its colors on the Stereo Module


def profile(s, stream_type, fmt, w=1280, h=720, index=None):
    for p in s.get_stream_profiles():
        vp = p.as_video_stream_profile()
        if p.stream_type() == stream_type and p.format() == fmt and p.fps() == 30 and vp \
                and ( vp.width(), vp.height() ) == ( w, h ) and index in ( None, p.stream_index() ):
            return p
    pytest.skip( f"No {stream_type} {fmt} {w}x{h}@30" )


def pd_profile(dev):
    return next( p for p in sensor( dev, 'Perception' ).get_stream_profiles()
                 if p.stream_type() == rs.stream.object_detection and p.fps() == 30 )


def occ_profile(dev):
    return next( p for p in sensor( dev, 'Depth Mapping Camera' ).get_stream_profiles()
                 if p.stream_type() == rs.stream.occupancy )


def mux_state(dev, stream_id):
    """(requested, applied, capture_active) as the camera reports it, or None if it rejects the query."""
    dp = rs.debug_protocol( dev )
    reply = bytes( dp.send_and_receive_raw_data( dp.build_command( MUX_CONTROL, 1, MUX_GET_STATE, stream_id, 0 ) ) )
    opcode, = struct.unpack_from( '<I', reply )
    return struct.unpack_from( '<III', reply, 4 ) if opcode == MUX_CONTROL and len( reply ) >= 16 else None


def wait_state(dev, stream_id, expected, timeout=2.):
    """The camera applies enables asynchronously; poll GET_STATE until it reports `expected` or the timeout passes."""
    deadline = time.monotonic() + timeout
    while True:
        state = mux_state( dev, stream_id )
        if state == expected or time.monotonic() > deadline:
            return state
        time.sleep( 0.1 )


def start_inputs(dev, extra=()):
    """Depth and Color, as an application runs them, plus any extra Stereo Module profiles; returns the started sensors."""
    stereo = dev.first_depth_sensor()
    stereo_profiles = [profile( stereo, rs.stream.depth, rs.format.z16 )] + list( extra )
    started = [stereo]
    if is_3c( dev ):
        rgb = sensor( dev, 'RGB Camera' )
        rgb.open( profile( rgb, rs.stream.color, rs.format.rgb8 ) )
        rgb.start( lambda f: None )
        started.append( rgb )
    else:
        # Dual RGB: both colors ride the Stereo Module
        stereo_profiles += [profile( stereo, rs.stream.color, rs.format.nv12, index=i ) for i in ( 1, 2 )]
    stereo.open( stereo_profiles )
    stereo.start( lambda f: None )
    return started


def stop(sensors):
    for s in sensors:
        s.stop()
        s.close()


class Collector:
    """Frames per stream type, with the checks this file needs taken at arrival."""
    def __init__(self):
        self.frames = {}
    def __call__(self, f):
        info = { 'time': time.monotonic(), 'size': f.get_data_size() }
        if f.is_object_detection_frame():
            info['magic'] = bytes( f.get_data() )[:4]
        if f.get_profile().stream_type() == rs.stream.occupancy:
            md = rs.frame_metadata_value
            info['grid'] = tuple( f.get_frame_metadata( m ) if f.supports_frame_metadata( m ) else None
                                  for m in ( md.occupancy_grid_rows, md.occupancy_grid_columns ) )
            info['cells_ok'] = all( b <= 100 or b == 255 for b in bytes( f.get_data() ) )   # int8 [-1, 100]
            info['counter'] = f.get_frame_metadata( md.frame_counter ) if f.supports_frame_metadata( md.frame_counter ) else None
        self.frames.setdefault( f.get_profile().stream_type(), [] ).append( info )
    def count(self, stream_type, since=0.):
        return sum( 1 for i in self.frames.get( stream_type, [] ) if i['time'] >= since )
    def wait(self, stream_type, since, count, timeout=FRAME_TIMEOUT_S):
        deadline = time.monotonic() + timeout
        while self.count( stream_type, since ) < count and time.monotonic() < deadline:
            time.sleep( 0.05 )
        assert self.count( stream_type, since ) >= count, f"too few {stream_type} frames"


def test_perception_sensors_enumerate(device):
    pd = pd_profile( device )
    assert pd.format() == rs.format.y8 and pd.fps() == 30
    assert mux_state( device, PD_ID ) == ( 0, 0, 0 )
    occ = occ_profile( device ).as_video_stream_profile()
    assert ( occ.format(), occ.width(), occ.height(), occ.fps() ) == ( rs.format.y8, 320, 256, 30 )
    assert mux_state( device, OCC_ID ) == ( 0, 0, 0 )


def test_detection_distance(device):
    pd = sensor( device, 'Perception' )
    if not pd.supports( rs.option.detection_distance ):
        pytest.skip( "No Detection Distance on this FW" )
    inputs = start_inputs( device )
    pd.open( pd_profile( device ) )
    pd.start( lambda f: None )
    time.sleep( 1 )   # right after PD starts the camera reads the value back invalid, failing the set
    original = pd.get_option( rs.option.detection_distance )
    for value in ( 0, 1, original ):
        pd.set_option( rs.option.detection_distance, value )
        assert pd.get_option( rs.option.detection_distance ) == value
    pd.stop()
    pd.close()
    stop( inputs )


@pytest.mark.parametrize( 'occ_first', [False, True] )
def test_pd_and_occ_independent(device, occ_first):
    """Each MUX member starts and stops on its own while the other keeps streaming, in either start order."""
    inputs = start_inputs( device )
    pd, mapping = sensor( device, 'Perception' ), sensor( device, 'Depth Mapping Camera' )
    frames = Collector()

    def step(action, pd_on, occ_on):
        action()
        since = time.monotonic() + SWITCH_S
        for stream_type, on, count in ( ( rs.stream.object_detection, pd_on, MIN_PD_FRAMES ),
                                        ( rs.stream.occupancy, occ_on, MIN_OCC_FRAMES ) ):
            if on:
                frames.wait( stream_type, since, count )
        time.sleep( max( 0., since + QUIET_S - time.monotonic() ) )
        for stream_type, on in ( ( rs.stream.object_detection, pd_on ), ( rs.stream.occupancy, occ_on ) ):
            if not on:
                assert frames.count( stream_type, since ) == 0, f"disabled {stream_type} still delivers frames"
        active = int( pd_on or occ_on )
        assert wait_state( device, PD_ID, ( int( pd_on ), int( pd_on ), active ) ) == ( int( pd_on ), int( pd_on ), active )
        assert wait_state( device, OCC_ID, ( int( occ_on ), int( occ_on ), active ) ) == ( int( occ_on ), int( occ_on ), active )

    def start(s, p):
        s.open( p )
        s.start( frames )

    if occ_first:
        step( lambda: start( mapping, occ_profile( device ) ), False, True )
        step( lambda: start( pd, pd_profile( device ) ), True, True )
        step( lambda: stop( [mapping] ), True, False )
        step( lambda: stop( [pd] ), False, False )
    else:
        step( lambda: start( pd, pd_profile( device ) ), True, False )
        step( lambda: start( mapping, occ_profile( device ) ), True, True )
        step( lambda: stop( [pd] ), False, True )
        step( lambda: stop( [mapping] ), False, False )
    stop( inputs )

    pd_frames, occ = frames.frames[rs.stream.object_detection], frames.frames[rs.stream.occupancy]
    assert all( i['size'] == 2347 and i['magic'] == b'ODET' for i in pd_frames )   # GMSL pads PD to the profile
    assert all( i['size'] == 320 * 256 for i in occ )
    assert all( i['grid'] == ( 256, 320 ) for i in occ ), "OCC frames need their occupancy metadata"
    assert all( i['cells_ok'] for i in occ )
    assert all( i['counter'] is not None for i in occ ), "OCC counter validity is independent of source timestamp"
    assert all( b['counter'] > a['counter'] for a, b in zip( occ, occ[1:] ) )


def test_stale_request_cleared(device):
    """A stream request another client left behind is cleared when the SDK starts the capture."""
    dp = rs.debug_protocol( device )
    dp.send_and_receive_raw_data( dp.build_command( MUX_CONTROL, 1, MUX_SET_ENABLE, OCC_ID, 1 ) )
    assert mux_state( device, OCC_ID )[0] == 1
    inputs = start_inputs( device )
    pd = sensor( device, 'Perception' )
    pd.open( pd_profile( device ) )
    pd.start( lambda f: None )
    assert mux_state( device, OCC_ID )[0] == 0   # cleared before the capture starts
    pd.stop()
    pd.close()
    stop( inputs )


@pytest.mark.parametrize( 'width, height', [(640, 360), (1280, 720)] )
def test_ir_only_rejected_while_occupancy_runs(device, width, height):
    """The shared GMSL capture owner rejects IR before committing it; OCCG keeps streaming."""
    stereo = device.first_depth_sensor()
    ir = profile( stereo, rs.stream.infrared, rs.format.y8, w=width, h=height, index=1 )
    mapping, frames = sensor( device, 'Depth Mapping Camera' ), Collector()
    mapping.open( occ_profile( device ) )
    mapping.start( frames )
    frames.wait( rs.stream.occupancy, time.monotonic(), MIN_OCC_FRAMES )
    with pytest.raises( RuntimeError, match="IR channel" ):
        stereo.open( [ir] )
    frames.wait( rs.stream.occupancy, time.monotonic(), MIN_OCC_FRAMES )
    assert wait_state( device, OCC_ID, (1, 1, 1) ) == (1, 1, 1)
    mapping.stop()
    mapping.close()


def test_ir_and_perception_exclude_each_other(device):
    """IR and Perception share the GMSL IR channel."""
    stereo = device.first_depth_sensor()
    ir = profile( stereo, rs.stream.infrared, rs.format.y8, index=1 )   # same resolution as depth
    inputs = start_inputs( device, [ir] )
    with pytest.raises( RuntimeError, match="IR channel" ):
        sensor( device, 'Perception' ).open( pd_profile( device ) )
    assert mux_state( device, PD_ID ) == ( 0, 0, 0 )
    stop( inputs )

    inputs = start_inputs( device )
    pd = sensor( device, 'Perception' )
    pd.open( pd_profile( device ) )
    pd.start( lambda f: None )
    stop( inputs )
    with pytest.raises( RuntimeError, match="IR channel" ):
        start_inputs( device, [ir] )
    pd.stop()
    pd.close()
