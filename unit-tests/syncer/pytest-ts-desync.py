# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import logging
import pytest
import pyrealsense2 as rs
import sw_syncer as sw

log = logging.getLogger(__name__)


# The timestamp jumps are closely correlated to the FPS passed to the video streams:
# syncer expects frames to arrive every 1000/FPS milliseconds!
#
# Module-scope autouse fixture: pytest runs the code before `yield` once before
# the first test in this file, the tests run at the `yield` point, and the code
# after `yield` runs once after the last test. autouse=True wires it into every
# test in the module without needing to declare it as a parameter.
@pytest.fixture(scope="module", autouse=True)
def _sw_session():
    sw.fps_c = sw.fps_d = 30
    sw.init( syncer_matcher = rs.matchers.dic_c )
    sw.start()
    yield  # tests in this module run here
    sw.stop()
    sw.reset()


#############################################################################################
def test_ts_desync():
    log.debug( "Init" )
    # It can take a few frames for the syncer to actually produce a matched frameset (it doesn't
    # know what to match to in the beginning)

    # D  C  @timestamp  comment
    # -- -- ----------- ----------------
    # 0     @0          so next expected frame timestamp is at 0+16.67
    #    0  @0
    #
    sw.generate_depth_and_color( frame_number = 0, timestamp = 0 )
    sw.expect( depth_frame = 0 )                          # syncer doesn't know about C yet, so releases right away
    sw.expect( color_frame = 0, nothing_else = True )     # no hope for a match: D@0 is already out, so it's released
    #
    # The syncer now knows about both streams, and is empty -- that was what we wanted

    #############################################################################################
    log.debug( "Go past Color's Next Expected; get a lone Depth frame" )

    # 1     @7952 -> NE=7985; it's released because WAY past C.NE
    #
    sw.generate_depth_frame( 1, 7952 )
    sw.expect( depth_frame = 1, nothing_else = True )

    #############################################################################################
    log.debug( "Generate a Color frame which will wait for Depth" )

    #    2  @7978 will wait, as it's ~= D.NE
    #
    sw.generate_color_frame( 2, 7978 )
    sw.expect_nothing()

    #############################################################################################
    log.debug( "Generate Depth for release BEFORE the waiting Color" )

    # 3     @7952 -> needs to be released BEFORE C2!!
    #
    # NOTE: the timestamp is the SAME AS BEFORE! Imagine that, instead of a Depth frame, this was
    # an Infrared: the matcher would be (TS: (TS: Depth Infra Confidence) Color). But we have no
    # Infra or Confidence mechanism (in sw) so we just generate another D -- it should have the
    # same effect:
    #
    # NOTE: this used to crash (see LRS-289)!
    #
    sw.generate_depth_frame( 3, 7952 )
    sw.expect( depth_frame = 3 )

    #############################################################################################
    log.debug( "And only then get the Color when we generate a matching Depth" )

    sw.expect_nothing()  # C is still waiting for D.NE!

    # 4     @7986
    #
    sw.generate_depth_frame( 4, 7986 )
    sw.expect( depth_frame = 4, color_frame = 2, nothing_else = True )
