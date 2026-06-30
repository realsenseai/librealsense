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
    sw.fps_d = 100
    sw.fps_c =  10
    sw.init()
    sw.start()
    yield  # tests in this module run here
    sw.stop()
    sw.reset()


#############################################################################################
def test_ts_diff_fps():
    log.debug( "Wait for framesets" )

    # It can take a few frames for the syncer to actually produce a matched frameset (it doesn't
    # know what to match to in the beginning)

    # D  C  @timestamp
    # -- -- -----------
    # 0     @0
    # 1     @10
    #    0  @0
    #
    sw.generate_depth_frame( frame_number = 0, timestamp = sw.gap_d * 0 )
    sw.generate_depth_frame( 1, sw.gap_d * 1 )  # @10
    sw.generate_color_frame( 0, sw.gap_c * 0 )  # @0 -- small latency
    sw.expect( depth_frame = 0 )                          # syncer doesn't know about color yet
    sw.expect( depth_frame = 1 )
    #expect( color_frame = 0, nothing_else = True )
    # We'd expect C0 to not wait for another frame, but it does: @0 is comparable to @20 (D.NE)
    # because it's using C.fps (10 fps -> 100 gap / 2 = 50ms error, so 0~=20)!
    sw.expect_nothing()

    # 2     @20
    sw.generate_depth_frame( 2, sw.gap_d * 2 )
    sw.expect( depth_frame = 2, color_frame = 0, nothing_else = True )

    #############################################################################################
    log.debug( "Depth waits for next Color" )

    # 3     @ 30
    # 4     @ 40
    # 5     @ 50
    # 6     @ 60
    # 7     @ 70
    # 8     @ 80
    # 9     @ 90
    # 10    @100 -> wait
    # 11    @110
    #
    sw.generate_depth_frame(  3, sw.gap_d *  3 ); sw.expect( depth_frame =  3 )
    sw.generate_depth_frame(  4, sw.gap_d *  4 ); sw.expect( depth_frame =  4 )
    sw.generate_depth_frame(  5, sw.gap_d *  5 ); sw.expect( depth_frame =  5 )
    sw.generate_depth_frame(  6, sw.gap_d *  6 ); sw.expect( depth_frame =  6 )
    sw.generate_depth_frame(  7, sw.gap_d *  7 ); sw.expect( depth_frame =  7 )
    sw.generate_depth_frame(  8, sw.gap_d *  8 ); sw.expect( depth_frame =  8 )
    sw.generate_depth_frame(  9, sw.gap_d *  9 ); sw.expect( depth_frame =  9 )
    sw.generate_depth_frame( 10, sw.gap_d * 10 ); #sw.expect( depth_frame = 10 )
    # C.NE is @100, so it should wait...
    sw.expect_nothing()
    sw.generate_depth_frame( 11, sw.gap_d * 11 );
    sw.expect_nothing()

    #    1  @100 -> release (D10,C1) and (D11)
    sw.generate_color_frame(  1, sw.gap_c *  1 );  # @100 -- small latency
    sw.expect( depth_frame = 10, color_frame = 1 )
    sw.expect( depth_frame = 11 )
    sw.expect_nothing()

    # 12    @120 doesn't wait
    sw.generate_depth_frame( 12, sw.gap_d * 12 );
    sw.expect( depth_frame = 12 )
    sw.expect_nothing()

    #############################################################################################
    log.debug( "Color is early" )

    # 13    @130
    # 14    @140
    # 15    @150
    # 16    @160
    # 17    @170
    #    2  @200 -> wait
    # 18    @180 -> (D18,C2) ?! why not wait for (D20,C2) ?!
    #
    sw.generate_depth_frame( 13, sw.gap_d * 13 ); sw.expect( depth_frame = 13 )
    sw.generate_depth_frame( 14, sw.gap_d * 14 ); sw.expect( depth_frame = 14 )
    sw.generate_depth_frame( 15, sw.gap_d * 15 ); sw.expect( depth_frame = 15 )
    sw.generate_depth_frame( 16, sw.gap_d * 16 ); sw.expect( depth_frame = 16 )
    sw.generate_depth_frame( 17, sw.gap_d * 17 ); sw.expect( depth_frame = 17 )

    sw.generate_color_frame( 2, sw.gap_c * 2 ); sw.expect_nothing()
    # We're waiting for D.NE @180, because it's comparable (using min fps of the two)

    sw.generate_depth_frame( 18, sw.gap_d * 18 )
    # Now we get both back:
    sw.expect( depth_frame = 18, color_frame = 2, nothing_else = True )

    # But wait... why match C@200 to D@180 and not wait for D@200??
    # If we used the faster FPS of the two, 180!=200: we'd get D18, D19, then (D20,C20)
    #expect( depth_frame = 18, nothing_else = True )
    #generate_depth_frame( 19, sw.gap_d * 19 )
    #expect( depth_frame = 19, nothing_else = True )
    #generate_depth_frame( 20, sw.gap_d * 20 )
    #expect( depth_frame = 20, color_frame = 2, nothing_else = True )

    #############################################################################################
    log.debug( "Stop depth" )

    sw.generate_color_frame( 3, sw.gap_c * 3 )  # @300

    # D.NE is @190, plus 7*gap_d gives a cutout of @260, so the frame shouldn't wait for depth
    # to arrive, but it does:
    sw.expect_nothing()

    # The reason is that it's not using gap_d: it's using gap_c, so cutout is @890:
    sw.generate_color_frame( 4, sw.gap_c * 8 )
    sw.expect_nothing()
    sw.generate_color_frame( 5, sw.gap_c * 9 )
    sw.expect( color_frame = 3 )
    sw.expect( color_frame = 4 )
    sw.expect( color_frame = 5 )
    sw.expect_nothing()
