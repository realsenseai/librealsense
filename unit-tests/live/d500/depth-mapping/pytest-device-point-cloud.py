# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Device point cloud (RS2_STREAM_POINT_CLOUD) on the Depth Mapping sensor: frame shape, no texture
# coordinates, one Mapping stream at a time, and PCL rejected while Depth streams (Depth after PCL is allowed).
# Skips when the FW exposes no PCL profile.

import pytest
import time
import pyrealsense2 as rs
from pytest_check import check
import logging
log = logging.getLogger(__name__)


pytestmark = [
    pytest.mark.device_each( "D555", "D585" ),
    pytest.mark.context( "nightly" ),
]

MAPPING_SENSOR_NAME = "Depth Mapping Camera"
FRAMES_TO_CHECK = 10
TIMEOUT = 5  # [sec]


def mapping_sensor( dev ):
    return next( ( s for s in dev.query_sensors() if s.get_info( rs.camera_info.name ) == MAPPING_SENSOR_NAME ), None )


def pcl_profile( sensor ):
    return next( ( p for p in sensor.profiles if p.stream_type() == rs.stream.point_cloud ), None )


@pytest.fixture
def pcl( test_device ):
    dev, _ = test_device
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
        # every delivered frame is the bare W x H XYZ32F array (invalid frames are dropped, MAP1 headers stripped)
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


def test_pcl_rejected_while_depth_streams( pcl ):
    dev, sensor, profile = pcl
    depth_sensor = dev.first_depth_sensor()
    depth_profile = next( p for p in depth_sensor.profiles
                          if p.stream_type() == rs.stream.depth and p.is_default() )
    depth_sensor.open( depth_profile )
    depth_sensor.start( lambda f: None )
    try:
        with pytest.raises( Exception ):
            sensor.open( profile )
    finally:
        depth_sensor.stop()
        depth_sensor.close()


def test_depth_allowed_while_pcl_streams( pcl ):
    dev, sensor, profile = pcl
    depth_sensor = dev.first_depth_sensor()
    depth_profile = next( p for p in depth_sensor.profiles
                          if p.stream_type() == rs.stream.depth and p.is_default() )
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


def test_pipeline_rejects_depth_and_pcl( pcl ):
    dev, _, _ = pcl
    cfg = rs.config()
    cfg.enable_device( dev.get_info( rs.camera_info.serial_number ) )
    cfg.enable_stream( rs.stream.depth )
    cfg.enable_stream( rs.stream.point_cloud )
    pipe = rs.pipeline()
    with pytest.raises( Exception ):
        pipe.start( cfg )
