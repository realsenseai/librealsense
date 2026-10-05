# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# GMSL Perception MUX: Object Detection (PD) and Occupancy (OCC) time-share the GMSL IR channel and are told apart
# by their metadata. Each stream is still its own sensor and opens and closes independently; the camera starts the
# shared capture with the first and stops it with the last, and needs Depth and Color streaming as their inputs.
#
# Tests follow the camera's current mode: Occupancy exists on 3C only, so its tests skip on 2C.

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

DURATION_S = 4
MIN_PD_FRAMES = 10                    # PD runs at ~15 fps
MIN_OCC_FRAMES = 60                   # OCC runs at ~30 fps
PD_ID, OCC_ID = 6, 7                  # stream ids of the camera's MUX control (HWMC 0xC0)
MUX_CONTROL, MUX_SET_ENABLE, MUX_GET_STATE = 0xC0, 1, 2


@pytest.fixture
def device(test_device):
    dev, _ = test_device
    yield dev
    for sensor in dev.query_sensors():   # never hand the next test an open sensor
        for close in ( sensor.stop, sensor.close ):
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
    return sensor( dev, 'Depth Mapping Camera' ) is not None


def profile(s, stream_type, fmt, w=1280, h=720, index=None):
    for p in s.get_stream_profiles():
        vp = p.as_video_stream_profile()
        if p.stream_type() == stream_type and p.format() == fmt and p.fps() == 30 and vp \
                and ( vp.width(), vp.height() ) == ( w, h ) and index in ( None, p.stream_index() ):
            return p
    pytest.skip( f"No {stream_type} {fmt} {w}x{h}@30" )


def pd_profile(dev):
    return next( p for p in sensor( dev, 'Perception' ).get_stream_profiles()
                 if p.stream_type() == rs.stream.object_detection )


def occ_profile(dev):
    return next( p for p in sensor( dev, 'Depth Mapping Camera' ).get_stream_profiles()
                 if p.stream_type() == rs.stream.occupancy )


def mux_state(dev, stream_id):
    """(requested, applied, capture_active) as the camera reports it, or None if it rejects the query."""
    dp = rs.debug_protocol( dev )
    reply = bytes( dp.send_and_receive_raw_data( dp.build_command( MUX_CONTROL, 1, MUX_GET_STATE, stream_id, 0 ) ) )
    opcode, = struct.unpack_from( '<I', reply )
    return struct.unpack_from( '<III', reply, 4 ) if opcode == MUX_CONTROL and len( reply ) >= 16 else None


def start_inputs(dev, extra=()):
    """Depth and Color, the Perception inputs, plus any extra Stereo Module profiles; returns the started sensors."""
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
    """Frames per stream name, with the checks this file needs taken at arrival."""
    def __init__(self):
        self.frames = {}
    def __call__(self, f):
        info = { 'time': time.monotonic(), 'size': f.get_data_size(), 'ts': f.get_timestamp() }
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


def test_perception_sensors_enumerate(device):
    pd = pd_profile( device )
    assert pd.format() == rs.format.y8 and pd.fps() == 30
    if is_3c( device ):
        occ = occ_profile( device ).as_video_stream_profile()
        assert ( occ.format(), occ.width(), occ.height(), occ.fps() ) == ( rs.format.y8, 320, 256, 30 )
    else:
        assert sensor( device, 'Depth Mapping Camera' ) is None, "2C produces no occupancy"
    assert mux_state( device, PD_ID ) == ( 0, 0, 0 )


def test_pd_open_close_twice(device):
    inputs = start_inputs( device )
    pd = sensor( device, 'Perception' )
    for _ in range( 2 ):   # the shared capture must restart cleanly
        frames = Collector()
        pd.open( pd_profile( device ) )
        pd.start( frames )
        time.sleep( DURATION_S )
        state = mux_state( device, PD_ID )
        pd.stop()
        pd.close()
        got = frames.frames.get( rs.stream.object_detection, [] )
        assert len( got ) >= MIN_PD_FRAMES
        assert all( i['size'] == 2347 and i['magic'] == b'ODET' for i in got )
        assert all( b['ts'] > a['ts'] for a, b in zip( got, got[1:] ) )
        assert state == ( 1, 1, 1 )
        assert mux_state( device, PD_ID ) == ( 0, 0, 0 )
    stop( inputs )


def test_pd_stop_start_resumes(device):
    inputs = start_inputs( device )
    pd = sensor( device, 'Perception' )
    frames = Collector()
    pd.open( pd_profile( device ) )
    pd.start( frames )
    time.sleep( DURATION_S / 2 )
    pd.stop()
    restart = time.monotonic()
    pd.start( frames )
    time.sleep( DURATION_S / 2 )
    pd.stop()
    pd.close()
    stop( inputs )
    assert frames.count( rs.stream.object_detection, since=restart ) >= MIN_PD_FRAMES / 2


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


