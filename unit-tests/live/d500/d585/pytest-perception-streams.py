# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Perception streams on D585, over USB and GMSL, in 2C and 3C.
# Object Detection (PD) and Occupancy run on their own, with no Depth or Color capture. The camera builds Occupancy
# from Depth at 640x360 or 1280x720 at the Occupancy rate; Occupancy started first fixes that source at 640x360.
# The SDK rejects the depth sensor combinations the camera would refuse or starve before the camera is touched.

import pytest
import pyrealsense2 as rs
import struct
import time
import logging
log = logging.getLogger(__name__)


pytestmark = [
    pytest.mark.device_each( "D585" ),
    pytest.mark.device_exclude( "D585S" ),   # the safety SKU has no perception sensor
    pytest.mark.context( "nightly" ),
]

MIN_FRAMES = { rs.stream.object_detection: 8,   # GMSL runs PD at ~15 fps
               rs.stream.occupancy: 15, rs.stream.depth: 15, rs.stream.color: 15 }
FRAME_TIMEOUT_S = 5       # first frames of a fresh stream
QUIET_S = 0.3             # how long a stopped stream must stay silent
SDK_REJECT_S = 1.0        # a camera refusal takes longer
ODET_HEADER = 20          # magic, version, type, flags, payload size, spare, crc
ODET_PAYLOAD_MIN = 23     # payload header of a result with no detections
MD = rs.frame_metadata_value


@pytest.fixture
def device(test_device):
    dev, _ = test_device
    yield dev
    for s in dev.query_sensors():        # never hand the next test an open sensor
        for close in ( s.stop, s.close ):
            try:
                close()
            except RuntimeError:
                pass                     # not started/open, the normal path


def is_gmsl(dev):
    return dev.get_info( rs.camera_info.connection_type ) == 'GMSL'


def is_2c(dev):
    return not any( s.get_info( rs.camera_info.name ) == 'RGB Camera' for s in dev.query_sensors() )


def perception(dev, stream_type):
    """(sensor, profile) of a perception stream, skipping when the device does not expose it."""
    for s in dev.query_sensors():
        p = next( ( p for p in s.get_stream_profiles() if p.stream_type() == stream_type ), None )
        if p is not None:
            return s, p
    pytest.skip( f"Device has no {stream_type} stream" )


def stereo_profile(dev, stream_type, fmt, w, h, fps=30, index=None):
    for p in dev.first_depth_sensor().get_stream_profiles():
        vp = p.as_video_stream_profile()
        if p.stream_type() == stream_type and p.format() == fmt and p.fps() == fps and vp \
                and ( vp.width(), vp.height() ) == ( w, h ) and index in ( None, p.stream_index() ):
            return p
    pytest.skip( f"No {stream_type} {fmt} {w}x{h}@{fps}" )


def depth(dev, w, h, fps=30):
    return stereo_profile( dev, rs.stream.depth, rs.format.z16, w, h, fps )


def colors(dev):
    return [stereo_profile( dev, rs.stream.color, rs.format.nv12, 640, 360, index=i ) for i in ( 1, 2 )]


class Collector:
    """Frames with what the checks need, taken at arrival."""
    def __init__(self):
        self.frames = []
    def __call__(self, f):
        stream_type = f.get_profile().stream_type()
        info = { 'time': time.monotonic(), 'type': stream_type, 'size': f.get_data_size() }
        if stream_type == rs.stream.object_detection:
            head = bytes( f.get_data() )[:ODET_HEADER]
            info.update( magic=head[:4], payload=struct.unpack_from( '<I', head, 8 )[0], number=f.get_frame_number() )
        elif stream_type == rs.stream.occupancy:
            md = lambda m: f.get_frame_metadata( m ) if f.supports_frame_metadata( m ) else None
            info.update( counter=md( MD.frame_counter ), grid=( md( MD.occupancy_grid_rows ), md( MD.occupancy_grid_columns ) ) )
        self.frames.append( info )
    def of(self, stream_type, since=0.):
        return [i for i in self.frames if i['type'] == stream_type and i['time'] >= since]
    def wait(self, stream_type, since, count=None, timeout=FRAME_TIMEOUT_S):
        """The frames since `since`, once at least `count` arrived; fails on timeout."""
        count = count or MIN_FRAMES[stream_type]
        deadline = time.monotonic() + timeout
        while len( self.of( stream_type, since ) ) < count and time.monotonic() < deadline:
            time.sleep( 0.05 )
        frames = self.of( stream_type, since )
        assert len( frames ) >= count, f"{len( frames )} {stream_type} frames, expected {count}"
        return frames


def check_content(dev, profile, frames):
    if profile.stream_type() == rs.stream.object_detection:
        # USB sends the exact result length, GMSL pads it to the profile size; the header declares the valid part
        assert all( i['magic'] == b'ODET' and ODET_PAYLOAD_MIN <= i['payload'] <= i['size'] - ODET_HEADER for i in frames )
        assert all( b['number'] > a['number'] for a, b in zip( frames, frames[1:] ) )
        return
    vp = profile.as_video_stream_profile()
    assert all( i['grid'] == ( vp.height(), vp.width() ) for i in frames ), "grids need their occupancy metadata"
    if is_gmsl( dev ):   # over USB the FW reports counter 0 on every grid until its fix lands
        assert all( i['counter'] is not None for i in frames )
        assert all( b['counter'] > a['counter'] for a, b in zip( frames, frames[1:] ) )


def start(sensor, profiles, frames):
    sensor.open( profiles )
    sensor.start( frames )
    return time.monotonic()


