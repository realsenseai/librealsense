# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Device point cloud (RS2_STREAM_POINT_CLOUD) on the Depth Mapping sensor: frame shape, no texture
# coordinates, one Mapping stream at a time, and PCL refused by the FW while Depth streams (Depth after PCL is allowed).
# Skips when the FW exposes no PCL profile.

import pytest
import time
import pyrealsense2 as rs
import pyrsutils as rsutils
from pytest_check import check
from rspy.pytest.device_helpers import require_min_fw_version
import logging
log = logging.getLogger(__name__)


pytestmark = [
    pytest.mark.device_each( "D585" ),
    pytest.mark.device_exclude( "D585S" ),
    pytest.mark.context( "nightly" ),
]

MAPPING_SENSOR_NAME = "Depth Mapping Camera"
FRAMES_TO_CHECK = 10
TIMEOUT = 5  # [sec]
MIN_FW = rsutils.version( "7.61.47150" )  # older FW publishes the PCL profiles but delivers no frames


def mapping_sensor( dev ):
    return next( ( s for s in dev.query_sensors() if s.get_info( rs.camera_info.name ) == MAPPING_SENSOR_NAME ), None )


def pcl_profile( sensor ):
    return next( ( p for p in sensor.profiles if p.stream_type() == rs.stream.point_cloud ), None )


@pytest.fixture
def pcl( test_device ):
    dev, _ = test_device
    require_min_fw_version( dev, MIN_FW, "device point cloud" )
    sensor = mapping_sensor( dev )
    if not sensor:
        pytest.skip( "no Depth Mapping sensor" )
    profile = pcl_profile( sensor )
    if not profile:
        pytest.skip( "FW exposes no PCL profile" )
    return dev, sensor, profile


def collect( sensor, profile, count ):
    frames = []
    sensor.open( profile )
    sensor.start( lambda f: frames.append( f ) if len( frames ) < count else None )
    deadline = time.time() + TIMEOUT
    while len( frames ) < count and time.time() < deadline:
        time.sleep( 0.05 )
    sensor.stop()
    sensor.close()
    return frames


def test_pcl_frames( pcl ):
    _, sensor, profile = pcl
    vsp = profile.as_video_stream_profile()
    frames = collect( sensor, profile, FRAMES_TO_CHECK )
    check.equal( len( frames ), FRAMES_TO_CHECK )
    for f in frames:
        pts = f.as_points()
        check.equal( f.get_profile().stream_type(), rs.stream.point_cloud )
        # every delivered frame is the bare W x H XYZ32F array (invalid frames are dropped)
        check.equal( pts.size(), vsp.width() * vsp.height() )
        check.equal( f.get_data_size(), vsp.width() * vsp.height() * 12 )
        check.is_none( pts.get_texture_coordinates() )


def test_one_mapping_stream_at_a_time( pcl ):
    _, sensor, profile = pcl
    other = next( ( p for p in sensor.profiles if p.stream_type() != rs.stream.point_cloud ), None )
    if not other:
        pytest.skip( "no second Mapping stream" )
    with pytest.raises( Exception ):
        sensor.open( [profile, other] )


def test_pcl_refused_while_depth_streams( pcl ):
    # The FW refuses the combination; the SDK reports its error at open() or start()
    dev, sensor, profile = pcl
    depth_sensor = dev.first_depth_sensor()
    depth_profile = next( p for p in depth_sensor.profiles
                          if p.stream_type() == rs.stream.depth and p.is_default() )
    depth_frames = []
    depth_sensor.open( depth_profile )
    depth_sensor.start( lambda f: depth_frames.append( 1 ) )
    try:
        deadline = time.time() + TIMEOUT  # the FW refuses PCL once Depth is running, not while it starts
        while not depth_frames and time.time() < deadline:
            time.sleep( 0.05 )
        with pytest.raises( RuntimeError ):
            sensor.open( profile )
            sensor.start( lambda f: None )
    finally:
        for close in ( sensor.stop, sensor.close, depth_sensor.stop, depth_sensor.close ):
            try:
                close()
            except RuntimeError:
                pass


def test_depth_allowed_while_pcl_streams( pcl ):
    dev, sensor, profile = pcl
    depth_sensor = dev.first_depth_sensor()
    # Depth joins at the PCL source tuple; GMSL FW refuses another resolution or rate
    vsp = profile.as_video_stream_profile()
    depth_profile = next( ( p for p in depth_sensor.profiles
                          if p.stream_type() == rs.stream.depth and p.format() == rs.format.z16 and p.fps() == vsp.fps()
                          and p.as_video_stream_profile().width() == vsp.width()
                          and p.as_video_stream_profile().height() == vsp.height() ), None )
    if not depth_profile:
        pytest.skip( "no Z16 Depth profile at the PCL size and rate" )
    pcl_frames = []
    sensor.open( profile )
    sensor.start( lambda f: pcl_frames.append( 1 ) )
    try:
        depth_sensor.open( depth_profile )
        depth_sensor.start( lambda f: None )
        time.sleep( 1 )
        depth_sensor.stop()
        depth_sensor.close()
    finally:
        sensor.stop()
        sensor.close()
    check.greater( len( pcl_frames ), 0 )


def test_pipeline_depth_and_pcl_refused( pcl ):
    # The pipeline opens Depth first, so the FW refuses the point cloud and start() reports it
    dev, _, _ = pcl
    cfg = rs.config()
    cfg.enable_device( dev.get_info( rs.camera_info.serial_number ) )
    cfg.enable_stream( rs.stream.depth )
    cfg.enable_stream( rs.stream.point_cloud )
    pipe = rs.pipeline()
    try:
        with pytest.raises( RuntimeError ):
            pipe.start( cfg )
    finally:
        try:
            pipe.stop()  # only when the FW let it start, so the next test finds the camera free
        except RuntimeError:
            pass
