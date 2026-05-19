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
    sw.fps_c = sw.fps_d = 60
    sw.init()
    sw.start()
    yield  # tests in this module run here
    sw.stop()
    sw.reset()


#############################################################################################
def test_ts_same_fps():
    log.debug( "Wait for framesets" )

    # It can take a few frames for the syncer to actually produce a matched frameset (it doesn't
    # know what to match to in the beginning)

    sw.generate_depth_and_color( frame_number = 0, timestamp = 0 )
    sw.expect( depth_frame = 0 )                          # syncer doesn't know about color yet
    sw.expect( color_frame = 0, nothing_else = True )     # less than next expected of D
    #
    # NOTE: if the syncer queue wasn't 100 (see above) then we'd only get the color frame!
    #
    sw.generate_depth_and_color( 1, sw.gap_d * 1 )
    sw.expect( depth_frame = 1, color_frame = 1, nothing_else = True )  # frameset 1

    #############################################################################################
    log.debug( "Keep going" )

    sw.generate_depth_and_color( 2, sw.gap_d * 2 )
    sw.expect( depth_frame = 2, color_frame = 2, nothing_else = True )  # frameset 2
    sw.generate_depth_and_color( 3, sw.gap_d * 3 )
    sw.generate_depth_and_color( 4, sw.gap_d * 4 )
    sw.expect( depth_frame = 3, color_frame = 3 )                       # frameset 3
    sw.expect( depth_frame = 4, color_frame = 4, nothing_else = True )  # frameset 4

    #############################################################################################
    log.debug( "Stop giving color; wait_for_frames() should throw (after 5s)" )

    sw.generate_depth_frame( 5, sw.gap_d * 5 )

    # We expect the syncer to try and wait for a Color frame to fit the Depth we just gave it.
    # The syncer relies on frame inputs to actually run -- and, if we wait for a frame, we're
    # not feeding it frames so can't get back any, either.
    with pytest.raises( RuntimeError, match = "Frame did not arrive in time!" ):
        sw.syncer.wait_for_frames()

    #############################################################################################
    log.debug( "try_wait_for_frames() allows us to keep feeding frames; eventually we get one" )

    # The syncer will not immediately release framesets because it's still waiting for a color frame to
    # match Depth #10. But, if we advance the timestamp sufficiently, it should eventually release it
    # as it deems Color "inactive".

    # The last color frame we sent was at gap*4, so it's next expected at gap*5 plus a buffer of gap*7
    # (see the code in the syncer)... so we expect depth frame 5 to be released only when we're past gap*12

    sw.generate_depth_frame( 6, sw.gap_d * 6 )
    sw.expect( nothing_else = True )
    sw.generate_depth_frame( 7, sw.gap_d * 11.5 )
    sw.expect( nothing_else = True )
    sw.generate_depth_frame( 8, sw.gap_d * 12.5 )
    #
    # We'll get all frames in a burst. Again, if the syncer queue was 1, we'd only get the last!
    #
    sw.expect( depth_frame = 5 )
    sw.expect( depth_frame = 6 )
    sw.expect( depth_frame = 7 )
    sw.expect( depth_frame = 8, nothing_else = True )
