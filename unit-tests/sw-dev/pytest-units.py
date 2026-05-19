# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import logging
import pyrealsense2 as rs
from pytest_check import check
import sw_device as sw

log = logging.getLogger(__name__)


def test_depth_units():
    with sw.sensor( "Stereo Module" ) as sensor:
        depth = sensor.video_stream( "Depth", rs.stream.depth, rs.format.z16 )
        sensor.start( depth )

        log.debug( "By default, frames do not have units" )
        f = depth.frame()
        check.equal( f.depth_units, 0. )

        # Publish it
        f = sensor.publish( f )

        log.debug( "rs.stream.depth should generate depth-frames" )
        df = rs.depth_frame( f )
        assert df  # hard-abort: legacy used on_fail=test.ABORT here

        log.debug( "No DEPTH_UNITS; Units should be 0" )
        check.is_false( sensor.supports( rs.option.depth_units ) )
        check.equal( df.get_units(), 0. )
        check.equal( df.get_distance( 0, 0 ), 0. )

        log.debug( "Set the sensor DEPTH_UNITS" )
        sensor.add_option( rs.option.depth_units, rs.option_range( 0, 1, 0.000001, 0.001 ), True )
        #check.equal( sensor.get_option( rs.option.depth_units ), 0.001 )   it's not set yet
        sensor.set_option( rs.option.depth_units, 0.001 )

        log.debug( "Frame units should not change after the fact" )
        sensor.set_option( rs.option.depth_units, 0.001 )
        check.equal( df.get_units(), 0. )
        check.equal( df.get_distance( 0, 0 ), 0. )

        log.debug( "But if we generate a new frame..." )
        f = depth.frame()
        check.equal( f.depth_units, 0. )

        # Publish it
        f = sensor.publish( f )
        df = rs.depth_frame( f )

        log.debug( "New frame should pick up DEPTH_UNITS" )
        check.almost_equal( df.get_units(), 0.001, abs=0.00000001 )
        # sw.py uses 0x69 to fill the buffer, and Z16 is 16-bit so the pixel value should be 0x6969
        # and the units are 0.001, so distance (pixel*units) should be 26.985:
        check.almost_equal( df.get_distance( 0, 0 ), 26.985, abs=0.000001 )
