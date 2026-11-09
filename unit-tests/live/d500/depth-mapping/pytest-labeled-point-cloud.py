# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Labeled point cloud (RS2_STREAM_LABELED_POINT_CLOUD) of D585 and D585S: the stream
# starts and every frame is a well-formed organized cloud.
# The frame describes its own geometry; the stream profile may not. On D585S (SC1.1, USB) the profile is the
# 2880x1040 transport carrier while the cloud inside is 640x360, so consumers must size the cloud from
# labeled_points.get_width() / get_height(), never from the profile.
# The cloud content (which points / labels) depends on the scene and is not checked; only its shape.

import pytest
import time
import numpy as np
import pyrealsense2 as rs
from pytest_check import check
import logging
log = logging.getLogger(__name__)


pytestmark = [
    pytest.mark.device_each( "D585" ),   # name match: D585 and D585S
]

FRAMES_TO_CHECK = 30
TIMEOUT = 10                       # [sec]
CLOUD_SIZES = { (320, 180), (640, 360) }
BYTES_PER_POINT = 3 * 4 + 1        # XYZ float32 + uint8 label
INVALID_VALUE = 404.0              # see labeled_point_cloud_utilities::invalid_value


def lpcl_profile( dev ):
    for sensor in dev.query_sensors():
        profiles = [p for p in sensor.profiles if p.stream_type() == rs.stream.labeled_point_cloud]
        if profiles:
            return sensor, next( ( p for p in profiles if p.is_default() ), profiles[0] )
    return None, None


@pytest.fixture
def lpcl( test_device ):
    dev, _ = test_device
    sensor, profile = lpcl_profile( dev )
    assert profile, "no sensor exposes a labeled point cloud profile"
    return dev, sensor, profile


def summarize( f ):
    """Shape of one frame, read inside the callback so no frame is held (the frame pool is small)"""
    s = { 'is_lpcl': f.is_labeled_points(), 'stream': f.get_profile().stream_type(), 'data_size': f.get_data_size() }
    if not s['is_lpcl']:
        return s
    lp = f.as_labeled_points()
    s['width'], s['height'] = lp.get_width(), lp.get_height()
    vertices = np.asanyarray( lp.get_vertices() ).view( np.float32 ).reshape( -1, 3 )  # sized by the SDK's size()
    labels = np.asanyarray( lp.get_labels() )
    s['n_vertices'], s['n_labels'] = len( vertices ), len( labels )
    valid = np.all( np.isfinite( vertices ), axis=1 ) & np.all( vertices != INVALID_VALUE, axis=1 ) & ( vertices[:, 2] > 0 )
    s['n_valid'] = int( np.count_nonzero( valid ) )
    return s


def collect( sensor, profile, count ):
    summaries, errors = [], []

    def on_frame( f ):
        if len( summaries ) >= count:
            return
        try:
            summaries.append( summarize( f ) )
        except Exception as e:
            errors.append( str( e ) )

    sensor.open( profile )
    sensor.start( on_frame )
    try:
        deadline = time.time() + TIMEOUT
        while len( summaries ) < count and time.time() < deadline:
            time.sleep( 0.05 )
    finally:
        sensor.stop()
        sensor.close()
    return summaries, errors


def test_lpcl_streams_well_formed_frames( lpcl ):
    dev, sensor, profile = lpcl
    vsp = profile.as_video_stream_profile()
    frames, errors = collect( sensor, profile, FRAMES_TO_CHECK )

    check.equal( errors, [], "reading a labeled point cloud frame threw" )
    check.equal( len( frames ), FRAMES_TO_CHECK, f"frames received within {TIMEOUT}s" )

    total_valid = 0
    for i, s in enumerate( frames ):
        check.is_true( s['is_lpcl'], f"frame {i} is not labeled_points" )
        check.equal( s['stream'], rs.stream.labeled_point_cloud )
        if not s['is_lpcl']:
            continue
        w, h, n = s['width'], s['height'], s['n_vertices']
        check.is_in( (w, h), CLOUD_SIZES, f"frame {i}: unexpected cloud size {w}x{h}" )
        check.equal( n, w * h, f"frame {i}: {n} points in a {w}x{h} cloud" )
        check.equal( s['n_labels'], n, f"frame {i}: label buffer length" )
        check.greater_equal( s['data_size'], n * BYTES_PER_POINT, f"frame {i}: payload smaller than the cloud" )
        total_valid += s['n_valid']

    # Any non-empty scene gives some valid points; all-invalid means the payload is not being parsed
    check.greater( total_valid, 0, "no valid vertex in any frame" )

    if frames and frames[0]['is_lpcl']:
        f0 = frames[0]
        log.info( f"{dev.get_info( rs.camera_info.name )}: profile {vsp.width()}x{vsp.height()}, "
                  f"cloud {f0['width']}x{f0['height']}, {total_valid / len( frames ):.0f} valid points per frame" )
