# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import logging
import pyrealsense2 as rs
from pytest_check import check
import sw_device as sw

log = logging.getLogger(__name__)


def frame_metadata_values():
    return [rs.frame_metadata_value.__getattribute__( rs.frame_metadata_value, k )
            for k, v in rs.frame_metadata_value.__dict__.items()
            if str(v).startswith('frame_metadata_value.')]


#############################################################################################
#
def test_nothing_supported_by_default():
    with sw.sensor( "Stereo Module" ) as sensor:
        depth = sensor.video_stream( "Depth", rs.stream.depth, rs.format.z16 )
        #ir = sensor.video_stream( "Infrared", rs.stream.infrared, rs.format.y8 )
        sensor.start( depth )

        # Publish a frame
        f = sensor.publish( depth.frame() )

        # Some metadata may be automatically added, even for software sensors
        expected = set()
        for md in frame_metadata_values():
            log.debug( 'metadata %s', md )
            if md in expected:
                check.is_true( f.supports_frame_metadata( md ))
            else:
                check.is_false( f.supports_frame_metadata( md ))
#
#############################################################################################
#
def test_set_one_value():
    with sw.sensor( "Stereo Module" ) as sensor:
        depth = sensor.video_stream( "Depth", rs.stream.depth, rs.format.z16 )
        sensor.start( depth )

        # Metadata is set on the sensor, not the software frame
        sensor.set( rs.frame_metadata_value.white_balance, 0xbaad )

        # Publish the frame
        f = sensor.publish( depth.frame() )

        # Frame should have received the metadata from the sensor
        assert f.supports_frame_metadata( rs.frame_metadata_value.white_balance )
        assert f.get_frame_metadata( rs.frame_metadata_value.white_balance ) == 0xbaad
#
#############################################################################################
#
def test_post_frame_metadata_does_not_affect_frame():
    with sw.sensor( "Stereo Module" ) as sensor:
        depth = sensor.video_stream( "Depth", rs.stream.depth, rs.format.z16 )
        sensor.start( depth )

        sensor.set( rs.frame_metadata_value.white_balance, 0xbaad )

        f = sensor.publish( depth.frame() )

        sensor.set( rs.frame_metadata_value.white_balance, 0xf00d )

        assert f.get_frame_metadata( rs.frame_metadata_value.white_balance ) == 0xbaad
#
#############################################################################################
#
def test_metadata_is_kept_from_frame_to_frame():
    with sw.sensor( "Stereo Module" ) as sensor:
        depth = sensor.video_stream( "Depth", rs.stream.depth, rs.format.z16 )
        sensor.start( depth )

        sensor.set( rs.frame_metadata_value.white_balance, 0xbaad )
        f1 = sensor.publish( depth.frame() )
        assert f1.get_frame_metadata( rs.frame_metadata_value.white_balance ) == 0xbaad

        f2 = sensor.publish( depth.frame() )
        assert f2.get_frame_metadata( rs.frame_metadata_value.white_balance ) == 0xbaad
#
#############################################################################################
#
def test_prev_frame_does_not_pick_up_new_data_from_new_frame():
    with sw.sensor( "Stereo Module" ) as sensor:
        depth = sensor.video_stream( "Depth", rs.stream.depth, rs.format.z16 )
        sensor.start( depth )

        sensor.set( rs.frame_metadata_value.white_balance, 0xbaad )
        f1 = sensor.publish( depth.frame() )

        sensor.set( rs.frame_metadata_value.actual_fps, 0xf00d )
        f2 = sensor.publish( depth.frame() )

        assert f2.get_frame_metadata( rs.frame_metadata_value.white_balance ) == 0xbaad
        assert f2.get_frame_metadata( rs.frame_metadata_value.actual_fps ) == 0xf00d

        assert not f1.supports_frame_metadata( rs.frame_metadata_value.actual_fps )
#
#############################################################################################
#
def test_multiple_streams_per_sensor_should_share_metadata():
    with sw.sensor( "Stereo Module" ) as sensor:
        depth = sensor.video_stream( "Depth", rs.stream.depth, rs.format.z16 )
        ir = sensor.video_stream( "Infrared", rs.stream.infrared, rs.format.y8 )
        sensor.start( depth, ir )

        sensor.set( rs.frame_metadata_value.white_balance, 0xbaad )
        d1 = sensor.publish( depth.frame() )

        sensor.set( rs.frame_metadata_value.actual_fps, 0xf00d )
        ir1 = sensor.publish( ir.frame() )

        sensor.set( rs.frame_metadata_value.saturation, 0x1eaf )
        d2 = sensor.publish( depth.frame() )

        sensor.set( rs.frame_metadata_value.contrast, 0xfee1 )
        ir2 = sensor.publish( ir.frame() )

        sensor.set( rs.frame_metadata_value.contrast, 0x600d )
        d3 = sensor.publish( depth.frame() )
        ir3 = sensor.publish( ir.frame() )

        assert d1.get_frame_metadata( rs.frame_metadata_value.white_balance ) == 0xbaad
        assert ir1.supports_frame_metadata( rs.frame_metadata_value.white_balance )
        assert d2.get_frame_metadata( rs.frame_metadata_value.actual_fps ) == 0xf00d
        assert ir2.get_frame_metadata( rs.frame_metadata_value.saturation ) == 0x1eaf
        assert ir2.get_frame_metadata( rs.frame_metadata_value.contrast ) == 0xfee1
        assert d3.get_frame_metadata( rs.frame_metadata_value.contrast ) == 0x600d
        assert ir3.get_frame_metadata( rs.frame_metadata_value.contrast ) == 0x600d
#
#############################################################################################
#
def test_two_sensors_intermixed_frames():
    with sw.sensor( "Stereo Module" ) as stereo:
        depth = stereo.video_stream( "Depth", rs.stream.depth, rs.format.z16 )
        stereo.start( depth )
        with sw.sensor( "RGB Camera" ) as rgb:
            color = rgb.video_stream( "Color", rs.stream.color, rs.format.yuyv )
            rgb.start( color )

            stereo.set( rs.frame_metadata_value.white_balance, 0xbaad )
            d1 = stereo.publish( depth.frame() )
            assert d1.get_frame_metadata( rs.frame_metadata_value.white_balance ) == 0xbaad

            rgb.set( rs.frame_metadata_value.actual_fps, 0xf00d )
            stereo.set( rs.frame_metadata_value.actual_fps, 0xbaad )
            c1 = rgb.publish( color.frame() )
            assert not d1.supports_frame_metadata( rs.frame_metadata_value.actual_fps )
            assert not c1.supports_frame_metadata( rs.frame_metadata_value.white_balance )
            assert c1.get_frame_metadata( rs.frame_metadata_value.actual_fps ) == 0xf00d

            stereo.set( rs.frame_metadata_value.saturation, 0x1eaf )
            rgb.set( rs.frame_metadata_value.saturation, 0xfeed )
            d2 = stereo.publish( depth.frame() )
            assert not c1.supports_frame_metadata( rs.frame_metadata_value.saturation )
            assert d2.get_frame_metadata( rs.frame_metadata_value.saturation ) == 0x1eaf

            stereo.set( rs.frame_metadata_value.contrast, 0xdeaf )
            rgb.set( rs.frame_metadata_value.sharpness, 0xface )
            d3 = stereo.publish( depth.frame() )
            c2 = rgb.publish( color.frame() )
            assert not c2.supports_frame_metadata( rs.frame_metadata_value.contrast )
            assert not d3.supports_frame_metadata( rs.frame_metadata_value.sharpness )
#
#############################################################################################
