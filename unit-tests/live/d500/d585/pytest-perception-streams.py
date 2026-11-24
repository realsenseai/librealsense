# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Perception streams on D555/D585, over USB and GMSL, in 2C and 3C.
# Object Detection (PD), Occupancy, LPCL and Point Cloud run without host Depth or Color capture.
# With graph-rebuild firmware, Occupancy/LPCL can share any native Depth size at the Mapping frame rate.
# Over USB the SDK refuses 2C colors next to Occupancy/LPCL, which the camera would starve; the camera refuses the rest.

import pytest
import pyrealsense2 as rs
import struct
import time
import pyrsutils as rsutils
from rspy.pytest.device_helpers import require_min_fw_version
import logging
log = logging.getLogger(__name__)


pytestmark = [
    pytest.mark.device_each( "D555", "D585" ),
    pytest.mark.device_type_exclude( "DDS" ),   # SDK admission rules are USB/GMSL-only
    pytest.mark.device_exclude( "D585S" ),   # the safety SKU has no perception sensor
    pytest.mark.context( "nightly" ),
]

MIN_FRAMES = { rs.stream.object_detection: 8,   # GMSL runs PD at ~15 fps
               rs.stream.occupancy: 15, rs.stream.labeled_point_cloud: 15, rs.stream.point_cloud: 10,   # PCL 1280x720 runs at 15 fps
               rs.stream.depth: 15, rs.stream.infrared: 15, rs.stream.color: 15 }
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
    close_sensors( dev )


def close_sensors(dev):
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
        profile = f.get_profile()
        stream_type = profile.stream_type()
        info = { 'time': time.monotonic(), 'type': stream_type, 'index': profile.stream_index(), 'size': f.get_data_size() }
        if stream_type == rs.stream.object_detection:
            head = bytes( f.get_data() )[:ODET_HEADER]
            info.update( magic=head[:4], payload=struct.unpack_from( '<I', head, 8 )[0], number=f.get_frame_number() )
        else:
            md = lambda m: f.get_frame_metadata( m ) if f.supports_frame_metadata( m ) else None
            info.update( counter=md( MD.frame_counter ) )
            if stream_type == rs.stream.occupancy:
                info.update( grid=( md( MD.occupancy_grid_rows ), md( MD.occupancy_grid_columns ) ) )
            elif stream_type == rs.stream.labeled_point_cloud:
                points = f.as_labeled_points()
                info.update( points=( points.get_width(), points.get_height() ) )
            elif stream_type == rs.stream.point_cloud:
                info.update( points=f.as_points().size() )
        self.frames.append( info )
    def of(self, stream_type, since=0., index=None):
        return [i for i in self.frames if i['type'] == stream_type and i['time'] >= since and index in ( None, i['index'] )]
    def wait(self, stream_type, since, count=None, timeout=FRAME_TIMEOUT_S, index=None):
        """The frames since `since`, once at least `count` arrived; fails on timeout."""
        count = count or MIN_FRAMES[stream_type]
        deadline = time.monotonic() + timeout
        while len( self.of( stream_type, since, index ) ) < count and time.monotonic() < deadline:
            time.sleep( 0.05 )
        frames = self.of( stream_type, since, index )
        assert len( frames ) >= count, f"{len( frames )} {stream_type} frames (index {index}), expected {count}"
        return frames


def check_content(dev, profile, frames):
    if profile.stream_type() == rs.stream.object_detection:
        # USB sends the exact result length, GMSL pads it to the profile size; the header declares the valid part
        assert all( i['magic'] == b'ODET' and ODET_PAYLOAD_MIN <= i['payload'] <= i['size'] - ODET_HEADER for i in frames )
        assert all( b['number'] > a['number'] for a, b in zip( frames, frames[1:] ) )
        return
    vp = profile.as_video_stream_profile()
    if profile.stream_type() == rs.stream.labeled_point_cloud:
        assert all( i['points'] == ( vp.width(), vp.height() ) for i in frames )
    elif profile.stream_type() == rs.stream.point_cloud:
        assert all( i['points'] == vp.width() * vp.height() for i in frames )
    else:
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