def test_pd_and_occ_independent(device):
    if not is_3c( device ):
        pytest.skip( "Occupancy is 3C only" )
    inputs = start_inputs( device )
    pd, mapping = sensor( device, 'Perception' ), sensor( device, 'Depth Mapping Camera' )
    frames = Collector()

    def step(action, pd_on, occ_on):
        action()
        since = time.monotonic() + 0.6   # frames already in flight around a switch
        time.sleep( DURATION_S )
        assert ( frames.count( rs.stream.object_detection, since ) >= MIN_PD_FRAMES ) == pd_on
        assert ( frames.count( rs.stream.occupancy, since ) >= MIN_OCC_FRAMES ) == occ_on
        assert mux_state( device, PD_ID )[0] == pd_on and mux_state( device, OCC_ID )[0] == occ_on

    def start(s, p):
        s.open( p )
        s.start( frames )

    step( lambda: start( pd, pd_profile( device ) ), True, False )
    step( lambda: start( mapping, occ_profile( device ) ), True, True )
    step( lambda: stop( [pd] ), False, True )
    step( lambda: stop( [mapping] ), False, False )
    stop( inputs )
    assert mux_state( device, PD_ID ) == ( 0, 0, 0 ) and mux_state( device, OCC_ID ) == ( 0, 0, 0 )

    occ = frames.frames[rs.stream.occupancy]
    assert all( i['size'] == 320 * 256 for i in occ )
    assert all( i['grid'] == ( 256, 320 ) for i in occ ), "OCC frames need their occupancy metadata"
    assert all( i['cells_ok'] for i in occ )
    assert all( b['counter'] > a['counter'] for a, b in zip( occ, occ[1:] ) )


def test_occ_start_order(device):
    """OCC first, then PD on top - the order the viewer typically uses."""
    if not is_3c( device ):
        pytest.skip( "Occupancy is 3C only" )
    inputs = start_inputs( device )
    pd, mapping = sensor( device, 'Perception' ), sensor( device, 'Depth Mapping Camera' )
    frames = Collector()
    mapping.open( occ_profile( device ) )
    mapping.start( frames )
    time.sleep( 1 )
    pd.open( pd_profile( device ) )
    pd.start( frames )
    since = time.monotonic()
    time.sleep( DURATION_S )
    stop( [pd, mapping] )
    stop( inputs )
    assert frames.count( rs.stream.object_detection, since ) >= MIN_PD_FRAMES
    assert frames.count( rs.stream.occupancy, since ) >= MIN_OCC_FRAMES


def test_stale_request_cleared(device):
    """A stream request another client left behind is cleared when the SDK starts the capture."""
    if not is_3c( device ):
        pytest.skip( "Needs a second stream, OCC is 3C only" )
    dp = rs.debug_protocol( device )
    dp.send_and_receive_raw_data( dp.build_command( MUX_CONTROL, 1, MUX_SET_ENABLE, OCC_ID, 1 ) )
    assert mux_state( device, OCC_ID )[0] == 1
    inputs = start_inputs( device )
    pd = sensor( device, 'Perception' )
    pd.open( pd_profile( device ) )
    pd.start( lambda f: None )
    time.sleep( 1 )
    assert mux_state( device, OCC_ID )[0] == 0
    pd.stop()
    pd.close()
    stop( inputs )


def test_ir_and_perception_exclude_each_other(device):
    """IR and Perception share the GMSL IR channel."""
    stereo = device.first_depth_sensor()
    ir = profile( stereo, rs.stream.infrared, rs.format.y8, index=1 )   # same resolution as depth
    try:
        inputs = start_inputs( device, [ir] )
    except RuntimeError as e:
        pytest.skip( f"IR does not start on this device now: {e}" )
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


def test_refused_start_recovers(device):
    """Without its inputs the camera refuses Perception; the SDK must report it and leave nothing behind."""
    if is_3c( device ):
        pytest.skip( "A refused start can leave 3C streaming broken until a power cycle (FW)" )
    pd = sensor( device, 'Perception' )
    with pytest.raises( RuntimeError, match="refused" ):
        pd.open( pd_profile( device ) )
    assert mux_state( device, PD_ID ) == ( 0, 0, 0 )
    inputs = start_inputs( device )
    frames = Collector()
    pd.open( pd_profile( device ) )
    pd.start( frames )
    time.sleep( DURATION_S )
    pd.stop()
    pd.close()
    stop( inputs )
    assert frames.count( rs.stream.object_detection ) >= MIN_PD_FRAMES