def stop(sensor):
    sensor.stop()
    sensor.close()


def rejected(sensor, profiles, frames):
    """True when the SDK refuses the start quickly, before the camera is involved."""
    begin = time.monotonic()
    with pytest.raises( RuntimeError ):
        start( sensor, profiles, frames )
    return time.monotonic() - begin < SDK_REJECT_S


@pytest.mark.parametrize( 'stream', ['object_detection', 'occupancy'] )
def test_standalone_lifecycle(device, stream):
    """Runs with no other capture, resumes after stop, and a second session starts after the first one closed."""
    stream_type = getattr( rs.stream, stream )
    sensor, profile = perception( device, stream_type )
    frames = Collector()
    check_content( device, profile, frames.wait( stream_type, start( sensor, profile, frames ) ) )
    sensor.stop()
    sensor.start( frames )
    check_content( device, profile, frames.wait( stream_type, time.monotonic() ) )
    stop( sensor )
    stopped = len( frames.of( stream_type ) )
    time.sleep( QUIET_S )
    assert len( frames.of( stream_type ) ) == stopped, "no frames after stop"
    check_content( device, profile, frames.wait( stream_type, start( sensor, profile, frames ) ) )
    stop( sensor )


def test_pd_and_occ_standalone(device):
    pd, pd_profile = perception( device, rs.stream.object_detection )
    occ, occ_profile = perception( device, rs.stream.occupancy )
    frames = Collector()
    start( pd, pd_profile, frames )
    since = start( occ, occ_profile, frames )
    frames.wait( rs.stream.object_detection, since )
    frames.wait( rs.stream.occupancy, since )
    stop( occ )
    stop( pd )


@pytest.mark.parametrize( 'source, allowed', [
    ( ( 640, 360, 30 ), True ),
    ( ( 1280, 720, 30 ), True ),
    ( ( 848, 480, 30 ), False ),
    ( ( 640, 360, 15 ), False ),
] )
def test_occ_after_depth(device, source, allowed):
    """With Depth running, Occupancy starts only when Depth can feed it."""
    occ, occ_profile = perception( device, rs.stream.occupancy )
    stereo, frames = device.first_depth_sensor(), Collector()
    frames.wait( rs.stream.depth, start( stereo, [depth( device, *source )], frames ) )
    if allowed:
        since = start( occ, occ_profile, frames )
        frames.wait( rs.stream.occupancy, since )
        frames.wait( rs.stream.depth, since )
        stop( occ )
    else:
        assert rejected( occ, occ_profile, frames )
        frames.wait( rs.stream.depth, time.monotonic(), MIN_FRAMES[rs.stream.depth] // 2 )   # Depth unaffected
    stop( stereo )


@pytest.mark.parametrize( 'join', ['depth 640x360', 'depth 1280x720', 'depth 640x360@15', 'depth+ir 640x360'] )
def test_depth_joins_occ(device, join):
    """Occupancy started alone fixes its Depth source at 640x360: only that Depth, alone, can join it."""
    occ, occ_profile = perception( device, rs.stream.occupancy )
    stereo, frames = device.first_depth_sensor(), Collector()
    requests = { 'depth 640x360': lambda: [depth( device, 640, 360 )],
                 'depth 1280x720': lambda: [depth( device, 1280, 720 )],
                 'depth 640x360@15': lambda: [depth( device, 640, 360, 15 )],
                 'depth+ir 640x360': lambda: [depth( device, 640, 360 ),
                                              stereo_profile( device, rs.stream.infrared, rs.format.y8, 640, 360, index=1 )] }[join]()
    frames.wait( rs.stream.occupancy, start( occ, occ_profile, frames ) )
    if join == 'depth 640x360':
        since = start( stereo, requests, frames )
        frames.wait( rs.stream.depth, since )
        frames.wait( rs.stream.occupancy, since )
        stop( stereo )   # Occupancy keeps its own Depth source
    else:
        assert rejected( stereo, requests, frames )
    frames.wait( rs.stream.occupancy, time.monotonic() )
    stop( occ )


def test_colors_and_occ_2c(device):
    """2C colors stream from the depth sensor: they cannot join Occupancy, nor feed it without Depth."""
    if not is_2c( device ):
        pytest.skip( "3C colors stream from their own sensor" )
    occ, occ_profile = perception( device, rs.stream.occupancy )
    stereo, frames = device.first_depth_sensor(), Collector()
    frames.wait( rs.stream.occupancy, start( occ, occ_profile, frames ) )
    assert rejected( stereo, colors( device ), frames )
    stop( occ )
    frames.wait( rs.stream.color, start( stereo, colors( device ), frames ) )
    assert rejected( occ, occ_profile, frames )
    stop( stereo )


def test_depth_and_colors_then_occ_2c(device):
    """Over GMSL Occupancy joins running Depth and colors; over USB the colors would stop, so the SDK refuses it."""
    if not is_2c( device ):
        pytest.skip( "3C colors stream from their own sensor" )
    occ, occ_profile = perception( device, rs.stream.occupancy )
    stereo, frames = device.first_depth_sensor(), Collector()
    frames.wait( rs.stream.color, start( stereo, [depth( device, 640, 360 )] + colors( device ), frames ) )
    if not is_gmsl( device ):
        assert rejected( occ, occ_profile, frames )
    else:
        since = start( occ, occ_profile, frames )
        for stream_type in ( rs.stream.occupancy, rs.stream.depth, rs.stream.color ):
            frames.wait( stream_type, since )
        stop( occ )
    stop( stereo )