def rejected(sensor, profiles, frames, match=None):
    """True when the SDK refuses the start quickly, before the camera is involved."""
    begin = time.monotonic()
    with pytest.raises( RuntimeError, match=match ):
        start( sensor, profiles, frames )
    return time.monotonic() - begin < SDK_REJECT_S


@pytest.mark.parametrize( 'stream', ['object_detection', 'occupancy', 'labeled_point_cloud', 'point_cloud'] )
def test_standalone_lifecycle(device, stream):
    """Runs with no other capture, resumes after stop, and a second session starts after the first one closed."""
    if "D555" in device.get_info( rs.camera_info.name ):
        pytest.skip( "Perception lifecycle metadata checks are D585-only" )
    if stream == 'labeled_point_cloud':
        require_min_fw_version( device, rsutils.version( "7.62.0.0" ), "labeled point cloud" )   # older FW has no LPCL stream
    if stream == 'point_cloud':  # older FW publishes the profiles but delivers no frames
        require_min_fw_version( device, rsutils.version( "7.61.47150" ), "device point cloud" )
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
    if "D555" in device.get_info( rs.camera_info.name ):
        pytest.skip( "Concurrent PD/Occupancy lifecycle checks are D585-only" )
    pd, pd_profile = perception( device, rs.stream.object_detection )
    occ, occ_profile = perception( device, rs.stream.occupancy )
    frames = Collector()
    start( pd, pd_profile, frames )
    since = start( occ, occ_profile, frames )
    frames.wait( rs.stream.object_detection, since )
    frames.wait( rs.stream.occupancy, since )
    stop( occ )
    stop( pd )


# Both start orders must accept every native source, with either segmentation state.
DEPTH_SOURCES = [( 640, 360, 30 ), ( 1280, 720, 30 ), ( 848, 480, 30 ),
                 ( 640, 480, 30 ), ( 1280, 960, 30 ), ( 480, 270, 30 ), ( 424, 240, 30 )]


@pytest.fixture(params=[(stream, segmented) for stream in ('occupancy', 'labeled_point_cloud')
                       for segmented in (0, 1)],
                ids=['occupancy-seg-off', 'occupancy-seg-on', 'lpcl-seg-off', 'lpcl-seg-on'])
def mapping(device, request):
    require_min_fw_version( device, rsutils.version("7.61.47181"), "Mapping graph rebuild" )
    stream, segmented = request.param
    sensor, profile = perception( device, getattr( rs.stream, stream ) )
    option = rs.option.occupancy_grid_segmentation if stream == 'occupancy' else rs.option.lpcl_segmentation
    original = sensor.get_option( option ) if sensor.supports( option ) else None
    if original is None and (stream == 'labeled_point_cloud' or segmented):
        pytest.skip( "No Mapping segmentation control" )
    try:
        if original is not None:
            sensor.set_option( option, segmented )
        yield sensor, profile
    finally:
        # These controls are writable only before streaming, including after a failed test.
        close_sensors( device )
        if original is not None:
            sensor.set_option( option, original )


@pytest.mark.parametrize( 'source', DEPTH_SOURCES, ids=[f"{w}x{h}" for w, h, _ in DEPTH_SOURCES] )
def test_occ_after_depth(device, mapping, source):
    """With Depth running, Occupancy/LPCL starts with any native size at its frame rate."""
    occ, occ_profile = mapping
    stream_type = occ_profile.stream_type()
    stereo, frames = device.first_depth_sensor(), Collector()
    frames.wait( rs.stream.depth, start( stereo, [depth( device, *source )], frames ) )
    since = start( occ, occ_profile, frames )
    frames.wait( stream_type, since )
    frames.wait( rs.stream.depth, since )
    stop( occ )
    frames.wait( rs.stream.depth, time.monotonic(), MIN_FRAMES[rs.stream.depth] // 2 )   # Depth unaffected
    stop( stereo )


@pytest.mark.parametrize( 'source', DEPTH_SOURCES, ids=[f"{w}x{h}" for w, h, _ in DEPTH_SOURCES] )
def test_depth_joins_occ(device, mapping, source):
    """Native Depth can join either Mapping stream at the same rate."""
    occ, occ_profile = mapping
    stream_type = occ_profile.stream_type()
    stereo, frames = device.first_depth_sensor(), Collector()
    frames.wait( stream_type, start( occ, occ_profile, frames ) )
    since = start( stereo, [depth( device, *source )], frames )
    frames.wait( rs.stream.depth, since )
    frames.wait( stream_type, since )
    stop( stereo )   # Mapping keeps its own Depth source
    frames.wait( stream_type, time.monotonic() )
    stop( occ )


@pytest.mark.device_exclude( "D555" )
@pytest.mark.device_type_exclude( "GMSL" )
@pytest.mark.parametrize( 'source', [( 640, 360 ), ( 1280, 720 )], ids=['640x360', '1280x720'] )
@pytest.mark.parametrize( 'indices', [( 1, ), ( 2, ), ( 1, 2 )], ids=['ir1', 'ir2', 'ir1+ir2'] )
def test_depth_and_ir_join_mapping(device, mapping, source, indices):
    """D585 USB can add matching stereo profiles while Mapping is already delivering frames."""
    occ, occ_profile = mapping
    stream_type = occ_profile.stream_type()
    stereo, frames = device.first_depth_sensor(), Collector()
    requests = [depth( device, *source )] + [
        stereo_profile( device, rs.stream.infrared, rs.format.y8, *source, index=i ) for i in indices
    ]
    frames.wait( stream_type, start( occ, occ_profile, frames ) )
    since = start( stereo, requests, frames )
    frames.wait( rs.stream.depth, since )
    frames.wait( stream_type, since )
    for i in indices:
        frames.wait( rs.stream.infrared, since, index=i )
    stop( stereo )
    frames.wait( stream_type, time.monotonic() )
    stop( occ )


@pytest.mark.device_type_exclude( "GMSL" )
def test_colors_and_occ_2c(device, mapping):
    """Over USB 2C colors cannot join Mapping, with or without Depth: the camera would starve them."""
    if not is_2c( device ):
        pytest.skip( "3C colors stream from their own sensor" )
    occ, occ_profile = mapping
    stream_type = occ_profile.stream_type()
    stereo, frames = device.first_depth_sensor(), Collector()
    frames.wait( stream_type, start( occ, occ_profile, frames ) )
    assert rejected( stereo, colors( device ), frames, match="Color cannot start while" )
    assert rejected( stereo, [depth( device, 640, 360 )] + colors( device ), frames, match="Color cannot start while" )
    stop( occ )


def test_depth_and_colors_then_occ_2c(device, mapping):
    """Over GMSL Mapping joins Depth and colors; over USB preserve the SDK refusal for the shared sensor."""
    if not is_2c( device ):
        pytest.skip( "3C colors stream from their own sensor" )
    occ, occ_profile = mapping
    stereo, frames = device.first_depth_sensor(), Collector()
    frames.wait( rs.stream.color, start( stereo, [depth( device, 640, 360 )] + colors( device ), frames ) )
    if not is_gmsl( device ):
        assert rejected( occ, occ_profile, frames, match="cannot start while the depth sensor streams color" )
    else:
        since = start( occ, occ_profile, frames )
        for stream_type in ( occ_profile.stream_type(), rs.stream.depth, rs.stream.color ):
            frames.wait( stream_type, since )
        stop( occ )
    stop( stereo )
